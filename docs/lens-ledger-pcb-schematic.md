# Lens ledger: PCB schematic

Reviewed `hardware/pcb/pcb.kicad_sch` and `hardware/pcb/modules.kicad_sym` only. Lenses: correctness, scope, band-aids. Comments, architecture, and latent-design were not run.

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 1 | correctness | IO38 drives LC86L RXD at the 3.3 V rail. RXD VIHmax is 3.1 V. | `pcb.kicad_sch` GPS_RXD on U1 pin 31 and U5 pin 1 | FIXED |
| 2 | scope | Delete the cell divider R14 and R15. | `pcb.kicad_sch` R14, R15 | REJECTED — the pager reads cell voltage, and the ESP32 has no divider of its own (`docs/pcb-bom.md`, cell divider row) |
| 3 | scope | Delete the backlight switch and drive the LED from a GPIO. | `pcb.kicad_sch` Q1, Q2, R3, R4 | REJECTED — the backlight is about 45–60 mA (`docs/pcb-bom.md`, screen section). A GPIO cannot source it |
| 4 | band-aids | Keep one BSS138 on the LED cathode, tie the anode to 3.3 V, and delete the P-FET and its pull-up. | `pcb.kicad_sch` Q2, R4, U7 BLA and BLK | FIXED |
| 5 | scope, band-aids | Delete the GPS N-FET and drive the P-FET gate from the enable pin. | `pcb.kicad_sch` Q3, R5, Q4 gate | FIXED |
| 6 | scope | Delete the GPS reset transistor and leave RESET_N open. | `pcb.kicad_sch` Q6, R19 | REJECTED — Quectel says connect RESET_N so a hung module can be reset, and the pin forbids a pull-up, so the open-drain stays |
| 7 | scope | Tie the compass chip-select pins straight to 3.3 V and drop R11 and R12. | `pcb.kicad_sch` R11, R12 | FIXED |

## Round 2

Same two files, after the round 1 edits. Scope returned nothing to remove.

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 8 | correctness | V0, XV0, and VG are open, so the glass has no bias capacitors. | `modules.kicad_sym` V0, XV0, VG | REJECTED — the ERC12864-9 electrical spec names those pins and does not ask for an external network. The boost capacitors are on the glass |
| 9 | correctness | The charger cannot finish, and a flat cell only gets the 40 mA precondition current, because the pager stays on from the cell. | `pcb.kicad_sch` U2, R13, U3 EN | ASKED |
| 10 | correctness | GPS VCC has none of the 4.7 µF, 100 nF, and 33 pF Quectel shows at that pin. | `pcb.kicad_sch` GPS_VCC | FIXED |
| 11 | correctness | With the GPS switch off, the UART divider still feeds the unpowered RX pin. | `pcb.kicad_sch` R21, R22, Q4 | ASKED |
| 12 | correctness | The backlight anode sits on a 3.32 V rail and the glass maximum is 3.2 V. | `pcb.kicad_sch` BLA, R1, R2 | ASKED |
| 13 | correctness | The cell divider is 50 kΩ into the ADC with no capacitor. | `pcb.kicad_sch` R14, R15, BAT_ADC | FIXED |
| 14 | correctness | The buck lets the 3.3 V rail follow the cell down once the cell is near 3.3 V. | `pcb.kicad_sch` U3 | REJECTED — a buck cannot hold 3.3 V from a 3.0 V cell. That is the topology in `docs/pcb-bom.md` |
| 15 | band-aids | Delete Q6 and R19 and drive RESET_N from IO40 as an open-drain GPIO. | `pcb.kicad_sch` Q6, R19 | REJECTED — RESET_N maximum high is 3.1 V, and a GPIO high is the 3.3 V rail. The FET is what keeps that rail off the pin |

## Round 3 — BOM vs connections

Reviewed uncommitted `docs/pcb-bom.md` against `docs/pcb-connections.md` (connections is authority). Lenses: correctness, scope, band-aids. Architecture / latent / comments / ponytail skipped — inventory sync, not structure.

| # | Lenses | Finding | Where | Disposition |
|---|---|---|---|---|
| 16 | correctness | Passives table omitted C9/C10, R19, R20, and the second 2N7002 (Q6); 100 nF / 10 µF counts did not match connections. | `docs/pcb-bom.md` Passives | FIXED — table rewritten with refs from connections |
| 17 | correctness | ESP32 note said leave GPIO3 free; connections pulls IO3 down with R20. GPS note said reset was not designed; connections has Q6/R19. | `docs/pcb-bom.md` module blurbs | FIXED |
| 18 | correctness | Quoted Parts total still uses the old $2.50 passives estimate after the discrete list grew (charger caps, Q6, R19, R20). | `docs/pcb-bom.md` Quoted parts | ASKED — reprice or leave as rough estimate |
| 19 | correctness | Piezo blurb still says any free GPIO except the four strapping pins; connections hard-wires PIEZO to IO17, and IO3 is occupied by R20. | `docs/pcb-bom.md` GSC1030RB | OPEN |
| 20 | scope | Delete the retold net/polarity prose from module blurbs and point only at connections. | `docs/pcb-bom.md` What each module requires | REJECTED — shopping notes still need the short why; connections stays the pin authority |
| 21 | scope | SW2 has no quoted MPN while SW1 does. | `docs/pcb-bom.md` SW2 vs SW1 | REJECTED — footprint class is enough until a part is picked; connections only names it Back |
| 22 | band-aids | Nothing to strip. Q6/R19 stay per #6/#15. | — | — |
