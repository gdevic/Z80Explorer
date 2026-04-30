;==============================================================================
; test_every_pla_bucket.asm
;
; Executes one instruction from every PLA decoder bucket of the Z80, so the
; simulator exercises every decode row at least once in a single run. Bucket
; numbering matches P:\Z80\How-Z80-Ticks\data\timing_map.json:
;   B1..B80   base buckets (unprefixed + CB + ED)
;   B101..B126 IX buckets (DD + DDCB, plus undocumented IXh group)
; 106 buckets total. Trailing comments tag the bucket each opcode belongs to.
;
; Grouping is by category (register ALU -> memory -> block ops -> control
; flow -> IX), not strict bucket-id order. Each bucket is still hit at
; least once.
;
; Notes on coverage:
;   * Prefix-byte buckets are hit collaterally by every prefixed instruction:
;     B70 CB prefix, B75 DD prefix, B79 ED prefix.
;   * B21 RETN is hit via the NMI handler at 0x0066.
;   * B12 RST uses RST 38h (vector at 0x0038 is a plain RET).
;   * B48..B51 LDIR/CPIR/INIR/OTIR run with BC=1 (B=1 for I/O) so each does
;     exactly one iteration and falls through.
;   * B8/B9/B10/B33/B54 (conditional ops) take the not-taken path: A is
;     cleared by XOR A to force Z=1, which no intervening op disturbs, so
;     every "NZ" variant falls through.
;   * B67 HALT is released by an NMI that the trickbox fires when PC reaches
;     halt_label. The NMI handler RETNs back to stop, which then terminates
;     the simulation via the trickbox IO_CHAR=4 convention.
;==============================================================================

include trickbox.inc

    org 0
    jp  start                       ; skip over the fixed vectors

;------------------------------------------------------------------------------
; RST 38h vector - B12 RST returns here.
;------------------------------------------------------------------------------
    org 0x0038
    ret

;------------------------------------------------------------------------------
; NMI vector - B21 RETN lives here and also releases HALT.
;------------------------------------------------------------------------------
    org 0x0066
    retn                            ; B21 ED RETN (also covers ED RETI)

;------------------------------------------------------------------------------
; Main program
;------------------------------------------------------------------------------
    org 0x0080
start:
    ld  sp, 0xC000                  ; B28  LD rp,nn     (stack below trickbox)

    ; Arm NMI: fire when PC latches at halt_label+1. The Z80's PC register is
    ; incremented past HALT on the first fetch and stays there while halted,
    ; so matching against halt_label itself never fires - we match the
    ; post-HALT latch value instead (same address as stop:).
    ld  hl, stop
    ld  (tb_nmi_pc), hl             ; B56  LD (nn),HL   (trickbox config)
    ld  hl, 3
    ld  (tb_nmi_hold), hl           ; B56  LD (nn),HL

;------------------------------------------------------------------------------
; Register-only ALU / loads
;------------------------------------------------------------------------------
    ld  a, 0x5a                     ; B25  LD r,n
    ld  b, 1                        ; B25  LD r,n
    add a, b                        ; B1   ALU A,r       (exemplar ADD A,B)
    add a, 0                        ; B11  ALU A,n
    inc b                           ; B23  INC r
    dec b                           ; B24  DEC r
    ld  b, b                        ; B6   LD r,r

;------------------------------------------------------------------------------
; Accumulator rotates, flag ops, swap groups
;------------------------------------------------------------------------------
    rlca                            ; B30  RLCA/RRCA/RLA/RRA
    daa                             ; B57  DAA
    cpl                             ; B59  CPL
    scf                             ; B64  SCF
    ccf                             ; B66  CCF
    ex  af, af'                     ; B53  EX AF,AF'
    exx                             ; B73  EXX
    ex  de, hl                      ; B78  EX DE,HL

;------------------------------------------------------------------------------
; 16-bit register ops
;------------------------------------------------------------------------------
    ld  hl, data                    ; B28  LD rp,nn
    ld  bc, 1                       ; B28
    inc bc                          ; B29  INC rp
    dec bc                          ; B32  DEC rp
    add hl, bc                      ; B31  ADD HL,rp
    ld  hl, data                    ; reset HL after ADD above

;------------------------------------------------------------------------------
; (HL) memory ops
;------------------------------------------------------------------------------
    ld  (hl), 0x5a                  ; B63  LD (HL),n
    add a, (hl)                     ; B7   ALU A,(HL)
    ld  b, (hl)                     ; B26  LD r,(HL)
    ld  (hl), b                     ; B27  LD (HL),r
    inc (hl)                        ; B61  INC (HL)
    dec (hl)                        ; B62  DEC (HL)

