;==============================================================================
; Test BUSRQ and hi-Z (tristate) response
;
; This test will trigger BUSRQ pin to assert at cycle 350, for the duration of
; 20 cycles. At cycle 360 it will stop the simulation so you can examine the
; tristated pins. After that, you can single step to see the pins released.
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

boot:
    ld a,4
    ld (1),a
    ; Set the stack pointer
    ld  sp, 16384    ; 16 Kb of RAM
    ; Jump into the executable at 100h
    jmp 100h
stop:
    ld  (tb_stop), hl ; Writing to tb_stop immediately stops the simulation

;==============================================================================
; Test start
;==============================================================================
    org 100h
exec:
    ld hl, 20
    ld (tb_busrq_hold), hl
    ld hl, 350
    ld (tb_busrq_at),hl
    ld hl, 360
    ld (tb_cyc_stop), hl
loop:
    ld a,1
    ld a,2
    ld a,3
    ld a,4
    jmp loop
end
