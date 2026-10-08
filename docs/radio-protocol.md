# Radio protocol

Medium-high reference for how beepers use the air. LoRa is a clear pipe; pairing, paging, crypto, and relay all live in the app layer. Frame version is **1** everywhere. Max raw frame is **128 bytes**.

This is not a PHY spec. SX1262 parameters, CAD/LBT, and collision handling are not in firmware yet. The host emulator delivers every in-range TX to every in-range RX with no collisions.

---

## Roles on the wire

Every unit has an **8-byte `node_id`**. It is stored in a **100 KB** on-device partition (with contacts, canned favorites, inbox, and outbox) so it survives power-off. If that store is empty, a new id is invented at boot. Display name is in the same store; it is sent in HELLO (max 15 chars) so the peer can save it as the contact name. Empty name falls back to the last four hex of `node_id`.

The contact book holds up to **256** peers: `node_id`, name, **16-byte session key**, pair time. The session key is **never sent on air**. It is derived independently on both sides during pairing. Inbox and outbox keep up to **256** rows each. Rows store a **local HH:MM** (device clock at the event: received, sent, ACK’d). Those times are not on the air. The whole snapshot must fit in 100 KB; oldest inbox then outbox rows are dropped if a save would overflow. In-flight outbox rows restore as **failed** (retryable) because TX/retry RAM is not part of the store.

Two mutually exclusive radio modes:

- **Whisper pairing** — while Add-contact is live. Only HELLO / HELLO_ACK / PAIR_LEAVE are processed. Whisper is min TX power on hardware; the host just stores a flag. Incoming frames weaker than **−45 dBm** are ignored for pairing.
- **Paging** — the rest of the time. PAGE and PAGE_ACK are processed. Pairing frames are ignored. There is **no RSSI gate** on pages; range is a physical (or emulator) fact.

Firmware never loops its own TX back. A unit does not relay a PAGE it originated.

---

## Frame catalog

All frames start with `type` then `ver = 1`.

| Type | Name | Who talks | Typical size |
|------|------|-----------|----------------|
| `0x01` | HELLO | pairing broadcast | 51–66 |
| `0x02` | HELLO_ACK | pairing unicast | 34 |
| `0x04` | PAIR_LEAVE | pairing unicast | 34 |
| `0x03` | PAGE | origin + relays | 36 + body (max 123) |
| `0x05` | PAGE_ACK | dest + relays | 35 |

`0x00` and anything else are dropped.

### HELLO (`0x01`)

`node_id[8]`, pairing ephemeral `eph[32]`, `nonce[8]`, `name_len`, `name`.

Broadcast while pairing. Ephemeral and nonce are random for this session. Name is the sender’s display name.

### HELLO_ACK (`0x02`) and PAIR_LEAVE (`0x04`)

Same layout, different type: `src_id[8]`, `dest_id[8]`, `confirm[16]`.

`confirm` is the first 16 bytes of SHA-256 over the two raw HELLO frames, sorted lexicographically so both sides compute the same value. ACK means “I have your HELLO.” LEAVE means “I am leaving pairing.” LEAVE is only legal after HELLO_ACK. LEAVE without a prior ACK is a protocol error (`try again`).

### PAGE (`0x03`)

Clear header: `dest[8]`, `src[8]`, `nonce[8]`, `hop_ttl`, `body_len`, then **sealed body**, then `mac[8]`.

- `dest` / `src` / `nonce` / `hop_ttl` are plaintext so relays can forward without the session key.
- `hop_ttl` is **not** in the body MAC. Relays rewrite it in place.
- Body plaintext before seal is `msg_id[8] || UTF-8 text` (text 0–79 bytes). Wire body is therefore up to 87 bytes of ciphertext.

### PAGE_ACK (`0x05`)

`dest[8]`, `src[8]`, `nonce[8]`, `hop_ttl`, `mac[8]`.

Addresses are **swapped** relative to the PAGE: ACK `dest` = original PAGE `src` (origin), ACK `src` = original PAGE `dest` (intended recipient). `nonce` is that PAGE copy’s nonce. MAC is over the session key (see crypto). Relays forward ACK the same way they forward PAGE; they **do not** check the MAC.

---

## Crypto (app layer)

Hash is SHA-256. Truncations are prefixes.

**Session key (pairing).** Sort the two `node_id`s lexicographically. Concatenate `id_lo || id_hi || eph_of_lo || eph_of_hi`. Session key = first **16** bytes of SHA-256 of that. Same inputs on both units; key never hits the air.

**PAGE seal.** Cipher = plaintext XOR keystream, keystream = SHA-256(`session_key || nonce`) repeating as needed. MAC = first 8 bytes of SHA-256(`session_key || nonce || cipher`). Open fails closed if MAC mismatches.

**PAGE_ACK MAC.** First 8 bytes of SHA-256(`session_key || nonce || "ACK"`). Only the origin (who has the dest’s session key) verifies it. A well-formed ACK with a garbage MAC still cancels pending **PAGE relays** (relays have no key).

