# Repo organization

Read-only reviews of documentation, code and folder names, and what should stay out of git. Nothing else in the tree was moved or edited.

Reviews: [Docs locations and naming](0096007b-0fe7-433f-81cc-8e6bb9606dd4), [Code names and folders](f24a7e33-11b2-4424-8b17-2f0909819283), [Gitignore and repo junk](10453e8d-38ee-4015-bd94-34413017f79c).

The name **beeper** is four different things, and none of them is the ESP32 board packed for manufacturing. The image `make beeper` / `make flash-beeper` builds is a Feather nRF52840 pager. `hardware/beeper` is only the breadboard-carrier schematic. `hardware/pcb` is an ESP32-S3 board, and `pcbv2` is the layout that was packed for PCBWay. There is no ESP32 firmware in the tree.

| Place | What it actually is |
|---|---|
| `hal/beeper/`, `make beeper`, `build/beeper.uf2` | Live Feather nRF52840 image. `main.c` loops `board_pump` + `beeper_tick`. `board.c` drives the pins. |
| `hardware/beeper/` | KiCad schematic only. Feather carrier. No copper, no `.kicad_pcb`. |
| `firmware/src/beeper.c`, `firmware/include/beeper.h` | Portable pager object (`beeper_init` / `beeper_tick`). Not a board driver. |
| `firmware/include/beeple.h`, `firmware/art/beeple.c` | Mascot catalog. One letter away from the device object. |
| `hardware/pcb/pcb.kicad_pcb` | Older ESP32 layout of `pcb.kicad_sch`. Title block says `pcb`. |
| `hardware/pcb/pcbv2.kicad_pcb` | Current ESP32 layout. Same schematic, same title `pcb`. This is what the fab drop was exported from. |
| `docs/hardware.md` | Feather nRF52840 breadboard pin contract. Not the ESP32 schematic. |
| `docs/pcb-bom.md`, `docs/pcb-connections.md` | ESP32-S3 parts and nets. |
| `Send to PCBway/` | Untracked manufacturing drop for pcbv2. |

`docs/pcb-bom.md` already says this: the sheet in `hardware/beeper` matches the breadboard, and the ESP32 list is a later board. The folder names do not.

## Suggested order

Do these in order. Each step is safe to stop after.

