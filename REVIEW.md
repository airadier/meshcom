# MeshCom — Revisión Técnica del Firmware

> Revisión realizada el 2026-03-09. Cubre todo el código fuente, tests y documentación.

---

## 1. Resumen del estado actual

El proyecto está en **fase PoC (v0.1)** — compilable para ESP32 con ESP-IDF v5.2. La arquitectura es sólida y bien pensada: modular, con separación limpia de responsabilidades y buena documentación de diseño.

**Lo que funciona (lógica implementada):**
- ✅ Cifrado/descifrado AES-128-GCM con wire format bien definido
- ✅ Gestión de grupos con persistencia NVS
- ✅ VAD básico por energía
- ✅ Anti-duplicados por ventana de secuencia
- ✅ Pairing de grupo por ESP-NOW (share/join)
- ✅ Scaffold completo de BT HFP AG (init, scan, reconnect)
- ✅ UI con detección de hold de botones y patrones LED
- ✅ Tests host (11/11 pasan) con mocks para ESP-IDF

**Lo que falta para funcionar en hardware real:**
- ❌ `bt_hfp_send_audio()` es un stub (no hay ring buffer para SCO outgoing)
- ❌ `hfp_outgoing_data_cb()` devuelve silencio (no lee del mesh)
- ❌ Display ST7789V2 no implementado (solo log)
- ❌ Sin Opus — audio PCM raw puede exceder 250B de ESP-NOW
- ❌ Sin reconexión automática BT tras pérdida de enlace

---

## 2. Bugs y errores encontrados

### 🔴 P0 — Críticos (bloquean funcionalidad básica)

#### B1. Audio recibido del mesh no llega al intercom
**Archivo:** `bt_hfp.c:127-133` (`bt_hfp_send_audio`)
**Problema:** La función es un no-op. No hay ring buffer que almacene los PCM frames recibidos del mesh para que `hfp_outgoing_data_cb` los consuma. El flujo RX completo está roto.
**Impacto:** Nadie escucha nada — el audio del mesh se descifra correctamente pero se descarta.
**Fix:** Implementar un ring buffer thread-safe (ej. `xStreamBuffer` de FreeRTOS o buffer circular con mutex) que `bt_hfp_send_audio()` llene y `hfp_outgoing_data_cb()` consuma.

#### B2. Tamaño de payload PCM puede exceder ESP-NOW
**Archivo:** `audio_pipe.c:72-79`
**Problema:** Un frame HFP SCO es típicamente 60 bytes PCM, pero puede variar. Con `GROUP_OVERHEAD` = 28 bytes, el paquete cifrado ocupa 88 bytes — cabe en los 250B de ESP-NOW. **Pero** si el callback HFP entrega frames más grandes (posible en configuraciones SCO con mSBC o buffers acumulados), se superarían los 250B y se descartan silenciosamente.
**Impacto:** Pérdida de audio sin diagnóstico.
**Fix:** Fragmentar o limitar el tamaño en `audio_pipe_send()` con un assert o log adecuado. A medio plazo: Opus comprime a ~20-40B por frame.

### 🟡 P1 — Importantes (afectan robustez)

#### B3. Sequence number overflow a 0 causa falso duplicado
**Archivo:** `audio_pipe.c:65`
**Problema:** `s_tx_seq` es `uint16_t` y wrappea a 0 tras 65535. La ventana de dedup inicializada con `0xFFFF` no contiene 0, pero tras una primera transmisión exitosa con seq=0, el siguiente ciclo completo (tras 65536 paquetes ≈ 33 minutos a 30fps) re-enviará seq=0 que podría estar aún en la ventana. Más grave: si el dispositivo se reinicia (seq vuelve a 0) mientras otro dispositivo tiene seqs bajos en su ventana, se pierden los primeros ~32 paquetes.
**Impacto:** Pérdida de audio de hasta 1 segundo al reiniciar un dispositivo.
**Fix:** Inicializar `s_tx_seq` con un valor aleatorio en `audio_pipe_init()`. Considerar un timestamp parcial en la cabecera para distinguir sesiones.

#### B4. Race condition en pairing: `s_sharing` leído sin protección
**Archivo:** `pairing.c:42-47` (share_task) y `pairing.c:57` (timeout_cb)
**Problema:** `s_sharing` es un `bool` que la task `share_task` lee en un tight loop, mientras que `pairing_timeout_cb` (ejecutado desde el timer daemon task) lo escribe a `false`. En ARM Cortex esto probablemente funcione (escrituras atómicas a bool), pero es UB en C y no portable.
**Impacto:** Bajo en ESP32 (hardware atómico para words), pero mala práctica.
**Fix:** Usar `atomic_bool` de `<stdatomic.h>` o un `TaskNotify` para señalizar el fin.

