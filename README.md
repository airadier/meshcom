# MeshCom

Walkie-talkie de bajo coste para grupos de moteros, basado en ESP32 con ESP-NOW y Bluetooth HFP.

## Concepto

MeshCom es un pequeño dispositivo que se lleva en la moto y permite comunicación de voz en grupo sin necesidad de cobertura de datos, sin nodo central y con latencia muy baja. Se conecta al intercom del casco por Bluetooth y se comunica con otros dispositivos por ESP-NOW (Wi-Fi P2P sin AP).

## Estado

🚧 **PoC — firmware inicial compilable** (ESP-IDF v5.2, target ESP32)

## Build

Requiere [ESP-IDF v5.2+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).

```bash
# Firmware ESP32
idf.py set-target esp32
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

Para M5StickC Plus 2 (por defecto). Para DevKit genérico, añadir `-DBOARD_DEVKIT` en `main/CMakeLists.txt`.

## Tests de host (sin hardware)

Los tests unitarios se ejecutan en el host (Linux/macOS) con CMake puro, usando mbedTLS y Unity de ESP-IDF:

```bash
cd test/host
mkdir build && cd build
IDF_PATH=/path/to/esp-idf cmake ..
make -j$(nproc)
./meshcom_host_test
```

Cubren: cifrado/descifrado AES-128-GCM, generación de claves, VAD, anti-duplicate.

## Documentos

- [`DESIGN.md`](./DESIGN.md) — Análisis técnico completo y decisiones de diseño
- [`docs/ARCHITECTURE.md`](./docs/ARCHITECTURE.md) — Arquitectura de componentes y flujo de datos
