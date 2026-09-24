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
_alu_ovf: 683, // Active-low overflow: XNOR of the carries into and out of ALU bit 3 (bit 7 in the high pass)
_iff2: 1278, // Inverted IFF2 node; gates the extra pulldown t4049 on pla83
_last_m: 184, // Complement of last_m (Ken's LAST_M_CYCLE/)
_last_t: 110,
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
_w148: 1281, // Active-low w148: skip from M1 or M2 straight to M4
_wr_ab: 680,
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
alu_hi_sel: 831, // ALU high-nibble pass select
alu_lo_sel: 747, // ALU low-nibble pass select
alu_or: 828, // ALU logic function OR/SET: one pass per bit
alu_orxor: 825, // ALU logic function OR/XOR/SET: kills the carry chain
alu_res_oe: 646, // Drives the ALU result onto ALUBUS (alulat on bits 0..3, aluout on 4..7)
alu_zero: 818, // ALU result is zero (Z source)
alua_ld: 604, // Loads alua from ALUBUS (A at every M1T4 clk 0)
alub_bus: 820,
branch_not_taken: 194, // BRANCH_NOT_TAKEN: a conditional or repeat instruction stops in this M-cycle
bus_650: 650,
cell_f0: 1854,
cf_isolate_n: 711,
cf_master_n: 2011,
cf_pass_gate: 766,
cf_release: 697,
cf_sel_736: 736,
cf_sel_1763: 1763,
cf_sel_1794: 1794,
cf_sel_1930: 1930,
cf_set_n: 682,
cf_src_727: 727,
cf_src_765: 765,
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
decode_inhibit: 1167, // Holds pla_ed_cb high so no base-table PLA row decodes (acknowledge, HALT)
disp_add: 220, // Displacement add in progress (jr, djnz, block I/O, IX+d); holds the IXY_D request
disp_req: 190, // IXY_D latch set request, sampled at M2T2
en_ir: 1800,
en_pc: 1799,
f_wr_pend: 1587, // F write pending: the next M1T4 clk 0 writes F instead of reading it
fetch_normal: 79, // Next instruction is an ordinary fetch: no NMI, no INT, no halt
flag_cf: 754, // Carry Flag state
flags_to_ubus: 551, // Drives the flag latches onto UBUS for the F write
force_ubus0_hi: 1838,
force_ubus0_lo: 1855,
halt_enter: 97, // HALT decoded and no interrupt taken; sets the HALT latch at m_cycle_start
halt_q: 143, // HALT latch; high = halted
halt_q_n: 1126, // HALT latch, complement of halt_q
halted: 108, // NOT halt_q_n; drives _halt and inhibits decode while halted
hf_carrier_1858: 1858,
hf_inA: 767,
hf_inB: 827,
hf_slaveA: 1811,
hf_slaveB: 699,
iff_restore: 183, // RETN/RETI strobe copying IFF2 into IFF1
iff_wr: 207, // EI/DI IFF write strobe at the next instruction's M1T2 clk 0
iff1: 1210,
iff1_master: 181, // IFF1 dynamic master node
iff2: 1239,
iff2_master: 206, // IFF2 dynamic master node
im2_sel: 1180, // imfa AND imfb: selects the IM 2 acknowledge sequence
imf_wr: 180, // IM n write strobe for IMFA/IMFB at the next instruction's M1T2 clk 0
imfa: 1215,
imfb: 1268,
insn_end: 152, // Last T of the last M-cycle of a non-prefix instruction; feeds m_cycle_start
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
ixy_d_phase: 210, // Selects (ix+d) addressing
ixy_d_phase_n: 1760, // NOT ixy_d_phase; blocks cf_set_n during the displacement add
last_m: 101,
last_t: 215,
latch_cb: 1255,
latch_ed: 1259,
latch_ixiy: 1254,
m_cycle_start: 122, // Instruction-boundary strobe at M1T1 clk 0; commits NMI_ACCEPT, the INT accept latch and the HALT latch
n_carrier_1789: 1789,
n726: 726,
n794: 794,
n1942: 1942,
n3352: 3352,
n3405: 3405,
n3406: 3406,
n3410: 3410,
n3414: 3414,
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
pla98_m2_stk: 3049, // Series-stack node between the pla98 pulldown and the m2 pass (t1 & m2 & pla98)
pv_carrier_647: 647,
pv_sel_ovf: 1460, // P/V mux select: overflow (_alu_ovf) instead of parity
pv_src: 3356, // P/V mux output, loaded into pv_carrier_647
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
px405: 405,
px409: 409,
px409n: 1457,
px411: 411,
px413: 413,
px415: 415,
px415n: 1486,
px416: 416,
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
px441: 441,
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
seq_im2: 270, // Pseudo PLA row: the IM2 acknowledge sequence
seq_rst: 228, // Pseudo PLA row: RST p and the NMI, IM0, IM1 acknowledge sequences
set_cf_path: 764,
set_nf_path: 757,
set_pv_path: 686,
set_sf_path: 768,
set_t1: 162,
set_zf_path: 515,
sf_carrier_729: 729,
single_m: 233, // SINGLE_M: the decoded instruction ends after M1
ubus_to_flags: 575, // Loads the flag latches from F on UBUS at M1T4 clk 0
wait_ff: 1161, // WAIT latch; 0 = insert TW, sampled at the falling edge ending T2 or TW clk 1
wait_gate_in: 1165, // NAND(wait_in, wait_t_advance); high sets a TW request into wait_ff while clk=1
wait_in: 54, // Buffered /WAIT; low when /WAIT is asserted
wait_in_n: 53, // High when /WAIT is asserted
wait_t_advance: 141, // T-state advance request; drops in I/O T2 to insert the automatic TW
work_cf_n: 1809, // Working CF input latch (active-low); sampled from UBUS[0] via gate 684; read by ADC/SBC/RLA/RRA/DAA
work_hf_n: 1864, // Working HF input latch (active-low); sampled from UBUS[4] via gate 684; read by DAA
wr_ab: 393,
wsrc0_inv_pre: 3412,
x_force_hi_2010: 2010,
x_force_lo_1987: 1987,
y_force_hi_2108: 2108,
y_force_lo_2083: 2083,
zf_carrier_737: 737,
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
