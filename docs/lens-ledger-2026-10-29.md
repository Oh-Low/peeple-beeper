# Lens ledger, backlight timer and radio sniff, 2026-10-29

Uncommitted working tree against `b48b1b1`. Six lenses. Dispositions are `FIXED`, `DEFERRED-<owner>`, `ASKED`, or `REJECTED-<citation>`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | correctness, architecture, ripoff | A saved Off never drives D13 low. Boot pull-up holds the lamp on, `light_lit` starts false, and `light_set` skips the pin write. | `firmware/src/screen.c:85`, `hal/beeper/board.c:239` | FIXED |
| 2 | correctness | A page or a locate prompt leaves the time-zone and backlight grabs set, so the next Settings visit changes that value while Name is selected. | `firmware/src/screen.c:368`, `firmware/src/screen.c:560`, `firmware/src/screen.c:734` | FIXED |
| 3 | latent | Continuous receive is the same switch as whisper. Fold it into the beeper whisper setter and drop `radio_set_continuous`. | `firmware/src/pair.c:169`, `hal/hal.h:77` | REJECTED-whisper and continuous are two radio facts. `pair_air` is the one place they move together. One pairing-mode op would put a session in the HAL contract (`docs/architecture.md` layer split). |
| 4 | latent, ripoff | Time zone and backlight each carry an open-value flag for one grabbed settings row. | `firmware/include/screen.h:107`, `firmware/include/screen.h:112` | ASKED |
| 5 | architecture | The sniff's timeout mask is in the shared modem list, so the wire test inherits it and its transmit wait still looks for that bit. | `hal/beeper/sx1262_setup.c:20`, `bringup/bringup.c:733` | FIXED |
| 6 | architecture | Preamble length, the 10 ms TCXO start, and the 28/32 ms sniff have no single owner. Read the setup table when choosing the window. | `hal/beeper/board.c:602`, `hal/beeper/sx1262_setup.c:7` | REJECTED-`docs/architecture.md` keeps the sniff in `board.c` and the preamble in the shared list. A second reader of that table would restate the fit. The false sentence is row 8. |
| 7 | architecture | The backlight timer proof sits in `store_test`, which is the NVM round-trip. | `firmware/tests/store_test.c:171`, `docs/architecture.md:139` | FIXED |
| 8 | comments | Two radio comments are false or overclaim, the setup legend restates the length byte, and the hardware note repeats the preamble claim. | `hal/beeper/board.c:582`, `hal/beeper/board.c:602`, `hal/beeper/sx1262_setup.c:19`, `docs/hardware.md:57` | FIXED |
| 9 | ripoff | Off and on share a zero window. One table should hold the label, the duration, and the held level. | `firmware/src/screen.c:77` | REJECTED-after row 1 the steady level is applied once, in `light_wake`. `light_label` stays the labels. A table does not remove a second stepper. |
| 10 | scope | Dropping the emulator backlight field and the continuous-receive stub changes nothing the pager runs. | `hal/emu/emu_hal.h:29`, `hal/emu/emu_hal.c:64` | REJECTED-the screen proof reads `backlight_on`. The pager links `board_backlight`. The stub is the desktop mesh, which has no duty cycle (`Makefile` does not link `hal/emu` into `make beeper`). |

Second pass, same tree after rows 1–10. Row 4 stays ASKED. Row 6 stays REJECTED. Row 9 stays REJECTED. Correctness reported no defect.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 11 | latent, ripoff | The shared list writes the IRQ mask and the pager immediately overwrites it. One command, and each image passes its own mask byte. | `hal/beeper/sx1262_setup.c:19`, `hal/beeper/board.c:561` | FIXED |
| 12 | comments | The timeout-mask comment and the poll comment overclaim what those lines do. `Saved. Starts on.` repeats init. | `hal/beeper/board.c:561`, `hal/beeper/board.c:692`, `firmware/include/screen.h:110` | FIXED |
| 13 | scope | Settings Back clears the two grabs by hand, and `light_service` returns on a zero window that `light_wake` already cleared. | `firmware/src/screen.c:596`, `firmware/src/screen.c:104` | FIXED |
| 14 | scope | The notes say older saves store the backlight as on or off. Only versions 9 and 10 do. Earlier blobs leave it on. | `docs/source-overview.md` store paragraph, `firmware/src/store.c:332` | FIXED |
| 15 | architecture | `screen_test` writes locate’s queue fields instead of driving the session. | `firmware/tests/screen_test.c:100` | REJECTED-`locate_ask_pending` is `ask_n` and `ask_alert_ready` (`firmware/src/locate.c:495`). `locate_tracked` is `tracked_note` (`firmware/src/locate.c:540`). The screen proof sets those fields. A full exchange is `locate_test`. |
| 16 | architecture | Preamble, TCXO start, and the sniff are still one budget with no owner. | `hal/beeper/sx1262_setup.c:18`, `hal/beeper/board.c:605` | REJECTED-row 6. |
| 17 | ripoff | `light_lit` remembers a comparison the clock already answers. | `firmware/src/screen.c:101` | REJECTED-`board_backlight` rewrites the pin config on every call (`hal/beeper/board.c:100`). `light_lit` is what drives the off edge once. |
| 18 | scope | `screen_light_apply` only calls `light_wake`. | `firmware/src/screen.c:131`, `firmware/src/store.c:386` | REJECTED-`store_load` is the caller outside `screen.c`. The header name is that call. |

Third pass. Row 4 stays ASKED. Rows 6, 9, 16, and 17 stay REJECTED. Comments found nothing to change. Correctness found no defect.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 19 | ripoff | `sx1262_irq_mask` is a second call both images have to remember after setup. The mask byte belongs on `sx1262_setup`. | `hal/beeper/sx1262_setup.c:31`, `hal/beeper/board.c:569`, `bringup/bringup.c:672` | FIXED |
| 20 | scope | The weak `board_radio_continuous` is unused because the pager links `board.c`. | `hal/beeper/beeper_hal.c:211` | REJECTED-`gps_test` links `beeper_hal.c` without `board.c` (`Makefile:90-91`). `radio_set_continuous` calls `board_radio_continuous` (`hal/beeper/beeper_hal.c:63`). |
| 21 | architecture | The preamble, the 10 ms clock start, and the 32 ms sleep are still one budget split across two files. | `hal/beeper/sx1262_setup.c:18`, `hal/beeper/board.c:603` | REJECTED-row 6. |
