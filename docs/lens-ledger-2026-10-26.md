# Lens ledger 2026-10-26

Rows 1, 2, 3, and 6 were taken up after this review. `ASKED` is still waiting on you. `DEFERRED` is a real cleanup this review did not apply; the owner is you, on the next edit pass. `REJECTED` includes the citation.

## Range and profile

Branch `main` matches `origin/main`, and there is no open PR, so the default upstream range is empty. The review range is the rewrite that is the current tree:

`git diff 6808a5e..HEAD` (merge-base is `6808a5e`; the base tip is not ahead). One commit: `a41a1c5` Replace the browser host with C firmware and a desktop emulator.

Outside the range, and ignored: deleted `docs/BEEPLE PNG/camnon.png`, untracked `docs/BEEPLE PNG/KillCorder.png`.

| Fact | Value |
|---|---|
| Stack | C11 (`clang -std=c11 -Wall -Wextra -Werror`), SDL2 desktop host, Python art tools. The previous tree was a TypeScript/Vite host. |
| Checks | `make test` (page, store, mesh, locate). `make nrf-check` compiles `firmware/src` except `hal_ram.c`, plus `hal/nrf/nrf_hal.c`. `make emu` needs SDL2. No lint config and no CI workflow remain. |
| Ships | `firmware/src`, `firmware/include`, `hal/nrf`, `emu/main.c`, `emu/font.c`, `emu/seed.c`. `emu/third_party` is vendored. |
| Tests | `firmware/tests`. `tools/` is the offline art pipeline. |
| Doctrine | `docs/architecture.md`, plus `docs/firmware-app.md`, `docs/hal.md`, `docs/hardware.md`, `docs/radio-protocol.md`. No `CLAUDE.md`, no `CONTEXT.md`, no `## Review lenses` block. Lenses used the stack-neutral briefs. |

Lenses: [ripoff-bandaids](f0ebfe04-e446-4885-8434-6ee547271d89), [scope](a754978d-3121-4e86-9ac9-7278f4261704), [correctness](47fb9abc-2e25-440f-8ded-d6f5d30d07df), [latent-design](cc68b67f-8701-4e94-a21b-0ce9bb4287a1), [architecture](f9a6277a-785a-49ec-bb3f-70d696d41999), [comments](abf485bb-2a2d-42e1-8a0e-c4c863c70855).

I re-read the three tracking/pairing paths, the GPS unit comment, the crypto include comment, the text-edit walk, `air.c`'s includes, and `Makefile`'s `nrf-check` set. I did not re-run `make test`. The correctness lens reported that `make test` and `make nrf-check` pass, and that the tests do not cover the three bugs below.

## ASKED — behavior

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 1 | correctness | Allow sends the stored fix and a 30-minute grant to whoever requested last. `answer_req` overwrites `ask_id` while the prompt is already up. `pager_poll_track` leaves the screen alone when it is already `SCREEN_TRACKASK`, and `pager_render` runs after `pager_input`, so the press can answer a contact the previous frame did not show. | `firmware/src/locate.c` `hold_ask`, `firmware/src/pager.c` `pager_poll_track` | FIXED |
| 2 | correctness | The tick that opens the tracking prompt sets Deny and returns before `pager_input`, and it does not drop the encoder queue. The next tick treats that queued press as Deny, or as Allow if a clockwise detent wrapped `sel`. | `firmware/src/pager.c` `pager_poll_track` | FIXED |
| 3 | correctness | A new HELLO from the same node id does not clear `they_have_us`. `pair_start` clears only the local session. The peer's next burst still sends `PAIR_LEAVE`, and the restarted Add takes the "leave before ack" path and aborts with "try again" until the peer's window ends. | `firmware/src/pair.c` `pair_on_hello` | FIXED |
| 4 | correctness | `send_loc` reads a live fix and `remember_fix`s it before sealing the reply. `locate_gps_tick` only powers the receiver on the listen-start tick. `docs/firmware-app.md` says a locate reply sends the stored fix, and that a listen with no sky keeps the previous fix. On the emulator the HAL already has a valid fix, so this tick replaces it. | `firmware/src/locate.c:151`, `firmware/src/locate.c:488`, `docs/firmware-app.md` (Locate: "A locate reply sends the stored fix.") | ASKED |
| 5 | architecture | `add_radio` loads NVM into the RAM HAL, `beeper_init`s an empty book, then `introduce_emulator_unit` mints a session key into both sides. The new unit's book is replaced on the first `store_load`. Peers keep the minted key. A new unit that already has a blob disagrees with them. Cold start of the seeded trio reloads everyone and stays symmetric. | `emu/main.c:186`, `emu/seed.c:74` | ASKED |

