# Lens ledger, settings beep and vibe, 2026-10-30

Uncommitted working tree against `46c8d81`. Code only. Docs and the untracked schematic were not in the review. Six lenses. Dispositions are `FIXED`, `DEFERRED-<owner>`, `ASKED`, or `REJECTED-<citation>`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | ripoff, correctness, latent, architecture | A settings test still armed writes `vibe(0)` and `beep(0)` from render after a page or a tracking prompt already posted its chirp in the same tick. | `firmware/src/screen.c:402`, `firmware/src/screen.c:1591`, `firmware/src/beeper.c:74` | FIXED |
| 2 | ripoff | BEEP TEST calls `beep(250)` on press and again on every render. | `firmware/src/screen.c:655` | FIXED |
| 3 | ripoff | `vibe_on` remembers a level `drive` already applies. | `hal/beeper/board.c:901` | REJECTED-`drive` writes `PIN_CNF` on every call (`hal/beeper/board.c:97`). That rewrite is why the backlight keeps `light_lit` (`docs/lens-ledger-2026-10-29.md` row 17). `vibe_on` is the same edge for A5. |
| 4 | scope | The KiCad ignore lines change nothing the pager runs. | `.gitignore:9` | REJECTED-those patterns are what keep KiCad backups untracked. Git is the consumer. The pager does not read them. |
| 5 | scope | `beeper_reset`'s `beep(0)` is not in the device link. | `firmware/src/beeper.c:98` | REJECTED-`screen_reset` takes no `hal` (`firmware/src/screen.c:1736`) and only clears the flags (`firmware/src/screen.c:1756`). `beeper_reset` is the caller that can stop the pin. `firmware/tests/seed_test.c` calls it. The device image already drops the `vibe(0)` on the line above the same way. |
| 6 | scope | `beep_test = false` in `screen_reset` has no device caller. | `firmware/src/screen.c:1757` | REJECTED-row 5. It is the same reset as `vibe_test` on the previous line. |
| 7 | scope | `vibe_set` has one caller. | `hal/beeper/board.c:901` | FIXED |
| 8 | correctness | An 80 ms page chirp stays on through the ack transmit and the flash wait. Those loops call `board_sample`, and only `board_pump` expires the tone. | `hal/beeper/board.c:674`, `hal/beeper/board.c:759`, `hal/beeper/board.c:895` | FIXED |
| 9 | correctness | The first boot that clears the NFC pin may sit in the reset loop if the system-reset write never lands. | `hal/beeper/board.c:850` | REJECTED-the write is `SYSRESETREQ` and the spin is the wait for that reset. No second reset path on a plausible-only report. The false comment is row 12. |
| 10 | architecture | The live-deadline compare is copied in `board_heading`, `vibe_service`, and `beep_service`. | `hal/beeper/board.c:516`, `hal/beeper/board.c:896`, `hal/beeper/board.c:908` | REJECTED-a helper would name the compare and leave the pin service, the tone service, and the heading read. The motor deadline and the tone deadline are different words. |
| 11 | comments | `D2 stays an NFC pin until this bit is cleared` is early. The pin stays NFC until a reset samples the bit clear. | `hal/beeper/board.c:850` | FIXED |

Second pass, same tree after rows 1–11. Correctness did not return (provider error). Comments found nothing further. Scope restated rows 4–6.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 12 | ripoff, latent, architecture | Clearing the test flags is what keeps the next render from writing `vibe(0)` and `beep(0)` over a chirp. Page and locate do that in opposite orders. | `firmware/src/screen.c:751`, `firmware/src/screen.c:1669` | FIXED |
| 13 | architecture | A settings tone re-armed for 250 ms goes silent during a transmit. That wait sits in `board_sample`, which ends the tone when the deadline passes, and render cannot run until the wait returns. | `firmware/src/screen.c:1579`, `hal/beeper/board.c:677` | FIXED |
| 14 | scope | `beeper_reset`'s `beep(0)` and `screen_reset`'s `beep_test = false` are not in the device link. The KiCad ignore lines change nothing the pager runs. | `firmware/src/beeper.c:98`, `firmware/src/screen.c:1756`, `.gitignore:9` | REJECTED-rows 4, 5, and 6 |
