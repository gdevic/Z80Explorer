// This file contains fixed known set of net names.
//
// The application will read, but never modify or write to this file, so it is safe to
// edit by hand and maintain comments. That also means the app will not be able to persist
// any changes to the names on this list, since they will simply be reloaded on the next app run.
//
// Names on this list may be overriden by "netnames.js" (which *will* be written back by the app)
// This file contains only net names, not bus names. Define bus names in "netnames.js" file.
//
var nodenames ={
// Pads
vss: 1, // GND pad
vcc: 2, // +5V pad
clk: 3, // CLK pad
ab0: 5, // Address bu pads
ab1: 6,
ab2: 7,
ab3: 8,
ab4: 9,
ab5: 10,
ab6: 11,
ab7: 12,
ab8: 13,
ab9: 14,
ab10: 15,
ab11: 16,
ab12: 17,
ab13: 18,
ab14: 19,
ab15: 20,
db0: 32, // Data bus pads
db1: 33,
db2: 34,
db3: 35,
db4: 36,
db5: 37,
db6: 38,
db7: 39,
_reset: 21,
_wait: 22,
_int: 23,
_nmi: 24,
_busrq: 25,
_m1: 26,
_rd: 27,
_wr: 28,
_mreq: 29,
_iorq: 30,
_rfsh: 31,
_halt: 40,
_busak: 41,
// T-States
t1: 115,
t2: 137,
t3: 144,
t4: 166,
t5: 134,
t6: 168,
// Machine cycles
m1: 155,
m2: 173,
m3: 163,
m4: 159,
m5: 209,
m6: 210,
// EXX latches
ex_af: 631,
ex_bcdehl: 1770,
ex_dehl0: 625,
ex_dehl1: 629,
ex_dehl_combined: 626,
// Registers
reg_a0: 2245,
reg_a1: 2319,
reg_a2: 2357,
reg_a3: 2442,
reg_a4: 2463,
reg_a5: 2552,
reg_a6: 2586,
reg_a7: 2656,
reg_f0: 1827,
reg_f1: 1903,
reg_f2: 1928,
reg_f3: 2009,
reg_f4: 2032,
reg_f5: 2107,
reg_f6: 2132,
reg_f7: 2209,
reg_b0: 2243,
reg_b1: 2317,
reg_b2: 2355,
reg_b3: 2440,
reg_b4: 2461,
reg_b5: 2550,
reg_b6: 2584,
reg_b7: 2654,
reg_c0: 1825,
reg_c1: 1901,
reg_c2: 1926,
reg_c3: 2007,
reg_c4: 2030,
reg_c5: 2105,
reg_c6: 2130,
reg_c7: 2207,
reg_d0: 2241,
reg_d1: 2315,
reg_d2: 2353,
reg_d3: 2438,
reg_d4: 2459,
reg_d5: 2548,
reg_d6: 2582,
reg_d7: 2652,
reg_e0: 1823,
reg_e1: 1899,
reg_e2: 1924,
reg_e3: 2005,
reg_e4: 2028,
reg_e5: 2103,
reg_e6: 2128,
reg_e7: 2205,
reg_h0: 2239,
reg_h1: 2313,
reg_h2: 2351,
reg_h3: 2436,
reg_h4: 2457,
reg_h5: 2546,
reg_h6: 2580,
reg_h7: 2650,
reg_l0: 1821,
reg_l1: 1897,
reg_l2: 1922,
reg_l3: 2003,
reg_l4: 2026,
reg_l5: 2101,
reg_l6: 2126,
reg_l7: 2203,
reg_w0: 2234,
reg_w1: 2308,
reg_w2: 2346,
reg_w3: 2431,
reg_w4: 2452,
reg_w5: 2541,
reg_w6: 2575,
reg_w7: 2645,
reg_z0: 1816,
reg_z1: 1892,
reg_z2: 1917,
reg_z3: 1998,
reg_z4: 2021,
reg_z5: 2096,
reg_z6: 2121,
reg_z7: 2198,
reg_pch0: 2232,
reg_pch1: 2306,
reg_pch2: 2344,
reg_pch3: 2429,
reg_pch4: 2450,
reg_pch5: 2539,
reg_pch6: 2573,
reg_pch7: 2643,
reg_pcl0: 1814,
reg_pcl1: 1890,
reg_pcl2: 1915,
reg_pcl3: 1996,
reg_pcl4: 2019,
reg_pcl5: 2094,
reg_pcl6: 2119,
reg_pcl7: 2196,
reg_sph0: 2235,
reg_sph1: 2309,
reg_sph2: 2347,
reg_sph3: 2432,
reg_sph4: 2453,
reg_sph5: 2542,
reg_sph6: 2576,
reg_sph7: 2646,
reg_spl0: 1817,
reg_spl1: 1893,
reg_spl2: 1918,
reg_spl3: 1999,
reg_spl4: 2022,
reg_spl5: 2097,
reg_spl6: 2122,
reg_spl7: 2199,
reg_ixh0: 2237,
reg_ixh1: 2311,
reg_ixh2: 2349,
reg_ixh3: 2434,
reg_ixh4: 2455,
reg_ixh5: 2544,
reg_ixh6: 2578,
reg_ixh7: 2648,
reg_ixl0: 1819,
reg_ixl1: 1895,
reg_ixl2: 1920,
reg_ixl3: 2001,
reg_ixl4: 2024,
reg_ixl5: 2099,
reg_ixl6: 2124,
reg_ixl7: 2201,
reg_iyh0: 2236,
reg_iyh1: 2310,
reg_iyh2: 2348,
reg_iyh3: 2433,
reg_iyh4: 2454,
reg_iyh5: 2543,
reg_iyh6: 2577,
reg_iyh7: 2647,
reg_iyl0: 1818,
reg_iyl1: 1894,
reg_iyl2: 1919,
reg_iyl3: 2000,
reg_iyl4: 2023,
reg_iyl5: 2098,
reg_iyl6: 2123,
reg_iyl7: 2200,
reg_i0: 2233,
reg_i1: 2307,
reg_i2: 2345,
reg_i3: 2430,
reg_i4: 2451,
reg_i5: 2540,
reg_i6: 2574,
reg_i7: 2644,
reg_r0: 1815,
reg_r1: 1891,
reg_r2: 1916,
reg_r3: 1997,
reg_r4: 2020,
reg_r5: 2095,
reg_r6: 2120,
reg_r7: 2197,
reg_aa0: 2244,
reg_aa1: 2318,
reg_aa2: 2356,
reg_aa3: 2441,
reg_aa4: 2462,
reg_aa5: 2551,
reg_aa6: 2585,
reg_aa7: 2655,
reg_ff0: 1826,
reg_ff1: 1902,
reg_ff2: 1927,
reg_ff3: 2008,
reg_ff4: 2031,
reg_ff5: 2106,
reg_ff6: 2131,
reg_ff7: 2208,
reg_bb0: 2242,
reg_bb1: 2316,
reg_bb2: 2354,
reg_bb3: 2439,
reg_bb4: 2460,
reg_bb5: 2549,
reg_bb6: 2583,
reg_bb7: 2653,
reg_cc0: 1824,
reg_cc1: 1900,
reg_cc2: 1925,
reg_cc3: 2006,
reg_cc4: 2029,
reg_cc5: 2104,
reg_cc6: 2129,
reg_cc7: 2206,
reg_dd0: 2240,
reg_dd1: 2314,
reg_dd2: 2352,
reg_dd3: 2437,
reg_dd4: 2458,
reg_dd5: 2547,
reg_dd6: 2581,
reg_dd7: 2651,
reg_ee0: 1822,
reg_ee1: 1898,
reg_ee2: 1923,
reg_ee3: 2004,
reg_ee4: 2027,
reg_ee5: 2102,
reg_ee6: 2127,
reg_ee7: 2204,
reg_hh0: 2238,
reg_hh1: 2312,
reg_hh2: 2350,
reg_hh3: 2435,
reg_hh4: 2456,
reg_hh5: 2545,
reg_hh6: 2579,
reg_hh7: 2649,
reg_ll0: 1820,
reg_ll1: 1896,
reg_ll2: 1921,
reg_ll3: 2002,
reg_ll4: 2025,
reg_ll5: 2100,
reg_ll6: 2125,
reg_ll7: 2202,
// Data buses and control
dp_dl: 82,
dl_dp: 165,
load_ir: 1354,
_load_ir: 326,
dlatch0: 123,
dlatch1: 195,
dlatch2: 414,
dlatch3: 930,
dlatch4: 1000,
dlatch5: 872,
dlatch6: 751,
dlatch7: 358,
dl_d: 87,
d_dl: 133,
dbus0: 138,
dbus1: 196,
dbus2: 412,
dbus3: 480,
dbus4: 485,
dbus5: 486,
dbus6: 380,
dbus7: 370,
instr0: 1348,
instr1: 1359,
instr2: 1365,
instr3: 1379,
instr4: 1387,
instr5: 1394,
instr6: 1369,
instr7: 1374,
d_u: 546,
ubus0: 545,
ubus1: 528,
ubus2: 526,
ubus3: 770,
ubus4: 779,
ubus5: 790,
ubus6: 716,
ubus7: 525,
u_v: 750,
vbus0: 755,
vbus1: 772,
vbus2: 783,
vbus3: 796,
vbus4: 803,
vbus5: 808,
vbus6: 836,
vbus7: 839,
rl_wr: 678,
rh_wr: 652,
r_u: 692,
r_v: 693,
regbit0: 702,
regbit1: 732,
regbit2: 738,
regbit3: 775,
regbit4: 776,
regbit5: 807,
regbit6: 809,
regbit7: 864,
regbit8: 870,
regbit9: 902,
regbit10: 906,
regbit11: 934,
regbit12: 935,
regbit13: 970,
regbit14: 973,
regbit15: 999,
r_p: 1785,
r_x1: 608,
pcbit0: 703,
pcbit1: 731,
pcbit2: 739,
pcbit3: 774,
pcbit4: 777,
pcbit5: 806,
pcbit6: 810,
pcbit7: 863,
pcbit8: 871,
pcbit9: 901,
pcbit10: 907,
pcbit11: 933,
pcbit12: 936,
pcbit13: 969,
pcbit14: 974,
pcbit15: 998,
// ALU
alubus0: 837,
alubus1: 889,
alubus2: 937,
alubus3: 983,
alubus4: 852,
alubus5: 903,
alubus6: 951,
alubus7: 995,
alua0: 850,
alua1: 899,
alua2: 947,
alua3: 993,
alua4: 868,
alua5: 920,
alua6: 968,
alua7: 1007,
alub0: 845,
alub1: 897,
alub2: 944,
alub3: 988,
alub4: 867,
alub5: 918,
alub6: 966,
alub7: 1005,
aluout0: 2211,
aluout1: 2338,
aluout2: 2504,
aluout3: 816,
alulat0: 865,
alulat1: 912,
alulat2: 960,
alulat3: 1002,
// Internal PLA nets to instruction register bits, positive and inverted signals
pla_ir0: 1347,
pla_ir1: 1358,
pla_ir2: 1364,
pla_ir3: 248,
pla_ir4: 247,
pla_ir5: 1390,
pla_ir6: 1368,
pla_ir7: 1373,
_pla_ir0: 374,
_pla_ir1: 375,
_pla_ir2: 378,
_pla_ir3: 385,
_pla_ir4: 1386,
_pla_ir5: 1393,
_pla_ir6: 1371,
_pla_ir7: 1377,
// PLA output signals
pla0: 287,  // ldx/cpx/inx/outx brk
pla1: 332,  // exx
pla2: 314,  // ex de,hl
pla3: 333,  // IX/IY prefix
pla4: 315,  // ld x,a/a,x
pla5: 334,  // ld sp,hl
pla6: 316,  // jp (hl)
pla7: 335,  // ld rr,nn
pla8: 317,  // ld (rr),a/a,(rr)
pla9: 336,  // inc/dec rr
pla10: 318, // ex (sp),hl
pla11: 361, // cpi/cpir/cpd/cpdr
pla12: 261, // ldi/ldir/ldd/lddr
pla13: 337, // ld direction: ld (rr),a/ld(**),a/ld(**),hl
pla14: 319, // dec rr
pla15: 363, // rrd/rld
pla16: 288, // push rr
pla17: 338, // ld r,n
pla18: 320, // ldi/ldir/ldd/lddr
pla19: 364, // cpi/cpir/cpd/cpdr
pla20: 325, // outx/otxr
pla21: 324, // inx/inxr
pla22: 308, // CB prefix w/o IX/IY
pla23: 289, // push/pop
pla24: 339, // call nn
pla25: 313, // rlca/rla/rrca/rra
pla26: 340, // djnz e
pla27: 290, // in/out r,(c)
pla28: 341, // out (n),a
pla29: 291, // jp nn
pla30: 342, // ld hl,(nn)/(nn),hl
pla31: 292, // ld rr,(nn)/(nn),rr
pla32: 365, // ld i,a/a,i/r,a/a,r
pla33: 293, // ld direction: ld (**),rr
pla34: 362, // out (c),r
pla35: 294, // ret
pla36: 331, // ld(rr),a/a,(rr)
pla37: 295, // out (n),a/a,(n)
pla38: 343, // ld (nn),a/a,(nn)
pla39: 296, // ex af,af'
pla40: 297, // ld (ix+d),n
pla41: 298, // IX/IY
pla42: 344, // call cc,nn
pla43: 299, // jp cc,nn
pla44: 269, // CB prefix
pla45: 300, // ret cc
pla46: 237, // reti/retn
pla47: 301, // jr e
pla48: 345, // jr ss,e
pla49: 302, // CB prefix with IX/IY
pla50: 346, // ld (hl),n
pla51: 264, // ED prefix
pla52: 266, // add/sub/and/or/xor/cp (hl)
pla53: 347, // inc/dec (hl)
pla54: 303, // Every CB with IX/IY
pla55: 356, // Every CB op (hl)
pla56: 227, // rst p
pla57: 366, // ld i,a/r,a
pla58: 304, // ld r,(hl)
pla59: 305, // ld (hl),r
pla60: 271, // rrd/rld
pla61: 348, // ld r,r'
pla62: 306, // For all CB opcodes
pla63: 309, // ld r,*
pla64: 311, // add/sub/and/or/xor/cmp a,imm
pla65: 312, // add/sub/and/or/xor/cmp a,r
pla66: 307, // inc/dec r
pla67: 367, // in
pla68: 272, // adc/sbc hl,rr
pla69: 349, // add hl,rr
pla70: 273, // rlc r
pla71: 350, // rlca/rla/rrca/rra
pla72: 274, // bit b,r
pla73: 351, // res b,r
pla74: 275, // set b,r
pla75: 276, // dec r
pla76: 268, // 111 (CP)
pla77: 352, // daa
pla78: 277, // 010 (SUB)
pla79: 278, // 011 (SBC)
pla80: 279, // 001 (ADC)
pla81: 280, // cpl
pla82: 368, // neg
pla83: 281, // ld a,i/a,r
pla84: 282, // 000 (ADD)
pla85: 283, // 100 (AND)
pla86: 284, // 110 (OR)
pla87: 285, // ld a,i / ld a,r
pla88: 286, // 101 (XOR)
pla89: 321, // ccf
pla90: 353, // djnz *
pla91: 322, // inx/outx/inxr/otxr
pla92: 354, // scf
pla93: 323, // cpi/cpir/cpd/cpdr
pla94: 369, // ldi/ldir/ldd/lddr
pla95: 258, // halt
pla96: 249, // im n
pla97: 245, // di/ei
pla98: 355, // out (*),a/in a,(*)
}
