# Temporary punch list

From the source-overview critique. Temporary. Check an item off when it is done.

- [x] Split `firmware/src` so product code, host stand-ins, and generated picture bytes are not one flat directory. Product stays in `firmware/src`. Host stand-ins are `firmware/host`. Picture bytes are `firmware/art`.
- [x] HAL implementations live under `hal/`, one directory per machine: `hal/emu` (tests and the desktop emulator) and `hal/beeper` (the board). The shared contract is `hal/hal.h`.
- [x] HAL files are named for the machine: `hal/emu/emu_hal.c` and `hal/beeper/beeper_hal.c`.
- [x] Move `art.c` out of the folder that holds the tick and the radio. It is a generated bitmap table. It now lives in `firmware/art/art.c`.
- [x] The desktop foot-grid and RSSI falloff is `firmware/host/mesh.c`.
- [x] The screen is `screen.c`. The message session is `message.c`.
- [x] `message.c`, `pair.c`, and `locate.c` are peer radio sessions. `beeper.c` reads the radio and hands each frame to the session that owns it.
- [x] One reliable-send loop lives in `attempt.c`: nonce list, attempt count, next-send time, and accepting a reply only from that peer on a stored nonce while the send is in flight. `message.c` and `locate.c` stay separate files. A page is nine tries, ten seconds then a minute. A locate is four tries, two seconds apart. Text, mascot, fix, denial, and no-sky stay in the session.
- [ ] Keep one forward policy. `relay.c` is the cache. `message.c` and `locate.c` each decide which frames enter it. `page_test.c` and `locate_test.c` each re-prove the jitter, the hop drop, and the already-sent suppression.
- [ ] Give the 22.5° eight-way bucket one owner. `gps.c` and `heading.c` both snap a north/east offset to the same eight directions. The hold-flat, hold-still, and 400 ms wait stay in `heading.c`.
- [ ] Give the blank-name rule one owner. `identity.c` and `contacts.c` both fall back to the last four hex digits of the id.
- [ ] Move the GPS duty cycle out of `locate.c`. Asking, Allow/Deny, and the warning stay. Powering the receiver for a minute every half hour sits beside `nmea.c`, `gps.c`, and `heading.c`.
- [ ] One NMEA ingest path. `beeper_hal.c` parses RMC off a UART byte sink. `emu_hal.c` only stores a fix the caller installed.
- [ ] One home for a mascot. The index is `beeple.h`, the pixels are `art.c`, and `beeple_to_art.py --write` also replaces the mascot table in `screen.c` and sets the count. `png_to_art.py` stays the generic packer.
- [ ] Name the tests for what they pin down. `page_test.c` is also the relay suite. `locate_test.c` covers the foot grid, RMC, the arrow, receiver sleep, and tracking consent. `mesh_test.c` covers RSSI, who hears whom, contact seeding, and restarting a pair.

---

create a temporary punch list for the problems suggested here and save to docs. paste the entirety of this message below the punch list
