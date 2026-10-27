# Lens ledger — firmware/src split

Range: uncommitted working tree against `dc31b5f` on `main`. No upstream, no pull request. The three `.c` moves are content-identical. `make test` passed. `make nrf-check` compiles `firmware/src`, `firmware/art/art.c`, and `hal/nrf/nrf_hal.c`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | ripoff, scope, correctness, architecture, comments | The open HAL item still says `hal_ram.c` lives in `firmware/src`. | `docs/temp-punch-list.md:6` | FIXED |
| 2 | ripoff, scope | Delete `docs/source-overview.md` and `docs/temp-punch-list.md`. Nothing in the build reads them. | both files | REJECTED-the user asked for both files in this session. False sentences are rows 3–8. |
| 3 | comments | "With no identity yet … nothing is written." `store_save` still calls `nvm_save` with a zero-length buffer. | `docs/source-overview.md:33`, `firmware/src/store.c:375-377` | FIXED |
| 4 | correctness, comments | A fresh device does not stay tracking-open: `locate_init` sets `track_open`, and the first tick clears it when `store_load` fails. A locate reply does not call `locate_gps_soon`. | `docs/source-overview.md:43`, `firmware/src/beeper.c:33`, `firmware/src/locate.c:14`, `firmware/src/locate.c:75-91`, `emu/main.c:501` | FIXED |
| 5 | correctness | "A page and a locate cannot be on the air at the same time." A locate reply transmits inside `on_raw_frame` without entering `LOCATE_SENDING`, so `locate_busy` stays false and a due page retry can transmit on that same tick. | `docs/source-overview.md:15`, `firmware/src/page.c:153-155`, `firmware/src/page.c:233-234`, `firmware/src/locate.c:21-22` | FIXED |
| 6 | correctness | Change the radio so a locate reply and a page retry cannot both transmit on one tick. | `firmware/src/page.c:232-234`, `firmware/src/locate.c:216` | ASKED |
| 7 | comments | The bench does not draw lines between radios. It prints a `hears` list. | `docs/source-overview.md:77`, `emu/main.c:409-417` | FIXED |
| 8 | comments | Overview overclaims: a page is acknowledged only after the seal opens; facing uses `(degrees + 22) / 45` while the target offset uses the tangent split; tilt is clamped in the host setter, and the sample the firmware reads copies tilt through; the mesh hands the same buffer to a ring that can drop or truncate it; the nRF stub's display buffer, counter, and GPS power flag are also live, and a newline stores a fix only when the parser accepts the line. | `docs/source-overview.md:15`, `:47`, `:67`, `:69`, `:71` | FIXED |
| 9 | comments | Delete the pasted prompt under the punch list. | `docs/temp-punch-list.md:22-23` | REJECTED-the user asked for that paste. |
| 10 | architecture | Move `hal_ram.h` and `air.h` out of `firmware/include` so `nrf-check` cannot see the host types. | `firmware/include/hal_ram.h`, `firmware/include/air.h`, `Makefile:2` | ASKED |
| 11 | architecture | Link `air.c` only into `mesh_test` and the emulator. `page_test` and `store_test` only need `hal_ram.c`. | `Makefile:29-30` | ASKED |
| 12 | latent, architecture | `page_tick` is the receive switch for locate frames and then calls `locate_tick`. | `firmware/src/page.c:149-157`, `firmware/src/page.c:289-298` | DEFERRED-temp-punch-list peer radio sessions |
| 13 | latent | The mascot catalog is a private table in `pager.c` that `beeple_to_art.py` rewrites. | `firmware/src/pager.c`, `tools/beeple_to_art.py:163-177` | DEFERRED-temp-punch-list one home for a mascot |
| 14 | architecture | `docs/architecture.md` lists `beeper_t` without `locate`. The struct has it. | `docs/architecture.md:63-69`, `firmware/include/beeper.h:18` | FIXED |
| 15 | scope | Dropping the `png_to_art.py` hunk would point `--write` at the removed path. The shipping binary does not run the script. | `tools/png_to_art.py:27` | REJECTED-the hunk is the writer path. It stays. |
| 16 | scope | `CMakeLists.txt` and the path-only doc edits change no device behavior. | `CMakeLists.txt`, `README.md`, `docs/architecture.md`, `docs/firmware-app.md`, `docs/hal.md`, `docs/hardware.md`, `docs/radio-protocol.md` | REJECTED-they are the second build entry and the paths the move would otherwise leave false. |
| 17 | scope | Dropping the `air.c` or `hal_ram.c` move puts host sources back on the `firmware/src` wildcard. | `firmware/host/air.c`, `firmware/host/hal_ram.c`, `Makefile:31` | REJECTED-the moves are what keep `FW_CORE` off the host. |
| 18 | latent, architecture | Two send loops, two relay caches, the octant helper, the blank-name fallback, the GPS duty cycle, the two NMEA ingests, and the remaining renames are unchanged by this diff. | punch list | DEFERRED-temp-punch-list |
| 19 | scope | `build/nrf/air.o` and `build/nrf/hal_ram.o` can linger from an earlier `nrf-check`. | `build/nrf/` | REJECTED-`build/` is compiler output. `make clean` removes it. |