1. **Ignore local and generated files** before the next `git add`. Untrack the two `.kicad_prl` files. Do not add `pcbv2.step`, `pcbv2.kicad_prl`, or both the gerber zip and the exploded gerbers. See [Gitignore](#gitignore).
2. **Fix the front door.** One root README. Turn `docs/README.md` into an index that names the Feather image, the emulator, and the ESP32 board with no firmware yet. Stop calling `hal/beeper` a compile-only stub.
3. **Rename docs so the board is in the filename.** Move lens-ledger files into `docs/reviews/`. Leave code paths alone in this step.
4. **Rename machine folders.** `hal/beeper` → `hal/feather`, `hardware/beeper` → `hardware/feather-carrier`, `Send to PCBway` → `manufacturing/pcbv2`. Give the two `feather.ld` files different names. Leave `beeper_t` until after that.
5. **Archive the old ESP32 layout** only after a copper diff of `pcb.kicad_pcb` against `pcbv2.kicad_pcb`. That diff was not done.

Skipping the `beeper_t` → `unit_t` rename is safe once the three machine paths (`hal/beeper`, `hardware/beeper`, `make beeper`) have moved. Those are the names that point at the wrong board.

## Target tree

Code paths below are the end state from the code review. Doc paths are the end state from the docs review. Mascot PNGs move to `art/mascots/` because `tools/beeple_to_art.py` reads them as a build input. Screen mockups move with them so `docs/` is prose.

```
README.md                         build, emulator keys, one link to docs/README.md

firmware/                         portable pager, talks only to hal/hal.h
  include/beeper.h                keep until a later rename (optional: unit.h)
  src/beeper.c                    keep until a later rename (optional: unit.c)
  include/beeple.h                mascot index, keep the name
  art/beeple.c                    mascot catalog
  art/art.c                       sprite bytes
  tests/message_test.c            was page_test.c

hal/
  hal.h
  ram/                            was hal/emu/   (tests and bench)
  feather/                        was hal/beeper/
    main.c                        device entry
    feather_hal.c                 was beeper_hal.c
    feather_board.c               was board.c
    start.S
    sx1262_setup.c                shared with bringup
    pager.ld                      was hal/beeper/feather.ld  (NVM hole at 0xDA000)
  esp32/                          empty until a port exists

bench/                            was emu/  (SDL window, font, seed, third_party)
  mesh.c                          was firmware/host/mesh.c

bringup/
  bringup.c
  bringup.ld                      was bringup/feather.ld  (no NVM hole)

tools/
  uf2.py                          was bringup/uf2.py  (both UF2s use it)
  png_to_art.py
  beeple_to_art.py                default PNG dir becomes art/mascots

art/
  mascots/                        was docs/BEEPLE PNG/
  layouts/                        was docs/Layouts/

hardware/
  feather-carrier/                was hardware/beeper/  (schematic only)
  esp32/                          was hardware/pcb/
    peeple.kicad_sch              was pcb.kicad_sch
    peeple.kicad_pcb              was pcbv2.kicad_pcb
    modules.pretty, modules.kicad_sym, modules.3dshapes
  archive/
    esp32-layout-v1.kicad_pcb     was pcb.kicad_pcb, only after a copper diff

manufacturing/pcbv2/              was "Send to PCBway/"
                                  track the two markdown files; ignore plots (below)

docs/
  README.md                       index only
  architecture.md, hal.md, firmware-app.md, radio-protocol.md, source-overview.md
  feather.md                      was hardware.md
  breadboard-assembly.md          was prototype-assembly.md
  feather-carrier.md              new, from the schematic text block, then corrected
  esp32-bom.md                    was pcb-bom.md
  esp32-connections.md            was pcb-connections.md
  esp32-fab.md                    was Send to PCBway/README.md + ORDER-NOTES.md
  esp32-bringup.md                was first-boot-notes.md
  esp32-datasheets/               was datasheets/
  reviews/                        all lens-ledger-*.md, filenames kept
```

Makefile names after the move: `make test`, `make bench` (was `emu`), `make bringup`, `make feather` (was `beeper`), `make flash-feather`, `make check-feather` (was `nrf-check`).

Menu strings **Peeple** and **Beeple**, the repo name, and `peeple-emu` stay. They are the product. The collision is the folders and the make target.

Index rows worth writing once, in `docs/README.md`:

| Doc | Board |
|---|---|
| `feather.md`, `breadboard-assembly.md`, `feather-carrier.md` | Feather nRF52840. Firmware is `hal/feather` (today `hal/beeper`). |
| `esp32-bom.md`, `esp32-connections.md`, `esp32-fab.md`, `esp32-bringup.md` | ESP32-S3 schematic and pcbv2 fab package. No firmware image yet. |
| `architecture.md`, `hal.md`, `firmware-app.md`, `radio-protocol.md`, `source-overview.md` | C pager shared by the emulator and the Feather image. |
| `reviews/` | Session logs. Paths inside them may be gone. |

## Severe

### 1. The README calls the live Feather HAL a stub

`README.md` and `docs/README.md` say “a compile-only nRF stub”, “`hal/beeper` Beeper board HAL stub (compile check)”, and “`beeper_hal.c` is a stub that must compile.” `docs/hal.md` opens the same way.

`hal/beeper/` is the device image. `main.c` is the entry. `board.c` talks to the pins. `make beeper` and `make flash-beeper` ship `build/beeper.uf2` to the Feather bootloader (`FTHR840BOOT`, family `0xADA52840`, base `0x26000`). `docs/hardware.md` already documents D-pin assignments, radio power, and the flash window `0x26000`–`0xDA000`.

`docs/README.md` is the same 88 lines as the root README. Its links are written as `docs/architecture.md`, so from inside `docs/` they point at `docs/docs/…`. It is also the only file a reader would treat as the docs index, and it omits the ESP32 docs, the assembly end-state, `source-overview.md`, and `first-boot-notes.md`.

**Do:** one short root README (build commands plus one link). Rewrite `docs/README.md` as an index. Change the opening of `hal.md` so it matches the Feather section. Say in the index that `hal/beeper` and `make beeper` are the Feather nRF52840 image, until those paths are renamed.

### 2. `hal/beeper` is the wrong place to start an ESP32 port

A newcomer looking for “the beeper board” edits nRF52840 / Feather pin code. The ESP32 PCB has no HAL. Extending `hal/beeper` for the new board mixes two machines.

**Do:** rename `hal/beeper/` → `hal/feather/`. Rename `beeper_hal.c` / `beeper_hal.h` → `feather_hal.c` / `feather_hal.h`. Rename `board.c` / `board.h` → `feather_board.c` / `feather_board.h`. Rename make targets `beeper` / `flash-beeper` → `feather` / `flash-feather` (output `build/feather.uf2`). Leave `hal/esp32/` empty until a port exists.

### 3. `hardware/beeper` looks like the product PCB

That folder is a schematic, a `.kicad_pro`, and a `.kicad_prl`. Title “Peeple Beeper”. Comment: “Carrier for the working breadboard. Feather pins unchanged.” Symbol `Feather_nRF52840`. No board file. The project `boards` list is empty.

Next to `hardware/pcb`, this is the folder someone opens to edit the beeper. That edits the carrier, not the board at the fab.

**Do:** rename `hardware/beeper/` → `hardware/feather-carrier/`. Keep it schematic-only. Point the assembly doc and the BOM at the new path.

### 4. `docs/hardware.md` is a different board from the schematic people will edit

`docs/hardware.md` is titled Hardware and specifies an nRF52840-class MCU, SX1262, PA1616S, and “Buses, GPIOs, and regulators are not in the repo.”

`docs/pcb-connections.md` is the nets on `hardware/pcb/pcb.kicad_sch`. The MCU section is ESP32 (radio reset IO8, GPS power IO39, piezo IO17, and so on). `docs/pcb-bom.md` says the PCB image is a later firmware, separate from the Feather image and the emulator.

`architecture.md` lists `hardware.md` as “intended pager hardware” and does not link the PCB pair. None of the ESP32 docs are in the README table.

Three machines share the product name:

| Place | Computer | Radio | GPS | Pack |
|---|---|---|---|---|
| `docs/hardware.md`, `docs/prototype-assembly.md`, `hal/beeper`, `make beeper` | Adafruit Feather nRF52840 | Waveshare Pico-LoRa-SX1262 | Adafruit Ultimate GPS (PA1616S). 1200 mAh in the assembly guide | Feather charger, 200 mA |
| `hardware/beeper/beeper.kicad_sch` | Same Feather, as a carrier | Same Waveshare module | Same Ultimate GPS. Compass becomes bare LSM303AGRTR | J1 into Feather BAT |
| `docs/pcb-bom.md`, `docs/pcb-connections.md`, `hardware/pcb/` | ESP32-S3-WROOM-1-N8 | Heltec HT-RA62 | Quectel LC86LICMD | 2000 mAh cell, MCP73831 at 500 mA |

**Do:** rename `docs/hardware.md` → `docs/feather.md`. Rename the ESP32 pair to `docs/esp32-bom.md` and `docs/esp32-connections.md`. Index both. One sentence on which firmware exists today: Feather `make beeper`; ESP32 has a schematic and no firmware image.

### 5. Two ESP32 layouts, one schematic, and the unsuffixed name is the old one

`hardware/pcb/pcb.kicad_sch` is the only schematic. `pcb.kicad_pcb` and `pcbv2.kicad_pcb` are two layouts of it. Both title blocks say title `pcb`, date `2026-11-01`. Both `.kicad_pro` files point at `pcb.kicad_sch`. Both are two copper layers, 1.6 mm.

Edge.Cuts on `pcb` span about 100 × 61 mm. Edge.Cuts on `pcbv2` span about 103 × 61 mm, which matches the fab sheet. `Send to PCBway/README.md` names `pcbv2.kicad_pcb`. The docs never mention `pcbv2`. The fab folder never mentions `pcb.kicad_pcb`. Opening `pcb.kicad_pro` shows the current schematic on the old copper.

The BOM disagrees with both layouts and with the fab sheet:

- `docs/pcb-bom.md`: “Ten 4-layer boards near 60 × 70 mm.”
- Both checked-in boards: two copper layers, about 100 × 61 mm and 103 × 61 mm.
- `Send to PCBway/ORDER-NOTES.md`: 2-layer, ~103 × 61 mm.

**Do:** treat `pcbv2` plus the fab notes as the snapshot to order, and `pcb-connections.md` as the net list. Rewrite the “4-layer / 60 × 70 mm” sentence so it matches the layout you still intend to order. Set KiCad titles that are not the word `pcb` on both files. Move the current pair toward `hardware/esp32/peeple.kicad_sch` and `peeple.kicad_pcb`. Archive `pcb.kicad_pcb` as `archive/esp32-layout-v1.kicad_pcb` only after a copper diff. Do not merge the two `.kicad_pcb` files. They are different outlines.

`hardware/pcb/sym-lib-table` points at `/Users/ohlow/Projects/peeple-beeper/hardware/pcb/modules.kicad_sym`. `fp-lib-table` already uses `${KIPRJMOD}`. Keep the file in git. Change the URI to `${KIPRJMOD}` when you next edit it. Untracked `pcbv2.kicad_pcb` and `pcbv2.kicad_pro` set the plot directory to `/Users/ohlow/Desktop/`. Clear that before the first commit of those files. The tracked `pcb.kicad_pcb` has an empty plot directory.

### 6. Two linker scripts, both named `feather.ld`, with different flash lengths

`hal/beeper/feather.ld` has FLASH length `0xB4000` and stops at the store (`0xDA000`). `bringup/feather.ld` has FLASH length `0xCE000` and runs to the bootloader at `0xF4000`, with no `__nvm_start`.

The pager link line uses the first. Bringup uses the second. Pointing the pager at the bringup script lets the image grow through the 100 KB store.

**Do:** `hal/feather/pager.ld` (with the NVM hole) and `bringup/bringup.ld` (no hole). Do not symlink them.

### 7. Breadboard guide says the firmware does not know the pins, then flashes a pager that uses them

`docs/prototype-assembly.md` opens with “The pager firmware in this repository does not yet know these pins.” The same file’s “What done means” is `make beeper`, add-contact transmitting, and Settings BEEP TEST toggling the piezo. `docs/hardware.md` already assigns those Feather pins to `board.c`.

**Do:** split the page into “wire it with power off” and “flash `make bringup` / `make beeper`”. Drop the “does not yet know these pins” sentence. Name the file `docs/breadboard-assembly.md`.

### 8. Feather-carrier notes live only in the schematic, and they contradict the firmware docs

The text block at the bottom of `hardware/beeper/beeper.kicad_sch` is the only write-up of that sheet. It says firmware does not drive A5 yet and does not toggle BEEP on D2 yet. `docs/hardware.md` and `docs/source-overview.md` say A5 is held low and goes high for a vibe, and D2 toggles at 4 kHz. The 2026-10-30 review log records those paths as the live pin service.

**Do:** copy that text into `docs/feather-carrier.md`, fix the A5/D2 sentences to match `docs/hardware.md`, and leave a one-line pointer on the schematic. `pcb-bom.md` should link that file.

### 9. Current specs name `pager.h`, which is not in the tree

`docs/firmware-app.md` and `docs/radio-protocol.md` cite `PAGE_KEEP_MAX` and `CLOCK_NONE` in `pager.h`. Both macros are in `firmware/include/screen.h`. There is no `pager.h`, `page.h`, `firmware/src/pager.c`, or `firmware/src/page.c`. The UI spec’s module table already says `screen.c` and `message.c`.

Older lens ledgers give line numbers in `pager.c` and `page.c` as the place to edit. Those files are gone. Today those jobs are `screen.c` and `message.c`.

**Do:** replace `pager.h` with `screen.h` in the two current specs. Do not edit code from ledger paths without checking the file still exists.

### 10. Deferred work points at files that are gone

`docs/lens-ledger-2026-10-27.md`, `docs/lens-ledger-2026-10-27-feather.md`, and `docs/lens-ledger-src-split.md` cite `docs/temp-punch-list.md`. That file is not in the repo.

`lens-ledger-src-split.md` says `make nrf-check` compiles `hal/nrf/nrf_hal.c`. There is no `hal/nrf/` now. `architecture.md` says `make nrf-check` compiles the Feather table without `board.c`. `lens-ledger-2026-10-26.md` still describes `hal/nrf/nrf_hal.c` as the board stub. Current `architecture.md` says `hal/beeper`.

`build/nrf/` still has stale objects from that tree (`page.o`, `pager.o`, `air.o`, `nrf_hal.o`, `hal_ram.o`) next to current ones. `make clean` already deletes `build/`.

**Do:** move every `lens-ledger*.md` to `docs/reviews/`. Add a one-line banner: session log, paths may be gone. If a DEFERRED row is still wanted, copy that sentence into `docs/esp32-bringup.md` or a short `docs/open-questions.md` using today’s filenames (`locate.c`, `beeper.c`, `screen.c`). Rename the make target to `check-feather` and write objects under `build/check/`.

### 11. Tracked KiCad local UI state

`hardware/pcb/pcb.kicad_prl` and `hardware/beeper/beeper.kicad_prl` are in the index. `pcb.kicad_prl` is already dirty (viewport, hidden nets `GND` and `+3V3`, layer visibility). `pcbv2.kicad_prl` is untracked and would join them on the next add. These change every session. They are not the design.

**Do:** `git rm --cached` the two tracked files, leave them on disk, add `*.kicad_prl`. Do not `git add` `pcbv2.kicad_prl`.

### 12. The manufacturing drop will drift if it is committed as it sits

`Send to PCBway/` is untracked, so history is still clean. Committing it as it sits would add:

- `pcbv2.step` at 12.7 MB. A re-export replaces that blob every plot. The fab README does not list the STEP as an upload.
- `gerbers/` (728 KB, 14 files) and `pcbv2-gerbers.zip` (166 KB) as two copies of the same Gerbers, name for name and size for size.
- Three centroid CSVs of the same positions (`pcbv2-centroid.csv`, `pcbv2-centroid-mm.csv`, `pcbv2-centroid-PCBWay.csv`). The README says to upload the PCBWay one.

The board source is `hardware/pcb/pcbv2.kicad_pcb` plus `pcb.kicad_sch`. Those two KiCad files are not in git yet and should be added. They are source. `pcbv2.kicad_pcb` is about 900 KB.

**Do:** do not add `pcbv2.step` or `gerbers/`. If you want a frozen handoff, add at most the zip, one BOM, one centroid (`pcbv2-centroid-PCBWay.csv`), the assembly PDF, and the two markdown files. Otherwise ignore the generated files and keep only the markdown. Do not keep both the zip and the exploded gerbers.

## Medium

### Documentation

**“beeper” is the product, the HAL directory, the carrier schematic, the make target, the UF2, the NVM folder, and `beeper.c`.** In the index, three rows: emulator (`emu/`, `hal/emu`), Feather image (`hal/beeper`, `make beeper`), ESP32 board (`hardware/pcb`, no firmware image yet). Do not rename the C symbols in a docs-only cleanup.

**Lens ledgers are review logs sitting beside the specs.** Ten `docs/lens-ledger*.md` files. Titles like “Lens ledger, settings beep and vibe, 2026-10-30”. They are useful history (FIXED / ASKED / REJECTED). They also contain OPEN rows the later specs have already passed. Example: a ledger row is OPEN because the piezo blurb “says any free GPIO”; current `pcb-bom.md` says “10 × 10 mm piezo on IO17 through 100 Ω.” Another row describes an IO3 pull-down on R20; current BOM and connections both use R13. Move them to `docs/reviews/`. Keep the dated filenames.

**Manufacturing notes sit in a folder whose name is an instruction.** `Send to PCBway/README.md` and `ORDER-NOTES.md` belong with the ESP32 docs (`docs/esp32-fab.md`). The binaries belong in `manufacturing/pcbv2/` when you are ready to touch paths. The space in the folder name is awkward in shells. `Send to PCBway/docs/` is empty.

**`source-overview.md` restates the architecture, UI, and protocol docs, and the index never lists it.** Keep it. Add it to the index as the file tour. One stale phrase: “The board stub holds the partial line” in the `nmea.c` paragraph. Point that sentence at `beeper_hal.c` / `nmea_push_byte`.

**Menu label drift.** `firmware/src/screen.c` copies `"Locate"` and `"Add"`. `firmware-app.md` says “Locate / Add” in the screen table and “Locate and Contact” later. `README.md` says “Contact on both”. Use Add in the README pair steps and in that one `firmware-app.md` paragraph.

**UI spec still says BEEP TEST calls `beep(250)` every render.** `firmware/src/screen.c` calls `hal->beep(hal->ctx, 65535)` while `beep_test` is set and the mode is settings. The 2026-10-30 ledger marked the 250 ms repeat as FIXED. The spec was not updated. One sentence in `firmware-app.md` should match the 65535 hold. Confirm cancel-on-leave against `screen.c` when editing.

**Art and layout folders read as scrapbooks. One is a build input.** `docs/BEEPLE PNG/` is the default directory in `tools/beeple_to_art.py`. `KNOWN` lists `camnon.png`. The 2026-10-26 ledger says that PNG was deleted. It is not in the folder now. Generated `art_camnon` is still in `firmware/art/`. `KillCorder.png` is in the folder and not in `KNOWN`. `docs/Layouts/` holds the PSD and screen PNGs. Nothing in the README links them. Move to `art/mascots/` and `art/layouts/`, update `DEFAULT_DIR` in the same change, and add a short caption list for the layout PNGs. Whether Camnon’s missing PNG and KillCorder’s extra PNG are still the catalog is an art decision.

**`first-boot-notes.md` is the PCB-stage punch list and nothing links to it.** Three boxes: magnetometer cal, cardinal cal, force a sky wait on first boot. The breadboard keeps its current compass numbers until then. Rename to `docs/esp32-bringup.md`. `pcb-bom.md` already says breadboard compass offsets do not transfer.

**README test count is behind the tree.** README says “All three tests must print `ok`.” The command block above it lists four tests. `architecture.md` lists twelve test binaries, including `screen_test` and `relay_test`. Point the README at `make test` and the architecture table.

**Protocol intro still says the board is in the future.** `docs/radio-protocol.md`: “Firmware is C and runs in a desktop emulator and, later, on the board.” Feather paging is described as transmitting in `hardware.md`. The ESP32 image is the thing that is still later. Say: emulator and Feather image today; ESP32 board has a schematic and no firmware image.

**Datasheets are the ESP32 set, unlabeled.** Sixteen PDFs under `docs/datasheets/` (about 14.1 MB, ~81% of the tracked tree). They match ESP32-board parts. No nRF52840, SX1262, or PA1616S PDF. `pcb-bom.md` also links vendor PDFs that are not local (USBLC6, LM66100, MBR0530, AO3401A, AO3400A, TL3305). Move the folder to `docs/esp32-datasheets/` or add one index line. Leave the PDFs in git.

### Code and folders

**Desktop support is split across three names that all sound like the emulator.** `emu/` is the SDL window. `hal/emu/` is the RAM HAL that tests also link. `firmware/host/mesh.c` is the desktop foot-grid and is not in the UF2. A wildcard over `firmware/` would be one edit away from pulling `mesh.c` into a board image. The Makefile avoids that today by using `FW_CORE` for the UF2 and `FW_LINK` for the host.

**Do:** `emu/` → `bench/`. `hal/emu/` → `hal/ram/`. `firmware/host/mesh.c` → `bench/mesh.c`. Keep `firmware/` as portable product code only. Drop `firmware/host/` once `mesh.c` has moved.

**`make nrf-check` is the old directory’s name.** It host-compiles portable sources plus `beeper_hal.c`. It does not compile `board.c`, does not link, and does not flash. Rename to `check-feather` (or `check-host`) and write objects under `build/check/`.

**Bringup is not self-contained, and `uf2.py` is named as if only bringup uses it.** `bringup/uf2.py` packs both UF2s. `hal/beeper/start.S` and `sx1262_setup.c` are linked into bringup and the pager. Changing the UF2 family id or the startup file while “fixing bringup” changes the pager image too.

**Do:** `bringup/uf2.py` → `tools/uf2.py`. Leave `sx1262_setup.c` shared under `hal/feather/` and say so in the Makefile. Startup can stay with the Feather HAL if bringup’s link line names that path on purpose.

**`beeper.h` and `beeple.h` are different modules one letter apart.** `beeper_*` is the device object. `beeple_*` is the mascot catalog. Menu strings in `screen.c` use both “Peeple” and “Beeple” on purpose. `png_to_art.py` and `beeple_to_art.py` both rewrite `art.c` / `art.h`.

**Do:** keep the menu words and `beeple.h`. Optional later rename, and it touches almost every firmware file: `beeper_t` → `unit_t`, `firmware/src/beeper.c` → `firmware/src/unit.c`, `firmware/include/beeper.h` → `firmware/include/unit.h`. Do this only after the machine folders have moved.

**The test named `page` is the message session, and CMake is a second, narrower build.** `page.c` was renamed to `message.c`. The test was not. `page_t` in `hal.h` is still the right name for a received page, so “page” means both the LCD page buffer and the air message. `CMakeLists.txt` builds host tests and `peeple-emu` only. It has no `screen_test` and no UF2 targets. `make test` and `cmake --build` do not run the same set.

**Do:** `page_test.c` → `message_test.c`. Leave `page_t` and the LCD “page” wording alone. Either teach CMake the UF2s and `screen_test`, or state in the README that CMake is host-only.

**`beeper_t.radio` is the message session, not the modem.** The field is `message_session_t radio`. The modem is `hal_t`’s `radio_*`. Rename the field to `message` when touching that header.

### Gitignore

**Small generated fab files.** Assembly PDF (199 KB), BOM, and the extra centroid CSVs are reproducible and will go stale next to the board. Same verdict as the zip: one snapshot, or ignore them.

**Editor junk that is already ignored** should stay ignored. Do not delete these rules. None of these paths are tracked:

| Path | Approx size |
|---|---|
| `.toolchain/` | 1.02 GB |
| `.tmp-cq/` | 896 MB |
| `hardware/pcb/.history/` | 26.7 MB |
| `build/` | 15.8 MB |
| `hardware/pcb/.mcp-backups/` | 4.4 MB |

Also already ignored: `.DS_Store`, `emu-nvm-*.bin`, KiCad `~*` lockfiles, `.cursor/mcp.json` (local node/python paths, no credentials).

**Datasheets, layout PNGs, the PSD, the font, custom footprints, and the six module STEP models stay.** They are source. The module STEPs total about 174 KB and are not a regen of `pcbv2.step`. `emu/third_party/roboto/Roboto-Regular.ttf` (515 KB) and `stb_truetype.h` stay. Git LFS for the datasheets is optional later. It is not a reason to ignore them.

No secrets showed up (no `.env`, keys, pems, or token-shaped strings).

## Light

**Folder polish.** `docs/BEEPLE PNG` (space, all caps), `docs/Layouts` (capital L), `Send to PCBway` (space, verb phrase). `docs/README.md` is titled `# PEEPLE BEEPER`, same as the root, so it does not read as an index. Covered by the target tree.

**Historical sentence left in the README porting section.** “Do not keep a TypeScript UI and swap in U8g2.” `architecture.md` already says there is no TypeScript pager. Safe to delete once the index exists.

**`architecture.md` directory map undersells `docs/`.** It says the folder is this map plus protocol, app, and HAL notes. The folder also holds PCB docs, assembly, art, and review logs.

**Screen-map shorthand.** Layout filenames (`Tracking Bot Diag`, `Tracking forward`, `Compose beep retry`) line up with `firmware-app.md` screens only if you already know the product. A five-line caption list in `art/layouts/README.md` is enough. Do not duplicate the screen table.

**Leftover “nRF” and “beeper” spellings inside the Feather HAL.** `static gps_fix_t nrf_fix` in `beeper_hal.c`. Include guard `BEEPER_SX1262_SETUP_H` in `sx1262_setup.h`. Rename with the file move (`feather_fix`, `FEATHER_SX1262_SETUP_H`).

**KiCad lock files on disk.** `hardware/pcb/~pcb.kicad_sch.lck` and the matching `~pcb.kicad_pro.lck` and `~pcb.kicad_pcb.lck`. Already gitignored. They look like a third schematic because of the `~`.

**Root `emu-nvm-*.bin`.** Gitignored runtime copies, 94 bytes each. The app now saves under the per-user Peeple/Beeper directory. They can be deleted locally. No rename.

**Missing ignore patterns** for files that are absent today, or that only stay out because they live under `build/`:

- `_autosave*` (KiCad autosave). `hardware/pcb/_autosave-pcb.kicad_pcb` is not ignored. `*.kicad_pcb-bak` and `*-backups/` are.
- `*.o` `*.elf` `*.uf2` `*.map` outside `build/`. Today’s `beeper.uf2`, `beeper.elf`, `bringup.uf2`, and the `.o` files are all under `build/`.
- `*.bak` in general. Only `*.kicad_sch-bak` and `*.kicad_pcb-bak` are covered.
- `__pycache__/`, `*.pyc`.
- `Thumbs.db`.
- `.cursor/` as a directory. Only `.cursor/mcp.json` is ignored. Other files under `.cursor/` would be addable.
- `*.lck` is listed twice. Harmless.

A blanket `*.bin` is optional. `emu-nvm-*.bin` and `build/` already cover the real files. A global `*.bin` would also hide a future intentional blob.

## Gitignore

Tracked tree is 173 files, about 17.4 MB on disk. History’s large blobs are datasheets and design files, one copy each. Nothing generated is in the index today.

Current `.gitignore` already covers `build/`, `.toolchain/`, `emu-nvm-*.bin`, KiCad backups, `fp-info-cache`, `*.lck`, `.cursor/mcp.json`, `.DS_Store`, `.history/`, `.mcp-backups/`, `~*`, and `.tmp-cq/`.

Add this. It was not applied.

```gitignore
# KiCad local UI (viewport, hidden nets). Untrack the copies already in the index.
*.kicad_prl
_autosave*

# Python
__pycache__/
*.pyc

# Build products if they land outside build/
*.o
*.elf
*.uf2
*.map

# OS
Thumbs.db

# Fab plots regenerated from the ESP32 board.
# README.md and ORDER-NOTES.md in the manufacturing folder can stay tracked.
# Adjust the directory if "Send to PCBway" is renamed first.
Send to PCBway/gerbers/
Send to PCBway/*.zip
Send to PCBway/*.step
Send to PCBway/*.pdf
Send to PCBway/*.csv
```

Stop tracking, keep the working copies:

- `hardware/pcb/pcb.kicad_prl`
- `hardware/beeper/beeper.kicad_prl`

Do not delete, and do not ignore:

- `hardware/pcb/pcbv2.kicad_pcb`, `pcbv2.kicad_pro` (clear the `/Users/ohlow/Desktop/` plot path first)
- `hardware/pcb/pcb.kicad_pcb`, `pcb.kicad_sch` (source; the old layout stays until the copper diff)
- `hardware/beeper/beeper.kicad_sch` and its project file
- `docs/datasheets/*.pdf`
- `docs/Layouts/`, `docs/Layouts/Beeper layouts.psd`, `docs/BEEPLE PNG/`
- `hardware/pcb/modules.pretty/`, `modules.kicad_sym`, `modules.3dshapes/*.step`
- `fp-lib-table`, `sym-lib-table`
- `emu/third_party/`

Do not add: `pcbv2.kicad_prl`, `Send to PCBway/pcbv2.step`, `Send to PCBway/gerbers/`.

Candidate verdicts:

| Path | Tracked? | Size | Verdict |
|---|---|---|---|
| `hardware/pcb/pcb.kicad_prl` | tracked, dirty | 2.6 KB | untrack, keep on disk |
| `hardware/beeper/beeper.kicad_prl` | tracked | 2.3 KB | untrack, keep on disk |
| `hardware/pcb/pcbv2.kicad_prl` | untracked | 2.6 KB | ignore |
| `Send to PCBway/pcbv2.step` | untracked | 12.7 MB | ignore |
| `Send to PCBway/gerbers/` | untracked | 728 KB | do not add; duplicate of the zip |
| `Send to PCBway/pcbv2-gerbers.zip` | untracked | 166 KB | ignore, or the one snapshot you keep |
| `Send to PCBway/pcbv2-assembly.pdf` | untracked | 199 KB | ignore, or one snapshot |
| `Send to PCBway/pcbv2-centroid*.csv` (3 files) | untracked | ~17 KB | keep at most the PCBWay CSV |
| `Send to PCBway/pcbv2-BOM-PCBWay.csv` | untracked | 2.9 KB | ignore, or one snapshot |
| `Send to PCBway/README.md`, `ORDER-NOTES.md` | untracked | ~3 KB | keep |
| `hardware/pcb/pcbv2.kicad_pcb` | untracked | 900 KB | keep |
| `hardware/pcb/pcbv2.kicad_pro` | untracked | 18 KB | keep |
| `docs/datasheets/*.pdf` | tracked | 14.1 MB | keep |
| `docs/Layouts/Beeper layouts.psd` | tracked | 497 KB | keep |
| `emu/third_party/roboto/Roboto-Regular.ttf` | tracked | 515 KB | keep |

## What the builds actually are

| What you run | Entry | What it is |
|---|---|---|
| `make beeper`, `make flash-beeper` | `hal/beeper/main.c` | Pager for the Feather nRF52840. Links `firmware/src`, `firmware/art`, `beeper_hal.c`, `board.c`, `sx1262_setup.c`, `start.S`, `hal/beeper/feather.ld`, then `bringup/uf2.py`. |
| `make bringup`, `make flash-bringup` | `bringup/bringup.c` | Wire test on the same Feather. No `beeper_tick`, no `firmware/src`. Its own `feather.ld`. Same bootloader drive, file `bringup.uf2`. |
| `make emu` / `make run` / `peeple-emu.command` | `emu/main.c` | SDL bench, up to 16 pagers. Output `build/peeple-emu`. Not flashable. |
| `make test` | each `firmware/tests/*_test.c` | Host proofs. `gps_test` also links `beeper_hal.c`. |
| `make nrf-check` | none | Host `clang -c` of firmware, art, and `beeper_hal.c` into `build/nrf/`. Does not compile `board.c`. |
| `cmake -B build/cmake` | `emu/main.c` and the test mains | Host only. Omits `screen_test` and both UF2s. |

`make flash-beeper` and `make flash-bringup` write different files to the same Feather drive. Neither image runs on the ESP32-S3 PCB.

Old names gone from source but still in `build/nrf/` and the lens ledgers: `hal/nrf/nrf_hal.c`, `page.c`, `pager.c`, `air.c`, `hal_ram.c`. Today those jobs are `hal/beeper/beeper_hal.c`, `message.c`, `screen.c`, `firmware/host/mesh.c`, and `hal/emu/emu_hal.c`.

## Doc inventory

| Path | Role | Status |
|---|---|---|
| `README.md` | Build, emulator, pair steps, doc index | Current file, stale stub claims |
| `docs/README.md` | Same text as the root README | Duplicate; links break from inside `docs/` |
| `docs/architecture.md` | Layer cake, file ownership, tests | Current firmware map |
| `docs/hal.md` | `hal_t` contract, RAM HAL, Feather pins | Current body; opening is stale |
| `docs/firmware-app.md` | Screens, menus, store, alerts | Current; leftover `pager.h` and menu names |
| `docs/radio-protocol.md` | Frames, crypto, pairing, relay, locate | Current; cites `pager.h` |
| `docs/source-overview.md` | File-by-file tour | Current; overlaps the three docs above; unindexed |
| `docs/hardware.md` | Feather nRF52840 pin contract | Current for the breadboard only |
| `docs/prototype-assembly.md` | Breadboard wiring and wire-test | Intro contradicts the rest of the file |
| `docs/first-boot-notes.md` | Mag cal, true north, first sky wait | Current TODO, unindexed |
| `docs/pcb-bom.md` | ESP32 shopping list, qty 10, prices dated 14 Nov 2026 | Current parts list; board-size paragraph is stale |
| `docs/pcb-connections.md` | Nets for `hardware/pcb/pcb.kicad_sch` | Current schematic companion |
| `docs/lens-ledger-*.md` (10 files) | Review logs | Historical. Paths may be gone |
| `Send to PCBway/README.md` | What to upload for pcbv2 | Current fab cover sheet, untracked |
| `Send to PCBway/ORDER-NOTES.md` | PCBWay options, 2-layer, ~103 × 61 mm | Current order sheet, untracked |
| `hardware/beeper/beeper.kicad_sch` text block | Only write-up of the Feather carrier | Pin-drive sentences are stale |
| `docs/Layouts/` | Screen mockups, PNG + PSD | Visual reference, unindexed |
| `docs/BEEPLE PNG/` | Mascot PNGs | Art source. Build input for `beeple_to_art.py` |
| `docs/datasheets/` | 16 PDFs | ESP32-board datasheets. Keep |

No `CONTEXT.md`, `AGENTS.md`, changelog, or `docs/temp-punch-list.md`.

## Not done in this pass

- Copper diff of `pcb.kicad_pcb` against `pcbv2.kicad_pcb`. Do not delete the old layout until that exists.
- Net-by-net check of `docs/prototype-assembly.md` against `hardware/beeper/beeper.kicad_sch`. The schematic title and `docs/pcb-bom.md` both call that sheet the Feather carrier.
- No ESP32 source, linker script, or flash rule turned up. `docs/pcb-bom.md` calls that image later firmware. It is not a directory that was missed.
- Whether anyone still builds with CMake on purpose. CMake omitting `screen_test` looks like drift.
- Whether `camnon.png` should come back and whether `KillCorder.png` belongs in the mascot catalog.
