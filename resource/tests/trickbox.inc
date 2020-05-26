;==============================================================================
; Defines the simulator trickbox control segment
;==============================================================================

trickbox        equ 0xD000
tb_cyc_stop     equ (trickbox +  0) ; Cycle number at which to stop the simulation
tb_int_at       equ (trickbox +  2) ; Cycle number at which to assert INT pin
tb_int_len      equ (trickbox +  4) ; Number of cycles to hold INT asserted
tb_nmi_at       equ (trickbox +  6) ; Cycle number at which to assert NMI pin
tb_nmi_len      equ (trickbox +  8) ; Number of cycles to hold NMI asserted
tb_busrq_at     equ (trickbox + 10) ; Cycle number at which to assert BUSRQ pin
tb_busrq_len    equ (trickbox + 12) ; Number of cycles to hold BUSRQ asserted
tb_wait_at      equ (trickbox + 14) ; Cycle number at which to assert WAIT pin
tb_wait_len     equ (trickbox + 16) ; Number of cycles to hold WAIT asserted
tb_reset_at     equ (trickbox + 18) ; Cycle number at which to assert RESET pin
tb_reset_len    equ (trickbox + 20) ; Number of cycles to hold RESET asserted
tb_cycle_low    equ (trickbox + 22) ; Curent cycle number (low word)
tb_cycle_high   equ (trickbox + 24) ; Curent cycle number (high word)

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
