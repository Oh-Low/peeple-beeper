# Architecture

Peeple Beeper firmware is C under `firmware/`. It talks only to a HAL (`hal/hal.h`). Implementations live under `hal/`, one directory per machine: `hal/emu` for tests and the desktop emulator, `hal/beeper` for the beeper board. `firmware/host/` is the desktop foot-grid mesh, not a HAL.

There is no second pager in TypeScript. Host code is an SDL2 window, `hal/emu`, and the foot-grid mesh.

---

## Layer cake

The host never implements pairing or PAGE crypto. Firmware never knows about SDL, NVM files, or where the radios sit. They meet only at the HAL.

```mermaid
flowchart TB
  subgraph host ["Host — emu/ + firmware/host"]
    UI["SDL2 bench / tests"]
    HALImpl["hal/emu/emu_hal.c"]
    Air["firmware/host/mesh.c"]
    Seed["emu/seed.c contact directory"]
  end
  subgraph fw ["C firmware — firmware/src"]
    Beeper["beeper.c composition root"]
    Apps["screen · pair · message · store · display"]
  end
  Frames["App-layer frames on the air"]
  UI --> HALImpl
  UI --> Seed
  HALImpl --> Beeper
  Beeper --> Apps
  Apps -->|"radio_send_raw / radio_poll_raw"| HALImpl
  HALImpl --> Air
  Air -->|"push_raw to in-range peers"| HALImpl
  Apps --> Frames
  Frames -.-> Air
```

Tick order, frame types, and crypto are in [firmware-app.md](firmware-app.md) and [radio-protocol.md](radio-protocol.md).

---

## Directory map

| Path | Role |
|------|------|
| `hal/hal.h` | HAL contract (`hal_t` ops + `ctx`). Shared. Not an implementation. |
| `hal/emu/` | RAM HAL for tests and the desktop emulator. |
| `hal/beeper/` | Feather board HAL. `make beeper` links `board.c`. `sx1262_setup.c` is the modem command list, also linked into `make bringup`. `make nrf-check` compiles the table without either. |
| `firmware/include/beeper.h` | `beeper_init` / `tick` / `reset` / `debug_key`. |
| `firmware/src/*.c` | Screen, message session, pair, locate, store, display, crypto, frames. |
| `firmware/host/` | Desktop foot-grid and RSSI (`mesh.c`) only. Tests and the emulator. |
| `firmware/art/` | Generated 1-bit picture bytes (`art.c`). |
| `firmware/tests/` | `page_test`, `relay_test`, `store_test`, `screen_test`, `mesh_test`, `seed_test`, `pair_test`, `gps_test`, `heading_test`, `receiver_test`, `locate_test`. |
| `emu/` | SDL2 window + `seed.c` host directory. |
| `Makefile` | `make test`, `make emu`, `make nrf-check`, `make bringup`, `make beeper`. |
| `docs/` | This map and protocol/app/HAL notes. |
| `tools/png_to_art.py` | Any PNG to a 1-bit C array. |
| `tools/beeple_to_art.py` | Mascot PNGs into `firmware/art`. |

A new `.c` file goes in the directory of its kind. Product logic stays in `firmware/src`. The desktop mesh stays in `firmware/host`. Generated picture bytes stay in `firmware/art`.

Every HAL file is named for its machine: `hal/emu/emu_hal.c` and `hal/beeper/beeper_hal.c`. A new machine gets `hal/<machine>/<machine>_hal.c` and implements `hal/hal.h`. It does not go in `firmware/src` or `firmware/host`.

`firmware/host/mesh.c` is the desktop range model: feet and RSSI. The packet path is `frames.c` (byte layout), `crypto.c` (SHA-256 constructions), and `relay.c` (the forward cache). Frame layout, seals, and hop policy do not go in `mesh.c`.

---

## One decision, one file

A change lands in the file that already owns that decision. When two files would both have to change, the decision has no owner: put it in one place and have the other call it. Two files stay apart when they change for different reasons.

`screen.c` is the screen. `message.c` is the message session. A new file does not take a name one letter away from either. `message.c`, `pair.c`, and `locate.c` are peer sessions on one radio. `beeper.c` is the only place that chooses which session runs on a tick, and the only place that keeps a page and a locate off the air on that tick. A locate attempt or a locate reply goes first. A new frame kind is dispatched there. `message.c` handles page and page-ack. It does not drain another session's frames, and it does not tick another session.

