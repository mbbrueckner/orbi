# orbi

DIY virtual pet (Tamagotchi-inspired) on a Seeed XIAO ESP32-C6 + Seeed Round Display for XIAO (1.28",
GC9A01, touch), built with ESP-IDF in plain C.

## Status

- [x] ESP-IDF toolchain installed
- [x] Board boots (hello_world flashed)
- [x] I2C scan done (touch chip identified: CHSC6X at 0x2E, RTC BM8563 at 0x51)
- [x] Display shows a test color (esp_lcd_gc9a01)
- [ ] LVGL integrated, demo widget visible
- [ ] PC simulator (SDL2) set up
- [ ] Touch registered with LVGL
- [ ] pet_core done + tested
- [ ] UI/sprites done
- [ ] Persistence (NVS) done
- [ ] Final assembly (soldering, battery)

## Structure

- `components/pet_core/` – pure game logic, hardware independent
- `components/ui/` – LVGL rendering, sprites
- `components/input/` – touch driver
- `components/storage/` – NVS persistence
- `main/` – app entry point

## Build

```
idf.py set-target esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```
