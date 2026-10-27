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
- [x] Keep one forward policy. `relay.c` is the cache. `message.c` and `locate.c` each decide which frames enter it. `relay_test.c` proves the jitter, the hop drop, and the already-sent suppression.
- [x] Give the 22.5° eight-way bucket one owner. `gps_octant` snaps a north/east offset. The hold-flat, hold-still, and 400 ms wait stay in `heading.c`.
- [x] Give the blank-name rule one owner. `identity_blank_name` is the last four hex digits of the id. A contact book calls it.
- [ ] Move the GPS duty cycle out of `locate.c`. Asking, Allow/Deny, and the warning stay. Powering the receiver for a minute every half hour sits beside `nmea.c`, `gps.c`, and `heading.c`.
- [x] One NMEA ingest path. `nmea_push_byte` assembles the UART line and parses RMC. `beeper_gps_push_byte` feeds it those bytes. `emu_hal.c` only stores a fix the caller installed.
- [x] One home for a mascot. The index is `beeple.h`, the pixels are `art.c`, and the menu table is `firmware/art/beeple.c`. `beeple_to_art.py --write` rewrites that table and sets the count. `png_to_art.py` stays the generic packer.
- [x] Name the tests for what they pin down. `relay_test.c` is the relay suite. `gps_test.c` is the foot grid and RMC. `heading_test.c` is the arrow. `receiver_test.c` is receiver sleep. `locate_test.c` is the position exchange and tracking consent. `mesh_test.c` is RSSI and who hears whom. `seed_test.c` is contact seeding. `pair_test.c` is restarting a pair.

---

create a temporary punch list for the problems suggested here and save to docs. paste the entirety of this message below the punch list
