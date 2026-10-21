; MDT backend test - vector lanes
; Loads two 4-lane vectors, adds and multiplies them.
;
; V1 = [1, 2, 3, 4]
; V2 = [10, 20, 30, 40]
; Expect: V3 = [11, 22, 33, 44], V4 = [10, 40, 90, 160]
;
; Run: mdtsim tests/backend/vector_ops.s --dump-vector

.data
vec_a:
    .float 1.0, 2.0, 3.0, 4.0
vec_b:
    .float 10.0, 20.0, 30.0, 40.0

.text
.global _start

_start:
    ADD   R1, R0, 0         ; address of vec_a
    ADD   R2, R0, 16        ; address of vec_b

    LOAD  V1, [R1 + 0]
    LOAD  V2, [R2 + 0]

    VADD  V3, V1, V2        ; element-wise sum
    VMUL  V4, V1, V2        ; element-wise product
    HALT