This is close-range pager crypto, not a hostile-RF design.

---

## Pairing

Window: **20 s**. HELLO (and, once applicable, ACK and LEAVE) rebroadcast about every **400 ms**.

1. Both units enter Add-contact → whisper on, fresh `eph` + HELLO nonce, start broadcasting HELLO.
2. Hear a foreign HELLO above −45 dBm → remember that peer (one pending peer; a different HELLO replaces it) and send HELLO_ACK.
3. Hear HELLO_ACK whose `dest` is us, `src` is the pending peer, and `confirm` matches → they have our HELLO.
4. Once we have a pending peer **and** they have us, broadcasts include PAIR_LEAVE.
5. Hear a matching PAIR_LEAVE → send LEAVE, **commit the contact** (derive session key, upsert book), whisper off, vibe, done.

Timeout, full book (16), or LEAVE-before-ACK abort without committing. Pairing is **not** relayed and is **not** mixed with paging on the same tick.

---

## Paging: send, receipt, retry

A composed message becomes one **outbox** item: dest id, sealed `msg_id` (random 8 bytes, stable for the whole send), UTF-8 body, and a list of **nonces actually transmitted**.

**TX schedule (9 attempts):** t = 0, 10 s, 20 s, 30 s, then every 60 s at 90 / 150 / 210 / 270 / 330 s. After the last TX the origin waits another 60 s for ACK, then the outbox is **failed**. The UI can re-queue a failed send (new `msg_id` and new nonces).

**Each attempt uses a new random nonce.** Same `msg_id` and text. Relays key on `(frame type, src, nonce)`, so a retry is a new flood, not a duplicate of the previous copy. `hop_ttl` on origin TX is **3** (origin plus up to two relay hops).

**Dest:** look up `src` in the contact book. Open body. Inbox **once** per `(src, msg_id)`. ACK **every** copy (every nonce), including repeats, so a late relay still produces a receipt. ACK `hop_ttl` is also 3.

**Origin:** an ACK counts if `dest` is us, `src` is the outbox dest, `nonce` is any nonce from this send, and the ACK MAC checks. Then the outbox is **delivered**. An ACK for an unknown nonce is ignored.

Unknown `src` on a PAGE (not in the book) is dropped; no ACK.

---

## Relay (PAGE and PAGE_ACK)

Anyone who is **not** dest and **not** origin may forward, if `hop_ttl > 1`. Forwarded copy has `hop_ttl − 1`. `ttl ≤ 1` is not queued.

Seen cache: ~**24** recent `(kind, src, nonce)` entries. PAGE is keyed by origin `src` + nonce. PAGE_ACK is keyed by ACK `src` (the page dest) + nonce.

**Jitter:** first time a copy is eligible, wait a random **80–400 ms**, then TX once. After TX, that nonce is not armed again.

**Duplicate PAGE or ACK while still waiting:** do **not** cancel. Re-roll the jitter (new 80–400 ms from now). Keep the already-prepared copy (do not decrement ttl again). This is so a nearby relay TX does not silence a unit that can still reach someone the first relay cannot.

**Cancel pending PAGE only on PAGE_ACK** for that origin+nonce (ACK `dest` = PAGE `src`, same nonce). Proof the dest decrypted a copy. If ACK is heard **before** PAGE, never schedule that PAGE relay.

**Do not cancel a pending ACK relay** because another ACK was heard. Same topology problem on the return path: the unit that can still reach the origin must still forward. Duplicate ACK → re-roll, still send once.

Each node sends a given nonce at most once (PAGE) or once (ACK). Dense meshes serialize extra copies instead of suppressing them; origin retries still bound the conversation.

Relays never decrypt PAGE bodies. They never verify ACK MAC.

---

## Dispatch and HAL

Tick order: if pairing is active, only the pair path polls raw RX. Otherwise the page path polls raw RX, then origin retry, then due relays.

HAL:

- `radioSendRaw` / `radioPollRaw` — app-layer frames (this protocol).
- `radioSend` / `radioPollRx` — leftover plaintext inject for UI; not used by pairing or encrypted paging.
- `radioSetWhisper` — pairing TX power.

---

## Host emulator (not firmware)

Units sit on a 1-D line of **feet from origin**. A hop exists only if `|pos_i − pos_j| < 100 ft`. RSSI is about −30 dBm at 0 ft, falling ~60 dB across that span; at 100 ft the link is silent (`null`, no RX). Pairing still requires ≥ −45 dBm on top of being in range.

The mesh fans each TX to in-range peers with **no loopback, no collision, no CAD**. Firmware paging does not look at RSSI. Adding/removing radios rewires the mesh and seeds contact books; that is host directory logic, not air protocol.

---

## Intentionally out of scope

- LoRa CAD / LBT / airtime / spreading factor
- Encrypted dest/src (relays need them in the clear)
- Authenticated ACK at relays
- Live hop_ttl config (compiled as 3; retries use unique nonces instead of re-arming at ttl==3)
- Multi-peer pairing in one window (one pending HELLO at a time)
