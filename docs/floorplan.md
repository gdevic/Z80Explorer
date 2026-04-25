# Z80 die floorplan (Z80Explorer coordinate system)

Empirical spatial model of the Z80 die in the coordinate system used by Z80Explorer's `z80_net_info` / `z80_trans_info` bounding boxes.

## Coordinate system

`bbox = [x, y, width, height]`.

- Origin at **top-left** of the die.
- **x increases rightward**, range ≈ 0..4550.
- **y increases downward**, range ≈ 0..4860.
- Envelope derived from `clk` bbox (194, 154, 4356, 4709) — `clk` reaches essentially everywhere.

## Anchor landmarks

Bboxes pulled from `z80_net_info` for well-known nets:

| Net      | x    | y    | Block hint |
|----------|------|------|-----------|
| _mreq    | 2779 | 87   | top-edge pad (center) |
| _nmi     | 3822 | 152  | top-edge pad (right) |
| _int     | 4135 | 165  | top-edge pad (far right) |
| iff1     | 3775 | 778  | interrupt/flop region, above PLA |
| pla0     | 1413 | 943  | PLA fanout, left end |
| pla87    | 3720 | 1053 | PLA fanout, right side |
| pla49    | 2641 | 1152 | PLA fanout, middle |
| pla98    | 3966 | 1220 | PLA fanout, rightmost |
| _m1      | 160  | 1448 | left-edge pin area |
| alubus0  | 2864 | 3692 | horizontal ALU bus (top of ALU) |
| alua0    | 3982 | 3702 | ALU column (right) |

## Register File block

Corners confirmed by user (transistor-level landmarks):

| Corner        | Transistor | x    | y    |
|---------------|------------|------|------|
| top-left      | t4154      | 1345 | 3231 |
| top-right     | t4167      | 2333 | 3231 |
| bottom-left   | t6512      | 1362 | 4353 |
| bottom-right  | t6526      | 2316 | 4354 |

Extent: `x ≈ 1345..2333, y ≈ 3231..4354` (≈988 × 1123).

## ALU block

Edges confirmed by user:

| Edge     | Landmark net | bbox |
|----------|--------------|------|
| top      | alubus0      | (2864, 3692, 1271, 172)  |
| bottom   | alubus7      | (2839, 4338, 1296, 165)  |
| left     | alu_a (VBUS-MUX ctrl) | (2755, 3447, 206, 974)  |
| left     | alu_b (VBUS-MUX ctrl) | (2836, 3318, 394, 1146) |
| left     | alu_c (VBUS-MUX ctrl) | (2863, 2981, 320, 1439) |
| right    | net 553 (alubus precharge driver) | (4048, 2710, 159, 1800) |

Body extent: `x ≈ 2836..4207, y ≈ 3318..4503` (≈1371 × 1185).

Sits directly to the right of the Register File, with a ~500 px gap between them (RF right edge x≈2333, ALU left edge x≈2836). The ALU body extends a bit higher (y≈3318) and a bit lower (y≈4503) than the Register File.

`alu_a`, `alu_b`, `alu_c` are control signals for VBUS MUXes that feed the ALU from its left edge. Net 553 is the alubus precharge driver — it gates all 8 `alubus*` bits at the right edge of the ALU.

## Approximate y-bands (top → bottom)

- `y ≈ 80–500` — top-edge control/status pads (`_mreq`, `_nmi`, `_int`).
- `y ≈ 500–1000` — interrupt / flop region above the PLA (`iff1` at y=778).
- `y ≈ 940–2400` — **PLA fanout strip.** 99 vertical wires (`pla0..pla98`), each starting at its row in the PLA decode matrix and extending down into consumer logic. Horizontal span ≈ x=1413..4050. Rows are staggered; top-y varies with row index (pla0 at y≈943, pla87 at y≈1053, pla98 at y≈1220). "Above the PLA table" maps roughly to `y < 1050` on the right half, `y < 950` on the left half.
- `y ≈ 2400–3230` — control/decode logic fed by PLA outputs, between the PLA and the register file.
- `y ≈ 3231–4354` — **Register File** (x=1345..2333) and **ALU** (x=2836..4207) side by side. RF on the left, ALU on the right.
- `y > 4354` — lower perimeter / pads.
