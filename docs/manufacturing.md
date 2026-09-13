# Manufacturing

Everything needed to have OSCD Demo PCB rev 1.0 built is already committed under [`production/`](../production). You do not need KiCad to order a board — only to change the design.

## Board specification

Quote the board with these parameters:

| Parameter | Value |
|---|---|
| Layers | 4 |
| Dimensions | ~93 × 93 mm, non-rectangular outline (see `Edge.Cuts`) |
| Thickness | 1.6 mm |
| Stackup | 0.035 / 0.1 / 0.035 / 1.24 / 0.035 / 0.1 / 0.035 mm, FR4 (ε<sub>r</sub> 4.5) |
| Copper weight | 1 oz outer |
| Minimum track / clearance | 0.10 mm / 0.15 mm |
| Minimum via | 0.45 mm pad / 0.30 mm drill |
| Surface finish | Lead-free HASL (as designed; ENIG is a drop-in upgrade and gives flatter touch pads) |
| Solder mask | Blue (as designed — any colour works) |
| Silkscreen | White |
| Assembly | Single-sided, top only |

The outline is the OSCD logo silhouette, so expect a routing (not V-score) charge and check that your fabricator accepts the internal radii.

## What is in `production/`

| File | Purpose |
|---|---|
| `gerber.zip` | RS-274X gerbers for all four copper layers, mask, silkscreen, paste, edge cuts, plus PTH/NPTH Excellon drill files and drill maps |
| `bom.csv` | Bill of materials grouped by value, with LCSC part numbers |
| `positions.csv` | Pick-and-place / CPL file (`Designator, Mid X, Mid Y, Rotation, Layer`) |
| `designators.csv` | Designator-to-quantity list |
| `netlist.ipc` | IPC-D-356 netlist, for fabricator electrical test |

The BOM carries LCSC part numbers throughout, so the design drops straight into JLCPCB's assembly service. Two caveats before you upload:

- **`J2`/`J3` have no LCSC number.** They are ordinary 2×5, 2.54 mm pin headers — source them yourself and solder them by hand, or leave them off entirely if you do not need the breakout.
- **`R8` is intentionally absent** from the BOM (see [hardware.md](hardware.md#user-buttons)). This is correct, not an omission.

## Hand assembly

Open [`bom/ibom.html`](../bom/ibom.html) in a browser for an interactive board view that highlights each part as you work through the list. It is a self-contained file — no server or internet needed.

Everything is 0805 or larger apart from the ICs, which are LGA-14 (`U3`), DFN-6 (`U5`), UQFN-20 (`U6`) and the 2020-size WS2812Bs. Those want a stencil, solder paste and a hotplate or reflow oven; the passives, headers and buttons are comfortable with an iron.

Order of work that tends to go smoothly: reflow the ICs and LEDs first, then hand-solder `J1`, the buttons and the headers, then power the board from a current-limited supply and check `TP4` reads 3.3 V before plugging it into a computer.

## Regenerating the fabrication files

The contents of `production/` are produced by the **Fabrication Toolkit** KiCad plugin; its settings are stored in [`fabrication-toolkit-options.json`](../fabrication-toolkit-options.json) at the project root. Open the board in Pcbnew and run the plugin — it re-exports the whole folder in one pass, targeting JLCPCB conventions (including footprint rotation correction).

The other generated artefacts:

- **`bom/ibom.html`** — the *Interactive HTML BOM* plugin, run from Pcbnew.
- **`schematic.pdf`** — Eeschema, *File → Plot*, PDF output.

Regenerate all three whenever the schematic or layout changes, and commit them together with the source files so the repository always describes one consistent revision.

## Before you re-spin

Run **DRC in Pcbnew** and **ERC in Eeschema** and resolve or explicitly acknowledge everything. Then check that the outputs above were regenerated — a `production/` folder that predates the last board edit is the most common way an open hardware repository ships an unbuildable design.
