# Suggested PCBWay order options (pcbv2)

These match the exported Gerbers. Adjust if you prefer different cosmetics.

## PCB fabrication

| Option | Suggestion |
|---|---|
| Layers | 2 |
| Dimension | ~103 × 61 mm (confirm in their Gerber viewer) |
| Thickness | 1.6 mm |
| Material | FR-4 |
| Min track / spacing | Confirm from design (typically ≥0.15 mm / 6 mil if that was the rule set) |
| Min hole size | Confirm from drill files |
| Copper weight | 1 oz |
| Surface finish | HASL lead-free or ENIG |
| Solder mask | Green (or your choice) |
| Silkscreen | White |
| Edge connector | No |
| Via process | Tenting vias |
| Castellated holes | No |
| Impedance control | No (unless you later add RF controlled impedance) |

## Assembly

| Option | Suggestion |
|---|---|
| Assembly side | Top only (all SMD on F.Cu) |
| Quantity | Same as PCB qty |
| Parts | Turn-key or kitted — BOM has order numbers for modules/ICs |
| Thru-hole | Yes — AE1 antenna spring + SW1 encoder (hand-solder / selective) |
| Stencil | Yes (paste Gerbers included) |

## Do not assemble

- LiPo cell (customer installs on J2)  
- Display glass (installs in J3 FPC socket)