## ASKED — structure, more than one lens

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 6 | ripoff, latent-design, architecture | PAGE and locate each own a full seen-cache, 80–400 ms jitter, re-roll, and cancel-on-reply. The protocol states one relay rule. Keep two 24-slot caches so a page flood cannot evict a locate hop. Do not fold the two origin-retry schedules; the backoffs differ. | `firmware/src/relay.c`, `firmware/include/relay.h` | FIXED |
| 7 | ripoff, architecture | `locate_init` sets `track_open`. Empty NVM is corrected on the first `beeper_tick`, and `store_apply` also starts from ask. Any caller that answers a request off `locate_init` alone starts open. The header comment says the tests start open; the default is the production struct. | `firmware/src/locate.c:17`, `firmware/include/locate.h:70`, `firmware/src/beeper.c:33` | ASKED |
| 8 | latent-design, architecture | `page_tick` demuxes `LOC_REQ` / `LOC`, calls `locate_tick`, and each side reads the other's busy flag. `page.h` includes `locate.h`. The composition root already chooses pair versus page. | `firmware/src/page.c:255`, `firmware/include/page.h:7`, `firmware/src/beeper.c:40` | ASKED |
| 9 | comments, and `docs/hal.md` repeats it | `hal.h` says the stored fix is the RMC `ddmm.mmmm` field. `nmea.c` stores `degrees * 60 * 10000 + minutes`. Those match only while the degree digits are 0. The first sentence (signed arcminutes times 10,000) matches the code. `docs/hal.md` copies the false second sentence. | `firmware/include/hal.h:42`, `firmware/src/nmea.c:58`, `docs/hal.md` (GPS fix) | ASKED |
| 10 | comments | `crypto.c` says it uses a bare `8` to avoid a `frames.h` cycle. `frames.h` includes `crypto.h`. `crypto.h` does not include `frames.h`. `crypto.c` can include `frames.h`. | `firmware/src/crypto.c:43`, `firmware/include/frames.h:3` | ASKED |
| 11 | comments | The text-edit comment says a turn toward E walks SPACE, SAVE, DEL, OK. `flatten` emits the left column from row 3 down: OK, DEL, SAVE, SPACE, then E. | `firmware/src/textedit.c:72`, `firmware/src/textedit.c:81` | ASKED |
| 12 | architecture | `nrf-check` compiles every `firmware/src/*.c` except `hal_ram.c`, so the board set includes `air.c`, the desktop mesh. | `Makefile:28`, `firmware/src/air.c:1` | ASKED |

## REJECTED

| # | Lenses | Finding | Disposition |
|---|---|---|---|
| 13 | architecture | `air.c` includes `hal_ram.h`. | REJECTED-`firmware/src/air.c:1` includes `air.h` and `gps.h`. The compile-set half of this finding is row 12. |
| 14 | ripoff | `SCREEN_FAILED` is accidental complexity: it is the retry chrome while the radio is still sending, so a later `PAGE_FAILED` does not change the screen. | REJECTED-`docs/firmware-app.md` defines that screen: Sending shows SEARCHING, and about 10 seconds later the same chrome becomes CAN'T FIND / WILL TRY / AGAIN, while the outbox row becomes failed only after the attempts end. The name matches the spec. The pager.h comment that calls this "Retry" is still a comment defect; see the deferred comment list. |
| 15 | scope | Drop `docs/*.md`, the README, and `.gitignore`. Nothing in the emulator binary reads them. | REJECTED-they are the doctrine and the build notes. `docs/architecture.md` is what lens 5 cites. A file the linker ignores is not bloat. |
| 16 | scope | Drop `nmea.c`, `nrf_hal.c`, and their headers. The emulator posts `gps_from_feet`, and the only caller of `nrf_hal_init` is `locate_test`. | REJECTED-`docs/architecture.md` says `hal/nrf/nrf_hal.c` is the board stub, and `docs/hal.md` puts RMC parsing on `nrf_gps_push_byte`. There is no board `main` yet. Dead on the emulator is the expected shape of a compile-only stub. |
| 17 | scope | Drop `tools/beeple_to_art.py`, `tools/png_to_art.py`, and the Beeple PNGs. `make` does not run them. | REJECTED-`docs/firmware-app.md` says the pictures come from `docs/BEEPLE PNG` via that script. They are the art source. The generated bytes live in `art.c`. The doc does not say `make` packs the PNGs. |
| 18 | scope | Drop `CMakeLists.txt`. `peeple-emu.command` runs `make`. | REJECTED-the README documents `cmake --build` as the other way to build the emulator and the four tests. |
| 19 | scope | Restoring `src/` and the Vite host would leave the emulator link unchanged, so the deletion changes nothing a user can observe. | REJECTED-`docs/architecture.md` says there is no second pager in TypeScript. Putting that tree back would violate the doctrine even though the link line would not change. |
| 20 | scope | `docs/Layouts` and the PSD are unused by the binary. | REJECTED-same as the docs. They are the screen references, not a second implementation. |
| 21 | latent-design | Folding the two origin-retry loops into one ARQ. | REJECTED-the lens dropped it. Page waits 10 s then 60 s; locate waits 2 s, four times. One callback object would braid two schedules. |
| 22 | latent-design | A screen function table, deleting `has_outbox`, merging the blit helpers with a mode flag, a pose enum for hold-flat and hold-still. | REJECTED-the lens dropped these. Each either leaves the transition bodies in place or hides a state the code allows (both hold flags true). |

