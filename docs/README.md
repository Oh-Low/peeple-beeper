# Docs

The C pager is shared by the bench and the Feather image. The RP2350 board (v3) is the current board: a schematic, no layout, and no firmware image yet. The ESP32-S3 board (v2) it replaces still has its layout and fab package.

| Doc | What it is |
|---|---|
| [architecture.md](architecture.md) | Where each kind of code lives, and which test covers it |
| [hal.md](hal.md) | The `hal_t` contract, the RAM HAL, and the Feather pins |
| [firmware-app.md](firmware-app.md) | Screens, menus, store, alerts |
| [radio-protocol.md](radio-protocol.md) | Frames, crypto, pairing, relay, locate |
| [source-overview.md](source-overview.md) | What each `.c` file does |
| [feather.md](feather.md) | Feather nRF52840 breadboard. Firmware is `hal/feather` (`make feather`) |
| [breadboard-assembly.md](breadboard-assembly.md) | Wire that breadboard, then flash `make bringup` or `make feather` |
| [feather-carrier.md](feather-carrier.md) | The same breadboard as a carrier schematic |
| [rp2350-bom.md](rp2350-bom.md) | RP2350 (v3) parts list, and what it saves over v2 |
| [rp2350-shopping-list.md](rp2350-shopping-list.md) | What to buy for ten boards, with links and cart totals |
| [rp2350-connections.md](rp2350-connections.md) | Nets on `hardware/rp2350/peeple.kicad_sch` |
| [rp2350-bringup.md](rp2350-bringup.md) | Hand assembly, first power, and the firmware port |
| [rp2350-datasheets/](rp2350-datasheets/) | PDFs for parts new in v3 |
| [esp32-bom.md](esp32-bom.md) | ESP32-S3 (v2) parts list |
| [esp32-connections.md](esp32-connections.md) | Nets on `hardware/esp32/peeple.kicad_sch` |
| [esp32-fab.md](esp32-fab.md) | What to upload for the v2 layout |
| [esp32-bringup.md](esp32-bringup.md) | Compass and first-sky work that waited for that board |
| [esp32-datasheets/](esp32-datasheets/) | PDFs for the ESP32 board. v3 links here for the parts it kept |
| [../art/layouts/](../art/layouts/) | Screen pictures. Behavior is firmware-app.md |
| [reviews/](reviews/) | Session logs. Paths in there may be gone |

`hal/rp2350/` and `hal/esp32/` are reserved for later images. Do not start either port inside `hal/feather/`.