;------------------------------------------------------------------------------
; (BC) / (DE) / absolute memory
;------------------------------------------------------------------------------
    ld  bc, data                    ; B28
    ld  (bc), a                     ; B38  LD (BC),A   (also LD (DE),A family)
    ld  a, (bc)                     ; B39  LD A,(BC)   (also LD A,(DE) family)
    ld  hl, data
    ld  (data), hl                  ; B56  LD (nn),HL
    ld  hl, (data)                  ; B58  LD HL,(nn)
    ld  (data), a                   ; B60  LD (nn),A
    ld  a, (data)                   ; B65  LD A,(nn)

;------------------------------------------------------------------------------
; Stack push/pop and EX (SP),HL
;------------------------------------------------------------------------------
    push bc                         ; B35  PUSH rp
    pop  bc                         ; B34  POP rp
    ex  (sp), hl                    ; B76  EX (SP),HL
    ld  hl, data                    ; EX (SP),HL scrambled HL; restore it

;------------------------------------------------------------------------------
; Port I/O
;------------------------------------------------------------------------------
    out (0), a                      ; B72  OUT (n),A
    in  a, (0)                      ; B74  IN  A,(n)
    ld  c, 0                        ; B25
    in  b, (c)                      ; B17  ED IN r,(C)    (B79 via prefix)
    out (c), b                      ; B18  ED OUT (C),r   (B79 via prefix)

;------------------------------------------------------------------------------
; CB-prefixed group  (B70 CB prefix collateral on every row below)
;------------------------------------------------------------------------------
    rlc b                           ; B2   CB rot/shift r
    bit 0, b                        ; B3   CB bit  r
    res 0, b                        ; B4   CB res  r
    set 0, b                        ; B5   CB set  r
    rlc (hl)                        ; B13  CB rot/shift (HL)
    bit 0, (hl)                     ; B14  CB bit  (HL)
    res 0, (hl)                     ; B15  CB res  (HL)
    set 0, (hl)                     ; B16  CB set  (HL)

;------------------------------------------------------------------------------
; ED-prefixed group (non-block)  (B79 ED prefix collateral on every row below)
;------------------------------------------------------------------------------
    ld  bc, data                    ; B28
    sbc hl, bc                      ; B19  ED SBC/ADC HL,rp
    ld  (data), bc                  ; B36  ED LD (nn),rp
    ld  bc, (data)                  ; B37  ED LD rp,(nn)
    neg                             ; B20  ED NEG
    ld  i, a                        ; B41  ED LD I,A    (also LD R,A)
    ld  a, i                        ; B42  ED LD A,I    (also LD A,R)
    ld  hl, data
    rrd                             ; B43  ED RRD       (also RLD)
    im  0                           ; B22  ED IM y      (covers IM 0/1/2)

;------------------------------------------------------------------------------
; ED block-transfer / block-I/O - one iteration apiece (BC=1, or B=1 for I/O).
; For the repeating forms (LDIR/CPIR/INIR/OTIR) BC=1 means the repeat test
; fails after the first iteration and they fall through instead of looping.
;------------------------------------------------------------------------------
    ld  hl, data
    ld  de, data2
    ld  bc, 1
    ldi                             ; B44  ED LDI   (also LDD)
    ld  hl, data
    ld  bc, 1
    cpi                             ; B45  ED CPI   (also CPD)
    ld  hl, data
    ld  bc, 0x0100                  ; B=1, C=0
    ini                             ; B46  ED INI   (also IND)
    ld  hl, data
    ld  bc, 0x0100
    outi                            ; B47  ED OUTI  (also OUTD)
    ld  hl, data
    ld  de, data2
    ld  bc, 1
    ldir                            ; B48  ED LDIR  (single iter; also LDDR)
    ld  hl, data
    ld  bc, 1
    cpir                            ; B49  ED CPIR  (single iter; also CPDR)
    ld  hl, data
    ld  bc, 0x0100
    inir                            ; B50  ED INIR  (single iter; also INDR)
    ld  hl, data
    ld  bc, 0x0100
    otir                            ; B51  ED OTIR  (single iter; also OTDR)

;------------------------------------------------------------------------------
; Interrupt enable/disable
;------------------------------------------------------------------------------
    di                              ; B40  DI (also EI - same bucket)

