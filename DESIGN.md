# MeshCom — Análisis Técnico y Decisiones de Diseño

> Documento generado el 2026-03-01 a partir de la sesión de diseño inicial.

---

## 1. Problema a resolver

Los intercomunicadores de moto baratos no permiten hablar en grupo. Las alternativas como Discord presentan:
- Alta latencia
- Ruido de fondo mal gestionado
- Problemas con desconexión/reconexión
- Dependencia de cobertura de datos

**Objetivo:** comunicación de voz tipo walkie-talkie para grupos de moteros, funcionando sin internet, con baja latencia y tolerante a cortes por curvas y obstáculos.

---

## 2. Escenario de uso

- Grupo de 3–8 moteros
- Van juntos pero pueden separarse decenas de metros
- Curvas de montaña, túneles, obstrucciones físicas
- Los móviles llevan intercoms Bluetooth (HFP) ya funcionando con Discord
- Se quiere eliminar la dependencia de datos

---

## 3. Opciones de conectividad exploradas

### App Android con Wi-Fi local

**Ventajas:**
- No requiere hardware extra
- BT HFP ya probado (Discord funciona)

**Problemas graves:**
- Requiere nodo central (AP o Wi-Fi Direct Group Owner)
- Si cae el nodo, cae toda la red
- Android background muy restrictivo en audio continuo (Android 12+)
- Latencia 40–80ms
- Batería del móvil se drena rápido (Wi-Fi + BT + audio)
- Reconexión lenta en fallos de AP (5–15 segundos)

### App Android + bbTalkie por cable (Frankenstein)

Se estudió conectar el móvil al intercom por BT y a un dispositivo ESP32 por jack 3.5mm o USB-C, para que el ESP32 gestionase la comunicación inalámbrica.

**Problema:** el routing de audio en Android entre interfaz BT y wired simultáneamente añade complejidad sin simplificar el software. Más cable, más puntos de fallo. **Descartado.**

### ESP32 standalone — ✅ Opción elegida

Un pequeño dispositivo físico por rider que:
- Se conecta al intercom del casco por **Bluetooth HFP AG** (el ESP32 se comporta como un "teléfono" ante el intercom)
- Se comunica con otros dispositivos por **ESP-NOW** (P2P sin AP, sin internet)

---

## 4. Por qué ESP-NOW

ESP-NOW es un protocolo propietario de Espressif que opera sobre la capa física Wi-Fi sin asociarse a ninguna red:

- Envía "vendor action frames" directamente entre dispositivos
- Sin AP, sin IP, sin TCP/UDP
- **Broadcast nativo:** un dispositivo transmite, todos los que están en rango reciben
- **Latencia:** <5ms (frente a 40–80ms de soluciones sobre Wi-Fi normal)
- **Alcance:** similar a Wi-Fi 2.4GHz, mejorable con antena externa (~100–200m en campo abierto)
- Sin nodo central → si un rider cae fuera de rango, el resto sigue hablando

Esta es la arquitectura que usa **bbTalkie** (github.com/RealCorebb/bbTalkie), un walkie-talkie DIY basado en ESP32-S3 que fue tomado como referencia de inspiración.

---

## 5. Por qué ESP32 original (no S3)

bbTalkie usa **ESP32-S3**, que solo tiene BLE (Bluetooth Low Energy). HFP requiere **Bluetooth Classic (BR/EDR)**.

El **ESP32 original** (WROOM-32) tiene:
- ✅ Bluetooth Classic + BLE
- ✅ Wi-Fi (ESP-NOW)
- ✅ Dual-core Xtensa LX6 @ 240MHz
- ✅ HFP AG soportado en ESP-IDF
- ✅ Aceleración hardware AES
- ✅ Suficiente potencia para Opus 16kHz + VAD básico + noise gate
- ✅ Precio: ~4–6€ el módulo

No necesitamos las instrucciones vectoriales del S3 porque no hay speech-to-text ni keyword detection (eso requería la potencia del S3 en bbTalkie).

**Módulo recomendado:** `ESP32-WROOM-32U` (variante con conector de antena externa para mayor alcance).

---

## 6. Arquitectura del sistema

```
Rider 1:
  Intercom casco ←—BT HFP AG—→ [ESP32] ←—ESP-NOW broadcast—→ [ESP32] ←—BT HFP AG—→ Intercom casco
                                                ↕ (todos en rango)
                                            [ESP32] ←—BT HFP AG—→ Intercom casco
                                          Rider 3

App móvil (opcional):
  [Móvil] ←—BLE—→ [ESP32]   (solo para config/UI, no en el path de audio)
```

### Flujo de audio

