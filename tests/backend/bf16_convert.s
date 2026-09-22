; MDT backend test - CVT.F32.BF16 / CVT.BF16.F32 (mixed-precision storage)
;
; R1 = 0x3EAAAAAB, the exact IEEE-754 FP32 bit pattern for 1.0/3.0
; (0.33333334f). Its low 16 bits (0xAAAB) are non-zero, so rounding to BF16
; precision actually changes something - this is not a value that happens
; to already be BF16-exact.
;
; docs/isa-extensions.md section 2.3: CVT.F32.BF16 masks off the bottom 16
; bits (Rd = Rs & 0xFFFF0000); CVT.BF16.F32 is an exact copy under that bit
; convention, since a BF16-rounded value is already a valid FP32 pattern.
;
; Expect:
;   R1 = 0x3EAAAAAB  (unchanged - the original value)
;   R2 = 0x3EAA0000  (R1 rounded to BF16 precision - low 16 bits zeroed)
;   R3 = 0x3EAA0000  (R2 copied through CVT.BF16.F32 - identical to R2)
;
; Run: mdtsim tests/backend/bf16_convert.s --dump-scalar

.text
.global _start

_start:
    ADD           R1, R0, 0x3EAAAAAB   ; R1 = 1/3, full FP32 precision
    CVT.F32.BF16  R2, R1                ; R2 = R1 rounded to BF16 (0x3EAA0000)
    CVT.BF16.F32  R3, R2                ; R3 = R2, unchanged (already valid FP32)
    HALT