;------------------------------------------------------------------------------
; Conditional control-flow - not-taken path. XOR A sets Z=1; none of the
; following ops touch flags, so Z stays 1 and every "NZ" falls through.
;------------------------------------------------------------------------------
    xor a                           ; Z=1 for the rest of this block
    ret nz                          ; B8   RET cc   (not taken)
    jp  nz, cc1                     ; B9   JP cc,nn (not taken)
cc1:
    call nz, cc2                    ; B10  CALL cc,nn (not taken)
cc2:
    jr  nz, cc3                     ; B33  JR cc,e  (not taken)
cc3:
    ld  b, 1
    djnz cc4                        ; B54  DJNZ     (B->0, not taken)
cc4:

;------------------------------------------------------------------------------
; Unconditional control-flow
;------------------------------------------------------------------------------
    nop                             ; B52  NOP
    jr  jr1                         ; B55  JR e
jr1:
    rst 38h                         ; B12  RST y      (vector at 0x0038)
    call subr                       ; B71  CALL nn    (subr's RET is B69)
    ld  hl, jphl1
    jp  (hl)                        ; B77  JP (HL)
jphl1:
    jp  jpnn1                       ; B68  JP nn
jpnn1:

;------------------------------------------------------------------------------
; LD SP,HL
;------------------------------------------------------------------------------
    ld  hl, 0xC000
    ld  sp, hl                      ; B80  LD SP,HL

;------------------------------------------------------------------------------
; IX group  (B75 DD prefix collateral on every row below)
;------------------------------------------------------------------------------
    ld  ix, data                    ; B101 LD IX,nn
    ld  (data), ix                  ; B102 LD (nn),IX
    ld  ix, (data)                  ; B103 LD IX,(nn)
    inc ix                          ; B104 INC IX
    dec ix                          ; B105 DEC IX
    add ix, bc                      ; B106 ADD IX,rp
    push ix                         ; B107 PUSH IX
    pop  ix                         ; B108 POP  IX
    ld  ix, data
    ex  (sp), ix                    ; B109 EX (SP),IX
    ld  ix, data
    ld  sp, ix                      ; B111 LD SP,IX
    ld  sp, 0xC000                  ; restore a clean stack

    ld  ix, data
    ld  b, (ix+0)                   ; B112 LD r,(IX+d)
    ld  (ix+0), b                   ; B113 LD (IX+d),r
    ld  (ix+0), 0                   ; B114 LD (IX+d),n
    add a, (ix+0)                   ; B115 ALU A,(IX+d)
    inc (ix+0)                      ; B116 INC (IX+d)
    dec (ix+0)                      ; B117 DEC (IX+d)

    ; Undocumented IXh group - IX high byte as an 8-bit register.
    ld  b, ixh                      ; B118 LD r,IXh
    ld  ixh, 0                      ; B119 LD IXh,n
    inc ixh                         ; B120 INC IXh
    dec ixh                         ; B121 DEC IXh
    add a, ixh                      ; B122 ALU A,IXh
    ld  ix, data                    ; restore IX as a valid data pointer

    ; DDCB group - bit/rot/res/set on (IX+d), dual-op form where the result
    ; is also copied into a register (B123/B125/B126 are undocumented).
    rlc (ix+0), b                   ; B123 CB rot/shift (IX+d),r
    bit 0, (ix+0)                   ; B124 CB bit (IX+d)    (no copy, all "r" alias)
    res 0, (ix+0), b                ; B125 CB res (IX+d),r
    set 0, (ix+0), b                ; B126 CB set (IX+d),r

;------------------------------------------------------------------------------
; Jump via IX into HALT. Trickbox asserts NMI when PC hits halt_label;
; the handler at 0x0066 RETNs back to stop.
;------------------------------------------------------------------------------
    ld  ix, halt_label
    jp  (ix)                        ; B110 JP (IX)

halt_label:
    halt                            ; B67  HALT  (released by NMI)

;------------------------------------------------------------------------------
; Self-terminate. Writing 4 to IO_CHAR is the trickbox "stop simulation"
; convention used by the other tests; the alternative is a write to tb_stop.
;------------------------------------------------------------------------------
stop:
    ld  a, 4
    out (IO_CHAR), a

;------------------------------------------------------------------------------
; Call target. The RET here is B69; the CALL above is B71.
;------------------------------------------------------------------------------
subr:
    ret                             ; B69  RET

data:   dw 0
data2:  dw 0
    end
