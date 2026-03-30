# my-app

RGB LED color cycling demo for Seeed XIAO BLE (nRF52840).

Cycles through 8 colors: Red → Green → Blue → Yellow → Cyan → Magenta → White → Off, repeating every 4 seconds.

## Hardware

XIAO BLE has an onboard RGB LED (active-low):

| Color | Alias | Pin    |
|-------|-------|--------|
| Red   | led0  | P0.26  |
| Green | led1  | P0.30  |
| Blue  | led2  | P0.06  |

## Building

From the west workspace root directory:

```bash
.venv/bin/west build -p always -b xiao_ble my-app
```

## Flashing

Double-click the RESET button on the XIAO BLE to enter UF2 bootloader mode, then:

```bash
.venv/bin/west flash -r uf2
```

Or manually copy the UF2 file:

```bash
cp build/zephyr/zephyr.uf2 /Volumes/XIAO-SENS/
```

## Project Structure

```
my-app/
├── CMakeLists.txt      # Build script
├── prj.conf            # Project Kconfig configuration
├── .gitignore          # Git ignore rules
├── README.md           # This file
├── boards/
│   └── xiao_ble.conf   # Board-specific overrides
└── src/
    └── main.c          # RGB LED color cycling source code
```
