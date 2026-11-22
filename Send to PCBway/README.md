# Send to PCBWay — peeple-beeper pcbv2

Manufacturing package for **`hardware/pcb/pcbv2.kicad_pcb`** (schematic: `pcb.kicad_sch`).

Board outline ≈ **103 × 61 mm**, **2-layer**.

## Upload these

| File | Use |
|---|---|
| `pcbv2-gerbers.zip` | PCB fabrication (Gerbers + Excellon drills) |
| `pcbv2-BOM-PCBWay.csv` | Assembly BOM (CSV) |
| `pcbv2-centroid-PCBWay.csv` | Pick-and-place / centroid (SMD only, mm) |

Optional extras in this folder:

| File | Use |
|---|---|
| `pcbv2-centroid-mm.csv` | Same centroid, bare mm numbers |
| `pcbv2-centroid.csv` | Raw KiCad position export |
| `pcbv2-assembly.pdf` | Fab/silk assembly drawing |
| `gerbers/` | Unzipped Gerber + drill files |
| `ORDER-NOTES.md` | Suggested PCBWay order options |

## Assembly enquiry fields (approx.)

- Quantity: however many boards you are ordering  
- Unique parts: **46** (BOM line count)  
- SMD parts: **91**  
- Thru-holes: **8** pins (AE1 ×1 + SW1 ×7)

## Not on the assembled board

- **Battery cell** (plugs into J2) — do not order as a placed part  
- **ERC12864FSF-9 glass** — plugs into J3 after assembly  

## Notes for PCBWay

1. Upload the **zip** for fab, not loose Gerbers, unless their form asks otherwise.  
2. Paste layers (`F_Paste` / `B_Paste`) are included for SMT.  
3. PTH and NPTH drills are separate (`pcbv2-PTH.drl`, `pcbv2-NPTH.drl`).  
4. Passive MPN cells are blank on purpose — value + package is enough for R/C. ICs/connectors have DigiKey/LCSC order numbers filled in.  
5. Confirm silkscreen polarity marks for diodes/FETs against `pcbv2-assembly.pdf` before approving the online Gerber viewer.
