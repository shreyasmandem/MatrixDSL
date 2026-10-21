; MDT backend test - a complete neural network layer
;
;   Y = relu(W x X)
;
; This is the workload the entire project exists to compile. In MatrixDSL it
; is two statements; here it is four MDT instructions, of which two are the
; matrix instructions that motivated the target.
;
; Run: mdtsim tests/backend/ai_layer.s --dump-matrix M3

.data
weights:
    .float  0.5, -0.2,  0.3,  0.1
    .float -0.4,  0.6, -0.1,  0.2
    .float  0.2,  0.3, -0.5,  0.4
    .float -0.3,  0.1,  0.2, -0.6
inputs:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ADD    R1, R0, 0            ; R1 = address of weights
    ADD    R2, R0, 64           ; R2 = address of inputs (64 bytes later)

    LOAD   M0, [R1 + 0]         ; M0 = W
    LOAD   M1, [R2 + 0]         ; M1 = X
    MATMUL M2, M0, M1           ; M2 = W x X
    RELU   M3, M2               ; M3 = relu(W x X)

    HALT
