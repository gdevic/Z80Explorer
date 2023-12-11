;==============================================================================
; Blank test file: add any instructions you want to test
;==============================================================================
include trickbox.inc
    org 0
start:
    nop
stop:
    ld  (tb_stop), hl ; Writing to tb_stop immediately stops the simulation
end
