// This file contains custom net names, overrides of the names defined in nodenames.js
// and definitions of buses (collections of nets). The app rewrites this file in full whenever
// the net names are saved, so hand edits made while it is running will be overwritten.
var nodenames_override = {
_abus0: 3370,
_abus1: 3381,
_abus2: 1968,
_abus3: 1970,
_abus4: 792,
_abus5: 797,
_abus6: 2160,
_abus7: 2163,
_abus8: 2279,
_abus9: 2280,
_abusa: 2396,
_abusb: 921,
_abusc: 954,
_abusd: 957,
_abuse: 1003,
_abusf: 1004,
_aincdec_hold_req: 399, // Active-low incrementer hold request: LD SP,HL at M1T4, EX (SP),HL at M5T3
_alu_lo_done: 1526, // Active-low end of the low-nibble ALU pass; switches alu_lo_sel to the high nibble
_alu_ovf: 683, // Active-low overflow: XNOR of the carries into and out of ALU bit 3 (bit 7 in the high pass)
_alua_out_req: 496, // Active-low alua readback term: M2T1 of IN/OUT (n),A, M5T3 of RST and acknowledges, M3T2 of IM2
_alua_zero: 405, // Active-low: clears ALU operand A to 00h after the operand load (NEG, CPL, LD, IN r,(C), rotates, RST)
_alub_clr: 756, // Active-low ALU operand B clear; clears every alub bit through alub_and38 and alub_clr38
_alub_clr_1693: 1693, // Active-low alub clear term: M4T2 of RRD/RLD, M3T3 of the displacement add
_alub_out_ld_ir: 1534, // Active-low alub readback term: M1T4 of LD I,A / LD R,A; M3T1 of every M3 but IM2 (feeds the first RRD/RLD nibble transfer)
_alub_out_rst: 1520, // Active-low alub readback term: M4T3 of RST and the NMI/IM0/IM1 acknowledges, M3T3 of IM2, RRD/RLD
_alubus_to_vbus: 603, // Active-low enable of the ALUBUS to VBUS drivers used by ALU result write-backs
_ex_dehl_a: 1713, // NOT ex_dehl_combined, gate of the _sel_hl_log to _sel_hl_col pass
_ex_dehl_b: 1709, // NOT ex_dehl_combined, gate of the _sel_de_log to _sel_de_col pass
_flag_szp_ld: 500, // Active-low S, Z and P/V flag carrier load term
_hl_req: 581, // Active-low logical HL request: sel_hl, or a field select decoding to H/L/HL
_hl_sel_m1t4: 441, // Active-low: selects HL (IX/IY) at the clk 0 after M1T4
_iff_restore_req: 1224, // Active-low NAND(t2, m1, pla46): RETN/RETI restore request, sampled into _iff_restore_req_s
_iff_restore_req_s: 1227, // _iff_restore_req held from the clk=1 half; iff_restore fires at M1T3 clk 0 after RETN/RETI
_iff_wr_req: 1283, // Active-low NAND(t1, m1, pla97): EI/DI write request, sampled into _iff_wr_req_s
_iff_wr_req_s: 1269, // _iff_wr_req held from the clk=1 half; iff_wr = NOR(this, clk) fires at the next M1T2 clk 0
_iff1: 191, // NOT IFF1 master 181; int_enable and the iff1 buffer read IFF1 through it
_iff2: 1278, // Inverted IFF2 node; gates the extra pulldown t4049 on pla83
_im1_m1t5: 1524, // Complement of im1_m1t5; input of alua_to_alubus
_insn_end_s: 121, // NOT insn_end_s; m_cycle_start = NOR(this, clk)
_int_ack_im01: 1260, // Active-low: interrupt acknowledge in IM 0 or IM 1
_ir_xfer: 537, // Active-low I/R transfer request for LD I,A / LD A,I / LD R,A / LD A,R at M1T5 clk 0
_ixiy_latch: 1641, // Storage node of the IX/IY prefix select
_ixy_d_sel: 511, // Active-low: M2T2 and M3T3 of an IX+d memory instruction
_last_m: 184, // Complement of last_m (Ken's LAST_M_CYCLE/)
_last_t: 110,
_m3_nobus: 416, // Active-low: M3 is an internal cycle with no bus access
_nf_load: 1802, // Active-low N flag load request, suppressed in DAA; clocked into nf_load
_pair_xfer: 457, // Active-low enable of the half-by-half register pair transfers: writes of the M2/M4 bytes at M3T1/M5T1, reads of the high half at M4T1 and low half at M5T1 for stack writes
_pla83_s: 1419, // NOT pla83_s
_pvz_preset: 623, // Active-low request behind pvz_preset
_rd_hi_req: 1558, // Active-low high-half register read term for the M4T1 stack write (r_v)
_rd_lo_req: 523, // Active-low low-half register read term for the M5T1 stack write (r_u)
_regbit0: 708,
_regbit1: 715,
_regbit2: 745,
_regbit3: 753,
_regbit4: 785,
_regbit5: 799,
_regbit6: 834,
_regbit7: 847,
_regbit8: 880,
_regbit9: 886,
_regbit10: 914,
_regbit11: 923,
_regbit12: 949,
_regbit13: 959,
_regbit14: 980,
_regbit15: 985,
_regsel_a: 505, // Active-low AF select term: ALU result to A, rotates, accumulator transfers
_regsel_bc: 508, // Active-low BC select term: DJNZ, INI/OUTI, IN/OUT (C), block instruction BC decrement
_regsel_de: 1553, // Active-low DE select term: M2T3 and M3T2 of LDI/LDD/LDIR/LDDR
_regsel_hl_506: 506, // Active-low HL select term: 16-bit adds, RRD/RLD, block instruction HL update
_regsel_hl_507: 507, // Active-low HL select term: M1T4 of HL memory operands and 16-bit adds, LD r,n, INI/OUTI
_regsel_ir: 1556, // Active-low I/R select term: LD I,A / LD A,I / LD R,A / LD A,R and the IM2 acknowledge
_regsel_qq: 1557, // Active-low PUSH/POP pair select term (AF in place of SP)
_regsel_r: 567, // Active-low select and read enable term for the register named by the r field
_regsel_rr_510: 510, // Active-low IR[5:4] pair select term: LD rr,nn, INC/DEC rr, LD (rr),A, LD A,(rr), EX (SP),HL
_regsel_rr_1541: 1541, // Active-low IR[5:4] pair select term: EX (SP),HL, 16-bit adds, LD (nn),rr and LD rr,(nn)
_regsel_sp: 1552, // Active-low SP select term: T2 of stack M-cycles, M1T5 of stack instructions and LD SP,HL
_rh_wr_req: 1642, // Active-low high-byte register write request
_rl_wr_req: 1648, // Active-low low-byte register write request
_sel_af: 584, // Active-low AF pair request
_sel_de_col: 1707, // Active-low physical DE-column request: _sel_de_log, or _sel_hl_log when ex_dehl_combined
_sel_de_log: 601, // Active-low logical DE select, before the EX DE,HL swap
_sel_hl_col: 1706, // Active-low physical HL-column request: _sel_hl_log, or _sel_de_log when ex_dehl_combined
_sel_hl_log: 616, // Active-low logical HL select, before the EX DE,HL swap
_sel_sp_q: 602, // Copy of _sel_sp_s; reg_sel_sp = NOR(clk, this)
_sel_sp_req: 579, // Active-low SP request: sel_sp, or sel_rr with pair field 11
_sel_sp_s: 580, // _sel_sp_req sampled while clk is high
_sel_wz_q: 599, // Active-low WZ select, NOT sel_wz_req; reg_sel_wz = NOR(clk, this)
_szpv_load: 1694, // Active-low request behind szpv_load
_w148: 1281, // Active-low w148: skip from M1 or M2 straight to M4
_wr_ab: 680,
_wz_inhibit: 504, // Low for the PC-handling slots of CALL/RST, DJNZ/JR, IM2 ack and block repeat M4T4
abus_is_one: 691,
abus0: 1776,
abus1: 1886,
abus2: 1914,
abus3: 1992,
abus4: 2018,
abus5: 2088,
abus6: 2118,
abus7: 2187,
abus8: 2225,
abus9: 2301,
abusa: 2343,
abusb: 2422,
abusc: 2449,
abusd: 2534,
abuse: 2572,
abusf: 2660,
aincdec_dec: 436, // Incrementer decrement select
aincdec_dec_m1: 410, // High for DEC rr, PUSH, RST and the acknowledges; its M1T4 term selects decrement for M1T5
aincdec_hold: 406, // Incrementer hold: the 16-bit value passes without +1 (also in every HALT fetch)
aincdec0: 707,
aincdec1: 722,
aincdec2: 744,
aincdec3: 769,
aincdec4: 784,
aincdec5: 804,
aincdec6: 833,
aincdec7: 857,
aincdec8: 879,
aincdec9: 893,
aincdeca: 913,
aincdecb: 929,
aincdecc: 948,
aincdecd: 964,
aincdece: 979,
aincdecf: 994,
alu_a: 793,
alu_and: 826, // ALU logic function AND/BIT/RES: forces the carry nodes
alu_b: 735,
alu_c: 643,
alu_cin: 754, // Carry into bit 0 of the ALU core: XNOR(cf_set_n, hf_latch) = N XOR H; not storage
alu_cin_xa: 2011, // XNOR internal node of alu_cin, pulled low by cf_set_n
alu_cin_xb: 766, // XNOR internal node of alu_cin, pulled low by hf_latch
alu_core_a0: 869, // ALU core bit 0 operand A input
alu_cy_lo: 1807, // Low-nibble carry out, sampled from 3377 while clk is high
alu_cy_lo_n: 698, // NOT alu_cy_lo; source of hf_ld_nib
alu_cy_out: 728, // Carry (add) or borrow (subtract) out of core bit 3: XNOR of NOT alu_cy3_out and NOT cf_set_n
alu_cy_out_n: 727, // NOT alu_cy_out; source of cf_ld_alu
alu_cy3_in: 767, // Carry into ALU core bit 3
alu_cy3_in_n: 1811, // NOT alu_cy3_in
alu_cy3_out: 827, // Carry out of ALU core bit 3
alu_cy3_out_n: 699, // NOT alu_cy3_out
alu_hi_sel: 831, // ALU high-nibble pass select
alu_lo_sel: 747, // ALU low-nibble pass select
alu_or: 828, // ALU logic function OR/SET: one pass per bit
alu_orxor: 825, // ALU logic function OR/XOR/SET: kills the carry chain
alu_res_oe: 646, // Drives the ALU result onto ALUBUS (alulat on bits 0..3, aluout on 4..7)
alu_sample: 684, // Clk-low sample strobe after T1/T3/T5: ALUBUS into the RRD/RLD staging nodes, UBUS into work_cf_n / work_hf_n
alu_zero: 818, // ALU result is zero (Z source)
alu_zero_n: 759, // NOT alu_zero; loaded into zf_latch_n by szpv_load
alua_ld: 604, // Loads alua from ALUBUS (A at every M1T4 clk 0)
alua_preset: 832, // Presets all alua bits high at M1T5 clk 0 of NMI, RST and the IM1 acknowledge
alua_to_alubus: 552, // Drives the complement of the alua latch onto ALUBUS
alua_to_alubus_pre: 1542, // Unclocked OR of the alua readback terms; sampled into alua_to_alubus
alub_and38: 821, // Clears alub bits 0..2 and 6..7 (alub AND 38h); with alub_clr38 clears all bits
alub_bus: 820,
alub_clr38: 822, // Clears alub bits 3..5; with alub_and38 clears all bits
alub_mask_en: 1775, // Enables alub_and38 from M4T2 clk 1 to M4T3 clk 0 of RST and the IM1 acknowledge (not NMI)
alub_to_alubus: 574, // Drives the alub latch onto ALUBUS
aluout3_n: 1944, // NOT aluout3; S source loaded by szpv_load
bc_was_one: 502, // BC was 1 before the decrement (abus_is_one sample)
branch_not_taken: 194, // BRANCH_NOT_TAKEN: a conditional or repeat instruction stops in this M-cycle
bus_650: 650,
cc_ir5: 462, // IR bit 5 masked to 0 for JR cc, for the condition flag and shift fill selects
cell_f0: 1854,
cf_latch_n: 726, // C latch node, NOT C
cf_ld_alu: 1763, // C latch load from the adder carry/borrow: next M1T3, M4T4 of 16-bit adds, M3T3 of the displacement add
cf_ld_nh: 1794, // C latch load of NOT H at the next M1T3 clk 0 (AND, CCF, SCF)
cf_ld_rot: 1930, // C latch load of the rotator bit (pla70 / pla71)
cf_rot_src: 1942, // Rotator carry: UBUS[0] (alu_b, right) or UBUS[7] (alu_c, left)
cf_set_daa: 736, // Sets C = 1 when DAA corrects the high nibble
cf_set_n: 682,
cf_src_1943: 1943,
clear_cf_path: 2066,
clr_hf_path: 2062,
clr_nf_path: 823,
clr_pv_path: 829,
clr_sf_path: 2063,
clr_zf_path: 2061,
const66_cf: 717,
const66_gen: 795,
const66_hi: 1859,
const66_hin: 2064,
const66_lo: 1911,
const66_lon: 2084,
ctl_edx: 1248,
ctl_inc_r: 617, // Increments R register using 16-bit inc/dec
ctl_load_ir_1: 244, // Load IR due to latch
ctl_load_ir_2: 255, // Load IR due to IXY/CB
ctl_m1: 259,
ctl_m2: 140,
ctl_m3: 1228,
ctl_m4: 1225,
ctl_m5: 1221,
ctl_m6: 241,
ctl_set_t1: 1111, // Sets T1 on the next CLK low
ctl_set_t2: 167, // Sets T2 on the next CLK low
ctl_set_t3: 1143, // Sets T3 on the next CLK low
ctl_set_t4: 1315, // Sets T4 on the next CLK low
ctl_set_t5: 1190, // Sets T5 on the next CLK low
ctl_set_t6: 1106, // Sets T6 on the next CLK low
ctl_t1: 92,
ctl_t2: 147,
ctl_t3: 118,
ctl_t4: 1271,
ctl_t5: 1170,
ctl_t6: 89,
ctl_tri: 69,
ctl_tri_ab: 627,
daa_hi_ok: 819,
daa_lo_ok: 762,
db_precharge: 1507,
de_req: 1574, // Logical DE request: _regsel_de, or a field select decoding to D/E/DE
de_req_s: 1665, // de_req sampled while clk is high
decode_inhibit: 1167, // Holds pla_ed_cb high so no base-table PLA row decodes (acknowledge, HALT)
disp_add: 220, // Displacement add in progress (jr, djnz, block I/O, IX+d); holds the IXY_D request
disp_req: 190, // IXY_D latch set request, sampled at M2T2
en_ir: 1800,
en_pc: 1799,
f_wr_pend: 1587, // F write pending: the next M1T4 clk 0 writes F instead of reading it
fetch_normal: 79, // Next instruction is an ordinary fetch: no NMI, no INT, no halt
flag_init_slot: 612, // First clk 0 after M1T4, and M4T4 of IX+d and CB-memory rotates: the flag latch initialisation slot
flag_slot: 558, // Flag slot: M1T3 F write when f_wr_pend, F read otherwise; M4T2 re-read for IX+d and CB-memory ops
flags_to_ubus: 551, // Drives the flag latches onto UBUS for the F write
fld_is_hl: 1647, // Register field is 10x (H, L, HL)
force_ubus0_hi: 1838,
force_ubus0_lo: 1855,
force_ubus3_hi: 2010, // Bit-3 register read driver, pull-up half: NOR(r_u, force_ubus3_lo)
force_ubus3_lo: 1987, // Bit-3 register read driver, pull-down half: NOR(_regbit3, r_u)
force_ubus5_hi: 2108, // Bit-5 register read driver, pull-up half: NOR(r_u, force_ubus5_lo)
force_ubus5_lo: 2083, // Bit-5 register read driver, pull-down half: NOR(_regbit5, r_u)
halt_enter: 97, // HALT decoded and no interrupt taken; sets the HALT latch at m_cycle_start
halt_q: 143, // HALT latch; high = halted
halt_q_n: 1126, // HALT latch, complement of halt_q
halted: 108, // NOT halt_q_n; drives _halt and inhibits decode while halted
hf_clr: 1787, // H latch <- 0 (carry-in 0) at the first clk 0 after M1T4, instructions outside px421 / px423
hf_latch: 711, // H latch output, true H: the carry into the high nibble
hf_latch_n: 697, // H latch dynamic node, NOT H; loaded by hf_clr, hf_set, hf_ld_cf, hf_ld_nib
hf_ld_cf: 1758, // H latch <- old C: ADC, SBC, ADC/SBC HL, CCF, the 16-bit high byte, the displacement high byte
hf_ld_nib: 1793, // H latch <- low-nibble carry at the end of the low-nibble pass
hf_refresh: 1858, // NOT hf_latch; refreshes hf_latch_n through the clk pass t7885
hf_set: 1755, // H latch <- 1 (carry-in 1) for px421: INC/DEC, BIT, CPL, AND, DJNZ, INI/OUTI
hl_req: 593, // Logical HL request not diverted to IX/IY: NOR(_hl_req, px1474)
hl_req_s: 1686, // hl_req sampled while clk is high
iff_restore: 183, // RETN/RETI strobe copying IFF2 into IFF1
iff_wr: 207, // EI/DI IFF write strobe at the next instruction's M1T2 clk 0
iff1: 1210,
iff1_master: 181, // IFF1 dynamic master node
iff2: 1239,
iff2_master: 206, // IFF2 dynamic master node
im1_ack: 229, // IM 1 acknowledge in progress: int_ack, imfa and not imfb, decode inhibited
im1_m1t5: 1545, // M1T5 of the IM 1 acknowledge; turns alu_a off
im2_sel: 1180, // imfa AND imfb: selects the IM 2 acknowledge sequence
imf_wr: 180, // IM n write strobe for IMFA/IMFB at the next instruction's M1T2 clk 0
imfa: 1215,
imfb: 1268,
insn_end: 152, // Last T of the last M-cycle of a non-prefix instruction; feeds m_cycle_start
insn_end_s: 1138, // insn_end held from the clk=1 half
int_accept: 150, // INT accept latch, loaded from int_take by m_cycle_start; high = acknowledge
int_accept_n: 149, // INT accept latch, complement of int_accept
int_ack: 188, // INT acknowledge: clears IFF1/IFF2 and HALT, inhibits decode in IM1/IM2
int_asserted: 1066, // NOT int_in; high when /INT is asserted
int_enable: 231, // NOR(pla97, NOT iff1 master): IFF1 set and no EI/DI being decoded
int_in: 80, // Follows the /INT pin polarity; low when /INT is asserted
int_in_n: 1073, // High when /INT is asserted (first input inverter)
int_mask: 1166, // NOT int_enable: INT mask closed
int_mask_s: 1122, // int_mask held from the clk=1 half; input of int_take
int_reset: 95,
int_samp_m: 94, // INT sampler master: NOT int_in while clk=0, held from the rising edge
int_samp_m_n: 1092, // INT sampler master, complement of int_samp_m
int_samp_s: 1079, // INT sampler slave, loaded while clk=1; high = /INT sampled low
int_samp_s_n: 1095, // INT sampler slave; low = /INT sampled low
int_take: 114, // INT request NOR(nmi_sampled, int_mask_s, int_samp_s_n): /INT sampled, enabled, no NMI
int_take_n: 1110, // NOT int_take; clears the INT accept latch at m_cycle_start
ixiy_is_ix: 591, // IX/IY prefix select: high after DD, low after FD, loaded at M1T3 clk 0 of the prefix
ixiy_is_iy: 1711, // NOT ixiy_is_ix
ixiy_ld: 521, // DD/FD prefix strobe that loads IR bit 5 into the IX/IY prefix select
ixy_d_phase: 210, // Selects (ix+d) addressing
ixy_d_phase_n: 1760, // NOT ixy_d_phase; blocks cf_set_n during the displacement add
ixy_mem_op: 466, // Index prefix latched and HL memory operand: the IX+d instructions
ixy_req: 531, // Index-register request: HL request under px1474, or the IX+d M2T2/M3T3 term
ixy_req_s: 1631, // ixy_req sampled while clk is high
last_m: 101,
last_t: 215,
latch_cb: 1255,
latch_ed: 1259,
latch_ixiy: 1254,
m_cycle_start: 122, // Instruction-boundary strobe at M1T1 clk 0; commits NMI_ACCEPT, the INT accept latch and the HALT latch
m2_addr_reload: 1425, // Low for EX (SP),HL and the IM2 acknowledge: M3 uses the M2 address plus or minus one (no reload at the end of M2)
m2_pc_operand: 483, // High when M2 reads an operand byte at PC
m2_step_req: 1417, // NAND(m2_addr_reload, px432): block instructions, EX (SP),HL/IX and IM2 ack step the M2 address at M2T2
m3_dec_req: 394, // M3T3 decrement request: SP before a CALL push, BC in LDI/CPI
n794: 794,
n3352: 3352,
n3405: 3405,
n3406: 3406,
n3410: 3410,
n3414: 3414,
nf_add: 1575, // NOT nf_sub_class; passed into nf_latch_n by nf_load
nf_latch: 1778, // N (true), buffered from nf_latch_n
nf_latch_n: 696, // N latch node, NOT N; loaded by nf_load, ubus_to_flags and 1592
nf_ld_ubus7: 1592, // Loads UBUS[7] into the N latch (block I/O: N = bit 7 of the byte)
nf_load: 1812, // Loads the subtract class into the N flag node 696 at the next M1T1 clk 0
nf_q_n: 1789, // NAND(nf_to_alu_en, nf_latch): NOT N while nf_to_alu_en is high
nf_sub_class: 460, // Subtract class (SUB, SBC, CP, NEG, DEC, CPL, DJNZ, CPI/INI/OUTI); the N flag source
nf_to_alu_en: 1761, // Enables N into the ALU (nf_q_n); low forces nf_q_n high
nmi_accept_q: 1171, // NMI_ACCEPT commit latch Q (HIGH = NMI acknowledged, drives nmi_ack)
nmi_accept_qbar: 148, // NMI_ACCEPT commit latch Qbar (complement of nmi_accept_q)
nmi_ack: 135, // NMI Acknowledge
nmi_asserted: 59, // Final combinational /NMI after 3 input inverters (HIGH = /NMI active)
nmi_d: 116, // D input of the NMI_Q1 clk-gated D-flop
nmi_in: 60, // Buffered /NMI after second input inverter
nmi_in_n: 52, // Inverted /NMI after first input inverter
nmi_pending_q: 68, // NMI_PENDING SR-latch Q (captures NMI edge until committed)
nmi_pending_qbar: 56, // NMI_PENDING SR-latch Qbar (complement of nmi_pending_q)
nmi_q1: 1101, // NMI_Q1 clk-gated D-flop dynamic storage node
nmi_samp_m: 57, // NMI sampler master: follows NMI_PENDING while clk=0, holds while clk=1
nmi_samp_m_n: 67, // NMI sampler master, complement of nmi_samp_m
nmi_samp_s: 73, // NMI sampler slave, loaded from the master while clk=1; high = NMI sampled
nmi_samp_s_n: 70, // NMI sampler slave, complement of nmi_samp_s
nmi_sampled: 106, // NMI sampled at the last rising clk edge; committed into NMI_ACCEPT by m_cycle_start
old_cf: 1732, // C, clocked copy of the C latch
old_cf_n: 635, // NOT C from the C latch; into the H latch (ADC/SBC) and the shifter fill (RL/RR)
old_cf_n_m: 758, // NOT set_cf_path
old_cf_n_s: 1782, // NOT C sampled from old_cf_n_m while clk is high
pair_xfer: 488, // NOT _pair_xfer: enables the half-by-half pair writes and the stack-write reads
parity_odd: 700, // Parity of the ALU result, high for an odd number of set bits
pla_ab: 192,
pla_cb: 263,
pla_ed: 265,
pla_ed_cb: 267,
pla_ir3n: 449,
pla_ir4n: 1446,
pla22b: 1294,
pla22c: 1253,
pla22d: 193, // IX/IY+CB
pla39_m1_stk: 3058, // Series-stack node between the pla39 pulldown and the m1 pass (t1 & m1 & pla39)
pla83_s: 1416, // pla83 (LD A,I / LD A,R and IFF2) held from the clk=1 half
pla98_m2_stk: 3049, // Series-stack node between the pla98 pulldown and the m2 pass (t1 & m2 & pla98)
pv_bc_sel: 458, // LDI and CPI families: selects BC != 0 as P/V at M3T5 clk 0
pv_iff2: 1423, // Buffered pla83_s; pulls _alu_ovf low so P/V takes IFF2 for LD A,I / LD A,R
pv_latch_n: 647, // P/V latch node, NOT P/V
pv_load_bc: 1757, // Loads BC != 0 into the P/V latch for the LDI and CPI families
pv_load_part: 636, // Loads the P/V latch with parity; again at M4T5 clk 0 of a repeating block I/O iteration
pv_sel_ovf: 1460, // P/V mux select: overflow (_alu_ovf) instead of parity
pv_sel_par: 1805, // P/V mux select: parity
pv_src: 3356, // P/V mux output, loaded into pv_carrier_647
pvz_preset: 637, // Presets P/V = 1 and Z = 1 at the first clk 0 after M1T4 for the px415 rows
px10n: 1355,
px61n: 1424,
px62n: 1623,
px63n: 156,
px63nn: 164,
px77n: 1496,
px95n: 1099,
px202: 202,
px202bn: 1464,
px202n: 203,
px232: 232,
px238: 238,
px238n: 160,
px239: 239,
px242: 242,
px243: 243,
px251: 251,
px253: 253,
px253n: 217,
px330: 330,
px360: 360,
px377: 377,
px377n: 213,
px387: 387,
px395: 395,
px398: 398,
px400: 400,
px402: 402,
px403: 403,
px403n: 1456,
px404: 404,
px404n: 1485,
px409: 409,
px409n: 1457,
px411: 411,
px413: 413,
px415: 415,
px415n: 1486,
px417: 417,
px417n: 1463,
px420: 420,
px421: 421,
px421n: 1461,
px422: 422,
px422n: 401,
px423: 423,
px423n: 1459,
px424: 424,
px425: 425,
px425n: 1473,
px426: 426,
px427: 427,
px427n: 468,
px428: 428,
px428n: 1447,
px429: 429,
px430: 430,
px430n: 459,
px431: 431,
px432: 432,
px432n: 262,
px433: 433,
px433n: 455,
px435: 435,
px435n: 1489,
px437: 437,
px438: 438,
px439: 439,
px442: 442,
px442n: 464,
px443: 443,
px444: 444,
px444n: 1483,
px445: 445,
px445n: 478,
px446: 446,
px448: 448,
px467: 467,
px467n: 1421,
px470: 470,
px1325: 1325,
px1399: 1399,
px1406: 1406,
px1429: 1429,
px1429n: 1488,
px1444: 1444,
px1444n: 391,
px1454: 1454,
px1454n: 463,
px1467: 1467,
px1467n: 1497,
px1474: 1474,
px1490: 1490,
px1494: 1494,
px3039: 3039,
reg_fld_src: 450, // Register field select: IR[2:0] when high, IR[5:3] when low
reg_sel_af: 677,
reg_sel_af': 674,
reg_sel_bc: 673,
reg_sel_bc': 670,
reg_sel_de: 669,
reg_sel_de': 666,
reg_sel_exx: 634,
reg_sel_exx': 640,
reg_sel_hl: 665,
reg_sel_hl': 662,
reg_sel_ix: 661,
reg_sel_iy: 658,
reg_sel_sp: 657,
reg_sel_wz: 654,
rfield_lo: 1630, // Selected register field is C, E or L
rfield_wr: 585, // M1T2 register-field write slot, or M1T4 of LD I,A / LD R,A
rh_wr_mcyc: 1586, // High-byte write terms (M1T2 A write-back, M3/M5 16-bit loads)
rl_wr_mcyc: 532, // Low-byte write terms in M2..M4 (LD rr,nn, POP, interrupt)
rot_thru_c: 1752, // IR4 and not IR5: RL / RR / RLA / RRA; routes old_cf_n into the shifter fill
rr_is_sp: 1644, // Register pair field is 11 (SP)
rst_not_nmi: 310, // seq_rst and not the NMI acknowledge
rxd_nibble_ld: 740, // RRD/RLD nibble load into alub and alua at M3T3 clk 0 and M4T1 clk 0
sel_bc: 564, // BC pair request
sel_de_col: 1714, // NOT _sel_de_col; with reg_sel_exx / reg_sel_exx' selects DE or DE'
sel_fld: 1637, // A field-decoded select (sel_rr, _regsel_qq or _regsel_r) is active
sel_hl: 1579, // HL (IX/IY) select: OR of _regsel_hl_506 and _regsel_hl_507
sel_hl_col: 1712, // NOT _sel_hl_col; with reg_sel_exx / reg_sel_exx' selects HL or HL'
sel_ixy: 630, // Index-register select request; with ixiy_is_ix picks IX or IY
sel_other: 1583, // Some pair request other than WZ is active; sampled into sel_other_s
sel_other_s: 1598, // sel_other sampled while clk is high
sel_rr: 592, // IR[5:4] register pair select: OR of _regsel_rr_510 and _regsel_rr_1541
sel_sp: 536, // SP select, from _regsel_sp
sel_wz_req: 1578, // No other pair request pending and no I/R select: WZ is the default pair
seq_im2: 270, // Pseudo PLA row: the IM2 acknowledge sequence
seq_rst: 228, // Pseudo PLA row: RST p and the NMI, IM0, IM1 acknowledge sequences
set_cf_path: 764,
set_hf_path: 765, // H (true): H output pull-down on 3406 and the C = NOT H source of cf_ld_nh
set_nf_path: 757,
set_pv_path: 686,
set_sf_path: 768,
set_t1: 162,
set_zf_path: 515,
sf_latch_n: 729, // S latch node, NOT S; loaded by ubus_to_flags and szpv_load
sf_refresh: 1959, // NOT set_sf_path; refreshes sf_latch_n through the clk pass t8497
single_m: 233, // SINGLE_M: the decoded instruction ends after M1
szpv_load: 712, // Loads S, Z and P/V from the ALU at M1T3 clk 0 (and block-instruction slots), gated by px415n
ubus_to_flags: 575, // Loads the flag latches from F on UBUS at M1T4 clk 0
ubus3_n: 761, // NOT ubus3; bit-3 low-byte write-source drive
ubus3_nn: 1991, // NOT ubus3_n; bit-3 low-byte write-source drive
ubus5_n: 801, // NOT ubus5; bit-5 low-byte write-source drive
ubus5_nn: 2087, // NOT ubus5_n; bit-5 low-byte write-source drive
wait_ff: 1161, // WAIT latch; 0 = insert TW, sampled at the falling edge ending T2 or TW clk 1
wait_gate_in: 1165, // NAND(wait_in, wait_t_advance); high sets a TW request into wait_ff while clk=1
wait_in: 54, // Buffered /WAIT; low when /WAIT is asserted
wait_in_n: 53, // High when /WAIT is asserted
wait_t_advance: 141, // T-state advance request; drops in I/O T2 to insert the automatic TW
work_cf_n: 1809, // Working CF shadow (active low), sampled from UBUS[0] by gate 684; read only by the DAA mask chain (t4103)
work_hf_n: 1864, // Working HF input latch (active-low); sampled from UBUS[4] via gate 684; read by DAA
wr_ab: 393,
wsrc0_inv_pre: 3412,
zf_latch: 1839, // NOT zf_latch_n: Z
zf_latch_n: 701, // Z latch node, NOT Z; preset by pvz_preset, loaded by szpv_load and ubus_to_flags
zf_refresh: 737, // NOT zf_latch; drives set_zf_path and refreshes zf_latch_n through the clk pass t7862
// Buses:
AB: [5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],
ABUS: [1776,1886,1914,1992,2018,2088,2118,2187,2225,2301,2343,2422,2449,2534,2572,2660],
AINCDEC: [707,722,744,769,784,804,833,857,879,893,913,929,948,964,979,994],
ALUBUS: [837,889,937,983,852,903,951,995],
DB: [32,33,34,35,36,37,38,39],
DBUS: [138,196,412,480,485,486,380,370],
IM: [1215,1268],
INSTR: [1348,1359,1365,1379,1387,1394,1369,1374],
M: [155,173,163,159,209],
PCBUS: [703,731,739,774,777,806,810,863,871,901,907,933,936,969,974,998],
REGBIT: [702,732,738,775,776,807,809,864,870,902,906,934,935,970,973,999],
REG_A: [2245,2319,2357,2442,2463,2552,2586,2656],
REG_A': [2244,2318,2356,2441,2462,2551,2585,2655],
REG_AF: [1827,1903,1928,2009,2032,2107,2132,2209,2245,2319,2357,2442,2463,2552,2586,2656],
REG_AF': [1826,1902,1927,2008,2031,2106,2131,2208,2244,2318,2356,2441,2462,2551,2585,2655],
REG_B: [2243,2317,2355,2440,2461,2550,2584,2654],
REG_B': [2242,2316,2354,2439,2460,2549,2583,2653],
REG_BC: [1825,1901,1926,2007,2030,2105,2130,2207,2243,2317,2355,2440,2461,2550,2584,2654],
REG_BC': [1824,1900,1925,2006,2029,2104,2129,2206,2242,2316,2354,2439,2460,2549,2583,2653],
REG_C: [1825,1901,1926,2007,2030,2105,2130,2207],
REG_C': [1824,1900,1925,2006,2029,2104,2129,2206],
REG_D: [2241,2315,2353,2438,2459,2548,2582,2652],
REG_D': [2240,2314,2352,2437,2458,2547,2581,2651],
REG_DE: [1823,1899,1924,2005,2028,2103,2128,2205,2241,2315,2353,2438,2459,2548,2582,2652],
REG_DE': [1822,1898,1923,2004,2027,2102,2127,2204,2240,2314,2352,2437,2458,2547,2581,2651],
REG_E: [1823,1899,1924,2005,2028,2103,2128,2205],
REG_E': [1822,1898,1923,2004,2027,2102,2127,2204],
REG_F: [1827,1903,1928,2009,2032,2107,2132,2209],
REG_F': [1826,1902,1927,2008,2031,2106,2131,2208],
REG_H: [2239,2313,2351,2436,2457,2546,2580,2650],
REG_H': [2238,2312,2350,2435,2456,2545,2579,2649],
REG_HL: [1821,1897,1922,2003,2026,2101,2126,2203,2239,2313,2351,2436,2457,2546,2580,2650],
REG_HL': [1820,1896,1921,2002,2025,2100,2125,2202,2238,2312,2350,2435,2456,2545,2579,2649],
REG_I: [2233,2307,2345,2430,2451,2540,2574,2644],
REG_IR: [1815,1891,1916,1997,2020,2095,2120,2197,2233,2307,2345,2430,2451,2540,2574,2644],
REG_IX: [1819,1895,1920,2001,2024,2099,2124,2201,2237,2311,2349,2434,2455,2544,2578,2648],
REG_IY: [1818,1894,1919,2000,2023,2098,2123,2200,2236,2310,2348,2433,2454,2543,2577,2647],
REG_L: [1821,1897,1922,2003,2026,2101,2126,2203],
REG_L': [1820,1896,1921,2002,2025,2100,2125,2202],
REG_PC: [1814,1890,1915,1996,2019,2094,2119,2196,2232,2306,2344,2429,2450,2539,2573,2643],
REG_R: [1815,1891,1916,1997,2020,2095,2120,2197],
REG_SP: [1817,1893,1918,1999,2022,2097,2122,2199,2235,2309,2347,2432,2453,2542,2576,2646],
REG_WZ: [1816,1892,1917,1998,2021,2096,2121,2198,2234,2308,2346,2431,2452,2541,2575,2645],
T: [115,137,144,166,134,168],
UBUS: [545,528,526,770,779,790,716,525],
VBUS: [755,772,783,796,803,808,836,839],
_REGBIT: [708,715,745,753,785,799,834,847,880,886,914,923,949,959,980,985],
}