| Decision | Owner | What stays outside |
|----------|--------|-------------------|
| SX1262 modem command list: 915 MHz, SF7, 125 kHz, 4/5, 64-symbol preamble, TCXO on DIO3 | `hal/beeper/sx1262_setup.c` | Pins, SPI, and the wire-test glass stay in their own images. A send passes its own payload length. The pager's 28 ms / 32 ms sniff lives in `board.c`, which passes timeout mask `0x00`. The wire test passes `0x02`. |
| Wire layout of every frame | `frames.c` | Sessions call encode and decode. |
| SHA-256 session key, seal, ack tag, locate tag | `crypto.c` | Sessions pass bytes in. |
| Attempt loop for a sealed send: nonce list, attempt count, next-send time, accept a reply only from that peer on a stored nonce while the send is in flight | `attempt.c`. A page is nine tries, ten seconds then a minute. A locate is four tries, two seconds apart. The screen asks the session whether the first wait has elapsed. | Text, mascot, fix, denial, and no-sky stay in the session that owns them. `message.c` and `locate.c` stay two files. |
| Forward cache: remember a frame, wait 80–400 ms, decrement the hop, send once, cancel a request when its reply is already stored | `relay.c`. `message.c` and `locate.c` each decide which frames enter a cache. `relay_test` is the proof of that cache. | Sessions hand it a frame. A new entry rule is written once, in `relay.c`. |
| Eight-way cut of a north/east offset, boundary at the tangent of 22.5° | `gps_octant` in `gps.c`. | Hold flat, hold still, the 400 ms wait, and the arrow in the device frame stay in `heading.c`. |
| Empty display name shown as the last four hex digits of the id | `identity_blank_name` in `identity.c`. | A contact book calls it. |
| Receiver schedule and the stored fix: listen up to a minute about every half hour, keep one fix. EN stays high until the first fix. | Its own unit, called from `beeper_tick`. | The locate nonce list and the Allow/Deny queue stay on the locate session. Locate reads the stored fix when it answers. |
| RMC sentence | `nmea.c`, the only parser | The board HAL holds the partial line and pushes UART bytes into it. The RAM HAL stores a fix the bench already built. |
| Foot grid, arcminutes, distance | `gps.c` | |
| Mascot index, menu label, and bitmap | `beeple.h` is the index. `firmware/art/beeple.c` is the name and pixel catalog. The screen asks `beeple_at`. | `png_to_art.py` packs any PNG. `beeple_to_art.py` writes the bytes and the catalog. Arrows and side decorations are sprites in `art.c`. They are separate from mascot indexes. |

`display.c`, `textedit.c`, `canned.c`, `store.c`, and `emu/font.c` are already one concept each. The bench font is the SDL atlas for the window chrome. The panel font is the 5×7 and 3×6 bitmaps in `display.c`. Leave them apart. Folding `frames.c` or `crypto.c` into a session would put the wire format in whichever session was nearest.

---

## One beeper, many radios

`beeper_init()` is the composition root of **one** unit. RAM lives on that object, not in module globals.

```text
beeper_t
  identity     node_id + display name
  contacts     session keys for peers
  pager        screens, inbox, outbox, canned, editor
  pair         whisper pairing session
  radio        PAGE TX/RX/relay
  locate       position exchange
  nvm_ready / nvm_hash
```

The saved fix and the listen schedule still sit on `locate` today. New schedule fields go with the receiver unit in the table above, not onto the nonce list.

The emulator wraps each unit with an `emu_hal_t`, a name (`A`, `B`, …), and an east/north foot position. The bench starts with three radios at east 0 / 75 / 150, north 0. **+ add** / **− remove** in the top bar grow the bench up to 16; the window widens to keep every LCD on screen. The compass under each LCD moves that unit; the mesh uses the straight-line distance and relinks immediately. Each device is ticked independently (`beeper_tick`).

---

## Desktop mesh

Radios sit on the east/north foot grid. A hop exists only if the straight-line distance is under 100 ft (`RADIO_RANGE_FT`). RSSI is about −30 dBm at 0 ft and falls ~60 dB across that span; at 100 ft the link is silent (no `push_raw`). There is **no collision model**. Firmware paging does not gate on RSSI; pairing still requires ≥ −45 dBm.

The host also seeds contact books (`emu/seed.c`) so every unit can Compose to every other without walking HELLO. That seeding is host-only. It is not a frame kind.

---

## Persistence

| What | Where |
|------|--------|
| Device NVM | HAL `nvm_load` / `nvm_save`. Emulator files in the per-user app support directory (`emu-nvm-<id>.bin`). Tests: RAM blob. |
| Firmware RAM | Each `beeper_t` | Not persisted except via NVM |

`store.c` is the firmware codec (`PB01` v2). The HAL does not parse the blob.

---

## Run and test

```
make test
make emu
./build/peeple-emu
```

A test file is named for the decision it pins. A new proof of a decision goes in that file. The same proof does not get a second copy in a test named for the feature that happened to call it.

| Before changing | Run |
|-----------------|-----|
| Page session, ack, frames, page crypto | `page_test` |
| Forward cache: jitter, hop drop, already-sent | `relay_test` |
| Locate session, Allow/Deny, tracking warning | `locate_test` |
| Foot grid and RMC | `gps_test` |
| Eight-way arrow | `heading_test` |
| Receiver schedule | `receiver_test` |
| Desktop mesh: RSSI and who hears whom | `mesh_test` |
| Contact seeding | `seed_test` |
| Restarting a pair | `pair_test` |
| NVM blob round-trip, including the storage-budget guard | `store_test` |
| Backlight timer, and leaving a grabbed settings value | `screen_test` |

**HAL-only firmware.** Production files under `firmware/src` and `firmware/art` must not assume SDL or files. They call `hal->foo(hal->ctx, ...)`. They include `hal/hal.h` and no file under `hal/emu` or `hal/beeper`. The desktop mesh lives in `firmware/host`.

---

## Non-goals

- **No Autofix.** Session keys and contacts are not repaired on the air. The emulator’s preloaded directory is host seeding, not a protocol feature.
- **No shared module-global radio state.** Pairing, PAGE outbox, seen-cache, and inbox live on each `beeper_t`.
- **Art studio** (the old web pixel editor) is not in this tree. Sprite work can return later as a separate tool.

---

## Further reading

- [radio-protocol.md](radio-protocol.md) — HELLO through PAGE_ACK, crypto, relay.
- [hardware.md](hardware.md) — intended pager hardware.
- [hal.md](hal.md) — display buffer, encoder, raw radio, NVM.
- [firmware-app.md](firmware-app.md) — tick, screens, store.
