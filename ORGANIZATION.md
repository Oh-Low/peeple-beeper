# Organization

The rename in this note is done. Machine names are on the folders. `beeper_t` is still the portable pager object.

| Path | What it is |
|---|---|
| `firmware/` | Portable pager. Talks only to `hal/hal.h`. |
| `hal/ram/` | RAM HAL for tests and the bench. |
| `hal/feather/` | Feather nRF52840 image. `make feather`, `make flash-feather`. |
| `hal/rp2350/` | Empty until the v3 port exists. |
| `hal/esp32/` | Empty until that port exists. |
| `bench/` | SDL window, font, seed, and `mesh.c`. `make bench`. |
| `bringup/` | Feather wire test. Linker script is `bringup.ld`. |
| `tools/uf2.py` | Packs both UF2s. |
| `art/mascots/`, `art/layouts/` | Mascot PNGs and screen pictures. |
| `hardware/feather-carrier/` | Feather carrier schematic. |
| `hardware/rp2350/` | v3 RP2354A schematic and its local symbol and footprint libraries. No layout yet. |
| `hardware/esp32/` | v2 ESP32-S3 schematic and its layout (`peeple.kicad_pcb`). |
| `hardware/archive/` | Older ESP32 placement, same parts, five of them moved. |
| `manufacturing/pcbv2/` | Fab export folder. Plots are gitignored. |
| `docs/reviews/` | Session logs. Paths in them may be gone. |

`make test`, `make bench`, `make bringup`, `make feather`, `make check-feather`. CMake builds the host tests and the bench only.

The two ESP32 layouts share 93 reference designators. The v2 outline is 103 × 61 mm and 2-layer. The archived outline is 100 × 61 mm. AE1, C14, C15, L2, and U4 moved. The archived file stays so that placement is not thrown away.

Local KiCad UI files (`*.kicad_prl`) and the regenerated fab plots are ignored. The copies are still on disk.

Left as they are:

- `beeper_t`, `firmware/src/beeper.c`, the menu words Peeple and Beeple, and the `peeple-emu` binary.
- `beeper_t.message` is the message session. Test harnesses still call their own copy `radio`. `screen_t` still holds that session in `radio`.
- `art/mascots/` has no `camnon.png`, and `tools/beeple_to_art.py` still lists it. `KillCorder.png` is in the folder and is not in that list.
