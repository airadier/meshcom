# MeshCom Architecture

## Overview

MeshCom is a mesh walkie-talkie for motorcycle groups. Each rider carries an ESP32 that connects to their helmet intercom via Bluetooth HFP and communicates with other riders via ESP-NOW broadcast.

```
┌─────────────┐    BT HFP SCO    ┌──────────────────────────────────┐    ESP-NOW     ┌──────────────┐
│   Helmet     │◄────────────────►│          ESP32 (MeshCom)         │◄──────────────►│  Other ESP32s│
│   Intercom   │   PCM 8kHz/16b   │                                  │   AES-GCM      │  in group    │
│   (HFP HF)   │                  │  bt_hfp ↔ audio_pipe ↔ espnow   │   encrypted    │              │
└─────────────┘                   └──────────────────────────────────┘                └──────────────┘
```

## Components

### main.c — Entry Point
Initializes all subsystems in order: NVS → group_mgr → audio_pipe → espnow_comm → bt_hfp → ui.

### bt_hfp.c — Bluetooth HFP Audio Gateway
- Registers as HFP AG (the ESP32 acts as a "phone")
- Helmet intercom connects as HFP HF device
- Manages pairing, NVS persistence of intercom address, auto-reconnect
- Opens SCO link for bidirectional PCM audio (8kHz, 16-bit, mono)
- Routes incoming PCM to `audio_pipe_send()`

### espnow_comm.c — ESP-NOW Mesh Communication
- WiFi in station mode (no AP connection needed)
- Fixed channel 1 for all devices
- Broadcast to FF:FF:FF:FF:FF:FF
- Routes incoming packets to either `pairing_handle_packet()` or `audio_pipe_receive()`

### group_mgr.c — Group Management & Encryption
- AES-128-GCM encryption/decryption using mbedTLS
- Group key and ID persisted in NVS
- Auto-generates group on first boot
- Wire packet format:
  ```
  [group_id: 2B][seq: 2B][nonce: 8B][ciphertext: NB][auth_tag: 16B]
  ```

### audio_pipe.c — Audio Bridge
- Bridges HFP SCO ↔ ESP-NOW with encrypt/decrypt
- VAD (Voice Activity Detection): energy-based, only transmits when voice detected
- Anti-duplicate: sliding window of 32 sequence numbers
- Sequence numbering for packet ordering

### pairing.c — Group Key Exchange
- SHARE mode: broadcasts group key every 500ms for 30 seconds
- JOIN mode: listens for pairing packets for 30 seconds
- Pairing packet: `[MAGIC "MCPR": 4B][group_id: 2B][key: 16B]`
- ⚠️ Plaintext for PoC — TODO: ECDH key exchange

### ui.c — User Interface
- Button polling with hold-time detection
- LED patterns: solid (idle), slow blink (share), fast blink (join)
- Display support for M5StickC Plus 2 (ST7789V2, 135×240)
- Board abstraction via `board.h` (#ifdef BOARD_DEVKIT)

## Data Flow

### Transmit (rider speaks)
```
Intercom mic → HFP SCO → bt_hfp callback → audio_pipe_send()
  → VAD check → group_mgr_encrypt() → espnow_broadcast()
```

### Receive (other rider speaks)
```
ESP-NOW recv_cb → audio_pipe_receive()
  → group_mgr_decrypt() → dedup check → bt_hfp_send_audio()
  → HFP SCO → Intercom speaker
```

### Pairing
```
Device A (SHARE): group_mgr_get_key() → broadcast every 500ms
Device B (JOIN):  recv_cb → pairing_handle_packet() → group_mgr_save_key()
```

## FreeRTOS Tasks
| Task | Stack | Priority | Purpose |
|------|-------|----------|---------|
| main | default | 1 | Init only |
| ui | 4096 | 3 | Button polling, display update |
| pair_share | 2048 | 5 | Periodic pairing broadcast |
| BT (internal) | — | — | Managed by Bluedroid stack |

## Hardware Targets

| Feature | M5StickC Plus 2 | DevKit |
|---------|-----------------|--------|
| MCU | ESP32-PICO-V3-02 | ESP32 |
| Display | ST7789V2 135×240 | None |
| LED | GPIO 10 (active low) | GPIO 2 |
| Button A | GPIO 37 | GPIO 0 (BOOT) |
| Button B | GPIO 39 | N/A |
| Battery | Built-in | External |

## Future Work
- [ ] Opus audio compression (reduce ESP-NOW payload from ~88B to ~20B per frame)
- [ ] ECDH key exchange for secure pairing
- [ ] ST7789V2 display driver (currently log-only)
- [ ] Proper WebRTC VAD instead of simple energy threshold
- [ ] Mesh relay (multi-hop) for extended range
- [ ] OTA firmware updates
