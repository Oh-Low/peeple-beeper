# Lens ledger, Feather image, 2026-10-27

Uncommitted working tree against `7e6df5f`. Six lenses. Dispositions are `FIXED`, `DEFERRED-<owner>`, `ASKED`, or `REJECTED-<citation>`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | correctness | After the first fix, the next listen accepts that same sentence on the following tick and shuts the receiver off. The menu clock stays on that minute. | `hal/beeper/beeper_hal.c:122`, `firmware/src/locate.c:372` | FIXED |
| 2 | correctness | Coming back to level calls `locate_gps_capture`, which reads `gps_fix` in the same call that turns the receiver on. No new sentence can arrive, so the board stores the previous fix or nothing. | `firmware/src/locate.c:360`, `firmware/src/screen.c:1495` | FIXED |
| 3 | correctness | A failed GetPacketStatus still delivers the frame with RSSI 0, and pairing keeps frames at or above −45 dBm. | `hal/beeper/board.c:678`, `firmware/src/pair.c:224` | FIXED |
| 4 | correctness | A flash erase stalls the CPU, so knob edges inside that stall never reach the queue, and a multi-page save can drop NMEA. | `hal/beeper/board.c:706` | REJECTED-the nRF52840 does not execute from flash during an erase, so `board_sample` in `nvmc_wait` cannot run until the page finishes. One page is inside the 256-byte UART buffer. |
| 5 | ripoff, latent, architecture | The wire test and the pager each own the pin map, the bit-banged buses, the LCD transport, and the GPS UART. | `bringup/bringup.c:25`, `hal/beeper/board.c:16` | ASKED |
| 6 | ripoff | Weak `board_*` stubs exist so `gps_test` can link the Feather HAL. The overlong-line rule already lives in `nmea_push_byte`. | `hal/beeper/beeper_hal.c:170` | REJECTED-`firmware/tests/gps_test.c:85` is the UART seam. `docs/architecture.md` puts that line buffer on the board HAL. `make nrf-check` only compiles. |
| 7 | ripoff, latent, architecture | The RMC wall clock is a process-wide static in the parser. The HAL already keeps the fix and should keep the hour and minute. | `firmware/src/nmea.c:108`, `hal/beeper/beeper_hal.c:91` | FIXED |
| 8 | ripoff, scope | The missing-toolchain text and the UF2 copy recipe are written twice. | `Makefile:128`, `Makefile:146` | FIXED |
| 9 | scope | `bringup/start.S` and `hal/beeper/start.S` are the same vector table. | `bringup/start.S`, `hal/beeper/start.S` | FIXED |
| 10 | scope | `screen_set_menu_art` has one caller. The null-clear branch has none. | `firmware/src/screen.c:59` | FIXED |
| 11 | scope | `beeple_at` should return the row with no null check. | `firmware/art/beeple.c:21` | REJECTED-`BEEPLE_NONE` is 255 (`firmware/include/beeple.h`). A null return keeps that index off Voidfarer. |
| 12 | scope | Dropping the docs, the owner moves, and the test splits changes no device behavior. A reader would think those files changed the pager. | working tree vs `7e6df5f` | REJECTED-`docs/architecture.md` names these owners. The wiring notes and the test split are the record, not a second implementation. |
| 13 | latent, architecture | The listen schedule and the stored fix still sit on the locate session. | `firmware/include/locate.h:44`, `firmware/src/beeper.c:59` | DEFERRED-punch-list GPS duty cycle (`docs/temp-punch-list.md`) |
| 14 | architecture | `store_buf` is a firmware global. The composition root should own the scratch. | `firmware/src/store.c:366` | REJECTED-load, save, and sync do not nest (`firmware/src/beeper.c:55`, `firmware/src/beeper.c:79`). One scratch is the scope. A `beeper_t` member would repeat 100 KB for every bench radio. |
| 15 | architecture | The linker window is 106496 bytes and `NVM_BYTES` is 102400. One budget should derive the other. | `hal/beeper/feather.ld:11` | REJECTED-erase is 4 KB and the bootloader starts at `0xF4000`. The extra bytes are the page rounding above the 8-byte header. `board_nvm_save` checks both caps. |
| 16 | architecture | `bringup/uf2.py` is shared and should live under `tools/`. | `Makefile:142` | REJECTED-both images already call one packer. Moving the path does not remove a second decision. |
| 17 | comments | "A5 is not wired" is false. The motor is on A5. This image leaves the pin undriven. | `hal/beeper/board.c:830` | FIXED |
| 18 | comments | "Transmit stays off until a send is requested. Whisper is remembered for that send." Send drops the frame and never reads `whisper`. | `hal/beeper/beeper_hal.c:64` | FIXED |
| 19 | comments | The clock comment says any status-A RMC sets the time. A missing or illegal time field leaves the previous hour and minute. | `firmware/include/nmea.h:22` | FIXED |
| 20 | comments | Delete comments that restate the code: `bringup/bringup.c:1`, `:24`, `:83`, `bringup/uf2.py:2`, `firmware/include/nmea.h:19`, and the send comment in row 18. | those lines | FIXED |
| 21 | comments | Compress the pull-up, contrast, glyph, radio-mode, store, and art-tool comments to the one fact the names do not carry. | `bringup/bringup.c:235`, `hal/beeper/board.c:35`, `firmware/src/store.c:366`, `tools/beeple_to_art.py:3` | FIXED |
