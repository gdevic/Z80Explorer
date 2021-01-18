;==============================================================================
; Test code for the Z80 CPU that prints "Hello, World!"
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
; Prints "Hello, World!"
;==============================================================================
    org 100h
boot:
    ; Set the stack pointer
    ld  sp, 16384

    ld  hl,0
    ld  (counter),hl
exec:
    ld  de,hello
    ld  c,9
    call 5

    ; Print the counter and the stack pointer to make sure it does not change
    ld  hl, (counter)
    inc hl
    ld  (counter),hl

    ld  hl, text
    ld  a,(counter+1)
    call tohex
    ld  hl, text+2
    ld  a,(counter)
    call tohex

    ld  (stack),sp

; Several options on which values we want to dump, uncomment only one:
    ld  hl, text+5
;   ld  a,(stack+1)     ; Dump stack pointer (useful to check SP)
    ld  a, i            ; Show IR register
    call tohex
    ld  hl, text+7
;   ld  a,(stack)       ; Dump stack pointer (useful to check SP)
    ld  a, r            ; Show IR register
    call tohex

; Two versions of the code: either keep printing the text indefinitely (which
; can be used for interrupt testing), or print it only once and die
    jr exec
;    ld  (tb_stop), hl ; Writing to tb_stop immediately stops the simulation

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

; Print a counter before Hello, World so we can see if the
; processor rebooted during one of the interrupts. Also, print the content
; of the SP register which should stay fixed and "uninterrupted"
counter: dw 0
stack: dw 0

hello:
    db  13,10
text:
    db '---- ---- Hello, World!$'

end
