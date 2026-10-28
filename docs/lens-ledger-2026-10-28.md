# Lens ledger, heading and battery window, 2026-10-28

Uncommitted working tree against `3b36749`. Six lenses. Dispositions are `FIXED`, `DEFERRED-<owner>`, `ASKED`, or `REJECTED-<citation>`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | ripoff, latent, architecture | The breadboard compass correction is one decision written in both images. | `bringup/bringup.c:1126`, `hal/beeper/board.c:495` | ASKED |
| 2 | ripoff | Replace the −21° shift with the ~48° angle of the written north and south counts. Those poses come out NE and SW. | `bringup/bringup.c:1124`, `hal/beeper/board.c:503` | REJECTED-the pairs are two per-axis swings, not one device-north vector. The shift is the east aim subtracted after `atan2_deg(mx, my)`. |
| 3 | correctness | If +Y is north and +X is east, an east field on −X comes out W. | `hal/beeper/board.c:500`, `bringup/bringup.c:1202` | REJECTED-the letter is the corrected counts through `atan2_deg(mx, my)`, then −21°. A field of (−200, 0) is not the east aim that set that shift. |
| 4 | comments, correctness, latent, architecture, scope | The notes say the letter subtracts 275 from X and 45 from Y, and that those pulls stay on the sensor they were measured on. Both images add 275 and 45 on every boot. | `docs/hardware.md:131`, `docs/prototype-assembly.md:392` | FIXED |
| 5 | comments, correctness | Heading 0, and the letter N, are described as raw +Y north and +X east. Published 0 is that angle after −21°. `facing`'s comment swaps its parameters. | `docs/hardware.md:137`, `docs/prototype-assembly.md:392`, `bringup/bringup.c:1128`, `hal/beeper/board.c:502` | FIXED |
| 6 | comments | `trimmed_mv` is commented as one sample a second and the middle 80% of a half-minute. The function only drops `n/10` from each end. | `hal/beeper/board.c:909` | FIXED |
| 7 | comments, correctness | The notes say the percent only falls on the pack and only rises on USB. The first 30 samples track both ways, and the rise also requires the average at or above 3000 mV. | `docs/hal.md:81`, `docs/hardware.md:109`, `hal/beeper/board.c:959` | FIXED |
| 8 | correctness | Until 30 samples are stored, the percent should already be one-way. | `hal/beeper/board.c:959` | REJECTED-the first window is copied through so a boot reading does not latch. Row 7 says the one-way rule starts once the ring is full. |
| 9 | correctness, latent, architecture | The half-minute ring advances only while the home or Peeple menu draws the battery line. | `hal/beeper/board.c:946`, `firmware/src/screen.c:802` | FIXED |
| 10 | correctness | A window at 4200 mV sticks at 100% and clears the time-to-full for the rest of that USB connection. | `hal/beeper/board.c:971` | REJECTED-while VBUS is set and the average is at least 3000 mV the percent only rises. `charging` also requires the shown percent below 100, so a full reading clears the minutes until VBUS drops. |
| 11 | correctness, latent | A trimmed average under 3000 mV clears the USB flag and snaps the percent to 0 while VBUS is still set. | `hal/beeper/board.c:963` | REJECTED-below 3000 mV the reading is not a pack, so the percent becomes 0. The trim already drops the lowest tenth. |
| 12 | ripoff | The one-way latch is a second filter on the trimmed mean and should go. | `hal/beeper/board.c:959` | REJECTED-the one-way percent is the gauge. The trim stays in front of it. Row 7 states when it starts. |
| 13 | scope | `if (drop * 2 >= n) drop = 0` never fires for a ring of 1 to 30. | `hal/beeper/board.c:924` | FIXED |
| 14 | scope | `n >= 30` is tested again after the latch has already armed, and the two tests are the same. | `hal/beeper/board.c:959` | FIXED |
| 15 | scope | Dropping `bringup/bringup.c` changes nothing the pager runs. | `bringup/bringup.c:1202` | REJECTED-that call is the glass letter in the wire-test image. The pager arrow is the other copy in row 1. |
| 16 | scope | Dropping the three docs changes no device behavior. A reader would think the HAL and the wiring moved. | `docs/hal.md:81`, `docs/hardware.md:109` | REJECTED-`docs/architecture.md` lists `docs/` as the notes. The false sentences are rows 4, 5, and 7. |
