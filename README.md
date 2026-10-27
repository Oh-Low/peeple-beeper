# PEEPLE BEEPER

A 128×64 FSTN LoRa pager. Firmware lives in C under `firmware/` and talks only to a HAL. Two HAL implementations ship in-tree: a RAM double for tests and the desktop emulator, and a compile-only nRF stub.

There is one pager implementation: C under `firmware/`. The desktop emulator and a future nRF board are two HALs, not two pagers.

```
firmware/include     Public headers (beeper.h, frames, crypto, …)
firmware/src         C pager, pairing, paging, store, display
firmware/host        Desktop radio mesh (tests and emulator)
firmware/art         Generated 1-bit picture bytes
firmware/tests       page / store / 100ft mesh tests
hal/hal.h            HAL contract
hal/emu              RAM HAL for tests and the emulator
hal/beeper           Beeper board HAL stub (compile check)
emu                  SDL2 bench (fake air, NVM files)
```

The LCD is a 1024-byte ST7567-style page buffer (`page * 128 + x`, bit `y & 7`).

Onboarding docs:

| Doc | What it covers |
|-----|----------------|
| [docs/architecture.md](docs/architecture.md) | Repo map, C firmware, HALs, tests |
| [docs/hardware.md](docs/hardware.md) | Why nRF52840, SX1262, 128×64 FSTN, encoder, GPS, heading, 100 KB store |
| [docs/prototype-assembly.md](docs/prototype-assembly.md) | Wiring checklist for one breadboard prototype |
| [docs/hal.md](docs/hal.md) | How firmware talks to hardware (RAM HAL and nRF stub) |
| [docs/radio-protocol.md](docs/radio-protocol.md) | Pairing, PAGE/ACK, crypto, mesh relay |
| [docs/firmware-app.md](docs/firmware-app.md) | Screens, menus, inbox/outbox, identity, store |

## Build and test

```
make test          # page_test, store_test, mesh_test, locate_test
make emu           # build/peeple-emu (SDL2)
make nrf-check
make bringup       # breadboard wire test, writes build/bringup.uf2
make flash-bringup # copy that image onto FTHR840BOOT
make beeper        # pager image for the Feather, writes build/beeper.uf2
make flash-beeper  # copy that image onto FTHR840BOOT
```

`make bringup` and `make beeper` need `arm-none-eabi-gcc` (the Arm GNU Toolchain). Double-tap the Feather reset button so the `FTHR840BOOT` drive appears, then `make flash-bringup` or `make flash-beeper`. The wire-test glass shows the switches, the compass ids, one radio register, and a count of GPS sentences. The pager image shows the menu. Both leave the piezo and the motor alone, and neither transmits.

`make test` uses clang (`-std=c11 -Wall -Wextra -Werror`). All three tests must print `ok` lines and exit 0.

## Emulator

```
make run
```

Install SDL2, then `make run`. That builds the emulator if needed and opens the window. On macOS, double-click `peeple-emu.command` to do the same.

```
brew install sdl2 pkg-config          # macOS
sudo apt install libsdl2-dev pkg-config   # Debian / Ubuntu
sudo dnf install SDL2-devel pkgconf-pkg-config   # Fedora
pacman -S mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-pkgconf   # MSYS2
```

`cmake -B build/cmake && cmake --build build/cmake` is the same app if you already use CMake. It finds SDL2 through pkg-config.

Default bench is three radios at east 0 / 75 / 150 ft, all at north 0 (A reaches C only through B). **+ add** / **− remove** in the top bar grow the bench up to 16 units (the window widens). Each column is a scaled 128×64 LCD (paper 207,212,192; ink 28,34,24), the radio name, its place on the foot grid, and who it can hear.

- Click an LCD to focus that radio
- Scroll or drag on the LCD = encoder
- Click = shaft press + release (Select)
- Esc = Back
- **N / W / E / S** move that radio on the foot grid. North is up, south is down, west is left, east is right. A step is 5 ft, Shift is 25 ft. Scrolling the panel under an LCD moves it north or south. Straight-line distance is the radio range: under 100 ft they hear each other. The bench starts in an east-west line at 0 / 75 / 150, so A reaches C only through B. `(1, 0)` is 1 ft east. `(-150, 150)` is 150 ft west and 150 ft north. The coordinate under the compass is what a locate reply posts.
- **flat** / **tipped** and **still** / **moving** are the level check. Tipped is 25° off flat. **heading − / +** turns that radio. A step is 15°, Shift is 45°, and 0° means the top of the LCD points north. The compass letter is the 8-way bucket the arrow uses. A locate result asks the unit to hold flat and still before it draws the arrow.
- **sky** / **no sky** is whether a listen can see satellites. **sample** starts that listen now. The radio stores the fix and then sleeps the receiver. **no sky** during a listen keeps the last stored fix, which is what a locate reply sends.
- j/k and arrows turn the knob; Enter/Space selects
- Fake air is 100 ft (`firmware/include/mesh.h`)
- NVM is per user: `~/Library/Application Support/Peeple/Beeper/` on macOS, `~/.local/share/Peeple/Beeper/` on Linux, `%APPDATA%\Peeple\Beeper\` on Windows. A file still named `./emu-nvm-<id>.bin` in the current directory is read once, then saved in that folder.

The host seeds contact books so A/B/C can Compose without walking HELLO. That directory is emulator-only, not part of the air protocol.

## Pair two beepers (without the host directory)

1. Settings → Display name on each unit.
2. Contact on both, hold the units together (RSSI ≥ −45).
3. Compose → peer name → canned line.

## Porting to hardware

Implement `hal/hal.h` on the board (`hal/beeper/beeper_hal.c` is a stub that must compile). Firmware already calls `hal->foo(hal->ctx, ...)`. Do not keep a TypeScript UI and swap in U8g2.
