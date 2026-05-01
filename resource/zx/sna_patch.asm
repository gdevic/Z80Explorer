;==============================================================================
; Patch to apply to ZX Spectrum ROM to run snapshot files:
; https://worldofspectrum.org/faq/reference/formats.htm
; See sna.js file on how to use it
;==============================================================================
    org 0
    di
    jmp start

    org 3A00h
start:
    ld  sp, 16384 - 28
    pop af  ; I register in A
    ld  i, a

    pop hl
    pop de
    pop bc
    pop af
    exx
    ex  af,af'

    pop hl
    pop de
    pop bc
    pop iy
    pop ix

    pop af  ; Interrupt mode in A
    im  1   ; XXX: Hard-coded to IM1

    pop af
    ex  af,af'
    ld  sp, (reg_sp)
    ei      ; XXX: Assuming interrupts are enabled
    retn

    org 16384-27

;   Offset   Size   Description
;   ------------------------------------------------------------------------
;   0        1      byte   I
;   1        8      word   HL',DE',BC',AF'
;   9        10     word   HL,DE,BC,IY,IX
;   19       1      byte   Interrupt (bit 2 contains IFF2, 1=EI/0=DI)
;   20       1      byte   R
;   21       4      words  AF,SP
;   25       1      byte   IntMode (0=IM0/1=IM1/2=IM2)
;   26       1      byte   BorderColor (0..7, not used by Spectrum 1.7)
;   27       49152  bytes  RAM dump 16384..65535

reg_i:      db  0
reg_hl2:    dw  0
reg_de2:    dw  0
reg_bc2:    dw  0
reg_af2:    dw  0

reg_hl:     dw  0
reg_de:     dw  0
reg_bc:     dw  0
reg_iy:     dw  0
reg_ix:     dw  0

state_int:  db  0
reg_r:      db  0

reg_af:     dw  0
reg_sp:     dw  0

state_intm: db  0
state_scr:  db  0

end
