;==============================================================================
; Print the DAA table
;==============================================================================
include trickbox.inc
    org 0
start:
    jmp boot

    ; BDOS entry point for various functions
    ; We implement subfunctions:
    ;  C=2  Print a character given in E
    ;  C=9  Print a string pointed to by DE; string ends with '$'
    org 5
    ld  a,c
    cp  a,2
    jz  bdos_ascii
    cp  a,9
    jz  bdos_msg
    ret

bdos_ascii:
    ld a, e
    out (IO_CHAR), a
    ret

bdos_msg:
    push de
    pop hl
lp0:
    ld  e,(hl)
    ld  a,e
    cp  a,'$'
    ret z
    call bdos_ascii
    inc hl
    jmp lp0

;==============================================================================
; Dumps DAA table
;==============================================================================
    org 100h
boot:
    ; Set the stack pointer
    ld  sp, 16384

    ld  hl, flag_f
lp1:
    ld  a, (hl)
    cp  a, 0ffh
    jz  stop
    push hl
    ld  c,a
    ld  b,0
lp2:
    push bc
    ld  a,c
    ld  hl, text+2
    call tohex
    ld  a,b
    ld  hl, text+7
    call tohex

    push bc
    pop  af
    daa
    push af
    pop bc

    ld  a,b
    ld  hl, text+13
    call tohex
    ld  a,c
    ld  hl, text+18
    call tohex

    exx
    ld  de,text
    ld  c,9
    call 5
    exx

    pop bc
    inc b
    ld  a,b
    or  a,a
    jnz lp2

    pop  hl
    inc  hl
    jmp  lp1

tohex:
    ; HL = Address to store a hex value
    ; A  = Hex value 00-FF
    push af
    and  a,0fh
    cmp  a,10
    jc   skip1
    add  a, 'A'-'9'-1
skip1:
    add  a, '0'
    inc  hl
    ld   (hl),a
    dec  hl
    pop  af
    rra
    rra
    rra
    rra
    and  a,0fh
    cmp  a,10
    jc   skip2
    add  a, 'A'-'9'-1
skip2:
    add  a, '0'
    ld   (hl),a
    ret
stop:
    ld  (tb_stop), hl ; Writing to tb_stop immediately stops the simulation

flag_f:
    db  00h, 01h, 10h, 11h, 02h, 03h, 12h, 13h, 0ffh
text:
    ;    01234567890123456789
    db  "F:00 A:00 -> 00 F:00",13,10,'$'
end