#### B5. `share_task` no se termina limpiamente al salir de pairing manualmente
**Archivo:** `pairing.c:41-49`
**Problema:** Si se llama a `pairing_start_share()` y luego el timer expira (`s_sharing = false`), la task sale. Pero si el usuario inicia otra acción (JOIN mientras SHARE está activo), `pairing_start_join()` retorna `ESP_ERR_INVALID_STATE` — no hay forma de cancelar un SHARE activo para hacer JOIN.
**Impacto:** UX confusa — no se puede cambiar de modo sin esperar 30s.
**Fix:** Añadir `pairing_stop()` que ponga `s_active = false; s_sharing = false;` y espere a que la task termine (`vTaskDelete` o join).

#### B6. NVS mock con variables estáticas globales — contaminación entre tests
**Archivo:** `test/host/esp_stubs/nvs.h`
**Problema:** Las variables `_nvs_blob`, `_nvs_has_blob`, etc. son `static` en un header — cada translation unit tiene su copia. `group_mgr.c` y los tests usan copias diferentes. Si el mock NVS se modifica desde un test, `group_mgr.c` no ve el cambio y viceversa. **Funciona por casualidad** porque `group_mgr_init()` genera un grupo nuevo cuando NVS falla, y los tests no verifican persistencia cross-init.
**Impacto:** Los tests de NVS persistence no prueban realmente la persistencia.
**Fix:** Mover las variables NVS al .c de mocks y exponer funciones de acceso, no `static` en header.

### 🟢 P2 — Menores

#### B7. `device_has_hfp()` siempre retorna true
**Archivo:** `bt_hfp.c:68-76`
**Problema:** La función no parsea el EIR, acepta cualquier dispositivo BT. Si hay varios dispositivos BT visibles, se conectará al de mayor RSSI sin verificar que sea un intercom HFP.
**Impacto:** Bajo para PoC (usuario pone solo el intercom en pairing), pero arriesgado en entornos reales.

#### B8. `display_update()` se llama cada 50ms sin throttling
**Archivo:** `ui.c:131-135` (ui_task)
**Problema:** 20 veces por segundo se llama a `display_update()` que actualmente solo hace `ESP_LOGI`. Cuando se implemente el driver ST7789V2, esto será excesivo (SPI completo cada 50ms).
**Impacto:** Futuro — desperdicio de CPU/SPI.
**Fix:** Actualizar display solo cuando cambie el estado (flag `s_display_dirty`).

---

## 3. Mejoras recomendadas

### Rendimiento

| # | Mejora | Prioridad | Impacto |
|---|--------|-----------|---------|
| M1 | **Implementar ring buffer para SCO TX** | Alta | Habilita flujo RX completo |
| M2 | **Integrar Opus** (16kHz VOIP mode, ~20B/frame) | Alta | Reduce payload de 60B a 20B, permite más frames por segundo |
| M3 | **VAD con histéresis** — hold time de 200-300ms para evitar cortes de sílabas | Media | Mejora calidad de voz percibida |
| M4 | **Usar `esp_now_send()` con callback** para flow control | Media | Evita pérdida por envío en ráfaga |

### Robustez

| # | Mejora | Prioridad | Impacto |
|---|--------|-----------|---------|
| M5 | **Reconexión BT automática** — timer periódico si `!s_connected` | Alta | Supervivencia a desconexiones del intercom |
| M6 | **Watchdog** — `esp_task_wdt_add()` en tasks críticas | Media | Recuperación ante hangs |
| M7 | **Error counters** — contar decrypt fails, ESP-NOW send fails, etc. para diagnóstico | Media | Depuración en campo |
| M8 | **Proteger variables compartidas** con mutexes o atomics (esp. `s_connected`, `s_sco_open`, `s_active`) | Media | Correctitud en presencia de preemption |
| M9 | **Manejar `ESP_ERROR_CHECK` con gracia** en runtime (no abort) | Baja | No crashear en producción por errores recuperables |

### Seguridad

| # | Mejora | Prioridad | Impacto |
|---|--------|-----------|---------|
| M10 | **Nonce reutilización**: el nonce de 8 bytes random tiene probabilidad de colisión de ~1/2^32 tras ~65K paquetes (birthday paradox). Con audio a 30fps, eso es ~36 minutos. Colisión de nonce en GCM rompe la confidencialidad. | Alta | **Usar contador monotónico** como parte del nonce en lugar de puro random |
| M11 | **ECDH para pairing** (ya documentado como TODO) | Media | Protege contra sniffing de clave durante pairing |
| M12 | **Rate limiting** en recepción de pairing packets | Baja | Protege contra DoS de pairing |

### Arquitectura