```
Rider habla → mic intercom → BT HFP SCO → ESP32 captura audio
→ VAD (¿hay voz?) → si sí: encode Opus → cifrado AES-128-GCM
→ ESP-NOW broadcast → otros ESP32 reciben
→ descifrar → decode Opus → BT HFP SCO → altavoz intercom rider 2, 3...
```

---

## 7. Grupos y cifrado

ESP-NOW broadcast no soporta cifrado nativo (solo unicast). Se implementa cifrado a nivel de aplicación:

### Formato de paquete

```
┌─────────────────────────────────────────────┐
│ group_id   (2 bytes, en claro)              │
│ seq        (2 bytes, en claro)              │
│ nonce      (8 bytes)                        │
│ payload cifrado AES-128-GCM:                │
│   └─ audio Opus (~100–200 bytes)            │
│   └─ auth tag (16 bytes)                    │
└─────────────────────────────────────────────┘
```

- **group_id:** permite filtrar paquetes de grupos ajenos sin intentar descifrar
- **AES-128-GCM:** confidencialidad + autenticidad (nadie puede inyectar audio falso)
- **Hardware AES** del ESP32: coste computacional casi nulo
- Un dispositivo puede escuchar múltiples `group_id` simultáneamente (subgrupos/canales)

### Gestión de claves

La clave de grupo se gestiona íntegramente desde el dispositivo físico, sin app ni configuración externa. Ver sección 11b para el flujo completo; en resumen:

- **Generación:** mantener el botón de grupo 10 segundos → genera una nueva clave AES aleatoria y resetea el grupo.
- **Compartir (share):** pulso corto → el dispositivo emite la clave actual por ESP-NOW durante 15–30 s.
- **Unirse (join):** mantener 2 segundos → el dispositivo escucha; si hay alguien en modo share, recibe y guarda la clave.
- **Persistencia:** la clave se almacena en NVS (flash) y sobrevive a apagados y reinicios.

> ~~Métodos anteriores descartados: QR code (requería app + BLE), PIN compartido (PBKDF2), preflasheado (clave fija). Ninguno se implementa.~~

---

## 8. Stack de audio

| Componente | Decisión |
|-----------|----------|
| Codec | **Opus** modo VOIP, 16kHz, 12–24 kbps |
| VAD | Básico por energía/ZCR (suficiente para walkie) |
| Noise suppression | Noise gate + filtro paso-bajo sencillo (MVP); RNNoise si hay recursos |
| Latencia objetivo | <20ms end-to-end (encoding + red + decoding) |
| Activación | Push-to-talk o VAD automático |

---

## 9. Hardware BOM (por unidad)

| Componente | Precio aprox. |
|-----------|--------------|
| ESP32-WROOM-32U | ~5€ |
| Antena 2.4GHz externa (conector IPEX) | ~2€ |
| Batería LiPo 1000mAh | ~3€ |
| Módulo cargador TP4056 | ~1€ |
| PCB + caja impresa 3D | ~5€ |
| **Total estimado** | **~15–16€** |

---

## 10. Comparativa final de opciones

| | App Android | App + cable ESP32 | **ESP32 standalone** |
|---|---|---|---|
| Nodo central | Necesario | Necesario | **No (ESP-NOW)** |
| Latencia | 40–80ms | 40–80ms | **<10ms** |
| Failover | Complejo | Complejo | **Innecesario** |
| Batería móvil | Se drena | Se drena | **Cero impacto** |
| Background Android | Problema | Problema | **No aplica** |
| Alcance | ~50–80m | ~50–80m | **100–200m+** |
| Coste por rider | 0€ | ~20€ | **~15€** |
| Complejidad SW | Alta | Muy alta | **Media** |
| Hardware extra | Ninguno | Cable + ESP32 | **ESP32 en caja** |

---

## 11. Viabilidad por plataforma

| Plataforma | Viabilidad | Notas |
|-----------|-----------|-------|
| ESP32 standalone | ✅ **8/10** | Opción elegida para MVP |
| Android app pura | ⚠️ 6/10 | Viable pero con restricciones severas de OS |
| iOS app pura | ❌ 2/10 | Restricciones demasiado fuertes para este caso |
| iOS como cliente pasivo | ⚠️ 5/10 | Posible con CallKit, ciudadano de segunda clase |

---

## 11b. UX de emparejamiento — solo pulsador físico

No se requiere app móvil para gestionar grupos. Todo se opera con un único pulsador y un LED.

### Comportamiento del pulsador

| Acción | Resultado |
|--------|-----------|
| **Primera vez (boot)** | Autogenera clave AES aleatoria → queda en su propio grupo |
| **Mantener 10s** | Genera nueva clave → resetea grupo |
| **Pulso corto** | Modo *share*: emite la clave actual por ESP-NOW durante 15–30s |
| **Mantener 2s** | Modo *join*: escucha pairing; si hay un dispositivo emitiendo, recibe y guarda la clave |

