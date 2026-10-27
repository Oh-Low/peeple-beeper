# Lens ledger — uncommitted code

Range: working tree against `482df31`, code only. Docs and `README.md` were not in the diff. Untracked `firmware/src/attempt.c` and `firmware/include/attempt.h` were. No commits in range. Checks the lenses ran: `make test`, `make nrf-check`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | ripoff, latent, architecture | The attempt loop was two switches. Budget and wait were passed back in on every step. | `firmware/src/attempt.c` | FIXED — `MESSAGE_SENDING` stays on the session; architecture says the outcome stays there. `peer_sending` is row 8. |
| 2 | ripoff, scope, architecture | The page destination is stored twice. `dest_id` / `has_dest` are read only by `message_outbox_to`, which has no caller. `message_outbox_body` has no caller. | `firmware/include/message.h:41`, `firmware/src/message.c:28` | FIXED |
| 3 | ripoff | `mesh_distance_ft` is a one-axis span the mesh does not use. | `firmware/host/mesh.c:9`, `firmware/tests/mesh_test.c:53` | REJECTED-`mesh_test` pins that helper; the link path uses `gps_hypot_ft` on purpose |
| 4 | scope | `message_relay_tick` only calls `relay_tick`. One caller. | `firmware/src/message.c:232`, `firmware/src/beeper.c:36` | FIXED |
| 5 | scope | `attempt_reset` and `attempt_has` are not part of the session API. Null checks in `attempt_sent` and `attempt_stop` never see a null. | `firmware/src/attempt.c:7`, `firmware/include/attempt.h:25` | FIXED |
| 6 | scope | Rename-only files (HAL, mesh, screen, store, tests, Makefile, CMake, beeple script paths) change no shipped behavior. | the rename hunks | REJECTED-each rename is what the callers compile against; dropping one side does not build |
| 7 | architecture | Page and locate cadence numbers lived in the sessions and the screen. The screen splash used its own timestamp. | `firmware/include/attempt.h`, `firmware/src/screen.c` | FIXED |
| 8 | architecture | “Do not transmit together” was a caller-supplied `peer_sending` bool plus a one-way tick gate. | `firmware/src/beeper.c` | FIXED |
| 9 | latent | The tracking prompt kept its head in `ask_id` / `ask_nonce` beside `ask_wait[]`. | `firmware/include/locate.h`, `firmware/src/locate.c` | FIXED |
| 10 | correctness | No defect. The attempt loop matches the old send and ack rules and runs from `beeper_tick` when pairing is idle. | `firmware/src/beeper.c:61` | REJECTED-traced; `make test` passed |
| 11 | comments | Beeple table comment says filled pictures are inverted. Every row passes `false`. | `firmware/src/screen.c:22` | FIXED |
| 12 | comments | `mesh.h` says `push_raw`. The function returns false when out of range. The push is `emu_hal_push_raw`. | `firmware/include/mesh.h:28` | FIXED |
| 13 | comments | `emu/main.c` header still says RAM HAL and fake air. | `emu/main.c:1` | FIXED |
| 14 | comments | Beeple script says the first five mascots keep their order. `KNOWN` is the whole catalog. | `tools/beeple_to_art.py:21` | FIXED |
| 15 | comments | Screen comments overclaim “main menu only”, “Retry replaces Sending”, and “Looking, then Found”. | `firmware/include/screen.h:95`, `:108`, `:114` | FIXED |
| 16 | comments | Delete comments that restate the next lines: settings list, slant-bar title, slant header, `small_chars_fit`. | `firmware/src/screen.c:48`, `:778`, `:790`, `:870` | FIXED |
| 17 | comments | Compress the remaining carried comments to the one fact the names do not say. Keep the five one-line constraints (NULL north, track-prompt transition, slant corners, octant 0, send-status column). | `firmware/include/screen.h`, `firmware/include/message.h:44`, `firmware/host/mesh.c:16`, `hal/beeper/beeper_hal.h:9` | FIXED |
