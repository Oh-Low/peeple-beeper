# ESP32 fab

> v2. The RP2350 board (v3) has no layout yet, so there is nothing newer to upload. Do not order this board expecting v3 parts.

Manufacturing files for `hardware/esp32/peeple.kicad_pcb` (schematic: `peeple.kicad_sch`) live in `manufacturing/pcbv2/`. The plots are generated and are not in git. Export them again from that board before an order.

Board outline ≈ **103 × 61 mm**, **2-layer**, 1.6 mm.

## Upload these

| File | Use |
|---|---|
| `pcbv2-gerbers.zip` | PCB fabrication (Gerbers + Excellon drills) |
| `pcbv2-BOM-PCBWay.csv` | Assembly BOM |
| `pcbv2-centroid-PCBWay.csv` | Pick-and-place, SMD only, mm |

Keep one centroid file for the upload. The bare-mm and raw KiCad exports are the same positions with different headers. Upload the zip, not the exploded `gerbers/` folder, unless the form asks for loose files.

## Suggested order

| Option | Suggestion |
|---|---|
| Layers | 2 |
| Dimension | ~103 × 61 mm (confirm in their Gerber viewer) |
| Thickness | 1.6 mm |
| Material | FR-4 |
| Copper weight | 1 oz |
| Surface finish | HASL lead-free or ENIG |
| Solder mask | Green, or your choice |
| Silkscreen | White |
| Via process | Tenting vias |
| Impedance control | No |
| Assembly side | Top only |
| Thru-hole | AE1 antenna spring and SW1 encoder |
| Stencil | Yes |

## Not on the assembled board

- Battery cell (plugs into J2)
- ERC12864FSF-9 glass (plugs into J3)

Unique parts on the last export: 46 BOM lines, 91 SMD parts, 8 thru-hole pins (AE1 ×1 + SW1 ×7). Passive MPN cells are blank on purpose. Confirm silkscreen polarity for diodes and FETs against the assembly PDF before approving the viewer.
