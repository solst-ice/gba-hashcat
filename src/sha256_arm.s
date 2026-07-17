
    .syntax unified
    .cpu arm7tdmi
    .arm
    .section .iwram, "ax", %progbits
    .align 2

    .global _ZN2hc6sha256ERKN2bn11string_viewE
    .type _ZN2hc6sha256ERKN2bn11string_viewE, %function
_ZN2hc6sha256ERKN2bn11string_viewE:
    ldr     r3, [r1]                  @ string_view begin
    ldr     r2, [r1, #4]              @ string_view end
    sub     r2, r2, r3
    mov     r1, r3
    b       _ZN2hc6sha256EPKhm
    .size _ZN2hc6sha256ERKN2bn11string_viewE, . - _ZN2hc6sha256ERKN2bn11string_viewE

    .global hc_sha256_matches
    .type hc_sha256_matches, %function
hc_sha256_matches:
    orr     r3, r2, #1               @ tagged target pointer selects compare mode
    mov     r2, r1                   @ length
    mov     r1, r0                   @ data
    mov     r0, r3
    b       .Lsha256_entry
    .size hc_sha256_matches, . - hc_sha256_matches

    .global _ZN2hc6sha256EPKhm
    .type _ZN2hc6sha256EPKhm, %function

    .macro BSWAP word, temp
        eor     \temp, \word, \word, ror #16
        bic     \temp, \temp, #0x00ff0000
        mov     \word, \word, ror #8
        eor     \word, \word, \temp, lsr #8
    .endm

    .macro ZERO_SCHEDULE
        eor     r0, r0, r0
        mov     r3, r0
        mov     r12, r0
        mov     lr, r0
        mov     r1, sp
        stmia   r1!, {r0, r3, r12, lr}
        stmia   r1!, {r0, r3, r12, lr}
        stmia   r1!, {r0, r3, r12, lr}
        stmia   r1!, {r0, r3, r12, lr}
    .endm


    .macro SHA_ROUND a, b, c, d, e, f, g, h
        mov     r0, \e, ror #6
        eor     r0, r0, \e, ror #11
        eor     r0, r0, \e, ror #25
        eor     r1, \f, \g
        and     r1, r1, \e
        eor     r1, r1, \g
        add     r0, r0, \h
        add     r0, r0, r1
        ldr     r1, [r3], #4
        add     r0, r0, r12
        add     r0, r0, r1

        mov     r1, \a, ror #2
        eor     r1, r1, \a, ror #13
        eor     r1, r1, \a, ror #22
        eor     r12, \a, \b
        and     r12, r12, \c
        and     lr, \a, \b
        eor     r12, r12, lr
        add     r1, r1, r12
        add     \d, \d, r0
        add     \h, r0, r1
    .endm

    .macro STORED_ROUND slot, a, b, c, d, e, f, g, h
        ldr     r12, [sp, #((\slot) * 4)]
        SHA_ROUND \a, \b, \c, \d, \e, \f, \g, \h
    .endm

    /* Generate W[t] in its circular slot, then consume it immediately. */
    .macro EXPANDED_ROUND t, a, b, c, d, e, f, g, h
        ldr     r12, [sp, #((((\t) + 1) & 15) * 4)]   @ W[t - 15]
        mov     r0, r12, ror #7
        eor     r0, r0, r12, ror #18
        eor     r0, r0, r12, lsr #3
        ldr     r12, [sp, #((((\t) + 14) & 15) * 4)]  @ W[t - 2]
        mov     r1, r12, ror #17
        eor     r1, r1, r12, ror #19
        eor     r1, r1, r12, lsr #10
        ldr     r12, [sp, #(((\t) & 15) * 4)]          @ W[t - 16]
        add     r0, r0, r12
        ldr     r12, [sp, #((((\t) + 9) & 15) * 4)]   @ W[t - 7]
        add     r0, r0, r12
        add     r12, r0, r1
        str     r12, [sp, #(((\t) & 15) * 4)]
        SHA_ROUND \a, \b, \c, \d, \e, \f, \g, \h
    .endm

    .macro STORED_EIGHT base
        STORED_ROUND ((\base) + 0), r4,  r5,  r6,  r7,  r8,  r9,  r10, r11
        STORED_ROUND ((\base) + 1), r11, r4,  r5,  r6,  r7,  r8,  r9,  r10
        STORED_ROUND ((\base) + 2), r10, r11, r4,  r5,  r6,  r7,  r8,  r9
        STORED_ROUND ((\base) + 3), r9,  r10, r11, r4,  r5,  r6,  r7,  r8
        STORED_ROUND ((\base) + 4), r8,  r9,  r10, r11, r4,  r5,  r6,  r7
        STORED_ROUND ((\base) + 5), r7,  r8,  r9,  r10, r11, r4,  r5,  r6
        STORED_ROUND ((\base) + 6), r6,  r7,  r8,  r9,  r10, r11, r4,  r5
        STORED_ROUND ((\base) + 7), r5,  r6,  r7,  r8,  r9,  r10, r11, r4
    .endm

    .macro EXPANDED_EIGHT base
        EXPANDED_ROUND ((\base) + 0), r4,  r5,  r6,  r7,  r8,  r9,  r10, r11
        EXPANDED_ROUND ((\base) + 1), r11, r4,  r5,  r6,  r7,  r8,  r9,  r10
        EXPANDED_ROUND ((\base) + 2), r10, r11, r4,  r5,  r6,  r7,  r8,  r9
        EXPANDED_ROUND ((\base) + 3), r9,  r10, r11, r4,  r5,  r6,  r7,  r8
        EXPANDED_ROUND ((\base) + 4), r8,  r9,  r10, r11, r4,  r5,  r6,  r7
        EXPANDED_ROUND ((\base) + 5), r7,  r8,  r9,  r10, r11, r4,  r5,  r6
        EXPANDED_ROUND ((\base) + 6), r6,  r7,  r8,  r9,  r10, r11, r4,  r5
        EXPANDED_ROUND ((\base) + 7), r5,  r6,  r7,  r8,  r9,  r10, r11, r4
    .endm

_ZN2hc6sha256EPKhm:
.Lsha256_entry:
    push    {r4-r11, lr}
    sub     sp, sp, #116
    str     r0, [sp, #96]             @ hidden result pointer
    str     r2, [sp, #100]            @ original byte length
    str     r1, [sp, #104]            @ current input pointer

    ldr     r3, .Lentry_iv_pointer
    ldmia   r3, {r4-r11}
    cmp     r2, #56
    bhs     .Lgeneral_blocks

    ZERO_SCHEDULE
    ldr     r1, [sp, #104]
    mov     r0, sp
    movs    r12, r2, lsr #2
    beq     .Lfast_remainder

    tst     r1, #3
    bne     .Lfast_unaligned_words
.Lfast_aligned_words:
    ldr     r3, [r1], #4
    BSWAP   r3, lr
    str     r3, [r0], #4
    subs    r12, r12, #1
    bne     .Lfast_aligned_words
    b       .Lfast_remainder

.Lfast_unaligned_words:
    ldrb    r3, [r1], #1
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    str     r3, [r0], #4
    subs    r12, r12, #1
    bne     .Lfast_unaligned_words

.Lfast_remainder:
    and     r12, r2, #3
    cmp     r12, #0
    moveq   r3, #0x80000000
    beq     .Lfast_store_last_word
    ldrb    r3, [r1], #1
    mov     r3, r3, lsl #24
    cmp     r12, #1
    orreq   r3, r3, #0x00800000
    beq     .Lfast_store_last_word
    ldrb    lr, [r1], #1
    orr     r3, r3, lr, lsl #16
    cmp     r12, #2
    orreq   r3, r3, #0x00008000
    beq     .Lfast_store_last_word
    ldrb    lr, [r1]
    orr     r3, r3, lr, lsl #8
    orr     r3, r3, #0x80
.Lfast_store_last_word:
    str     r3, [r0]
    mov     r1, r2, lsl #3
    str     r1, [sp, #60]             @ W[15] = length in bits
    bl      .Lcompress
    b       .Lwrite_digest

    .align 2
.Lentry_iv_pointer:
    .word   .Lsha256_iv

.Lgeneral_blocks:
    cmp     r2, #64
    blo     .Lgeneral_tail
.Ldecode_full_block:
    mov     r0, sp
    add     r12, sp, #64
    tst     r1, #3
    bne     .Ldecode_word
.Ldecode_aligned_word:
    ldr     r3, [r1], #4
    BSWAP   r3, lr
    str     r3, [r0], #4
    cmp     r0, r12
    bne     .Ldecode_aligned_word
    b       .Ldecoded_full_block
.Ldecode_word:
    ldrb    r3, [r1], #1
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    str     r3, [r0], #4
    cmp     r0, r12
    bne     .Ldecode_word
.Ldecoded_full_block:
    sub     r2, r2, #64
    str     r1, [sp, #104]
    str     r2, [sp, #108]
    bl      .Lcompress
    ldr     r1, [sp, #104]
    ldr     r2, [sp, #108]
    cmp     r2, #64
    bhs     .Ldecode_full_block

.Lgeneral_tail:
    str     r2, [sp, #108]
    ZERO_SCHEDULE
    ldr     r1, [sp, #104]
    ldr     r2, [sp, #108]
    mov     r0, sp
    movs    r12, r2, lsr #2
    beq     .Lgeneral_remainder
    tst     r1, #3
    bne     .Lgeneral_unaligned_words
.Lgeneral_aligned_words:
    ldr     r3, [r1], #4
    BSWAP   r3, lr
    str     r3, [r0], #4
    subs    r12, r12, #1
    bne     .Lgeneral_aligned_words
    b       .Lgeneral_remainder
.Lgeneral_unaligned_words:
    ldrb    r3, [r1], #1
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    ldrb    lr, [r1], #1
    orr     r3, lr, r3, lsl #8
    str     r3, [r0], #4
    subs    r12, r12, #1
    bne     .Lgeneral_unaligned_words
.Lgeneral_remainder:
    and     r12, r2, #3
    cmp     r12, #0
    moveq   r3, #0x80000000
    beq     .Lgeneral_store_last_word
    ldrb    r3, [r1], #1
    mov     r3, r3, lsl #24
    cmp     r12, #1
    orreq   r3, r3, #0x00800000
    beq     .Lgeneral_store_last_word
    ldrb    lr, [r1], #1
    orr     r3, r3, lr, lsl #16
    cmp     r12, #2
    orreq   r3, r3, #0x00008000
    beq     .Lgeneral_store_last_word
    ldrb    lr, [r1]
    orr     r3, r3, lr, lsl #8
    orr     r3, r3, #0x80
.Lgeneral_store_last_word:
    str     r3, [r0]
    cmp     r2, #56
    blo     .Lgeneral_add_length

    bl      .Lcompress
    ZERO_SCHEDULE
.Lgeneral_add_length:
    ldr     r2, [sp, #100]
    mov     r1, r2, lsr #29
    str     r1, [sp, #56]             @ high 32 bits of bit length
    mov     r1, r2, lsl #3
    str     r1, [sp, #60]             @ low 32 bits of bit length
    bl      .Lcompress

.Lwrite_digest:
    ldr     r0, [sp, #96]
    tst     r0, #1
    bne     .Lcompare_digest
    BSWAP   r4, r1
    BSWAP   r5, r1
    BSWAP   r6, r1
    BSWAP   r7, r1
    BSWAP   r8, r1
    BSWAP   r9, r1
    BSWAP   r10, r1
    BSWAP   r11, r1
    stmia   r0, {r4-r11}
    b       .Lreturn

.Lcompare_digest:
    bic     r0, r0, #3
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r4, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r5, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r6, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r7, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r8, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r9, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0], #4
    BSWAP   r1, r3
    cmp     r10, r1
    bne     .Ldigest_mismatch
    ldr     r1, [r0]
    BSWAP   r1, r3
    cmp     r11, r1
    mov     r0, #0
    moveq   r0, #1
    b       .Lreturn
.Ldigest_mismatch:
    mov     r0, #0
.Lreturn:
    add     sp, sp, #116
    pop     {r4-r11, lr}
    bx      lr

.Lcompress:
    add     r12, sp, #64
    stmia   r12, {r4-r11}
    str     lr, [sp, #112]
    ldr     r3, .Lcompress_k_pointer
    b       .Lcompress_rounds
    .align 2
.Lcompress_k_pointer:
    .word   .Lsha256_k
.Lcompress_rounds:
    STORED_EIGHT   0
    STORED_EIGHT   8
    EXPANDED_EIGHT 16
    EXPANDED_EIGHT 24
    EXPANDED_EIGHT 32
    EXPANDED_EIGHT 40
    EXPANDED_EIGHT 48
    EXPANDED_EIGHT 56

    add     r12, sp, #64
    ldmia   r12, {r0-r3}
    add     r0, r0, r4
    add     r1, r1, r5
    add     r2, r2, r6
    add     r3, r3, r7
    stmia   r12!, {r0-r3}
    ldmia   r12, {r0-r3}
    add     r0, r0, r8
    add     r1, r1, r9
    add     r2, r2, r10
    add     r3, r3, r11
    stmia   r12, {r0-r3}
    sub     r12, r12, #16
    ldmia   r12, {r4-r11}
    ldr     lr, [sp, #112]
    bx      lr

    .size _ZN2hc6sha256EPKhm, . - _ZN2hc6sha256EPKhm

    .align 2
.Lsha256_iv:
    .word 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a
    .word 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
.Lsha256_k:
    .word 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
    .word 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
    .word 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
    .word 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
    .word 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
    .word 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
    .word 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
    .word 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
    .word 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
    .word 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
    .word 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
    .word 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
    .word 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
    .word 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
    .word 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
    .word 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2

    .section .note.GNU-stack, "", %progbits
