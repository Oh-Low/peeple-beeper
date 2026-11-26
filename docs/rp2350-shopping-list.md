# RP2350 shopping list — 10 boards

What to order for ten v3 boards. Parts match [rp2350-bom.md](rp2350-bom.md) and `hardware/rp2350/peeple.kicad_sch`. Prices checked **25 November 2026**. Shipping, tariffs, the stencil, and bare boards are not included.

**Cart total: $224.74** → **$22.47 per board** if you split the cart by ten.  
**Allocated (only the parts that go on the boards): $208.79** → **$20.88 per board**. The $15.95 gap is leftover passives and extra tactiles from 100-piece bags.

The cell is an AliExpress **103450** 2000 mAh pouch with PH 2.0 2-pin, ordered at **$1.80** each (sale + coupon). That replaces the $12.50 Adafruit 2011 DigiKey line and cuts about **$107** off the ten-board cart. Confirm polarity before plugging into J2 (pin 1 = +). Pack is about **10 × 34 × 50 mm**, thicker than the Adafruit 7 mm pouch.

## Vendor rule

Primary vendor is **LCSC**. A line moves elsewhere only when Octopart shows another stocked offer more than **15%** cheaper at the quantity we need, or when LCSC does not sell the part.

| Part | LCSC at qty 10 | Other (Octopart / DigiKey) | Δ | Decision |
|---|---:|---:|---:|---|
| PEC11R-4220F-S0024 | $2.17 | DigiKey $1.79 (8,377 in stock) | −17% | DigiKey |
| AOTA-B201610S3R3-101-T | $0.29 | DigiKey cut tape $0.18 (51k in stock) | −38% | DigiKey |
| TLV62568DBVR | $0.08 | DigiKey ~$0.25 | DigiKey higher | LCSC |
| ABM8-272-T3 | $0.51 | Newark / DigiKey cut tape ≥ $0.52 | DigiKey higher | LCSC |
| RP2354A | $1.25 | LCSC is the public stocked offer; Weyland $1.07 is a broker | — | LCSC |
| QMC5883P | $1.42 | DigiKey only has Adafruit breakouts ~$6 | — | LCSC |
| Motor | — | DigiKey only | — | DigiKey |
| Cell (103450 pouch) | — | AliExpress $1.80 (ordered) vs DigiKey Adafruit $12.50 | −86% | AliExpress |
| HT-RA62 | — | Heltec only | — | Heltec |
| Screen + ZIF | — | BuyDisplay | — | BuyDisplay |