| # | Mejora | Prioridad | Impacto |
|---|--------|-----------|---------|
| M13 | **Separar board abstraction** en `board.c` con funciones vs `#ifdef` en cada módulo | Baja | Limpieza de código |
| M14 | **Event system** (FreeRTOS event groups o queue) en lugar de llamadas directas entre módulos | Media | Reduce acoplamiento |
| M15 | **Power management** — light sleep entre transmisiones si VAD está en silencio | Baja | Mayor autonomía |

---

## 4. Gaps en los tests

### Cobertura actual
- ✅ `group_mgr`: encrypt/decrypt roundtrip, wrong group, tamper detection, new group, save/load key
- ✅ `audio_pipe`: VAD silence/loud/boundary, dedup reject/accept/window wrap
- **11 tests, 0 failures**

### No cubierto (debería estarlo)

| Gap | Módulo | Prioridad |
|-----|--------|-----------|
| **T1. Encrypt con buffer demasiado pequeño** | group_mgr | Alta |
| **T2. Decrypt de paquete truncado** (ej. 5 bytes) | group_mgr | Alta |
| **T3. Decrypt con `out_size` insuficiente** | group_mgr | Media |
| **T4. `audio_pipe_send` con len=0** | audio_pipe | Media |
| **T5. `audio_pipe_send` con len impar** (no alineado a 16-bit sample) | audio_pipe | Media |
| **T6. Sequence number wrap (65535 → 0)** | audio_pipe | Media |
| **T7. Múltiples `group_mgr_init()` consecutivos** (re-init) | group_mgr | Baja |
| **T8. Encrypt/decrypt con payload máximo** (~220B, al límite de ESP-NOW) | group_mgr | Media |
| **T9. Pairing packet con magic correcto pero datos truncados** | pairing | Alta |
| **T10. Pairing while not in JOIN mode** (should be ignored) | pairing | Media |
| **T11. NVS persistence real** (save, re-init, verify key loaded) | group_mgr | Alta — actualmente no funciona por bug B6 |
| **T12. VAD con señales negativas** (samples < 0) | audio_pipe | Baja |
| **T13. Test que `group_mgr_get_key/id` falla si no inicializado** | group_mgr | Baja |

### Módulos sin tests
- `bt_hfp.c` — requiere hardware o mock complejo de Bluedroid
- `espnow_comm.c` — requiere hardware o mock de esp_now
- `pairing.c` — testeable con mocks pero no hay tests
- `ui.c` — testeable parcialmente (lógica de hold detection)

---

## 5. Próximos pasos sugeridos

### Fase inmediata (hacer funcionar el audio end-to-end)

1. **Implementar ring buffer para SCO outgoing** (B1) — sin esto no hay audio RX. Usar `xStreamBufferCreate()` de FreeRTOS, ~2KB buffer.
2. **Fijar nonce con contador monotónico** (M10) — riesgo de seguridad real con nonces random.
3. **Inicializar `s_tx_seq` con random** (B3) — fix trivial de una línea.
4. **Mover variables NVS mock a .c** (B6) — fix de 10 minutos que habilita tests de persistencia reales.

### Fase corta (robustez básica)

5. **Reconexión BT automática** (M5) — timer de 10s que llame a `bt_hfp_reconnect()` si desconectado.
6. **VAD con histéresis** (M3) — hold de 200ms tras último frame con voz.
7. **Añadir tests T1, T2, T9** — edge cases de seguridad/robustez.
8. **Proteger compartidas con atomics** (M8) — al menos `s_connected`, `s_sco_open`.

### Fase media (preparar para hardware real)

9. **Integrar Opus** — reduce payload, mejora calidad a bajo bitrate.
10. **Implementar driver ST7789V2** para M5StickC Plus 2.
11. **Parsear EIR en scan BT** (B7) — filtrar solo dispositivos HFP HF.
12. **Probar con intercoms reales** — validar latencia y coexistencia BT+WiFi.

### Fase larga (producto)

13. **ECDH pairing** (M11)
14. **OTA updates**
15. **PCB diseñada + caja 3D**

---

## 6. Observaciones generales

**Puntos fuertes del proyecto:**
- Documentación excelente (DESIGN.md es un modelo de documento de decisiones técnicas)
- Arquitectura modular y limpia
- Tests host compilables sin hardware — facilita CI
- Wire format del paquete bien pensado con AAD para grupo+seq
- Buena elección tecnológica (ESP-NOW, ESP32 clásico con BT Classic)

**Puntos a mejorar:**
- La separación "compilable" vs "funcional" debería estar más explícita — alguien podría pensar que flasheando funciona
- Falta un `CHANGELOG.md` o tags de versión
- El `sdkconfig` debería tener comentarios sobre las opciones BT/WiFi habilitadas
- Considerar un CI con GitHub Actions que ejecute los host tests automáticamente

---

*Informe generado automáticamente como parte de la revisión técnica del proyecto.*
