/* test_main.S - command loop for tests/test_asm.py.
 *
 * Assembled into main.S only with -DTEST (never into the server): it
 * replaces the listening socket with a loop that reads one request from
 * standard input, calls the routine under test exactly as the server
 * does, and writes the result to standard output.  The routines are the
 * server's own instructions; only this driver is extra.
 *
 * Request = one command byte, then the fields listed at each handler.
 * End of input ends the process (xio's failure path is `die`, which in a
 * TEST build is exit).  Not size-optimised.
 *
 * The server's routines rely on what the code before them leaves in
 * registers (see main.S); where this driver calls one from a different
 * place it sets that up itself: rcx = 0 for the field operations and
 * run(), r8 = the modulus, the connection descriptor for xio. */

test_main:
tloop:  mov     esi, offset HB
        push    1
        pop     rdx
        call    t_rd
        mov     al, [rsi-1]
        cmp     al, 'S'
        je      t_sha
        cmp     al, 'C'
        je      t_chacha
        cmp     al, 'P'
        je      t_poly
        cmp     al, 'F'
        je      t_fp
        cmp     al, 'I'
        je      t_inv
        cmp     al, 'M'
        je      t_smult
        cmp     al, 'E'
        je      t_ecdsa
        cmp     al, 'T'
        je      t_tables
        cmp     al, 'N'
        je      t_mpint
        cmp     al, 'K'
        je      t_keygen
        cmp     al, 'A'
        je      t_aead
        jmp     die

/* read edx bytes to rsi from fd 0 / write edx bytes at rsi to fd 1 */
t_rd:   and     dword ptr [CFD], 0
        test    edx, edx
        jz      1f
        call    xrecv
1:      ret
t_wr:   mov     dword ptr [CFD], 1
        jmp     xsend

/* S: u32 n, n bytes -> SHA-256 (32 bytes) */
t_sha:  mov     esi, offset HB
        push    4
        pop     rdx
        call    t_rd
        mov     edx, [rsi-4]
        mov     esi, offset HB
        push    rdx
        call    t_rd
        pop     rdx
        mov     esi, offset HB
        lea     edi, [rsi+rdx]
        mov     edx, edi
        call    sha256
        lea     esi, [rdi-32]
        push    32
        pop     rdx
        call    t_wr
        jmp     tloop

/* C: key 32, u32 seq, u32 n, n + 16 bytes -> the n + 16 bytes, the first n
 * XORed with the keystream from block 0 */
t_chacha:
        mov     esi, offset CST + 16
        push    32
        pop     rdx
        call    t_rd
        mov     esi, offset HB
        push    8
        pop     rdx
        call    t_rd
        mov     eax, [rsi-8]
        mov     [SEQ], eax
        mov     edx, [rsi-4]
        add     edx, 16
        push    rdx
        call    t_rd
        mov     ecx, [rsp]
        sub     ecx, 16
        mov     edi, offset HB + 8
        mov     edx, offset CST
        call    chacha
        pop     rdx
        mov     esi, offset HB + 8
        call    t_wr
        jmp     tloop

/* P: key 32, u32 n, n bytes, 16 bytes (the tag to compare with)
 *    -> tag 16, then one byte: 1 if it equals the one given */
t_poly: lea     esi, [rbp-60]
        push    32
        pop     rdx
        call    t_rd
        mov     esi, offset HB
        push    4
        pop     rdx
        call    t_rd
        mov     edx, [rsi-4]
        push    rdx
        add     edx, 16
        mov     esi, ebp
        call    t_rd
        mov     ecx, [rsp]
        mov     r8d, offset QMOD
        call    poly
        setz    al
        pop     rsi
        add     esi, ebp
        mov     [rsi+16], al
        push    17
        pop     rdx
        call    t_wr
        jmp     tloop

/* F: op (0 add, 1 sub, 2 mul, 3 add in place, 4 sub in place, 5 double),
 *    modulus (0 p, 1 n, 2 2^130-5), a 32, b 32 -> result 32 */
t_fp:   mov     esi, offset HB
        push    66
        pop     rdx
        call    t_rd
        mov     esi, offset HB + 2
        lea     edi, [C(sX1)]
        push    64
        pop     rcx
        rep movsb
        movzx   eax, byte ptr [HB + 1]
        shl     eax, 5
        lea     r8d, [rax + PMOD]
        lea     edi, [C(sT1)]
        lea     esi, [C(sX1)]
        lea     edx, [C(sY1)]
        mov     al, [HB]
        xor     ecx, ecx
        cmp     al, 1
        jb      1f
        je      2f
        cmp     al, 3
        jb      3f
        je      4f
        cmp     al, 5
        jb      5f
        /* double: d = a = b */
        mov     edi, esi
        mov     edx, esi
        call    fp_add
        jmp     9f