La clave se persiste en **NVS** (Non-Volatile Storage, flash del ESP32) — sobrevive a apagados y reinicios.

### Estados del LED

| Patrón | Significado |
|--------|-------------|
| **Fijo** | En grupo, operativo |
| **Parpadeo lento** | Modo share — emitiendo clave |
| **Parpadeo rápido** | Modo join — esperando/recibiendo clave |

### Flujo típico en una salida

1. Todos encienden sus dispositivos (ya conectados a sus intercoms)
2. El jefe de ruta hace **pulso corto** → LED parpadeo lento
3. El resto hace **2s pulsado** → LED parpadeo rápido → al recibir clave, LED fijo
4. A rodar
5. Al volver, simplemente apagar (la clave se mantiene para la próxima salida)

### Nota de seguridad

Durante el pairing, la clave viaja en claro por ESP-NOW. Para el modelo de amenaza de un walkie de motos es aceptable: un atacante necesitaría estar físicamente en rango durante la ventana exacta de 15–30 segundos con hardware ESP32. Si en el futuro se requiere mayor seguridad, ECDH (Diffie-Hellman sobre curva elíptica, soportado por mbedTLS en ESP-IDF) resolvería esto de forma transparente sin cambiar el UX.

---

## 11c. UX de emparejamiento con el intercom (BT)

El M5StickC Plus 2 tiene dos botones, lo que permite separar funciones:

- **Botón A** → gestión de grupo (ver sección 11b)
- **Botón B** → gestión de conexión BT con el intercom

| Acción botón B | Resultado |
|----------------|-----------|
| **Hold 3s** | Entra en modo discoverable 60s — el ESP32 aparece como `MeshCom-XXXX` en el menú del intercom. El usuario conecta desde los ajustes BT del intercom. Dirección guardada en NVS. |
| **Pulso corto** | Fuerza reconexión al intercom guardado |
| **Primera vez (sin intercom en NVS)** | Entra automáticamente en modo discoverable al arrancar |

Pantalla muestra: `BT: Pairing...` / `BT: Connected` / `BT: Lost`

Para **DevKit genérico** (un solo botón BOOT/GPIO0): al primer arranque sin intercom guardado entra en discoverable automáticamente. Para volver a emparejar: mantener BOOT 10s.

---

## 12. Retos pendientes de validar

1. **BT HFP AG + ESP-NOW simultáneo:** coexistencia de BT Classic y Wi-Fi en el mismo chip (comparten radio 2.4GHz). ESP-IDF lo soporta con time-slicing pero hay que validar en práctica.
2. **Audio bidireccional SCO:** captura y reproducción HFP simultáneas con buena calidad.
3. **Reconexión automática al intercom:** si el rider apaga/enciende el intercom, que el ESP32 reempareje solo.
4. **Forma/factor y montaje en moto:** caja resistente a vibración, temperatura y lluvia.
5. **Consumo en ruta larga:** el ESP32 con BT+Wi-Fi activos consume ~150–200mA. Con LiPo 1000mAh → ~5h. Suficiente para una ruta, pero valorar batería más grande.

---

## 13. Hoja de ruta

### Fase 0 — PoC de audio (objetivo: 1–2 semanas)
- [ ] ESP32 captura audio por BT HFP AG desde intercom real
- [ ] ESP32 reproduce audio por BT HFP AG en intercom
- [ ] Envío de audio raw entre dos ESP32 por ESP-NOW
- [ ] Validar latencia y calidad subjetiva con intercoms reales

### Fase 1 — MVP funcional
- [ ] Integrar Opus encoding/decoding
- [ ] VAD básico
- [ ] Grupos y cifrado AES-128-GCM
- [ ] Noise gate básico
- [ ] LED indicador de estado (conectado, transmitiendo, error)

### Fase 2 — Robustez
- [ ] Reconexión automática BT
- [ ] App móvil mínima para config de grupos (BLE)
- [ ] PCB diseñada, caja impresa 3D

### Fase 3 — Extras
- [ ] RNNoise para mejor supresión de ruido de casco/viento
- [ ] Fallback a red de datos (internet) cuando hay cobertura
- [ ] Soporte iOS como cliente pasivo

---

## 14. Referencias

- [bbTalkie](https://github.com/RealCorebb/bbTalkie) — walkie DIY ESP32-S3 con ESP-NOW, VAD y speech-to-text. Inspiración principal para la arquitectura ESP-NOW y el stack de audio.
- [ESP-NOW docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [ESP-IDF HFP AG example](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/classic_bt/bt_hfp_ag)
- [Opus codec](https://opus-codec.org/)
- [RNNoise](https://jmvalin.ca/demo/rnnoise/)