## DEFERRED — dead surface and small copies

Owner: you, next edit pass. Tests stay. Vendored `stb_truetype.h` and Roboto stay.

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 23 | architecture | Nothing owns "the radio is free." Origin sends check both busy flags. The relay drains do not. | `firmware/src/page.c:370`, `firmware/src/locate.c:433` | DEFERRED-one lens, not re-executed. A single reservation is a behavior change if a relay currently overlaps an origin send on purpose. |
| 24 | architecture | Decrypted `page_t` sits in `hal.h`. No `hal_t` op takes it. | `firmware/include/hal.h:25` | DEFERRED-header placement. Both HALs and the pager include `hal.h` for it. |
| 25 | architecture | The bench Sample control calls `locate_gps_soon` instead of presenting a HAL edge. | `emu/main.c:501` | DEFERRED-host reaches past the HAL. The doctrine says they meet only at the HAL. |
| 26 | architecture | Degree-to-octant `(d + 22) / 45` and the compass names are encoded in the host and again in heading and the pager. | `emu/main.c:332`, `firmware/src/heading.c:8`, `firmware/src/pager.c:1083` | DEFERRED-real duplication. Axis 6. |
| 27 | architecture, latent-design (routed) | The host FNV finalizer differs from `store_hash_blob` (the store xors the length; the host maps 0 to 1). The host hashes every frame to skip a disk write because `store_save` does not record the hash. | `emu/main.c:80`, `firmware/src/store.c:142`, `emu/main.c:229` | DEFERRED-ripoff's direct fix is to write the file from `nvm_save`. Not a wrong fix on the air. |
| 28 | architecture | `ensure_pair` copies a session key onto the book that lacks it. | `emu/seed.c:39` | DEFERRED-host directory reconciler. The lens routed it to ripoff. I did not find a production caller. |
| 29 | ripoff, scope, latent-design | No caller for `gps_compass`, `page_rx_tick`, `page_outbox_to`, `page_outbox_body`, `str_clip_ascii`, `contacts_list`, `display_get_pixel`. `page_radio_t.dest_id` exists for the unused getter. | `firmware/src/gps.c:56`, `firmware/src/page.c:35`, `firmware/src/page.c:413`, `firmware/src/util.c:22`, `firmware/src/contacts.c:71`, `firmware/src/display.c:281` | DEFERRED-grepped by scope across production roots. Safe delete after one more grep on the edit pass. |
| 30 | scope | `art_left`, `art_right`, `art_beep` and their header macros have no caller. `pager.c` never names them. | `firmware/src/art.c:3` | DEFERRED-layouts may still be waiting on these sprites. Do not delete on a leanness pass without checking `docs/Layouts`. |
| 31 | ripoff, latent-design | HAL `display_clear` and `display_set_pixel` have no firmware caller. Pixels go through `display_buffer`. | `firmware/include/hal.h:60` | DEFERRED-removing them changes the port contract. `docs/hal.md` describes the buffer index, not a required pen. |
| 32 | ripoff | `page_ack_mac` and `loc_req_mac` differ by the three-byte tag. | `firmware/src/crypto.c:196` | DEFERRED-latent-design dropped this. A tag argument removes a body and no structure. Fine as a small edit, not as a redesign. |
| 33 | latent-design | `frame_encode_loc` re-rolls the PAGE body layout. The ack helpers already take a type byte. | `firmware/src/frames.c:188` | DEFERRED-wire bytes must stay put. Worth doing only with a frame test that locks the 11-byte body. |
| 34 | latent-design | Saved and remote fixes are loose lat/lon/clock scalars beside `gps_fix_t`. | `firmware/include/locate.h:54`, `firmware/src/locate.c:145` | DEFERRED-store version 8 layout has to stay byte-compatible. |
| 35 | scope | Ten functions are called only inside their own file and are still in the header (`sha256`, store codecs, `pager_set_menu_art`, `pager_go_menu`, three display helpers). | `firmware/include/crypto.h:12`, `firmware/include/store.h:10`, `firmware/include/pager.h:121` | DEFERRED-make them `static`. No behavior change. |
| 36 | scope | `air_distance_ft` is a 1-D span. The mesh uses `gps_hypot_ft`. `mesh_test` calls the 1-D helper. | `firmware/src/air.c:9` | DEFERRED-keep the test. Point it at `gps_hypot_ft` only if the 1-D helper is actually unused by design. |
| 37 | ripoff, scope | `beeper_reset` is only called from `mesh_test` immediately after `beeper_init`. | `firmware/src/beeper.c:64`, `firmware/tests/mesh_test.c:90` | DEFERRED-a named reset on the composition root. The test can stop calling it. Do not delete the function on that evidence. |
| 38 | ripoff | Store decode still accepts versions 1–7. No test builds those blobs. `docs/firmware-app.md` still says version 2. `STORE_VERSION` is 8. | `firmware/src/store.c:14`, `firmware/src/store.c:238` | DEFERRED-do not drop the gates until you know no `emu-nvm-*.bin` needs them. Do fix the doc's version number when you next touch it. |
| 39 | architecture | `store_load`, `store_save`, and `store_sync` share function-static 100 KB scratch. | `firmware/src/store.c:367` | DEFERRED-safe while `tick_all` does not nest. The one module-global buffer under `firmware/src`. |
| 40 | architecture | `finish_outbox` stamps the first `OUTBOX_SENDING` row. | `firmware/src/pager.c:176` | DEFERRED-holds only while one send is in flight. Below the bar until a second in-flight page exists. |
| 41 | architecture | The ST7567 index is re-encoded beside `display_buf_index` in both HALs, the emulator blit, and the sprite blit. | `firmware/src/display.c:269` | DEFERRED-stable hardware layout. Four copies, one function already exists. |
| 42 | ripoff | `str_ascii_len` is `strlen`. | `firmware/src/util.c:3` | DEFERRED-tiny. |
| 43 | scope | `bytes_eq` reimplements `memcmp` with a zero-length guard. | `firmware/include/util.h:8` | DEFERRED-the null guard is the reason it is not `memcmp`. Leave it unless a call site is wrong. |

