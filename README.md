# MeshCom

Walkie-talkie de bajo coste para grupos de moteros, basado en ESP32 con ESP-NOW y Bluetooth HFP.

## Concepto

MeshCom es un pequeño dispositivo que se lleva en la moto y permite comunicación de voz en grupo sin necesidad de cobertura de datos, sin nodo central y con latencia muy baja. Se conecta al intercom del casco por Bluetooth y se comunica con otros dispositivos por ESP-NOW (Wi-Fi P2P sin AP).

## Estado

🚧 **PoC — firmware inicial compilable** (ESP-IDF v5.2, target ESP32)

## Requisitos

- [ESP-IDF v5.2+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- Python 3.8+
- Cable USB-C (M5StickC Plus 2) o micro-USB (DevKit)
- Driver CP210x o CH340 según placa (normalmente ya incluido en Linux; en Windows/macOS puede requerir instalación)

## Selección de placa

En `main/CMakeLists.txt`, la línea `add_compile_definitions(BOARD_M5STICKC_PLUS2)` selecciona el hardware.

| Placa | Define |
|-------|--------|
| M5Stack StickC Plus 2 | `BOARD_M5STICKC_PLUS2` (por defecto) |
| ESP32 DevKit genérico | `BOARD_DEVKIT` |

Cambia el define antes de compilar si usas un DevKit.

## Build

```bash
# 1. Cargar el entorno de ESP-IDF (ajusta la ruta a tu instalación)
. ~/esp/esp-idf/export.sh

# 2. Configurar target
idf.py set-target esp32

# 3. (Opcional) Ajustar configuración
idf.py menuconfig

# 4. Compilar
idf.py build
```

## Flasheo

### Identificar el puerto serie

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# Típicamente: /dev/ttyUSB0
```

**macOS:**
```bash
ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART*
# Típicamente: /dev/cu.usbserial-0001
```

**Windows:** Ver "Administrador de dispositivos" → Puertos COM. Típicamente `COM3`, `COM4`...

### M5StickC Plus 2

El M5StickC Plus 2 **no tiene botón BOOT** visible. Para entrar en modo flash:
1. Apagar el dispositivo (mantener botón de encendido ~6 segundos)
2. Mantener pulsado el **botón A** (frontal grande)
3. Mientras lo mantienes, conectar el cable USB al PC
4. Soltar el botón A — ya está en modo download

```bash
idf.py -p /dev/ttyUSB0 flash
```

O en un solo paso (compila, flashea y abre monitor serie):
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

### ESP32 DevKit genérico

Los DevKit normales entran en modo flash automáticamente al usar `idf.py flash`:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Si falla, mantén pulsado el botón **BOOT/IO0** mientras pulsas **EN/RST** una vez, luego suelta BOOT.

### Velocidad de flasheo

Si la conexión es inestable, reducir baud rate:
```bash
idf.py -p /dev/ttyUSB0 -b 115200 flash
```

### Flash completo (incluye bootloader y particiones)

```bash
idf.py -p /dev/ttyUSB0 flash
# Equivalente a:
esptool.py -p /dev/ttyUSB0 -b 460800 \
  write_flash \
  0x1000  build/bootloader/bootloader.bin \
  0x8000  build/partition_table/partition-table.bin \
  0x10000 build/meshcom.bin
```

## Monitor serie

Para ver logs en tiempo real:
```bash
idf.py -p /dev/ttyUSB0 monitor
# Salir: Ctrl+]
```

## Emparejamiento con el intercom

El intercom es el dispositivo Bluetooth que se anuncia (HFP HF). El ESP32 es el que escanea y se conecta (HFP AG, rol de "teléfono"). **El ESP32 nunca se pone en modo discoverable** — siempre es él quien busca al intercom.

### Primera vez

1. Poner el intercom en **modo pairing** (consultar manual — normalmente mantener botón de encendido hasta que parpadee)
2. En el ESP32: **mantener botón B 3 segundos** (M5Stick) o **mantener BOOT 3 segundos** (DevKit)
   - LED parpadeo lento → escaneando...
   - LED parpadeo rápido → encontrado, conectando...
   - LED fijo → ✅ conectado y guardado
3. Si se pide PIN: `0000`

La dirección BT del intercom queda guardada en NVS. Las siguientes veces **reconecta automáticamente** al encender.

### Si hay varios dispositivos BT cerca

El ESP32 se conecta al dispositivo con **señal más fuerte (RSSI mayor)** durante el escaneo. En la práctica, si solo tienes tu intercom en modo pairing, siempre encontrará el correcto.

### Re-emparejar con otro intercom

**Mantener botón B 3 segundos** de nuevo — inicia un nuevo escaneo y sobreescribe la dirección guardada.

## Emparejamiento de grupo entre dispositivos

1. En el dispositivo "maestro": **pulso corto botón A** → LED parpadeo lento (modo share, 30s)
2. En los demás: **mantener botón A 2 segundos** → LED parpadeo rápido (modo join)
3. Al recibir la clave: LED fijo — ya están en el mismo grupo

## Tests de host (sin hardware)

Los tests unitarios se ejecutan en el host (Linux/macOS) con CMake puro:

```bash
cd test/host
mkdir build && cd build
IDF_PATH=~/esp/esp-idf cmake ..
make -j$(nproc)
./meshcom_host_test
```

Resultado esperado: `11 Tests 0 Failures 0 Ignored — OK`

Cubren: cifrado/descifrado AES-128-GCM, generación de claves, persistencia NVS (mock), VAD, anti-duplicate de secuencias.

## Documentos

- [`DESIGN.md`](./DESIGN.md) — Análisis técnico completo y decisiones de diseño
- [`docs/ARCHITECTURE.md`](./docs/ARCHITECTURE.md) — Arquitectura de componentes y flujo de datos
