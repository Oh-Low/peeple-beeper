# Temporary punch list

From the source-overview critique. Temporary. Check an item off when it is done.

- [x] Split `firmware/src` so product code, host stand-ins, and generated picture bytes are not one flat directory. Product stays in `firmware/src`. Host stand-ins are `firmware/host`. Picture bytes are `firmware/art`.
- [x] HAL implementations live under `hal/`, one directory per machine: `hal/emu` (tests and the desktop emulator) and `hal/beeper` (the board). The shared contract is `hal/hal.h`.
- [ ] Name the two HAL files the same way. They are still `hal/emu/hal_ram.c` and `hal/beeper/nrf_hal.c`.
- [x] Move `art.c` out of the folder that holds the tick and the radio. It is a generated bitmap table. It now lives in `firmware/art/art.c`.
- [ ] Rename `air.c`. The name reads as the packet path. The file is the desktop foot-grid and RSSI falloff.
- [ ] Rename `page.c` and `pager.c` so the screen and the radio switch are not one word apart.
- [ ] Treat `page.c`, `pair.c`, and `locate.c` as peer radio sessions. `page.c` is the receive switch: it pauses for locate, forwards other nodes, hands locate frames off, and ignores pairing.
- [ ] Keep one reliable-send loop. `page.c` and `locate.c` each keep a destination, a nonce list, an attempt count, and a next-send time, and each accepts a reply only from that peer, on a stored nonce, while a send is in flight. Leave the two sessions as separate files. The attempt counts and side effects differ.
- [ ] Keep one forward policy. `relay.c` is the cache. `page.c` and `locate.c` each decide which frames enter it. `page_test.c` and `locate_test.c` each re-prove the jitter, the hop drop, and the already-sent suppression.
- [ ] Give the 22.5° eight-way bucket one owner. `gps.c` and `heading.c` both snap a north/east offset to the same eight directions. The hold-flat, hold-still, and 400 ms wait stay in `heading.c`.
- [ ] Give the blank-name rule one owner. `identity.c` and `contacts.c` both fall back to the last four hex digits of the id.
- [ ] Move the GPS duty cycle out of `locate.c`. Asking, Allow/Deny, and the warning stay. Powering the receiver for a minute every half hour sits beside `nmea.c`, `gps.c`, and `heading.c`.
- [ ] One NMEA ingest path. `nrf_hal.c` parses RMC off a UART byte sink. `hal_ram.c` only stores a fix the caller installed.
- [ ] One home for a mascot. The index is `beeple.h`, the pixels are `art.c`, and `beeple_to_art.py --write` also replaces the mascot table in `pager.c` and sets the count. `png_to_art.py` stays the generic packer.
- [ ] Name the tests for what they pin down. `page_test.c` is also the relay suite. `locate_test.c` covers the foot grid, RMC, the arrow, receiver sleep, and tracking consent. `mesh_test.c` covers RSSI, who hears whom, contact seeding, and restarting a pair.

---

create a temporary punch list for the problems suggested here and save to docs. paste the entirety of this message below the punch list
