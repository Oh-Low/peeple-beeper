# Lens ledger — fishing

`git diff 286de72` against the working tree, fishing game and the Games menu only. `main` matches `origin/main` at `f4666e1`. Uncommitted: `firmware/src/fishing.c`.

| Fact | Value |
|---|---|
| Stack | C11 firmware (`clang -std=c11 -Wall -Wextra -Werror`). |
| Checks | `make test`. No CI workflow. |
| Ships | `firmware/src`, `firmware/include`, `firmware/art`, `hal/`. |
| Tests | `firmware/tests`. |
| Doctrine | `docs/architecture.md`. No `CLAUDE.md`, no `## Review lenses` block. |

Lenses: [ripoff-bandaids](80c21c9b-17a2-4cbf-914d-211a2c96930d), [scope](d679eab5-6d5a-473a-8b30-7a7c67731148), [correctness](92615ed4-a6c7-4ee1-a24c-259009e98bfd), [latent-design](ef28b46b-b1b2-4178-9f44-33fb64d216c7), [architecture](3af407fa-9676-4266-9fdc-724cb6db8ba7), [comments](0bf43a34-bfbf-4d7b-8843-6553bfd890d5).

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 1 | ripoff, correctness, latent, architecture | Leaving the game for a page or a tracking prompt clears the motor those screens just started. | `firmware/src/screen.c` `screen_poll_fish`; `firmware/src/fishing.c` `fishing_stop` | FIXED |
| 2 | correctness | A click during the bite is read after the 350 ms window has already been expired, so it does not catch. | `firmware/src/beeper.c` `beeper_tick` | FIXED |
| 3 | ripoff | Each ripple is drawn twice, and a flag punches holes in the inner copy. Three radius checks never fire. | `firmware/src/fishing.c` `draw_fish_waves` | FIXED |
| 4 | ripoff, scope, architecture | The ripple segment and the `#` stamp copy `draw_line` and `draw_icon`. | `firmware/src/fishing.c` `draw_fish_seg`, `draw_rows` | FIXED |
| 5 | scope | The game index is clamped twice. | `firmware/src/screen.c` `draw_beeple_games` | FIXED |
| 6 | comments | The gap comment is false: shifted rings do not each open at the corners and the bottom, and neighbors share holes. | `firmware/src/fishing.c` `fish_open` | FIXED |
| 7 | comments | The curve comment overclaims the span and that the table is only the calm ripples. The exponent holds. | `firmware/src/fishing.c` `FISH_WAVE` | FIXED |
| 8 | comments | The thickness comment overclaims a two-pixel stroke. A shoulder of the two polylines is three pixels wide. | `firmware/src/fishing.c` `fish_thick` | FIXED |
| 9 | comments | The `beeple_game` comment restates the Fishing/Farming labels. | `firmware/include/screen.h` | FIXED |
| 10 | ripoff | The Games label repeats `draw_marked` and then picks its own x. | `firmware/src/screen.c` `draw_beeple_game_label` | REJECTED-centered x is 19 and 18. The layout table is 21 and 19, and `test_beeple_games` locks those pixels. |
| 11 | ripoff | `beeple_turn` repeats the wrap four times. | `firmware/src/screen.c` `beeple_turn` | REJECTED-the mascot arm adds `dir` and calls `screen_set_beeple`. The other arms step by one and store the index. |
| 12 | latent, architecture | Fishing should be its own screen mode, and the poll that notices the mode changed should go away. | `firmware/src/screen.c` `SCREEN_BEEPLE` | REJECTED-the press returns `NAV_STAY` and Back stays on `SCREEN_BEEPLE`. A new mode arms the page slide when the mode changes (`screen_input`). The grab stays inside Beeple. Row 1 stops the poll from cancelling the other screen's motor. |
| 13 | latent | The Games row is three lists and an index. | `firmware/src/screen.c` `draw_beeple_game_art` | REJECTED-two cards, and only Fishing starts a game. A record would still special-case that click. |
| 14 | scope | `screen_test.c` changes nothing the pager runs. | `firmware/tests/screen_test.c` | REJECTED-tests are not a cut. |
| 15 | scope | The layout files have no firmware consumer. | `art/layouts/` | REJECTED-`art/layouts` is the design shelf. The firmware does not load these pictures. |
| 16 | architecture | The bite timing is pinned in `screen_test`, which the doctrine names for the backlight. | `firmware/tests/screen_test.c` `test_fishing` | REJECTED-the test enters through the Games row and checks the mode, the page, and the focus. That is the screen handoff. The phase checks are the same scenario. |
