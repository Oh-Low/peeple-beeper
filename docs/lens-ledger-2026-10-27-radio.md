# Lens ledger, radio transmit, 2026-10-27

Uncommitted working tree against `b6f5f9d`. Six lenses. Dispositions are `FIXED`, `DEFERRED-<owner>`, `ASKED`, or `REJECTED-<citation>`.

| # | Lenses | Finding | Where | Disposition |
|---|--------|---------|-------|-------------|
| 1 | ripoff, latent, architecture, comments | `board_radio_send` waits 0–219 ms and parks one frame in `rx_hold`. A hello caught during a pair burst stays invisible until the next tick, after that burst may already have sent leave. Relay and the attempt clock already decide when a frame goes out. | `hal/beeper/board.c:671` | FIXED |
| 2 | ripoff, latent, architecture | The SX1262 command list is copied in the pager and the wire test, and the post-calibrate wait has already drifted (20 ms vs 100 ms). | `hal/beeper/sx1262_setup.c` | FIXED |
| 3 | ripoff | `radio_write` already waits for BUSY low. The waits after calibrate, after the IRQ mask, and after `SetPacketParams` cannot fail differently. | `hal/beeper/board.c:584`, `hal/beeper/board.c:636`, `hal/beeper/board.c:706`, `bringup/bringup.c:600` | FIXED |
| 4 | scope | Dropping `bringup/bringup.c` changes nothing in the pager image. | `bringup/bringup.c` | REJECTED-`Makefile` links the wire test as its own image. The pager not linking it is the split in feather ledger row 5. Dropping this diff removes the ping and the TCXO from that image. |
| 5 | scope | The doc edits are not compiled, so a reader would think those sentences are what puts frames on the air. | `docs/hal.md`, `docs/hardware.md`, `docs/prototype-assembly.md` | REJECTED-the wiring notes are the board contract. False sentences are rows 9 and 10, not a reason to drop the docs. |
| 6 | scope | `rx_seen` and `last_rssi` are written and never read. The glass uses the instant and peak readings. | `bringup/bringup.c:61` | FIXED |
| 7 | scope | Init writes `SetPacketParams` by hand. `radio_packet_len(0xFF)` is those same bytes. | `hal/beeper/board.c:618` | FIXED |
| 8 | correctness | Every `SetPacketParams` leaves SX1262 register `0x0736` unrestored, so boards might transmit and never demodulate. | `hal/beeper/board.c:618` | REJECTED-both boards already exchange pages with IQ byte `0x00`. The TxDone-without-RxDone failure was the unpowered TCXO and cleared when DIO3 was powered. The Semtech note sets that bit for inverted IQ; this modem uses standard IQ, and rewriting it would change a link that works. |
| 9 | correctness | The hardware note and the source overview still say the Feather send is empty. | `docs/hardware.md:165`, `docs/source-overview.md:71` | FIXED |
| 10 | correctness | The wire-test legend says `S2` means the last command was accepted. Status 2 means a packet is waiting. | `docs/prototype-assembly.md:383` | FIXED |
| 11 | architecture | `hal.md` says `radio_send_raw` truncates at `RAW_FRAME_MAX`. The Feather drops the buffer. The RAM HAL truncates. | `hal/beeper/beeper_hal.c:68`, `hal/emu/emu_hal.c:72` | FIXED |
| 12 | architecture | `attempt_step` stamps `next_at` from the `now` it sampled before send, so it never sees time spent inside the driver. | `firmware/src/attempt.c:37` | REJECTED-the host send returns immediately (`hal/emu/emu_hal.c:69`) and the intervals are 2 s, 10 s, and 60 s. After row 1 the leftover block is airtime. The attempt clock is not this diff's decision. |
| 13 | comments | "SF7 keeps a hello inside the 400 ms pair gap." The 400 ms starts when the hello send returns. | `hal/beeper/board.c:35` | FIXED |
| 14 | comments | "so two boards started together are not both talking." The send still goes out. | `hal/beeper/board.c:671` | FIXED |
| 15 | comments | The D12 comments list the send and ping paths. The pulse is the next lines. | `hal/beeper/board.c:546`, `bringup/bringup.c:511` | FIXED |
| 16 | ripoff | `radio_prove` writes the sync byte `0x14` a second time when the readback misses. | `bringup/bringup.c:813` | REJECTED-that readback is what shows the restore landed. Leaving the sync word at `0xAB` makes this image deaf to a pager on `0x1424`. |
