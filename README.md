# PEEPLE BEEPER

A 128×64 FSTN LoRa pager. The C firmware under `firmware/` is shared by the desktop bench and the Feather nRF52840 image. The RP2350 board (v3, `hardware/rp2350`) has a schematic and no layout or firmware image yet. It replaces the ESP32-S3 board (v2).

Build, bench keys, and how to flash a Feather are below. The map of the tree is [docs/README.md](docs/README.md).

## Build

```
make test           # host tests, each prints ok
make bench          # build/peeple-emu (SDL2)
make run            # build it if needed, then open the window
make check-feather  # compile the Feather HAL table, no link and no flash
make bringup        # breadboard wire test, build/bringup.uf2
make flash-bringup  # copy that image onto FTHR840BOOT
make feather        # pager image, build/feather.uf2
make flash-feather  # copy that image onto FTHR840BOOT
```

`make feather` and `make bringup` need `arm-none-eabi-gcc`. Double-tap the Feather reset button so the `FTHR840BOOT` drive appears, then flash. The wire-test glass shows the switches, the compass ids, one radio register, and a count of GPS sentences. The pager image shows the menu.

`cmake -B build/cmake && cmake --build build/cmake` builds the same host tests and `peeple-emu`. It does not build a UF2.

## Bench

Install SDL2, then `make run`. On macOS, double-click `peeple-emu.command`.

```
brew install sdl2 pkg-config          # macOS
sudo apt install libsdl2-dev pkg-config   # Debian / Ubuntu
sudo dnf install SDL2-devel pkgconf-pkg-config   # Fedora
pacman -S mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-pkgconf   # MSYS2
```

Default bench is three radios at east 0 / 75 / 150 ft, all at north 0 (A reaches C only through B). **+ add** / **− remove** in the top bar grow the bench up to 16 units. Each column is a scaled 128×64 LCD, the radio name, its place on the foot grid, and who it can hear.

- Click an LCD to focus that radio
- Scroll or drag on the LCD = encoder
- Click = shaft press + release (Select)
- Esc = Back
- **N / W / E / S** move that radio on the foot grid. North is up. A step is 5 ft, Shift is 25 ft. Scrolling the panel under an LCD moves it north or south. Straight-line distance under 100 ft is radio range. `(1, 0)` is 1 ft east.
- **flat** / **tipped** and **still** / **moving** are the level check. Tipped is 25° off flat. **heading − / +** turns that radio. A step is 15°, Shift is 45°, and 0° means the top of the LCD points north.
- **sky** / **no sky** is whether a listen can see satellites. **sample** starts that listen now.
- j/k and arrows turn the knob; Enter/Space selects
- NVM is per user: `~/Library/Application Support/Peeple/Beeper/` on macOS, `~/.local/share/Peeple/Beeper/` on Linux, `%APPDATA%\Peeple\Beeper\` on Windows. A file still named `./emu-nvm-<id>.bin` in the current directory is read once, then saved in that folder.

The bench seeds contact books so A/B/C can Compose without walking HELLO. That directory is bench-only.

## Pair two pagers

1. Settings → Display name on each unit.
2. Add on both, hold the units together (RSSI ≥ −45).
3. Compose → peer name → canned line.
