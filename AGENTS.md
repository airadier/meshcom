# MeshCom — Agent Instructions

## Proyecto

Walkie-talkie ESP32 para grupos de moteros. Comunicación local por ESP-NOW (sin AP, sin internet), audio BT HFP AG con el intercom del casco, grupos cifrados con AES-128-GCM.

## Hardware objetivo

- **Primario:** M5Stack StickC Plus 2 (ESP32-PICO-V3-02) — `#define BOARD_M5STICKC_PLUS2`
- **Secundario:** ESP32 DevKit genérico — `#define BOARD_DEVKIT`
- Selección via `board.h`

## Stack técnico

- **Framework:** ESP-IDF nativo (no Arduino)
- **BT:** HFP AG (ESP32 actúa como "teléfono" ante el intercom)
- **Red:** ESP-NOW broadcast en canal 1 (2.4GHz), sin AP
- **Audio:** PCM 8kHz 16-bit mono (HFP SCO) — Opus está como TODO
- **Cifrado:** AES-128-GCM via mbedTLS (incluido en ESP-IDF)
- **Persistencia:** NVS (Non-Volatile Storage)
- **Tests:** Unity (host) para lógica pura, pytest-embedded para hardware

## Estructura de componentes

```
main/
├── main.c          # Entry point
├── board.h         # Defines de hardware
├── bt_hfp.c/h      # Bluetooth HFP AG
├── espnow_comm.c/h # ESP-NOW
├── group_mgr.c/h   # Grupos, claves, AES-GCM, NVS
├── audio_pipe.c/h  # Bridge HFP↔ESP-NOW, VAD
├── pairing.c/h     # Emparejamiento de grupo (botones)
└── ui.c/h          # Botones, LED, pantalla
```

## Formato de paquete de audio

```
[group_id: 2B][seq: 2B][nonce: 8B][payload AES-GCM cifrado][auth_tag: 16B]
```

## UX de botones

### M5StickC Plus 2
- **Botón A corto:** modo SHARE (emite clave 30s)
- **Botón A 2s:** modo JOIN (recibe clave 30s)
- **Botón A 10s:** nuevo grupo
- **Botón B 3s:** BT discoverable (emparejar intercom)
- **Botón B corto:** reconectar intercom

### DevKit (BOOT/GPIO0)
- **Corto:** SHARE
- **2s:** JOIN
- **5s:** nuevo grupo
- **10s:** BT discoverable

## Reglas de desarrollo

- No implementar Opus todavía — dejar `TODO(opus)` comentado
- No implementar ECDH todavía — pairing en claro, dejar `TODO(ecdh)` comentado
- PCM raw para el PoC es suficiente
- Cada módulo debe tener su `.h` con API clara y comentada
- FreeRTOS tasks separadas para BT, ESP-NOW rx, UI
- Al modificar un módulo, actualizar `docs/ARCHITECTURE.md`

## Tests

- `test/host/` — tests Unity que compilan y corren en Linux (sin hardware)
  - `test_group_mgr.c` — cifrado/descifrado AES-GCM, generación de claves, NVS mock
  - `test_audio_pipe.c` — VAD (umbral de energía), anti-duplicate (ventana de seq)
- `test/hardware/` — tests Unity que corren en ESP32 físico (futuro)
- Ejecutar host tests: `idf.py --preview set-target linux && idf.py build && ./build/meshcom.elf`

## Commits

- Mensajes descriptivos en inglés
- Formato: `feat:` / `fix:` / `test:` / `docs:`
- Push a `git@github.com:airadier/meshcom.git` rama `main`
