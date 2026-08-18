; MDT backend test - strided tile load
;
; Extracts a 4x4 tile from an 8x8 matrix. Because the source is 8 floats wide,
; consecutive tile rows are 32 bytes apart, so the load needs a row stride
; rather than a contiguous 64-byte transfer.
;
; The 8x8 source holds row-major values 1..64. The top-left 4x4 tile is
; therefore:
;
;   [  1  2  3  4 ]
;   [  9 10 11 12 ]
;   [ 17 18 19 20 ]
;   [ 25 26 27 28 ]
;
; Run: mdtsim tests/backend/tile_strided.s --dump-matrix M0

.data
big_matrix:
    .float  1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0
    .float 17.0, 18.0, 19.0, 20.0, 21.0, 22.0, 23.0, 24.0
    .float 25.0, 26.0, 27.0, 28.0, 29.0, 30.0, 31.0, 32.0
    .float 33.0, 34.0, 35.0, 36.0, 37.0, 38.0, 39.0, 40.0
    .float 41.0, 42.0, 43.0, 44.0, 45.0, 46.0, 47.0, 48.0
    .float 49.0, 50.0, 51.0, 52.0, 53.0, 54.0, 55.0, 56.0
    .float 57.0, 58.0, 59.0, 60.0, 61.0, 62.0, 63.0, 64.0

.text
.global _start

_start:
    ADD   R1, R0, 0
    ; stride 32 = 8 floats per source row x 4 bytes
    LOADT M0, [R1 + 0], 32
    HALT
