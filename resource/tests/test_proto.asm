;==============================================================================
; Prototype test file: it provides source for the character out and interrupt
; handling but the test itself simply terminates.
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
    ld  bc,8*256    ; Port to write a character out
    out (c),e
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

;---------------------------------------------------------------------
; RST38 (also INT M0)  handler
;---------------------------------------------------------------------
    org 038h
    push de
    ld  de,int_msg
int_common:
    push af
    push bc
    push hl
    ld  c,9
    call 5
    pop hl
    pop bc
    pop af
    pop de
    ei
    reti
int_msg:
    db  "_INT_",'$'

;---------------------------------------------------------------------
; NMI handler
;---------------------------------------------------------------------
    org 066h
    push af
    push bc
    push de
    push hl
    ld  de,nmi_msg
    ld  c,9
    call 5
    pop hl
    pop de
    pop bc
    pop af
    retn
nmi_msg:
    db  "_NMI_",'$'

;---------------------------------------------------------------------
; IM2 vector address and the handler (to push 0x80 by the IORQ)
;---------------------------------------------------------------------
    org 080h
    dw  im2_handler
im2_handler:
    push de
    ld  de,int_im2_msg
    jmp int_common
int_im2_msg:
    db  "_IM2_",'$'
boot:
    ld a,4
    ld (1),a
    ; Set the stack pointer
    ld  sp, 16384    ; 16 Kb of RAM
    ; Jump into the executable at 100h
    jmp 100h
die:
    ld (0),a ; Writing to address 0 of the simulated space terminates the simulation

;==============================================================================
;
; Test start
;
;==============================================================================
    org 100h
exec:
    ld hl,200
    ld (tb_cyc_stop), hl ; Terminate at cycle 200
    jmp 0
    jmp die
end