## DEFERRED — comments

Owner: you, next edit pass. Lens 6 reported no file where comments outnumber code. `display.c` has 191 comment lines against 515 code lines, and 190 of those are glyph tags. Tests in this range have no comments. No lint rule requires a comment. I re-traced rows 9–11 above. The rest of this list is lens 6's, cited, not re-opened line by line. Do not apply it blind: a KEEP that the lens marked borderline stays.

FALSE and OVERCLAIMING, not already in the ASKED table:

| # | Kind | Finding | Where | Disposition |
|---|---|---|---|---|
| 44 | FALSE | "One status line" does not match the panel. Chrome is two lines, and the draw writes a hears line and a fix line. | `emu/main.c:29`, `emu/main.c:38` | DEFERRED-comment-pass |
| 45 | FALSE | "A filled picture is inverted so the character stays ink on paper." Every `BEEPLES[]` row sets `invert` to false. | `firmware/src/pager.c:22` | DEFERRED-comment-pass |
| 46 | OVERCLAIM | Level and still are not the whole heading gate. The motor, the 400 ms settle, and a zero offset also block the arrow. | `firmware/include/hal.h:50`, `firmware/src/heading.c:38` | DEFERRED-comment-pass |
| 47 | OVERCLAIM | "Outside the 40° budget" is not a check. The code compares tilt with 20. | `firmware/include/heading.h:8`, `firmware/src/heading.c:38` | DEFERRED-comment-pass |
| 48 | OVERCLAIM | The arrow does not appear when the settle ends. A zero offset leaves `arrow` false. | `firmware/include/heading.h:22`, `firmware/src/heading.c:55` | DEFERRED-comment-pass |
| 49 | OVERCLAIM | `gps.h` calls arcminutes times 10,000 "the module's" unit. The module emits `ddmm.mmmm`. | `firmware/include/gps.h:9`, `firmware/src/gps.c:5` | DEFERRED-comment-pass |
| 50 | OVERCLAIM | `parse_coord` does not return a signed value. The hemisphere is applied later. | `firmware/src/nmea.c:32`, `firmware/src/nmea.c:106` | DEFERRED-comment-pass |
| 51 | OVERCLAIM | Not every RMC line returns true. A bad checksum returns false. | `firmware/include/nmea.h:7`, `firmware/src/nmea.c:90` | DEFERRED-comment-pass |
| 52 | OVERCLAIM | "Retry replaces Sending." The timer switches to the WILL TRY AGAIN frame while the send can still be in flight. The outbox word Retry is a different row. | `firmware/include/pager.h:107`, `firmware/src/pager.c:1567` | DEFERRED-comment-pass. The screen itself is the spec; see row 14. |
| 53 | OVERCLAIM | "Looking, then Found" reads as a locate result. The flip is the 2 s splash that still says ASKING NICELY while status is `LOCATE_SENDING`. The splash is what `docs/firmware-app.md` specifies. The comment overclaims the result. | `firmware/include/pager.h:113`, `firmware/src/pager.c:1120` | DEFERRED-comment-pass |
| 54 | OVERCLAIM | Saves are not "never the working directory." Pref-path failure uses `"./"`, and a missing pref file falls back to `emu-nvm-<id>.bin` in the cwd. | `emu/main.c:2`, `emu/main.c:100` | DEFERRED-comment-pass |
| 55 | OVERCLAIM | A finished RMC line updates the fix only when `nmea_parse_rmc` returns true. A bad checksum does not. | `hal/nrf/nrf_hal.h:9`, `hal/nrf/nrf_hal.c:141` | DEFERRED-comment-pass |

