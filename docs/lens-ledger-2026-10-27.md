# Lens ledger, 2026-10-27

Uncommitted working tree against `7e6df5f`. Six lenses. Dispositions are `FIXED`, `DEFERRED-<owner>`, `ASKED`, or `REJECTED-<citation>`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | scope, ripoff, latent | `mascot()` reclamps an index to 0 before `beeple_at`. Callers already have a valid index. Clamp-to-0 is the wrong picture for `BEEPLE_NONE`. | `firmware/src/screen.c:22` | FIXED |
| 2 | scope, ripoff | `gps_compass` has no caller. The screen's facing labels are separate. | `firmware/src/gps.c:70`, `firmware/include/gps.h:20` | FIXED |
| 3 | scope, ripoff | `identity_blank_name` returns empty when `node_id` is null. Both callers pass a struct array. | `firmware/src/identity.c:39` | FIXED |
| 4 | scope | Also drop the `out` / `cap` guard on `identity_blank_name`. | `firmware/src/identity.c:38` | REJECTED-`node_id_to_hex` at `firmware/src/identity.c:28` has the same guard. A null `out` would write through it. |
| 5 | ripoff, latent, architecture | The partial NMEA line is a firmware-wide static. The board HAL owns the UART and already keeps the accepted fix. | `firmware/src/nmea.c:6`, `hal/beeper/beeper_hal.c:134` | FIXED |
| 6 | correctness | An overlong UART line resets the length and parses the tail as a new sentence, so a checksum-valid tail is stored. | `firmware/src/nmea.c:22` | FIXED |
| 7 | correctness | A full line whose last byte is `*` makes `checksum_ok` read one byte past the line. | `firmware/src/nmea.c:40` | FIXED |
| 8 | comments | The `nmea.h` comment sits on `nmea_reset` and says a finished RMC writes `*out`. A bad checksum or a missing status field leaves `*out` untouched. | `firmware/include/nmea.h:11` | FIXED |
| 9 | comments | `beeple.h` says the pixels live in `beeple.c`. That file holds names and pointers. The bytes are in `art.c`. | `firmware/include/beeple.h:7` | FIXED |
| 10 | comments | `gps_octant`'s comment restates the return values. The tan(22.5°) cut is the part the body does not say. | `firmware/include/gps.h:17` | FIXED |
| 11 | latent, architecture | The listen schedule and the stored fix still sit on the locate session. | `firmware/include/locate.h:44`, `firmware/src/beeper.c:59` | DEFERRED-punch-list GPS duty cycle (`docs/temp-punch-list.md`) |
| 12 | latent | One `relay_cache_t` for page and locate, ticked once. | `firmware/include/message.h`, `firmware/include/locate.h` | REJECTED-`docs/architecture.md` says each session decides what enters a cache. A full page flood must not evict a locate hop, so each session keeps its own 24 slots. |
| 13 | latent | Side menus keep a snapshot of the chosen mascot beside `beeple_at`. | `firmware/include/screen.h:102` | REJECTED-the snapshot is the confirmed index. The Beeple list previews `sel` before confirm (`firmware/src/screen.c:821`). |
| 14 | ripoff | `menu_art_invert` and `blit_slot`'s invert argument are always false. The packer already set polarity. | `firmware/src/screen.c:707`, `firmware/include/screen.h:105` | FIXED |
| 15 | scope | `beeple_at` should return the row with no null check. | `firmware/art/beeple.c:20` | REJECTED-`BEEPLE_NONE` is 255 (`firmware/include/beeple.h`). A null return keeps that index off Voidfarer. The alert draws the spider instead (`firmware/src/screen.c:1389`). |
| 16 | architecture | `page_harness.h` and `locate_harness.h` duplicate the radio boot. | `firmware/tests/page_harness.h`, `firmware/tests/locate_harness.h` | REJECTED-tests are not bloat. The page fixture has no locate session. The locate fixture does. |
| 17 | scope | Dropping each file in the diff changes no device behavior. The edits move an owner or rename a test. | working tree vs `7e6df5f` | REJECTED-`docs/architecture.md` names these owners: blank name, north/east octant, mascot catalog, RMC sink, and one test file per decision. |
