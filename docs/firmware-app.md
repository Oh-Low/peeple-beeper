# Firmware app (pager UI)

How one beeper looks and behaves: screens, knob, inbox/outbox, pairing UX, identity, canned messages, local timestamps, and the NVM blob.

Wire bytes, HELLO/PAGE crypto, and retry timing live in [radio-protocol.md](radio-protocol.md). Pins, radio, and the LCD hardware live in [hardware.md](hardware.md). The HAL contract is [hal.md](hal.md). How firmware sits next to the host emulator is [architecture.md](architecture.md).

This is the document to read before you change a menu row, list drawing, or what gets saved.

---

## What “firmware” means here

`firmware/src` is a **C pager application** that talks only to `hal_t` (`hal/hal.h`). It is the product logic. A board port implements the same HAL; it does not keep a second UI.

Firmware must not include the emulator. Pairing and paging use `radio_send_raw` / `radio_poll_raw`.

One device is a `beeper_t` (`firmware/src/beeper.c`): identity, contacts, pair session, page radio, pager UI. `beeper_tick` is the whole frame.

---

## Boot

Every tick until NVM has been read, then every tick after:

1. **`store_load`** — one-shot. Blob into identity, contacts, canned favorites, inbox, outbox. Empty or corrupt store → empty RAM.
2. **`identity_load_or_create`** — if `node_id` is still null, invent 8 random bytes. Display name may still be empty (HELLO then uses the last four hex of the id).
3. **Pair *or* page** — if Add-contact is live, only `pair_tick`. Otherwise `beeper_sessions_tick` (RX, origin retry, relays). A successful pair that grew the book jumps UI to the menu.
4. **`screen_poll_send`** — radio status `delivered` / `failed` updates the matching outbox row.
5. **Input** — skipped on a tick that just received a page (alert holds; encoder/back stay queued).
6. **`screen_render`** — draw 128×64, copy to the HAL buffer, flush if pixels changed.
7. **`store_sync`** — encode RAM; save only if the blob hash changed.

UI always starts on **menu**. Contacts, canned, inbox, and outbox are already in RAM before the first paint of a restored device.

---

## Walkthrough: canned page and a receipt

