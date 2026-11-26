# Lens ledger — slide

Working tree only. `main` matches `origin/main`, so there is no commit range. `hardware/pcb/pcb.kicad_pro` was excluded.

| Fact | Value |
|---|---|
| Stack | C11 firmware (`clang -std=c11 -Wall -Wextra -Werror`). |
| Checks | `make test`. |
| Ships | `firmware/src`, `firmware/include`, `firmware/art`, `hal/`. |
| Tests | `firmware/tests`. |
| Doctrine | `docs/architecture.md`. No `CLAUDE.md`, no `## Review lenses` block. |

Lenses: [ripoff-bandaids](9d584604-25e2-4411-910b-80c9f2ef2321), [scope](eadf0e36-e1ac-4660-b889-efdcb576b38b), [correctness](b929de08-dad3-47b4-8053-28b8b0e4a196), [latent-design](4a2eb790-4560-4b80-9f02-63a356aae9c1), [architecture](712e7005-a1e9-4072-a30e-ee8a2016f09a), [comments](5f71cf71-d000-44f2-8bad-69416672cd6d).

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 1 | ripoff, latent, architecture | Press direction is a second copy of the navigation graph. `press_returns` re-reads the mode pair after `on_press`. Alert press and alert back both open Read and must slide opposite ways, so the pair is not enough. | `firmware/src/screen.c` `press_returns` | FIXED |
| 2 | correctness | Back during the 200 ms slide is discarded. On Add and Locating that is the only abort, so pairing can still commit and a locate keeps transmitting. The drop answer was about the encoder. | `firmware/src/screen.c` `screen_input` | FIXED |
| 3 | latent | A second encoder event in the same `screen_input` still runs after the event that armed the slide. The swallow only happens on a later call. | `firmware/src/screen.c` `screen_input` | FIXED |
| 4 | architecture | A poll can change mode before input. The next arm copied `screen->buf`, which is the interrupted destination, not the frame on the glass. | `firmware/src/screen.c` `arm_slide` | FIXED |
| 5 | architecture | Teaching every poll to cancel the slide would put the slide into writers that only assign a mode. Render still drops a slide whose destination is no longer current. | `firmware/src/screen.c` `screen_render` | REJECTED-docs/architecture.md one decision per file; polls do not own the slide. The glass snapshot in row 4 is the wrong-frame fix. |
| 6 | ripoff, scope | `display_slide` special-cases shift 0 and ±width. The page loop already covers those cuts. The clamp stays so a shift past the row cannot read off the buffer. | `firmware/src/display.c` `display_slide` | FIXED |
| 7 | scope | `SCREEN_SENT` and `SCREEN_FAILED` in `slide_cut` are never reached from `arm_slide`. Only `screen_poll_send` enters them, and it does not arm a slide. | `firmware/src/screen.c` `slide_cut` | FIXED |
| 8 | comments | The new comments on `display_slide` and `slide_from` restate the code. Neither is false. | `firmware/include/display.h`, `firmware/include/screen.h` | FIXED |
| 9 | architecture | `SLIDE_MS` is a second name for `SCREEN_SLIDE_MS`. | `firmware/src/screen.c` | FIXED |
| 10 | scope | Dropping `screen_test.c` changes nothing that ships. | `firmware/tests/screen_test.c` | REJECTED-docs/architecture.md lists `screen_test` as the screen's proof. Tests are not bloat. |