4:      mov     edi, esi                        /* d = a */
        call    fp_add
        jmp     9f
5:      mov     edi, edx                        /* d = b */
        call    fp_sub
        jmp     9f
1:      call    fp_add
        jmp     9f
2:      call    fp_sub
        jmp     9f
3:      call    fp_mul
9:      mov     esi, edi
        push    32
        pop     rdx
        call    t_wr
        jmp     tloop

/* I: modulus (0 p, 1 n), a 32 -> a^(m-2) 32 */
t_inv:  mov     esi, offset HB
        push    1
        pop     rdx
        call    t_rd
        lea     esi, [C(sZ1)]
        push    32
        pop     rdx
        call    t_rd
        movzx   eax, byte ptr [HB]
        shl     eax, 5
        lea     r8d, [rax + PMOD]
        lea     esi, [C(sZ1)]
        lea     edi, [C(sT0)]
        push    32
        pop     rcx
        rep movsb
        call    fp_inv
        lea     edi, [C(sZ1)]
        xor     eax, eax
        push    32
        pop     rcx
        rep stosb
        lea     esi, [C(sT0)]
        push    32
        pop     rdx
        call    t_wr
        jmp     tloop

/* M: k 32, point 64 -> [k]point 64 */
t_smult:
        lea     esi, [C(sK)]
        push    32
        pop     rdx
        call    t_rd
        mov     esi, offset HB
        push    64
        pop     rdx
        call    t_rd
        mov     esi, offset HB
        call    smult
t_pt:   lea     esi, [C(sX2)]
        push    64
        pop     rdx
        call    t_wr
        jmp     tloop

/* E: d 32, k 32, z 32 -> r || s, as the server signs: [k]G, then the
 * hash into its slot, then the ECDSA program */
t_ecdsa:
        lea     esi, [C(sD)]
        push    32
        pop     rdx
        call    t_rd
        lea     esi, [C(sK)]
        push    32
        pop     rdx
        call    t_rd
        mov     esi, offset gpoint
        call    smult
        lea     esi, [C(sX1)]
        push    32
        pop     rdx
        call    t_rd
        mov     r8d, offset NMOD
        mov     esi, offset ecdsa_prog
        xor     ecx, ecx
        call    run
        jmp     t_pt

/* T: -> K[64] and H0[8] as the server generated them (288 bytes) */
t_tables:
        mov     esi, offset SHAK
        mov     edx, 288
        call    t_wr
        jmp     tloop

/* N: number 32 -> u32 length of the mpint, then 40 bytes holding it */
t_mpint:
        lea     esi, [C(sX1)]
        push    32
        pop     rdx
        call    t_rd
        mov     edi, offset HB + 4
        lea     esi, [C(sX1)]
        call    mpint
        sub     edi, offset HB + 4
        mov     [HB], edi
        mov     esi, offset HB
        push    44
        pop     rdx
        call    t_wr
        jmp     tloop

/* K: -> the scalar keygen drew (32) and its public point (64) */
t_keygen:
        call    keygen
        lea     esi, [C(sK)]
        push    32
        pop     rdx
        call    t_wr
        jmp     t_pt

/* A: the packet layer back to back, as the two ends of one connection:
 * key 64 (K_2 || K_1), u32 seq, u32 n, n bytes payload
 *  -> u32 total, the packet send_packet() puts on the wire (written to
 *     fd 1 by it), which the caller feeds back in on fd 0;
 *  -> then u32 payload length and the payload recv_packet() returns.
 * Both directions use the same key here. */
t_aead: mov     esi, offset CST + 16
        push    32
        pop     rdx
        call    t_rd
        mov     esi, offset CST + 64 + 16
        push    32
        pop     rdx
        call    t_rd
        mov     esi, offset CST + 16
        mov     edi, offset CST + 128 + 16
        push    32
        pop     rcx
        rep movsb
        mov     esi, offset CST + 64 + 16
        mov     edi, offset CST + 192 + 16
        mov     cl, 32
        rep movsb
        mov     esi, offset HB
        push    8
        pop     rdx
        call    t_rd
        and     qword ptr [SEQ], 0
        mov     eax, [rsi-8]
        mov     [SEQ], eax
        push    rax
        mov     edx, [rsi-4]
        push    rdx
        lea     esi, [rbp+5]
        call    t_rd
        pop     rcx
        mov     dword ptr [CFD], 1
        mov     r8d, offset QMOD
        call    send_packet
        pop     rax
        mov     [SEQ], eax
        and     dword ptr [CFD], 0
        mov     edi, ebp
        call    recv_packet
        mov     [rbp+1], eax
        lea     edx, [rax+4]
        lea     esi, [rbp+1]
        call    t_wr
        jmp     tloop
