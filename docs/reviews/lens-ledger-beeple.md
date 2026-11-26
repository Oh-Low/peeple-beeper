# Lens ledger — beeple

Working tree only. `main` matches `origin/main` at `df5f03c`, so there is no commit range. `git diff HEAD` is `firmware/src/screen.c`, `firmware/include/screen.h`, and `art/layouts/Beeple stat screen.psd`. Untracked: `art/layouts/Beeple select screen.png`, `art/layouts/Beeple stat screen.png`.

| Fact | Value |
|---|---|
| Stack | C11 firmware (`clang -std=c11 -Wall -Wextra -Werror`). |
| Checks | `make test`. No CI workflow. |
| Ships | `firmware/src`, `firmware/include`, `firmware/art`, `hal/`. |
| Tests | `firmware/tests`. |
| Doctrine | `docs/architecture.md`. No `CLAUDE.md`, no `## Review lenses` block. |

Lenses: [ripoff-bandaids](b5921e27-f041-4026-991b-e325457f6cb3), [scope](f2627775-3cd2-4df7-b044-a82105fae7d9), [correctness](4a77dec2-5efd-4339-b4e8-a8c3d100dc76), [latent-design](8b201213-021e-4bd9-b2fa-cd60a0cabd56), [architecture](92362aa1-5ace-4e7b-ba8f-daf6ebc98d2e), [comments](f441b49d-d613-4c5b-97fa-a835665ea4da).

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 1 | ripoff, latent, architecture | The Beeple column slide is a second copy of `display_slide`. | `firmware/src/screen.c` `draw_beeple_column` | REJECTED-`display_slide` copies full page-rows (`firmware/src/display.c`). The body starts at y=22, which shares a page byte with the tab. A clip would be a new compositor. This slide is only the left column under the title. |
| 2 | ripoff, correctness, latent, architecture | A contact press returns stay, so a following detent moves the canned row, and Back still slides that same pair. | `firmware/src/screen.c` `on_press`, `screen_input` | FIXED |
| 3 | correctness | A Beeple press returns stay, so a following detent changes the saved mascot. | `firmware/src/screen.c` `on_press`, `beeple_turn` | FIXED |
| 4 | correctness | A detent already queued ahead of the press switches the page, so the click lands on Stats. | `firmware/src/screen.c` `screen_input` | REJECTED-events run in queue order. A turn that arrived before the click is a turn. |
| 5 | correctness | During the column slide the tab already names the destination, and the name row tears. | `firmware/src/screen.c` `draw_beeple_column` | REJECTED-the title row stays put and follows the page; the name is inside the body that slides. |
| 6 | architecture | A name detent writes the saved mascot through `screen_set_beeple`, and Back does not restore the previous index. | `firmware/src/screen.c` `beeple_turn` | REJECTED-turning the name updates the picture and keeps the choice. Back only returns focus to the tab. |
| 7 | ripoff, scope, architecture | `sel` is stored on Beeple entry and on a name turn, and that screen never reads it. | `firmware/src/screen.c` `beeple_turn`, menu press | FIXED |
| 8 | scope | The body clear repeats `display_clear`. The header, tab, and mascot are outside that rectangle, and the slide overwrites every column. | `firmware/src/screen.c` `draw_beeple_column` | FIXED |
| 9 | scope | The page clamp in the column draw is unreachable. Entry sets 0 and the turn wraps. | `firmware/src/screen.c` `draw_beeple_column` | FIXED |
| 10 | scope | `beeple_slide_from` is `1 - page` while there are two pages. | `firmware/include/screen.h` | REJECTED-the field is the page that was showing. Further titles use the same slide, so the leaving page is not always the other one. |
| 11 | scope | The three layout files have no firmware consumer. | `art/layouts/` | REJECTED-`art/layouts` is the design shelf. The firmware does not load these pictures; they are the layouts the screen was copied from. |
| 12 | ripoff, latent, architecture | The stat marks are a second bitmap encoding beside `art.c`. | `firmware/src/screen.c` `draw_beeple_stats` | REJECTED-the marks are painted once, beside placeholder text, the same way the chevron stays in the screen. Re-packing them would redo pixels that were just matched to the layout. |
| 13 | latent | Compose and canned should be one mode, so Back cannot slide a frame the press opted out of. | `firmware/src/screen.c` `SCREEN_CANNED` | REJECTED-the slide on that pair is row 2. Canned remains the screen that sends, and body edit returns to it. |
| 14 | comments | The new `screen.h` comment restates which page and focus values the screen already branches on. It is not false. | `firmware/include/screen.h` | FIXED |
