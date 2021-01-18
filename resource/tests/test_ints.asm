;==============================================================================
; Test all interrupt modes
;
; This test sets up all 3 interrupt modes (IM0/1/2) and tests them. It also
; shows how to inject a value onto the data bus to be consumed by the
; CPU interrupt sequence.
;
; This is a barebone and compact test suitable to trace cycle-by-cycle.
;==============================================================================
include trickbox.inc
    org 0
start:
    ei
    jmp exec

;---------------------------------------------------------------------
; Interrupt mode 2 handler; addressed by a vector at 0x304
;---------------------------------------------------------------------
im2_handler:
    ei
    reti

;---------------------------------------------------------------------
; RST38 handler
;---------------------------------------------------------------------
    org 038h
    ei
    reti

;---------------------------------------------------------------------
; NMI handler
;---------------------------------------------------------------------
    org 066h
    retn

;==============================================================================
;
; Test start
;
;==============================================================================
    org 100h
exec:
    ; Set the stack pointer
    ld  sp, 16384

    ; Set up for interrupt in mode 0
    ; This mode gets the next instruction by loading it from the data bus
    im 0
    ld a, 3Dh ; "dec a" instruction
    out (IO_INT), a
    ld hl, trigger_0
    ld (tb_int_pc), hl
trigger_0:
    nop

    ; Set up for interrupt in mode 1
    ; This mode simply jumps to the address 0x38
    im 1
    ld a, 0 ; In this mode, the value of data bus is ignored; we will present 0
    out (IO_INT), a
    ld hl, trigger_1
    ld (tb_int_pc), hl
trigger_1:
    nop

    ; Set up for interrupt in mode 2
    ; In this mode, a word is read from the address formed by combining the value
    ; of the "I" register with the byte read from the data bus. CPU then calls into
    ; the code at that word address
    im 2
    ld a, 3
    ld i, a ; High byte address of the interrupt handler
    ld a, 4 ; Low byte address of the interrupt handler, provided on the data bus
    out (IO_INT), a
    ld hl, trigger_2
    ld (tb_int_pc), hl
trigger_2:
    nop

    ; Trigger the NMI
    di ; NMI is taken even when interrupts are disabled
    ld hl, trigger_nmi
    ld (tb_nmi_pc), hl
trigger_nmi:
    nop

    ld a, 4 ; Terminate the simulation
    out (IO_CHAR), a

;---------------------------------------------------------------------
; IM2 vector address of the handler
;---------------------------------------------------------------------
    org 304h
    dw im2_handler
end
