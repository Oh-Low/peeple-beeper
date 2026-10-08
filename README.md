# PEEPLE BEEPER

Host emulator for a 128×64 FSTN LoRa pager. `src/firmware` is the original TypeScript pager UI (pager, display, font). It talks only to a HAL. There is no C firmware yet — `firmware/include/hal.h` is a contract sketch for a future port. `src/host` is the fake hardware. The host window runs **two independent beepers** (separate identity, contacts, pair session, and pager RAM) linked by a fake air interface.

```
firmware/include/hal.h   C HAL sketch for a future nRF52840 port
src/firmware/            TypeScript pager UI (not a twin of missing C)
src/host/                Encoder knob, LCD blit, air link, leftover inject
```

The LCD is a 1024-byte ST7567-style page buffer (`page * 128 + x`, bit `y & 7`).

App-layer air (pairing, PAGE/ACK, crypto, relay) is described in [docs/radio-protocol.md](docs/radio-protocol.md).

## Live emulator

https://oh-low.github.io/peeple-beeper/

Pushes to `main` build and publish that site via GitHub Pages.

## Run locally

```
npm install
npm run dev
```

## Pair two beepers

1. Use the two device panes (A and B).
2. On each unit, **Settings → Display name**. That string is what the other beeper stores as the contact name. Turn the knob to change a letter, click for the next character, Back to save. With a device focused you can also type and Backspace. If you skip this, HELLO carries the last four hex of the node id.
3. On each unit, choose **Add contact**.
4. Check **Peer is near** (RSSI −30, above the pairing gate of −45). Uncheck to put them far (−90); pairing will not complete.
5. Hold both on the ADD screen until each writes the other into its contact book, then returns to the menu.
6. Open **Compose** — the peer’s Display name should be in the list. Select it, then a canned message, to send an encrypted page.

## Controls

- **Peer is near** — both directions of the air link. Near = −30 dBm; far = −90 dBm.
- **Focus** — click a device (or tab to it). Keyboard only drives the focused unit.
- **Knob** — drag or scroll to turn (one detent per menu row). Click is the shaft button (Select).
- **Back** — side button. On the ADD / pair screen this aborts pairing.
- **Escape** — Back, when focus is not in a text field.
- **Arrows** — Left/Up and Right/Down turn the knob (when not typing in a field).
- **Enter / Space** — Select, when focus is not on a button or in a field.
- **Plaintext inbox inject** — leftover host tool. Injects an unencrypted page into device A’s RX queue for inbox UI testing. It does not pair or use session keys.

## Porting

A C port means rewriting pager, display, and font in C and implementing `firmware/include/hal.h` on the board. It is not keeping the TypeScript UI and swapping `createHostHal.ts` for U8g2 + RadioLib.
