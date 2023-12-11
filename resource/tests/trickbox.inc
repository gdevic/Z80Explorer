;==============================================================================
; Defines the simulator trickbox control segment
;==============================================================================

trickbox        equ 0xD000
tb_stop         equ (trickbox +  0) ; Writing to this address immediately stops the simulation
tb_cyc_stop     equ (trickbox +  2) ; Cycle number at which to stop the simulation
tb_cycle_low    equ (trickbox +  4) ; Curent cycle number (low word)
tb_cycle_high   equ (trickbox +  6) ; Curent cycle number (high word)
tb_int_at       equ (trickbox +  8) ; Cycle number at which to assert INT pin
tb_int_pc       equ (trickbox + 10) ; PC address at which to assert INT pin
tb_int_hold     equ (trickbox + 12) ; Number of cycles to hold INT asserted
tb_nmi_at       equ (trickbox + 14) ; Cycle number at which to assert NMI pin
tb_nmi_pc       equ (trickbox + 16) ; PC address at which to assert NMI pin
tb_nmi_hold     equ (trickbox + 18) ; Number of cycles to hold NMI asserted
tb_busrq_at     equ (trickbox + 20) ; Cycle number at which to assert BUSRQ pin
tb_busrq_pc     equ (trickbox + 22) ; PC address at which to assert BUSRQ pin
tb_busrq_hold   equ (trickbox + 24) ; Number of cycles to hold BUSRQ asserted
tb_wait_at      equ (trickbox + 26) ; Cycle number at which to assert WAIT pin
tb_wait_pc      equ (trickbox + 28) ; PC address at which to assert WAIT pin
tb_wait_hold    equ (trickbox + 30) ; Number of cycles to hold WAIT asserted
tb_reset_at     equ (trickbox + 32) ; Cycle number at which to assert RESET pin
tb_reset_pc     equ (trickbox + 34) ; PC address at which to assert RESET pin
tb_reset_hold   equ (trickbox + 36) ; Number of cycles to hold RESET asserted

IO_CHAR equ  0x80 ; IO address to write a character out
IO_INT  equ  0x81 ; IO address that holds the value to be shown on the data bus during an interrupt

; Examples of use:
;
; Stop 200 cycles from now:
;    ld  hl, (tb_cycle_low)
;    ld  bc, 200
;    add hl, bc
;    ld  (tb_cyc_stop), hl
;
; Stop at cycle 500:
;    ld  hl, 500
;    ld  (tb_cyc_stop), hl
;
; Issue NMI at 300 and hold it low for 3 clocks
;    ld  hl, 300
;    ld  (tb_nmi_at), hl
;    ld  hl, 3
;    ld  (tb_nmi_hold), hl
;
; Issue INT before a particular instruction
;    ld  hl, label1
;    ld  (tb_int_pc), hl
label1:
;    daa
;
; Stop the simulation
;    ld a, 4 ; Terminate the simulation
;    out (IO_CHAR), a
