# orbi

[![CI](https://github.com/mbbrueckner/orbi/actions/workflows/ci.yml/badge.svg)](https://github.com/mbbrueckner/orbi/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A small virtual pet for your desk: a pixel-art raccoon on a round 1.28" touch display. You feed it,
play with it and put it to bed. When you ignore it, it gets hungry, tired and sad. Inspired by the
Tamagotchi.

orbi runs on a **Seeed XIAO ESP32-C6** with the **Seeed Round Display for XIAO**. It is written in
plain C with [ESP-IDF](https://github.com/espressif/esp-idf) and [LVGL](https://lvgl.io).

<!-- TODO: add a photo or GIF of the device, e.g. docs/orbi.jpg -->

## Features

- **Four rooms**: swipe left or right to move between them (the carousel wraps around).
  - **Living room** shows the pet and all its stats at a glance.
  - **Dining room**: drag the apple onto the pet to feed it. One apple has four bites.
  - **Bedroom**: tap the ceiling lamp. Lights off puts the pet to sleep, lights on wakes it up.
  - **Garden**: tap the ball and the pet fetches it.
- **Three stats** (0–100): hunger, energy and happiness. They change in real time, and the pet's
  mood (happy, hungry, tired, sad, asleep) follows from them.
- **Keeps running while unplugged**: the pet is saved to flash, and the display's real-time clock
  tells orbi how long it was off. On the next boot that time is caught up, so a pet left alone
  over the weekend will be hungry.
- **Dims after one minute** without a touch, to go easy on an always-on screen.
- **Hardware-independent game logic** with unit tests that run on your PC.

The on-screen text is in German by default; English can be picked in menuconfig (see [Tweaking](#tweaking)).

## Hardware

| Part | Notes |
| --- | --- |
| [Seeed Studio XIAO ESP32-C6](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) | With pin headers (buy it pre-soldered or solder them yourself) |
| [Seeed Studio Round Display for XIAO](https://wiki.seeedstudio.com/get_start_round_display/) | 240×240 GC9A01 display, CHSC6X touch, BM8563 RTC |
| CR927 coin cell (optional) | Goes in the display's coin cell holder. Without it the clock resets when unplugged and time away is not caught up |
| USB-C cable | orbi is powered over USB, no battery needed |

Plug the XIAO into the socket on the back of the Round Display. No other wiring is needed.

<details>
<summary>Pin mapping (for reference)</summary>

| Function | XIAO pin | ESP32-C6 GPIO |
| --- | --- | --- |
| LCD CS | D1 | 1 |
| LCD DC | D3 | 21 |
| I2C SDA (touch + RTC) | D4 | 22 |
| I2C SCL (touch + RTC) | D5 | 23 |
| Backlight | D6 | 16 |
| SPI SCK | D8 | 19 |
| SPI MOSI | D10 | 18 |

I2C addresses: touch controller CHSC6X at `0x2E`, RTC BM8563 at `0x51`. The display has no reset
pin, so a software reset is used. GPIO16/17 are used by the display, which is why the log output
goes over USB (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`) instead of the UART pins.

</details>

## Getting started

### 1. Install ESP-IDF

Follow the [ESP-IDF getting started guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html)
for your OS. orbi is developed and tested with **ESP-IDF v6.0.2**. Then open a terminal with the
ESP-IDF environment loaded (`. $IDF_PATH/export.sh` or the ESP-IDF terminal of the VS Code
extension).

### 2. Build and flash

```sh
git clone https://github.com/mbbrueckner/orbi.git
cd orbi
idf.py set-target esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

`<PORT>` is the XIAO's serial port, for example `/dev/cu.usbmodem101` on macOS, `/dev/ttyACM0` on
Linux or `COM3` on Windows. The first build downloads LVGL and the GC9A01 driver from the
[ESP Component Registry](https://components.espressif.com) automatically.

Press `Ctrl+]` to leave the monitor.

### 3. Start fresh (optional)

The pet is stored in flash and survives reflashing. To start over with a new pet:

```sh
idf.py -p <PORT> erase-flash flash
```

## Running the tests

The game logic, the save format, the touch report parser and the RTC time conversion are plain C
and can be tested on your computer without the board. You need CMake and a C compiler.

```sh
cmake -S test -B test/build
cmake --build test/build
./test/build/orbi_test
```

## Tweaking

| What | Where |
| --- | --- |
| Display language (German or English) | `idf.py menuconfig` → *orbi* → *Display language* |
| How fast stats change, what feeding and playing do | `components/pet_core/pet_state.c` |
| Mood thresholds | `pet_get_mood()` in `components/pet_core/pet_state.c` |
| Speed up time for testing (`TIME_SPEEDUP`) | `main/main.c` |
| Save interval | `SAVE_INTERVAL_S` in `main/main.c` |
| Dim timeout and strength | `DIM_AFTER_MS`, `DIM_OPACITY` in `components/ui/ui_render.c` |

If you change `TIME_SPEEDUP`, erase the flash afterwards: the saved timestamp uses the same scale.

### Changing the artwork

Sprites and room backgrounds are PNG files in `components/ui/assets/`. After editing them,
regenerate the C data (needs Python 3 and [Pillow](https://pypi.org/project/pillow/)):

```sh
pip install pillow
python3 tools/gen_sprites.py
```

This rewrites `components/ui/sprites_data.c`. The generated file is committed, so a normal build
does not need Python.

## Project structure

```
main/                 App entry point: wires everything together, main loop, saving
components/
  pet_core/           Game rules and the time scheduler (no hardware dependencies)
  ui/                 Display driver setup, LVGL rooms, sprites
    assets/           Source PNGs for sprites and room backgrounds
  input/              CHSC6X touch driver for LVGL
  rtc/                BM8563 real-time clock driver and time helpers
  storage/            Saving the pet to NVS flash
test/                 Host-side unit tests (plain CMake)
tools/gen_sprites.py  Converts the PNGs into sprites_data.c
```

The rule of thumb: `pet_core` knows nothing about hardware or time sources. The current time is
passed in, so the same code runs on the device and in the tests.

## Roadmap

- [ ] PC simulator (SDL2) to try the UI without hardware
- [ ] Enclosure

## Credits

The raccoon, the items and the icons are hand-drawn pixel art by [@mbbrueckner](https://github.com/mbbrueckner). The room backgrounds were
generated with an image generator.

## License

MIT, see [LICENSE](LICENSE). This covers the code and the artwork.
