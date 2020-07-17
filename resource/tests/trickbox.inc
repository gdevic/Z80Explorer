;==============================================================================
; Defines the simulator trickbox control segment
;==============================================================================

trickbox        equ 0xD000
tb_stop         equ (trickbox +  0) ; Writing to this address immediately stops the simulation
tb_cyc_stop     equ (trickbox +  2) ; Cycle number at which to stop the simulation
tb_cycle_low    equ (trickbox +  4) ; Curent cycle number (low word)
tb_cycle_high   equ (trickbox +  6) ; Curent cycle number (high word)
tb_int_at       equ (trickbox +  8) ; Cycle number at which to assert INT pin
tb_int_len      equ (trickbox + 10) ; Number of cycles to hold INT asserted
tb_nmi_at       equ (trickbox + 12) ; Cycle number at which to assert NMI pin
tb_nmi_len      equ (trickbox + 14) ; Number of cycles to hold NMI asserted
tb_busrq_at     equ (trickbox + 16) ; Cycle number at which to assert BUSRQ pin
tb_busrq_len    equ (trickbox + 18) ; Number of cycles to hold BUSRQ asserted
tb_wait_at      equ (trickbox + 20) ; Cycle number at which to assert WAIT pin
tb_wait_len     equ (trickbox + 22) ; Number of cycles to hold WAIT asserted
tb_reset_at     equ (trickbox + 24) ; Cycle number at which to assert RESET pin
tb_reset_len    equ (trickbox + 26) ; Number of cycles to hold RESET asserted

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
;    ld  (tb_nmi_len), hl