Octopart pages used for the comparisons: [PEC11R-4220F-S0024](https://octopart.com/search?q=PEC11R-4220F-S0024), [AOTA-B201610S3R3-101-T](https://octopart.com/search?q=AOTA-B201610S3R3-101-T), [TLV62568DBVR](https://octopart.com/search?q=TLV62568DBVR), [RP2354A](https://octopart.com/search?q=RP2354A), [QMC5883P](https://octopart.com/search?q=QMC5883P), [ABM8-272-T3](https://octopart.com/search?q=ABM8-272-T3).

## Order summary

| Vendor | Cart | Link |
|---|---:|---|
| LCSC | $112.90 | [lcsc.com](https://www.lcsc.com/) |
| Heltec | $39.00 | [heltec.org/project/ht-ra62](https://heltec.org/project/ht-ra62/) |
| DigiKey | $32.04 | [digikey.com](https://www.digikey.com/) |
| BuyDisplay | $22.80 | [buydisplay.com](https://www.buydisplay.com/1-7-inch-low-cost-white-128x64-graphic-cog-lcd-display-st7567-spi) |
| AliExpress | $18.00 | [103450 pack](https://www.aliexpress.us/item/3256811808497990.html) |
| **Total** | **$224.74** | |

## AliExpress — $18.00 (ordered)

| Part | Order | Need | Buy | Unit | Ext | Link |
|---|---|---:|---:|---:|---:|---|
| Cell, 2000 mAh LiPo 103450, PH 2.0 2-pin | WHITECRANE / listing 3256811808497990 | 10 | 10 | $1.80 | $18.00 | [page](https://www.aliexpress.us/item/3256811808497990.html) |

Already ordered at $1.80 each (sale + coupon). About **10 × 34 × 50 mm**. Before first use: meter polarity (J2 pin 1 must be +), check for a PCM under the tape, and spot-check capacity on one pack. Adafruit 2011 ($12.50 at DigiKey) remains a known-good fallback.

## DigiKey — $32.04

| Part | DigiKey # | Need | Buy | Unit | Ext | Link |
|---|---|---:|---:|---:|---:|---|
| Coin motor JYC1027 | 5101-JYC1027-ND | 10 | 10 | $1.24 | $12.36 | [page](https://www.digikey.com/en/products/detail/jie-yi-electronics-limited/JYC1027/20193265) |
| Encoder PEC11R-4220F-S0024 | PEC11R-4220F-S0024-ND | 10 | 10 | $1.79 | $17.92 | [page](https://www.digikey.com/en/products/detail/bourns-inc./PEC11R-4220F-S0024/4499660) |
| Core inductor AOTA-B201610S3R3-101-T | 535-AOTA-B201610S3R3-101-TCT-ND | 10 | 10 | $0.18 | $1.76 | [page](https://www.digikey.com/en/products/detail/abracon-llc/AOTA-B201610S3R3-101-T/25621527) |

Buy the cut-tape (CT) AOTA line, not the 3,000-piece reel. Polarity mark goes toward VREG_LX.

## Heltec — $39.00

| Part | Order | Need | Buy | Unit | Ext | Link |
|---|---|---:|---:|---:|---:|---|
| HT-RA62, 915 MHz | ZC-144-915 | 10 | 10 | $3.90 | $39.00 | [page](https://heltec.org/project/ht-ra62/) |

Pick the 915 MHz band on the product page.

## BuyDisplay — $22.80

| Part | Order | Need | Buy | Unit | Ext | Link |
|---|---|---:|---:|---:|---:|---|
| ERC12864FSF-9 glass | ERC12864FSF-9 | 10 | 10 | $2.12 | $21.20 | [page](https://www.buydisplay.com/1-7-inch-low-cost-white-128x64-graphic-cog-lcd-display-st7567-spi) |
| 14-pin 1.0 mm ZIF (Molex 200528-0140 class) | option on that page | 10 | 10 | $0.16 | $1.60 | same page |

Add the ZIF connector in the dropdown when you add the glass to the cart.

## LCSC — $112.90

### Modules and ICs

| Part | LCSC # | Need | Buy | Unit | Ext | Link |
|---|---|---:|---:|---:|---:|---|
| RP2354A | C41378174 | 10 | 10 | $1.25 | $12.54 | [page](https://www.lcsc.com/product-detail/C41378174.html) |
| ABM8-272-T3 crystal | C20625731 | 10 | 10 | $0.51 | $5.13 | [page](https://www.lcsc.com/product-detail/C20625731.html) |
| TYPE-C-31-M-12 | C165948 | 10 | 10 | $0.19 | $1.87 | [page](https://www.lcsc.com/product-detail/C165948.html) |
| USBLC6-2SC6 | C2827654 | 10 | 10 | $0.05 | $0.45 | [page](https://www.lcsc.com/product-detail/C2827654.html) |
| TP4054 | C668215 | 10 | 10 | $0.04 | $0.43 | [page](https://www.lcsc.com/product-detail/C668215.html) |
| TLV62568DBVR | C163219 | 10 | 10 | $0.08 | $0.75 | [page](https://www.lcsc.com/product-detail/C163219.html) |
| FNR4030S2R2MT 2.2 µH | C167869 | 10 | 10 | $0.05 | $0.53 | [page](https://www.lcsc.com/product-detail/C167869.html) |
| B5819W | C2943878 | 10 | 10 | $0.02 | $0.18 | [page](https://www.lcsc.com/product-detail/C2943878.html) |
| AO3401A (Q2, Q3) | C15127 | 20 | 20 | $0.10 | $2.05 | [page](https://www.lcsc.com/product-detail/C15127.html) |
| T1-915M antenna | C726086 | 10 | 10 | $0.24 | $2.38 | [page](https://www.lcsc.com/product-detail/C726086.html) |
| ATGM336H-5NR32-G | C49233943 | 10 | 10 | $1.68 | $16.79 | [page](https://www.lcsc.com/product-detail/C49233943.html) |
| BWGPSCNX8-8B1 patch | C496567 | 10 | 10 | $1.70 | $16.96 | [page](https://www.lcsc.com/product-detail/C496567.html) |
| U.FL-R-SMT-1(80) | C88374 | 10 | 10 | $0.12 | $1.20 | [page](https://www.lcsc.com/product-detail/C88374.html) |
| LQW18AN47NG00D 47 nH | C98076 | 10 | 10 | $0.04 | $0.44 | [page](https://www.lcsc.com/product-detail/C98076.html) |
| SC7A20HTR | C19274408 | 10 | 10 | $0.24 | $2.37 | [page](https://www.lcsc.com/product-detail/C19274408.html) |
| QMC5883P | C2847467 | 10 | 10 | $1.42 | $14.18 | [page](https://www.lcsc.com/product-detail/C2847467.html) |
| BSS138 | C78284 | 10 | 10 | $0.02 | $0.22 | [page](https://www.lcsc.com/product-detail/C78284.html) |
| TS-1187A-B-A-B | C318884 | 30 | **50** | $0.02 | $1.03 | [page](https://www.lcsc.com/product-detail/C318884.html) |
| SRV05-4 | C558418 | 10 | 10 | $0.03 | $0.27 | [page](https://www.lcsc.com/product-detail/C558418.html) |
| AO3400A | C20917 | 10 | 10 | $0.09 | $0.90 | [page](https://www.lcsc.com/product-detail/C20917.html) |
| 1N4148W | C81598 | 10 | 10 | $0.01 | $0.12 | [page](https://www.lcsc.com/product-detail/C81598.html) |
| GSC1030RB-3V4000 | C255330 | 10 | 10 | $0.49 | $4.85 | [page](https://www.lcsc.com/product-detail/C255330.html) |
| PH-2PWB cell header | C2905019 | 10 | 10 | $0.03 | $0.32 | [page](https://www.lcsc.com/product-detail/C2905019.html) |
| NCD0805R1 LED | C84256 | 10 | 10 | $0.01 | $0.14 | [page](https://www.lcsc.com/product-detail/C84256.html) |
| 1×40 2.54 mm header | C2337 | 1 | 1 | $0.16 | $0.16 | [page](https://www.lcsc.com/product-detail/C2337.html) |

Need 30 tactiles (3 per board); buying **50**. One header strip cuts into thirteen 3-pin SWD headers.

### Passives (100-piece bags)

Need column is what ten boards consume. Buy column is the bag size. Leftovers stay in the drawer.

| Part | LCSC # | Need | Buy | Unit | Ext | Link |
|---|---|---:|---:|---:|---:|---|
| 5.1 kΩ 0805 | C27834 | 20 | **100** | $0.0056 | $0.56 | [page](https://www.lcsc.com/product-detail/C27834.html) |
| 27 Ω 0603 | C25190 | 20 | **100** | $0.0021 | $0.21 | [page](https://www.lcsc.com/product-detail/C25190.html) |
| 100 kΩ 0805 | C149504 | 60 | **100** | $0.0060 | $0.60 | [page](https://www.lcsc.com/product-detail/C149504.html) |
| 150 kΩ 0805 | C17470 | 10 | **100** | $0.0049 | $0.49 | [page](https://www.lcsc.com/product-detail/C17470.html) |
| 100 nF 0805 | C49678 | 130 | **200** | $0.0139 | $2.78 | [page](https://www.lcsc.com/product-detail/C49678.html) |
| 10 µF 0805 | C15850 | 60 | **100** | $0.0692 | $6.92 | [page](https://www.lcsc.com/product-detail/C15850.html) |
| 2 kΩ 0805 | C17604 | 10 | **100** | $0.0039 | $0.39 | [page](https://www.lcsc.com/product-detail/C17604.html) |
| 1 kΩ 0805 | C17513 | 20 | **100** | $0.0035 | $0.35 | [page](https://www.lcsc.com/product-detail/C17513.html) |
| 4.7 µF 0805 | C1779 | 30 | **100** | $0.0348 | $3.48 | [page](https://www.lcsc.com/product-detail/C1779.html) |
| 453 kΩ 0805 | C17702 | 10 | **100** | $0.0049 | $0.49 | [page](https://www.lcsc.com/product-detail/C17702.html) |
| 33 Ω 0603 | C23140 | 10 | **100** | $0.0025 | $0.25 | [page](https://www.lcsc.com/product-detail/C23140.html) |
| 4.7 µF 0603 | C69335 | 30 | **100** | $0.0425 | $4.25 | [page](https://www.lcsc.com/product-detail/C69335.html) |
| 100 nF 0603 | C14663 | 110 | **200** | $0.0141 | $2.82 | [page](https://www.lcsc.com/product-detail/C14663.html) |
| 1 kΩ 0603 | C21190 | 20 | **100** | $0.0020 | $0.20 | [page](https://www.lcsc.com/product-detail/C21190.html) |
| 15 pF 0603 | C1644 | 20 | **100** | $0.0114 | $1.14 | [page](https://www.lcsc.com/product-detail/C1644.html) |
| 0 Ω 0805 | C17477 | 10 | **100** | $0.0044 | $0.44 | [page](https://www.lcsc.com/product-detail/C17477.html) |
| 4.7 kΩ 0805 | C17673 | 20 | **100** | $0.0051 | $0.51 | [page](https://www.lcsc.com/product-detail/C17673.html) |
| 10 kΩ 0805 | C17414 | 20 | **100** | $0.0034 | $0.34 | [page](https://www.lcsc.com/product-detail/C17414.html) |
| 100 Ω 0805 | C17408 | 10 | **100** | $0.0042 | $0.42 | [page](https://www.lcsc.com/product-detail/C17408.html) |

Passive bags alone are about $27 of the LCSC cart; ten boards only use about $11 of that.

## Checkout order

1. AliExpress cell — **ordered** ($1.80 × 10).
2. DigiKey cart (motor, encoder, AOTA). Domestic shipping is usually a few dollars.
3. LCSC cart (everything else on the board). Expect international shipping and possible tariffs.
4. Heltec (10× HT-RA62, 915 MHz).
5. BuyDisplay (10× glass + 10× ZIF).

Still needed after parts: bare boards and a 4-mil stencil for U1, U6, and U7. Those are not in this list.

## Related

- [RP2350 inventory (CSV)](rp2350-inventory.csv) — order status, qty, reorder links, refs
- [RP2350 bill of materials](rp2350-bom.md)
- [RP2350 connections](rp2350-connections.md)
- [RP2350 bringup](rp2350-bringup.md)