DELETE (lens 6). These restate the next line, name a caller, or tag a glyph with its own code point:

`firmware/include/air.h:28`, `firmware/src/air.c:16`, `firmware/src/beeper.c:42`, `firmware/include/crypto.h:23`, `firmware/include/display.h:31`, `firmware/include/contacts.h:19`, `firmware/include/gps.h:17`, `firmware/include/locate.h:102`, `firmware/include/locate.h:104`, `firmware/include/locate.h:106`, `firmware/include/locate.h:109`, `firmware/include/locate.h:131`, `firmware/include/pager.h:77`, `firmware/include/pager.h:100`, `firmware/include/pager.h:121`, `firmware/include/pager.h:135`, `firmware/src/pager.c:48`, `firmware/src/pager.c:765`, `firmware/src/pager.c:778`, `firmware/src/pager.c:790`, `firmware/src/pager.c:870`, `firmware/src/pager.c:1260`, and the 190 glyph tags in `firmware/src/display.c:6` and `firmware/src/display.c:105`.

COMPRESS to one line (lens 6): `emu/font.h:8`, `firmware/include/air.h:22`, `firmware/include/art.h:5`, `firmware/include/beeple.h:3`, `firmware/include/contacts.h:32`, `firmware/src/display.c:103`, `firmware/include/hal.h:24` (keep the terminator clause), `firmware/include/hal.h:30`, `firmware/include/heading.h:27`, `firmware/include/locate.h:70`, `firmware/include/locate.h:94`, `firmware/include/page.h:58`, `firmware/include/pager.h:94`, `firmware/include/pager.h:126`.

Disposition for every row in those two lists: DEFERRED-comment-pass.

KEEP, and the lens could name the fact: `emu/font.h:13` (y is the top of the line, not the stbtt baseline), `firmware/include/display.h:11` and `:29` (the small-font advance includes a gap the width helper then drops), `firmware/include/frames.h:34` (sealed plaintext is `msg_id || beeple || text`), `firmware/src/gps.c:5` (the foot scale), `firmware/include/locate.h:17` and `:19` (one warning per requester; a live grant skips the prompt), `firmware/include/locate.h:78` (the grant is not written by the store). No disposition change. They stay.