Assume two units already paired (see [Pairing UX](#pairing-ux-vs-protocol)).

**Send**

1. Menu → **Compose** (knob to the row, shaft **press**). Empty book shows `pair first`.
2. Contact column: press a name. That sets `compose_dest` and moves the highlight to the message column.
3. Message column: hearted favorites (newest first), stock lines (`On my way`, `Running late`, …), then **Custom** at the bottom. Press a stock line.
4. `send_page` starts radio TX, inserts an outbox row at index 0 (`status: sending`, local sent HH:MM, ack clocks `CLOCK_NONE`), and opens **Sending**: the contact name, `SEARCHING`, and your current Beeple on the right. That screen stays up until the attempt's first wait elapses (`ATTEMPT_PAGE_FAST_MS`, 10 seconds), then changes to **Retry** while the radio keeps its full attempt schedule. An ACK still opens **Success**. Back leaves Sending or Retry for Outbox. Menu Outbox shows `Outbox (*)` while the radio is busy.

**Receipt**

5. Dest decrypts, ACKs. Origin `beeper_sessions_tick` sets radio `delivered`. Next `screen_poll_send` marks the first `sending` outbox row **sent**, stamps **ack** HH:MM from `hal->clock`, and (unless you are pairing, alerting, or editing text) opens **Success**: `BEEP` / `SUCCESS!`, with your current Beeple on the right.
6. Press or Back returns to Outbox. Open the row: title `delivered`, `sent HH:MM  ack HH:MM`.

**No receipt**

**Retry** (`CAN'T FIND` / `WILL TRY` / `AGAIN!`) replaces Searching after about 10 seconds. The radio is still sending. Press or Back returns to Outbox. After nine TX attempts (see protocol doc), the row becomes **failed**. Press on a failed list row or on the detail re-queues it: new radio send, row moved to the front, ack clocks cleared to `--:--`, and the Sending screen opens again. Failed-on-boot rows are the same (in-flight TX is not restored).

**Read an incoming page**

`screen_on_page` inserts the inbox row at index 0, vibes 250 ms, beeps 80 ms, flashes the full LCD ~400 ms, and opens **New beep**: slanted `NEW BEEP` bar, the sender and received time, the message wrapped on the left, and the sender's Beeple in the right half. A sender with no Beeple yet keeps the spider. The index arrives inside the sealed page and is saved on that contact. Pairing also carries it. Press or Back → full **read** (from, time, body, RSSI). Press again → Inbox list (`HH:MM from  body`). Inbox `(n)` on the menu is **kept row count**, not an unread flag — there is no read/unread bit.

---

## Screen map

`screen_mode_t` in `firmware/include/screen.h`. Selection is an inverted row, not a `>` glyph. List screens use `sel == 0` as `< back`, except Inbox, Outbox, and the compose columns, whose rows are the entries. Settings uses `BACK`. Opening a non-empty list starts on row 1. Inbox, Outbox, and Compose open on the first entry.

Encoder **turn** is ignored on alert, read, pair, sending, sent, failed, outread, locating, and locread (those are press/back only). Pair, sending, and locating also ignore press.

| Screen | What you see | Press | Back |
|--------|----------------|-------|------|
| `menu` | Left half: Inbox / Compose / Outbox / Peeple / Settings / Beeple. Clock and battery on the top row when Show Clock & Bat is on. Right half: the chosen Beeple. Voidfarer until another is picked | Open that item | — |
| `peeple` | Same layout as the main menu. Left half: Locate / Add. Right half: the chosen Beeple | Locate → contact list; Add → pair | Menu on Peeple |
| `inbox` | Slanted INBOX bar on the left, clock on the top right. As many rows as fit: contact name, time, then the message on that same line. No picture. No back row | Open row → `read` | Menu (Inbox) |
| `read` | From, time, wrapped body, RSSI | Inbox | Menu |
| `compose` | Split screen. Slanted COMPOSE header. Left: contacts, three rows. Right: canned lines, four rows, not highlighted | Pick dest → `canned` | Menu |
| `canned` | Same split. The chosen contact stays highlighted. Messages scroll in the rows above **Custom**, which stays on the bottom row | Custom → keyboard (`bodyedit`); line → send | Compose, same contact |
| `bodyedit` | Textedit, ♥ save + OK | OK sends; ♥ hearts draft | Canned |
| `outbox` | Same slanted header as Inbox, titled OUTBOX. As many rows as fit, in three columns: contact, status (`Fail`, `Retry`, or the sent time), then the message. No back row | Failed → retry; else `outread` | Menu (Outbox) |
| `outread` | Status, To, sent/ack times, body | Failed → retry; else outbox | Outbox |
| `sending` | Slanted `SENDING` bar, contact name, `SEARCHING`. The right half is your current Beeple, with the bar drawn over it. No clock. About 10 seconds, then `failed` | ignored | Outbox |
| `sent` | Same chrome. `BEEP` / `SUCCESS!` | Outbox (or alert if one is pending) | same |
| `failed` | Same chrome. `CAN'T FIND` / `WILL TRY` / `AGAIN!` | Outbox | same |
| `alert` | Slanted `NEW BEEP` bar, sender, received time, wrapped message. The right half is the sender's Beeple. A contact with no Beeple yet keeps the spider. Full invert while flashing | `read` | `read` |
| `pair` | ADD, countdown, `hold together`, RSSI, error | ignored | Abort whisper, Peeple on Add |
| `settings` | Header `SETTINGS`. Names on the left of a 1 px rule, values on the right: BACK, NAME, TRACKING (`ask`/`open`), CLOCK BAT (`on`/`off`), DARK MODE (`on`/`off`), BACKLIGHT (`on`/`off`), VIBE TEST (`on`/`off`). The cut-corner highlight covers the name only. Five rows show; a longer list scrolls | Name → `nameedit`; tracking toggles ask/open; CLOCK BAT toggles the menu clock; DARK MODE inverts every screen; BACKLIGHT off drives the light off; vibe pulses 250 ms | Menu on Settings |
| `beeple` | Left half: the mascots, five rows at a time. Right half: the highlighted one in the 64×64 slot. The pictures come from `docs/BEEPLE PNG` via `tools/beeple_to_art.py` | Save that Beeple and return to the menu | Menu on Beeple, choice unchanged |
| `trackask` | Contact name, Allow / Deny (Deny selected) | Allow sends the fix; Deny sends a refusal | Deny |
| `tracked` | Contact name, `is locating you` | Menu | Menu |
| `locate` | Contact names, or `pair first` | Pick a contact → `locating` | Peeple (Locate) |
| `locating` | Slanted `TRACKING` bar and the spider. For about 2 seconds: `{you} LOOKING FOR {them}`. Then, while the request is still out: `{them} FOUND!` / `ASKING NICELY` | ignored | Abort, back to the contact list |
| `locread` | With a fix and a settled compass: distance, an arrow toward the target, and `You're looking:` plus the direction this beeper is facing. `hold flat`, `hold still`, `do not move`, `no sky`, `no reply`, `no own fix`, and `radio busy` are unchanged. Denied is `DENIED.` / `THIS IS` / `devastating.` | Contact list | Contact list |
| `nameedit` | Textedit, no ♥ | OK saves name | Settings, name selected |

Menu select indices: 0 Inbox, 1 Compose, 2 Outbox, 3 Peeple, 4 Settings, 5 Beeple. With Show Clock & Bat on, the clock and battery share the top row of the left half and the words use an 8 px pitch, and all six rows fit. With it off, the words use a 10 px pitch and the clock stays off that screen, so Beeple scrolls in. The Peeple menu uses that same layout for Locate and Contact. The selected row is a bar with the bottom-right corner cut at 45 degrees, and it stops one pixel short of the picture. The picture is the saved Beeple (`screen_set_beeple`). Voidfarer is index 0 and starts selected. The choice is saved with the store. Settings rows (`SET_BACK` … `SET_VIBE`, count `SET_N`): BACK, NAME, TRACKING (`ask` or `open`), CLOCK BAT, DARK MODE, BACKLIGHT, VIBE TEST. Names are capitals in the 3×6 font and stay left of pixel 63. Values sit on the right. The selected name is a bar with the bottom-right corner cut at 45 degrees. `settings_scroll` keeps that row on screen; five rows fit, and further rows scroll. CLOCK BAT is saved with the store and starts on. DARK MODE is saved and starts off. When it is on, `screen_render` inverts the finished frame, so every screen flips. BACKLIGHT is saved and starts on. Off drives the backlight pin low.

**Locate.** Empty book shows `pair first`. A locate starts even while a page is armed: the tick sends the locate and holds the page. `radio busy` is the screen when this locate is already sending. Otherwise the contact replies once when tracking is open. A unit set to `Tracking: ask` holds the reply until Allow or Deny. Allow sends the stored fix and, for 30 minutes, answers later requests from that contact with no new prompt. Deny answers with no coordinates and the requester shows `denied`. Back on that prompt denies. Open tracking still warns the target: the screen says the contact `is locating you`. A repeat from the same contact inside 12 seconds does not raise that warning again. A device with no saved setting asks. Settings → Tracking switches `ask` and `open`. With both fixes the distance is whole feet (`75 ft`, or miles to one decimal at 1000 ft and up). The arrow waits on the compass. Tip past 20° and the screen says `hold flat`. Shake it, walk with it, or run the vibration motor and it says `hold still`. Held flat and still, it says `do not move` for 400 ms, then draws an 8-way arrow in the device frame: the top of the screen is whatever way the unit is facing. Under the distance, `Last Loc : HH:MM` is the target’s clock when it stored that fix, not when the reply arrived. Each unit powers the receiver for up to 60 seconds about every 30 minutes, stores one fix, and turns the receiver off. The first listen is at boot. A locate reply sends the stored fix. No sky during a listen keeps the previous fix and its time. Leaving flat or still and settling again asks the target for that store. If that reply is missing, or the target has never stored a fix, the arrow stays on the previous target fix and the time does not change. This unit’s own arrow uses a live fix while the receiver is on, and the stored fix while it is off. Returning to flat and still turns the receiver on for one reading and stores that position before the arrow is drawn. Coordinates stay on the air and are not drawn. `no sky` means they have never stored a fix. `no own fix` has no arrow. `no reply` is four attempts, two seconds apart. Back aborts a locate in progress. The result stays in RAM and is not written to the store.

---

## Input

Hardware is a rotary encoder plus a Back button. Firmware never reads GPIO; it polls HAL queues.

| HAL | Meaning in the app |
|-----|---------------------|
| `encoder_poll` → `ENC_CW` / `ENC_CCW` | One **detent** = one list step (or one textedit key). Wrap at the ends. |
| `encoder_poll` → `ENC_PRESS` | Select. The click **edge**. |
| `encoder_poll` → `ENC_RELEASE` | Other half of the same click. **Must not** run a second action. |
| `back_poll` | Consumed **edge**, not button level. |

Host keyboard while a device is focused (`beeper_debug_key`) only feeds `SCREEN_NAMEEDIT` / `SCREEN_BODYEDIT`. On-device users only have the encoder grid.

**Textedit** (`firmware/src/textedit.c`) — one instance for display name and custom pages:

- The slanted header carries the clock. The draft is the row under that header.
- **SPACE**, **SAVE**, **DEL**, and **OK** are a column on the left of a vertical rule. **SAVE** is only on a custom page. Turning off the left side of **E** walks that column in order: **SPACE**, **SAVE**, **DEL**, **OK**.
- To the right: frequency letters (`ETAOINSHRDL`, then `CUMWFGYPBVK`), digits, then symbols. After a letter, the selection returns to **E**.
- **SAVE** stores the draft into canned. Stock presets are not duplicated; max **16** saved, newest first, overflow drops oldest saved.

Limits: name `PAGE_FROM_MAX` (**15**), body `PAGE_BODY_MAX` (**79**). Name keeps ASCII 32–126.

---

## Drawing

- **128×64** ST7567-style page buffer (`page * 128 + x`, bit `y & 7`). Firmware draws into `pager.buf`, then copies to `hal->display_buffer` and calls `display_flush` if different.
- **5×7** glyphs in `firmware/src/display.c`, cell **6×8** (`CHAR_W` / `CHAR_H`). Includes `♥` and `⚙`. Unknown chars are a hollow box.
- **3×6** glyphs (`FONT_SMALL`, advance `FONT_SMALL_W` 4) for `display_draw_header` and the settings list. `display_text_width_small` is the ink span.
- **Lists** (`draw_list`): four visible rows, 12 px pitch, scroll when `sel` leaves the window. Selected row is a full-width **`display_invert_rect`**. Do not add a `>` prefix. Settings is its own list: see the screen map.
- Headers: `display_draw_header`. Solid inverted bar, `DISPLAY_HEADER_H` (9) px, title in the small font at (4, 1). List screens call it through `header()`. The clock stays on the main menu when Show Clock & Bat is on.
- `clip` ellipsizes with `.` ; `wrap` is space/newline wrap for read/outread bodies.

---

## Inbox and outbox rows

Times are **local device clock**, not on the air. `stamp_clock` reads `hal->clock` (hour 0–23, minute 0–59). `CLOCK_NONE` (**255**) means “no clock yet” (not ACKed); UI prints `--:--`.

| | Inbox | Outbox |
|---|--------|--------|
| Cap | `PAGE_KEEP_MAX` **256** | **256** |
| Newest | insert at index 0 | insert at index 0 |
| Overflow in RAM | drop oldest | drop oldest |
| Row | from, body, rssi, `received_ms`, received HH:MM | dest id, to, body, status, sent HH:MM, ack HH:MM |
| List | `14:32 Bob  On my way` | `* 14:32 Bob  On my way` |

Outbox status is the middle column: `Fail` when failed, `Retry` while sending, and the sent time once it is delivered. Only **one** radio send at a time (`page_busy`). Retry is UI: failed row → `page_tx` again (new `msg_id` / nonces). Attempt counts shown on outread come from `message_outbox_attempts` / `message_outbox_budget` (9). Schedule details: [radio-protocol.md](radio-protocol.md).

---

## Pairing UX vs protocol

**What the user does:** set **Display name** (that string is what the peer stores), then **Contact** on both units, hold them together until the menu returns. The menu returns when the pair commits, including when that contact was already in the book. A repeat pair replaces the session key and still leaves the ADD screen.

**What the ADD screen shows:** seconds left in a **20 s** window, `hold together`, last RSSI, then `timeout` / `book full` / `ERROR: Try again`. Short vibe on start; longer vibe on success. Back calls `pair_abort` (whisper off).

**What firmware does** (`firmware/src/pair.c`): whisper TX, HELLO every **400 ms**, ignore pairing frames weaker than **−45 dBm**, one pending peer, commit contact (`node_id`, name, 16-byte session key) on a valid LEAVE. Book max **256** (`CONTACTS_MAX`). Pairing and paging never share a tick.

Frame layout, confirm hash, and session-key derivation: [radio-protocol.md](radio-protocol.md) (HELLO / HELLO_ACK / PAIR_LEAVE). Do not re-document wire bytes here.

Compose lists `contacts_list` (newest first). Re-pairing the same id **replaces** the slot (new key and name).

---

## Store blob

`firmware/src/store.c`. Magic `PB01`, **version 2**, cap **`NVM_BYTES` = 100 KB**.

Persisted:

- 8-byte `node_id` + display name (15)
- Contacts: id, name, session key, `paired_ms` (up to 256)
- **Canned favorites only** (up to 16). Stock `CANNED_DEFAULTS` are compiled in, not stored.
- Inbox rows (including HH:MM)
- Outbox rows (including status + sent/ack HH:MM)

Not persisted: pair session, page-radio TX/retry RAM, seen/relay caches, current screen, textedit draft, vibe-test flag.

**Fit in 100 KB:** `store_encode` drops **oldest inbox**, then **oldest outbox**, until the blob fits (and writes those shorter arrays back into RAM). Contacts and canned are not dropped this way; a contacts overflow is a pair-time `book full`.

**In-flight restore:** status byte `sending` loads as **`failed`** so the user can retry. Ack clocks stay `255` if they were unset.

`store_sync` hashes the blob and skips `nvm_save` when unchanged. Tests: `firmware/tests/store_test.c` (`make test`) cover roundtrip of name, contacts, **canned**, inbox/outbox clocks, and sending→failed on load.

---

## Alerts

On a new page (`screen_on_page`):

- `hal->vibe(hal->ctx, 250)` and `hal->beep(hal->ctx, 80)`
- `alert` copy of the page, `flash_start` / `FLASH_MS` (400) for a full-buffer invert
- Screen becomes `alert` unless you are on the leftover `sending` screen (then the page is still stored and the flash is armed, but the sending drawer stays)

Settings **VIBE TEST** holds `vibe(250)` each render while Settings is open. Pressing BACK or NAME, or leaving Settings, stops it (`vibe(0)`).

---

## Changing a menu safely

Stay inside `firmware/src`. Call `hal->foo(hal->ctx, ...)` only — **no SDL, no files**. The emulator HAL lives in `hal/emu`. The desktop mesh lives in `firmware/host`. Tests may construct a RAM HAL; production pager code must not.

Checklist when you add/rename a menu row:

1. Menu labels (`MENU_N`, `MENU_PEEPLE`, `MENU_SETTINGS`, `MENU_BEEPLE`). Locate and Add live on the Peeple menu (`PEEP_LOCATE`, `PEEP_CONTACT`). The right-hand picture is the selected mascot (`screen_set_beeple`), not a menu row.
2. `on_turn` clamp (`MENU_N` on the menu, `SET_N` on settings). A new settings row needs a `SET_` index, a label, a value, an `on_press` case, and `SET_N` grows the scroll window.
3. `on_press` indices and `on_back` restore (`sel` of the item you came from).
4. `draw_menu` label array (Inbox count, Outbox `*`). Clock and battery follow `show_clock_bat`.
5. Any new screen: `screen_mode_t`, `on_turn` / `on_press` / `on_back`, `screen_render` branch, list `sel == 0` back row if it is a list.

Do not encode selection as ASCII `>`. Keep invert-rect lists.

**Tests to run or extend**

- `firmware/tests/page_test.c` — `make test` — send/ACK, retries, fail. Outbox UI status follows this radio.
- `firmware/tests/relay_test.c` — jitter, hop drop, already-sent.
- `firmware/tests/store_test.c` — blob shape, canned persistence, in-flight → failed.
- `firmware/tests/gps_test.c` — foot-grid quantization and RMC parse.
- `firmware/tests/heading_test.c` — eight-way arrow.
- `firmware/tests/receiver_test.c` — receiver sleep.
- `firmware/tests/locate_test.c` — locate reply, hop, no sky, tracking consent.
- `firmware/tests/mesh_test.c` — RSSI and who hears whom inside 100 ft.
- `firmware/tests/seed_test.c` — emulator directory (`emu/seed.c`).
- `firmware/tests/pair_test.c` — restarting a pair.

---

## Modules

| File | Role |
|------|------|
| `firmware/src/beeper.c` | Compose one device; tick order; text hook; reset |
| `firmware/src/screen.c` | Screens, menus, inbox/outbox RAM, compose, settings, draw |
| `firmware/src/pair.c` | Add-contact session (HELLO UX + whisper) |
| `firmware/src/message.c` | Encrypted page send, retry, and ack |
| `firmware/src/attempt.c` | Nonce list, attempt count, next-send time, in-flight reply check |
| `firmware/src/locate.c` | On-demand locate request, reply, retry, relay |
| `firmware/src/gps.c` | Foot grid ↔ arcminutes, distance, bearing, RMC text |
| `firmware/src/heading.c` | Flat / still gate, then an 8-way arrow in the device frame |
| `firmware/src/nmea.c` | PA1616S `$GPRMC` / `$GNRMC` → `gps_fix_t` |
| `firmware/src/identity.c` | `node_id`, display name |
| `firmware/src/contacts.c` | Book, upsert, `CONTACTS_MAX` |
| `firmware/src/canned.c` | Defaults + saved favorites |
| `firmware/src/store.c` | NVM encode/decode/sync |
| `firmware/src/textedit.c` | Frequency keyboard, snaps back to E |
| `firmware/src/display.c` | Pixels, invert, wrap, clip, glyphs |
| `hal/hal.h` | Constants + `hal_t` the app may call |
| `firmware/src/frames.c` / `crypto.c` | Wire (see protocol doc) |

### Constants (verify in code, not comments)

| Name | Value | Where |
|------|-------|--------|
| Display | 128×64 | `hal.h` |
| `PAGE_FROM_MAX` | 15 | name / from / HELLO name |
| `PAGE_BODY_MAX` | 79 | page body |
| `NVM_BYTES` | 100 KB | store partition |
| `CONTACTS_MAX` | 256 | `contacts.h` |
| `PAGE_KEEP_MAX` | 256 | `pager.h` inbox and outbox |
| `CLOCK_NONE` | 255 | `pager.h` |
| `CANNED_SAVED_MAX` | 16 | `canned.h` (persisted) |
| `PAIR_WINDOW_MS` | 20 000 | `pair.h` |
| `PAIR_RSSI_MIN` | −45 dBm | `pair.h` |
| `ATTEMPT_LOCATE_BUDGET` | 4 | `attempt.h` |
| `ATTEMPT_LOCATE_MS` | 2000 | `attempt.h` |
| `TRACK_ALLOW_MS` | 30 min | `locate.h` |

---

## See also

- [architecture.md](architecture.md) — firmware vs emulator vs board HAL
- [hal.md](hal.md) — what firmware may call
- [hardware.md](hardware.md) — board, LCD, encoder, radio, GPS, heading
- [radio-protocol.md](radio-protocol.md) — HELLO/PAGE bytes, crypto, retries, relay
