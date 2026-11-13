/*
 * Nano SSH Server - v32-asm
 *
 * The server of v31-trim - ecdh-sha2-nistp256, ecdsa-sha2-nistp256,
 * chacha20-poly1305@openssh.com, password authentication, one session
 * channel that says "Hello World" - written directly in x86-64 assembly.
 * One file, no libc, no compiler-generated code, nothing compressed: the
 * output file is this section, byte for byte, and the kernel maps it as
 * it is.  See optimization_log.txt for the sizes and the verification.
 *
 * What assembly buys over the C of v31-trim:
 *   - one base register (rbx) reaches the field elements and another
 *     (rbp) the packet buffer and the connection state with one-byte
 *     displacements, where the compiler loaded 32-bit addresses;
 *   - register conventions chosen per routine instead of the ABI's, so
 *     pointers stay where the next string instruction wants them;
 *   - arithmetic on the carry flag: a 256-bit add or subtract is a
 *     14-byte loop around adc/sbb, and loop counters wrap to zero in
 *     byte registers that are then already zero for the next use;
 *   - data and code laid out by hand: programs, tables and strings sit
 *     in the ELF header fields the kernel does not read, and the host
 *     key's wire form is the last bytes of the file plus the first bytes
 *     of bss.
 *
 * Register conventions (global):
 *   rbx   slot 4 of the element file (see C() below)
 *   rbp   TMP, the packet buffer; the connection state sits just below
 *   r8    the modulus of the field operations (p, n or 2^130-5)
 *   r12b  bit counter of the scalar multiplication     \  each counts
 *   r13b  bit counter of the field multiplication       > up to 256 and
 *   r15b  bit counter of the inversion                 /  is 0 at rest
 *   r14   the exchange hash's address, later the client's channel
 * Everything else is scratch unless a routine says otherwise.
 *
 * Assembled with -DTEST, the listening socket is replaced by the command
 * loop of test_main.S, which tests/test_asm.py drives.
 */

#ifndef PORT
#define PORT 2222
#endif

        .intel_syntax noprefix
        .text

/* ---- the element file: 16 slots of 32 bytes, big-endian numbers ----
 * D            the host key's secret scalar
 * X2, Y2       the point P of a scalar multiplication, then its result;
 *              ECDSA leaves r, s there.  Y2 doubles as the inversion's
 *              temporary; Poly1305 keeps h and r in the two
 * K            the scalar of every multiplication (host key, ephemeral
 *              key, signature nonce); Poly1305's block
 * ZERO         never written
 * KN           K + n, the ladder's scalar; the inversion's exponent
 * T0..T3       scratch; T0 holds an inverse
 * X1, Y1, Z1   the ladder's R = 2R (projective); X1 holds the hash that
 *              ECDSA signs
 * SX, SY, SZ   the ladder's S = R + P; scratch until written
 * The first eight are within a one-byte displacement of rbx. */
#define sD    0
#define sX2   1
#define sY2   2
#define sK    3
#define sZERO 4
#define sKN   5
#define sT0   6
#define sX1   7
#define sY1   8
#define sZ1   9
#define sSX   10
#define sSY   11
#define sSZ   12
#define sT1   13
#define sT2   14
#define sT3   15

#define BXOFF 128
#define C(s)  rbx + 32*(s) - BXOFF

/* A program for run(): two bytes per field operation,
 * { src1 << 4 | dst, op << 4 | src2 }; a zero first byte ends it (no
 * instruction has slot 0 as both dst and src1).  A multiplication's
 * destination must differ from its sources; additions and subtractions
 * may overwrite an operand. */
#define ADD(d,a,b) .byte ((a)<<4)|(d), (0<<4)|(b)
#define SUB(d,a,b) .byte ((a)<<4)|(d), (1<<4)|(b)
#define MUL(d,a,b) .byte ((a)<<4)|(d), (2<<4)|(b)
#define INV        .byte 0xff, 0x30             /* T0 = T0^(m-2), see fp_inv */
#define END        .byte 0

/* The largest packet_length accepted; every buffer a packet is received
 * into has room for it. */
#define MAXPKT 4092

/* Everything the program addresses - image, slots, buffers, stack - lies
 * in 0x400000..0x40ffff.  A register that already holds one of those
 * addresses is re-pointed by replacing its low 16 bits (4 bytes instead
 * of 5).  Only used where the register is known to hold such an address;
 * each use says why. */
#define LO(sym) offset (sym) - base

/* connection state, just below the packet buffer (rbp = TMP) */
#define PK    rbp - 60          /* 60 bytes of K_2 keystream, block 0:
                                   the Poly1305 key is its first 32 */
#define KS    rbp - 64          /* 4 bytes of K_1 keystream (the length's);
                                   zero until the keys are in use */
#define SEQ   rbp - 68          /* packets sent = both sequence numbers */
#define CFD   rbp - 72          /* the connection */
#define LFD   rbp - 76          /* the listening socket */

/* ---- memory map: bss follows the image ---- */
        .equ    HOSTPUB, bss                    /* 64: host public x || y */
        .equ    W, bss + 64                     /* 16 slots */
        .equ    BXA, W + BXOFF                  /* rbx */
        .equ    SHAK, W + 512                   /* K[64], H0 (64 words too) */
        .equ    SHAH, SHAK + 256
        .equ    PMOD, SHAK + 512                /* p, n, 2^130-5 */
        .equ    NMOD, PMOD + 32
        .equ    QMOD, PMOD + 64
        .equ    CST, PMOD + 96                  /* 4 ChaCha20 states:
                                                   K_2, K_1 client to server,
                                                   K_2, K_1 server to client */
        .equ    HB, base + 0x2000               /* handshake transcript;
                                                   256-aligned, ~9 KiB */
        .equ    STACKTOP, base + 0xb000
        .equ    TMP, base + 0xb080              /* packet buffer: the last
                                                   thing mapped */
        .equ    BSSEND, TMP + 4096 + 64

base:
/* ---------------- ELF header and program header ----------------
 * The program header starts at 0x30, inside the ELF header: its first
 * words double as e_flags, e_ehsize/e_phentsize and e_phnum.  Of the ELF
 * header the kernel reads the magic, e_type, e_machine, e_entry, e_phoff,
 * e_phentsize and e_phnum; of the program header p_type, p_flags,
 * p_offset, p_vaddr, p_filesz and p_memsz.  The fields in between hold
 * data the program uses.  p_offset = 1 (it is also e_phnum) with p_vaddr
 * = base + 1 maps file offset f at base + f. */
        .byte   0x7f, 'E', 'L', 'F', 2          /* 00 magic, 64-bit */
affine_prog:                                    /* 05 e_ident, unread: 11 bytes */
        /* x = X/Z, y = Y/Z into the P slots; Z - Z leaves the Z slot zero
         * for the next multiplication, which only sets its low byte */
        ADD(sT0, sZ1, sZERO)
        INV
        MUL(sX2, sX1, sT0)
        MUL(sY2, sY1, sT0)
        SUB(sZ1, sZ1, sZ1)
        END
        .word   2, 0x3e                         /* 10 ET_EXEC, x86-64 */
optab:  .byte   0, fp_sub - fp_add              /* 14 e_version, unread: where */
        .byte   fp_mul - fp_add, fp_inv - fp_add /*   run() finds each operation */
        .quad   _start                          /* 18 e_entry */
        .word   0x30                            /* 20 e_phoff */
confbody:                                       /* 22 CHANNEL_OPEN_CONFIRMATION
                                                   behind the recipient: */
        .byte   0, 0, 0, 0, 0, 0                /*    sender channel 0, */
        .byte   0x80, 0                         /* 28 e_shoff, unread: window
                                                   32768, */
poly_prog:                                      /*    and as the maximum packet
                                                   size the Poly1305 block
                                                   step h = (t + h) r */
        ADD(sK, sK, sX2)
        MUL(sX2, sK, sY2)
        END
        .byte   0
        .long   1                               /* 30 p_type PT_LOAD */
        .word   7, 56                           /* 34 p_flags RWX; e_phentsize */
        .word   1, 0, 0, 0                      /* 38 e_phnum = 1 = p_offset */
        .quad   base + 1                        /* 40 p_vaddr */
sockaddr:                                       /* 48 p_paddr, unread: AF_INET, */
        .word   2                               /*    the port, INADDR_ANY; */
        .byte   PORT >> 8, PORT & 0xff          /*    bind() ignores the eight */
        .long   0                               /*    bytes of sin_zero behind */
        .quad   bss - base - 1                  /* 50 p_filesz */
        .long   BSSEND - base - 1               /* 58 p_memsz, low five bytes */
        .byte   0
vs:     .ascii  "\0\0\0\017"                    /* 5d its top three; the length
                                                   of V_S, then V_S itself over
                                                   p_align (unread) and on */
vsline: .ascii  "SSH-2.0-NanoSSH\r\n"

/* ---------------- startup ---------------- */
        .globl  _start
_start:
#ifdef TEST
        mov     esp, offset STACKTOP
#endif
        /* SHA-256's constants: K[i] = frac(cbrt(prime_i)) 2^32 and H0[i] =
         * frac(sqrt(prime_i)) 2^32.  x = sqrt(q / x) from x = q converges
         * on cbrt(q) (the error halves each step), and stopped after two
         * steps it is sqrt(q): pass one (99 steps) fills K, pass two (2
         * steps) H0 - 64 words each, simpler than stopping at eight.  The
         * x87 keeps 64 bits; scaled by 2^56 and rounded to an integer, the
         * 32 bits wanted are its bytes 3..6 (the 24 below them are what
         * makes the rounding harmless - tests/test_asm.py checks all 72
         * words).  The store is 8 bytes wide, so each one's tail is
         * overwritten by the next. */
        mov     edi, offset SHAK
        push    56
        fild    dword ptr [rsp]                 /* stays on both stacks: see
                                                   setsockopt below */
        push    99
        pop     rbp
1:      push    1
        pop     rbx
2:      inc     ebx                             /* the next prime: */
        mov     esi, ebx
3:      mov     eax, ebx
        dec     esi
        cdq
        idiv    esi
        test    edx, edx
        jnz     3b
        dec     esi                             /* largest proper divisor - 1 */
        jg      2b
        push    rbx
        fild    dword ptr [rsp]
        pop     rbx
        fld     st(0)
        mov     ecx, ebp
4:      fdivr   st(0), st(1)
        fsqrt
        loop    4b
        fstp    st(1)
        fscale
        fistp   qword ptr [rdi]
        mov     eax, [rdi+3]
        stosd
        cmp     bl, 0x37                        /* 311, the 64th prime, is the
                                                   first whose low byte is 0x37 */
        jne     2b
        sub     ebp, 97                         /* 99, 2, -95 */
        jns     1b
        /* The moduli, big-endian, at rdi = PMOD (bss, so only the nonzero
         * bytes are written; the first four were hit by the last store
         * above):  p = ffffffff 00000001 0^96 f^96,
         *          n = ffffffff 00000000 f^64 || 16 bytes,
         *          2^130 - 5 = 0^120 03 f^120 fb. */
        push    -1
        pop     rax
        stosd
        inc     byte ptr [rdi+3]
        add     edi, 16
        mov     cl, 16                          /* rcx was 0 */
        rep stosb
        scasd
        stosq
        mov     esi, offset nlow
        movsq
        movsq
        add     edi, 16
        mov     byte ptr [rdi-1], 3
        mov     cl, 15
        rep stosb
        mov     byte ptr [rdi], 0xfb
        mov     ebx, offset BXA
        mov     ebp, offset TMP
#ifdef TEST
        jmp     test_main
#endif
        /* The host key: the scalar goes to D and the point to HOSTPUB,
         * where it completes the K_S that ends the image.  X2, Y2, K are
         * adjacent, and so are HOSTPUB and D. */
        call    keygen
        lea     esi, [C(sX2)]
        mov     di, LO(HOSTPUB)                 /* rdi: a slot */
        mov     cl, 96                          /* rcx was 0 */
        rep movsb
        /* socket(AF_INET, SOCK_STREAM, 0) */
        push    2
        pop     rdi
        push    1
        pop     rsi
        xor     edx, edx
        push    41
        pop     rax
        syscall
        xchg    eax, edi                        /* edi = fd, eax = 2 */
        mov     [LFD], edi
        /* setsockopt(fd, SOL_SOCKET = 1, SO_REUSEADDR = 2, &nonzero, 56):
         * the 56 still on the stack is the length, and what the stack then
         * starts with is argc */
        xchg    eax, edx                        /* edx = 2, eax = 0 */
        pop     r8
        mov     r10, rsp
        mov     al, 54
        syscall
        /* bind(fd, sockaddr, 16) */
        mov     esi, offset sockaddr
        mov     dl, 16
        mov     al, 49                          /* eax was 0 */
        syscall
        /* listen(fd, positive) */
        xchg    eax, edx                        /* edx = bind's result, eax = 16 */
        mov     al, 50
        syscall
        or      eax, edx
        jns     die                             /* i.e. serve; closing fd 0
                                                   on the way is harmless */
        push    60
        pop     rax
        syscall                                 /* exit(fd) */

/* ---------------- one connection after another ----------------
 * die abandons the connection from wherever a failed read or write or a
 * malformed packet is noticed: close it, reset the stack (nothing on it
 * is needed again) and accept the next.  A failed accept() leaves a bad
 * descriptor, whose first write comes back here. */
die:
#ifdef TEST
        push    60
        pop     rax
        xor     edi, edi
        syscall
#endif
        mov     edi, [CFD]
        push    3
        pop     rax
        syscall
        mov     esp, offset STACKTOP
serve:
        mov     edi, [LFD]
        xor     esi, esi
        push    43
        pop     rax
        syscall                                 /* accept(lfd, 0, -) */
        mov     [CFD], eax
        mov     [SEQ], rsi                      /* 0: also the length keystream */

        /* The ephemeral key pair first: 16 bytes of its public point are
         * our KEXINIT's cookie (random, ours, and public anyway). */
        call    keygen

        /* Version exchange.  The transcript the exchange hash covers is
         * assembled at HB as the strings arrive, each with its 4-byte
         * length in front: V_C V_S I_C I_S K_S Q_C Q_S K. */
        mov     si, LO(vsline)                  /* rsi: the affine program */
        push    17
        pop     rdx
        call    xsend
        mov     si, LO(HB + 4)                  /* rsi: what was sent */
        push    rsi
        /* a byte at a time up to the LF, 252 bytes at most: HB is
         * 256-aligned and the loop ends when the pointer's low byte wraps */
1:      inc     edx                             /* xio leaves it 0 */
        call    xrecv
        cmp     byte ptr [rsi-1], 10
        je      2f
        test    sil, sil
        jnz     1b
2:      pop     rdi
        sub     esi, 2                          /* without the CR LF */
        lea     eax, [rsi-4]
        mov     [rdi-1], al                     /* V_C's length: one byte, the
                                                   other three stay zero */

        /* The client's KEXINIT, received so that its payload lands behind
         * the length it gets as I_C; V_S (19 bytes with its length) then
         * goes in front, over the packet's first byte. */
        push    rsi
        lea     edi, [rsi+18]
        call    recv_packet
        pop     rdi
        mov     si, LO(vs)                      /* rsi: the packet */
        push    19
        pop     rcx
        rep movsb
        mov     edx, eax
        bswap   eax
        stosd
        add     edi, edx

        /* Our KEXINIT (I_S, 165 bytes): type, cookie, then the name-lists
         * kex, host key, cipher x 2, mac x 2 (empty: the cipher is an
         * AEAD), compression x 2, languages x 2 (empty), no guess, 0. */
        mov     eax, 0xa5000000
        stosd
        push    rdi
        mov     al, 20
        stosb
        lea     esi, [C(sX2)]
        mov     cl, 16                          /* rcx was 0 */
        rep movsb
        mov     si, LO(kexstr)                  /* rsi: a slot */
        mov     cl, 45                          /* kex and host key lists */
        rep movsb
        sub     esi, 78                         /* encstr, in front of them */
        mov     cl, 33
        rep movsb
        lea     esi, [rdi-33]                   /* and again */
        mov     cl, 33
        rep movsb
        xor     eax, eax
        stosq
        movabs  rax, 0x656e6f6e04000000         /* 0 0 0 4 "none" */
        stosq
        stosq
        xor     eax, eax
        stosq
        stosd
        stosb
        pop     rsi
        push    rdi                             /* t: stays on the stack */
        lea     edi, [rbp+5]
        mov     cl, 165
        rep movsb
        call    sendp

        /* KEX_ECDH_INIT, received so that its string Q_C lies at t + 108,
         * behind where K_S goes; K_S (108 bytes with its length) is then
         * written over the packet's header and type.  Only what follows
         * is taken from the packet, as it is: a malformed one gives a
         * transcript the client cannot verify. */
        pop     rdi
        push    rdi
        add     edi, 102
        call    recv_packet
        pop     rdi
        push    rdi
        mov     eax, 0x68000000
        stosd
        mov     si, LO(hkstr)                   /* rsi: the packet */
        push    104
        pop     rcx
        rep movsb                               /* 40 from the image, 64 bss */
        /* Q_S behind Q_C; its length and 04 are copied from Q_C's */
        mov     esi, edi
        add     edi, 69
        movsd
        movsb
        push    rsi                             /* Q_C's x || y */
        lea     esi, [C(sX2)]
        mov     cl, 64
        rep movsb
        pop     rsi
        push    rdi                             /* mp: stays on the stack */
        call    smult                           /* K = x of [k] Q_C */
        pop     rdi
        push    rdi
        lea     esi, [C(sX2)]
        call    mpint
        /* H = SHA-256(transcript), stored behind K: the key derivation
         * hashes K || H || ... from there */
        mov     r14d, edi
        mov     si, LO(HB)                      /* rsi: a slot */
        mov     edx, edi
        call    sha256

        /* The signature: a nonce and its point, SHA-256(H) into its slot
         * (after the multiplication, which uses that slot), the program. */
        call    keygen
        mov     esi, r14d
        lea     edi, [r14+32]
        lea     edx, [C(sX1)]
        call    sha256
        add     r8d, 32                         /* n follows p */
        mov     si, LO(ecdsa_prog)              /* rsi: the stack */
        call    run
        add     r8d, 32                         /* 2^130-5 follows n: set for
                                                   every Poly1305 from here */

        /* KEX_ECDH_REPLY: K_S and Q_S from the transcript, then
         * string(string "ecdsa-sha2-nistp256" || string(mpint r || mpint s)) */
        lea     edi, [rbp+5]
        mov     al, 31
        stosb
        mov     esi, [rsp+8]                    /* t */
        mov     cl, 108                         /* rcx was 0 */
        rep movsb
        add     esi, 69
        mov     cl, 69
        rep movsb
        push    rdi
        scasd
        lea     esi, [rbp+10]                   /* the name, as K_S starts */
        mov     cl, 23
        rep movsb
        push    rdi
        scasd
        lea     esi, [C(sX2)]
        call    mpint
        call    mpint
        mov     cl, 2                           /* the two lengths left open */
3:      pop     rsi
        lea     eax, [rdi-4]
        sub     eax, esi
        bswap   eax
        mov     [rsi], eax
        loop    3b
        call    sendp

        /* NEWKEYS: the client's comes as soon as it has the reply; it is
         * a bare type byte and is sent straight back.  (Like every packet
         * from here on it is taken for what the protocol says comes next;
         * a client that sends something else gets nowhere.) */
        call    recvt
        xchg    eax, ecx
        call    send_packet

        /* Key derivation (RFC 4253 7.2), 'C' client to server, 'D' server
         * to client: K_2 = HASH(K || H || id || H), K_1 = HASH(K || H ||
         * K_2), hashed straight into the key words of the four ChaCha20
         * states, which are 64 bytes apart.  The session id is H: this
         * server never rekeys.  al counts 2 id + (0 for K_2, 1 for K_1). */
        mov     edx, offset CST + 16
        mov     al, 2 * 'C'
4:      push    rax
        lea     edi, [r14+32]                   /* behind K || H */
        shr     al, 1
        jc      5f
        stosb                                   /* id || H */
        mov     esi, r14d
        jmp     6f
5:      lea     esi, [rdx-64]                   /* the K_2 just made */
6:      push    32
        pop     rcx
        rep movsb
        mov     esi, [rsp+8]                    /* mp */
        call    sha256
        lea     edx, [rdi+32]                   /* the next state's key */
        pop     rax
        inc     eax
        cmp     al, 2 * 'D' + 2
        jne     4b

        /* SERVICE_REQUEST -> SERVICE_ACCEPT: the same payload, type + 1 */
        call    recvt
        inc     byte ptr [rbp+5]
        xchg    eax, ecx
        call    send_packet

        /* USERAUTH.  One request is acceptable - user "user", service
         * "ssh-connection", method "password", no change, "password123" -
         * and it is a fixed 55-byte message, so the buffer is matched
         * against it whole.  (The payload's length is not looked at: the
         * 55 bytes compared are the client's own - this packet, or what
         * it sent before - or this connection's KEX reply, which is
         * longer than that and was built here; none of them holds the
         * password unless the client knew it.)  Anything else gets
         * USERAUTH_FAILURE "password", no partial success: byte 51, then
         * want[] from its method's length on. */
        shl     eax, 27                         /* xio left it 1 */
        xchg    eax, r14d                       /* 00 00 00 08 on the wire */
5:      call    recvt
        mov     cl, 55                          /* rcx was below 16: the last
                                                   thing run was chacha */
        mov     si, LO(want)                    /* rsi: the packet */
        lea     edi, [rbp+5]
        repe cmpsb
        je      7f
        mov     al, 51
        mov     si, LO(want + 31)               /* rsi: inside want */
        mov     cl, 9                           /* rcx was below 55 */
        call    send_chan
        jmp     5b
7:      mov     byte ptr [rbp+5], 52            /* USERAUTH_SUCCESS */
        inc     ecx
        call    send_packet

        /* CHANNEL_OPEN "session": the sender's channel is at a fixed
         * place.  Confirmation, the message, CLOSE (RFC 4254 5.3 lets a
         * channel close without EOF). */
        call    recvt
        mov     r14d, [rbp+17]
        mov     al, 91
        mov     si, LO(confbody)                /* rsi: the packet */
        mov     cl, 12                          /* rcx was below 16 */
        call    send_chan
        mov     al, 94
        mov     si, LO(hello)                   /* rsi: what was sent */
        push    17
        pop     rcx
        call    send_chan
        mov     al, 97
        xor     ecx, ecx
        call    send_chan
        /* then read and drop what the client still sends, 4096 bytes at
         * a time, until it closes the connection: that ends in die */
8:      mov     esi, ebp
        mov     dh, 16                          /* xio left edx 0 */
        call    xrecv
        jmp     8b

/* ---------------- packets out ---------------- */
/* send [al][r14d][ecx bytes at rsi] */
send_chan:
        lea     edi, [rbp+5]
        stosb
        mov     eax, r14d
        stosd
        rep movsb
/* send the packet whose payload is [TMP+5, rdi) */
sendp:
        lea     ecx, [rdi-5]
        sub     ecx, ebp
/* send the packet whose payload (ecx bytes) is at TMP+5: length and
 * padding length in front, zero padding behind, and once three packets
 * have gone out (KEXINIT, the reply, NEWKEYS) encrypted, with the tag
 * behind it all.  The packet length is the smallest that leaves at least
 * 4 bytes of padding and makes the encrypted part a multiple of 8: all of
 * it before NEWKEYS, all but the length word after. */
send_packet:
        cmp     dword ptr [SEQ], 3
        sbb     edx, edx
        and     edx, 4                          /* 4 before NEWKEYS, else 0 */
        lea     eax, [rcx+rdx+12]
        and     al, 0xf8
        sub     eax, edx                        /* packet length */
        lea     edi, [rbp+rcx+5]
        push    rax
        sub     eax, ecx
        dec     eax                             /* padding length */
        mov     [rbp+4], al
        xchg    eax, ecx
        xor     eax, eax
        rep stosb                               /* rdi = the packet's end */
        test    edx, edx
        jnz     1f
        pop     rcx
        push    rcx
        add     ecx, 64
        mov     edx, offset CST + 192
        call    crypt                           /* encrypts the payload */
1:      pop     rax
        lea     ecx, [rax+4]
        bswap   eax
        xor     eax, [KS]
        mov     [rbp], eax                      /* the length, encrypted */
        cmp     dword ptr [SEQ], 3
        jb      2f
        call    poly                            /* rdi = the tag's end */
2:      mov     edx, edi
        sub     edx, ebp
        inc     dword ptr [SEQ]
        mov     esi, ebp
        /* fall through */

/* write / read edx > 0 bytes at rsi, all of them; rsi ends behind them
 * and edx 0.  Failure and end of file are die. */
xsend:  push    1
        pop     rax
        jmp     xio
xrecv:  xor     eax, eax
xio:    push    rax
        mov     edi, [CFD]
        syscall
        cmp     eax, 1
        jl      .Ldie                           /* ZF is clear: see there */
        add     esi, eax
        sub     edx, eax
        pop     rax
        jnz     xio
        ret

/* ---------------- packets in ---------------- */
/* Receive a packet into TMP / at rdi (before NEWKEYS only): the packet is
 * read in place - length, padding length, payload, padding, tag - and
 * eax = the payload's length, the payload itself 5 bytes in.  Before the
 * rest is read the length is decrypted with keystream that crypt()
 * leaves at KS (zero before NEWKEYS); the tag is checked before the
 * payload is decrypted. */
recvt:  mov     edi, ebp
recv_packet:
        push    rdi
        xor     ecx, ecx                        /* no tag */
        cmp     dword ptr [SEQ], 3
        jb      1f
        mov     edx, offset CST + 64
        mov     cl, 60
        call    crypt                           /* length keystream, tag key */
        mov     cl, 16                          /* rcx was below 16 */
1:      pop     rsi
        push    rsi
        push    rcx
        push    4
        pop     rdx
        call    xrecv
        mov     eax, [rsi-4]
        xor     eax, [KS]
        bswap   eax
        dec     eax                             /* packet length - 1 */
        cmp     eax, MAXPKT - 1
        ja      .Ldie                           /* 0, or too long */
        pop     rdx
        push    rax
        push    rdx
        lea     edx, [rdx+rax+1]
        call    xrecv
        pop     rcx
        pop     rax
        pop     rsi
        jrcxz   2f
        push    rax
        lea     ecx, [rax+5]
        call    poly
.Ldie:  jnz     die                             /* every failed check here and
                                                   in xio arrives with ZF clear */
        pop     rax
        push    rax
        lea     ecx, [rax+65]
        mov     edx, offset CST + 64
        call    crypt                           /* decrypts the payload */
        pop     rax
        mov     esi, ebp
2:      movzx   edx, byte ptr [rsi+4]
        sub     eax, edx                        /* less the padding */
        jb      .Ldie
        ret

/* ---------------- ChaCha20 (chacha20-poly1305@openssh.com) ----------------
 * A direction's 64-byte key is K_2 (payload) || K_1 (length); the nonce
 * is the sequence number.  The length is encrypted with K_1, block 0; the
 * Poly1305 key is the first 32 bytes of K_2's block 0, and the payload is
 * encrypted with K_2 from block 1.
 *
 * crypt: rdx = the K_1 state of a direction (its K_2 state precedes it).
 * Zeroes KS and the Poly1305 key, XORs 4 bytes of K_1 keystream into KS
 * and ecx bytes of K_2 keystream, from block 0, over the bytes from PK
 * on: 60 give the Poly1305 key (and 28 unused bytes); 64 + n reach over
 * the length word at TMP, which the caller rewrites, and the n payload
 * bytes behind it.
 *
 * chacha: XOR ecx > 0 bytes (a multiple of 4) at rdi with the keystream,
 * from block 0, of the state at rdx, whose key words are set.  Preserves
 * rdx; leaves rcx below 16. */
crypt:
        push    rcx
        lea     edi, [KS]
        push    rdi
        push    36
        pop     rcx
        xor     eax, eax
        rep stosb
        pop     rdi
        mov     cl, 4
        call    chacha
        sub     edx, 64
        lea     edi, [PK]
        pop     rcx
chacha:
        push    rbp
        push    rcx                             /* bytes left */
        push    rdi                             /* where */
        mov     edi, edx
        movabs  rax, 0x3320646e61707865         /* "expand 32-byte k" */
        stosq
        movabs  rax, 0x6b20657479622d32
        stosq
        and     dword ptr [rdi+32], 0           /* block counter */
        mov     eax, [SEQ]                      /* nonce: the 64-bit big-endian
                                                   sequence number (its high
                                                   word stays zero) */
        bswap   eax
        mov     [rdi+44], eax
.Lcblock:
        mov     esi, edx
        lea     edi, [C(sX2)]                   /* x: a working copy */
        push    16
        pop     rcx
        rep movsd
        sub     edi, 64
        /* 80 quarter rounds.  Each word of qr packs the indices d:c:b:a;
         * one body does a += b, d = rol(d ^ a) and - with the word's
         * bytes swapped, which exchanges a/c and b/d - c += d, b = rol(b
         * ^ c), consuming the rotations 16, 12, 8, 7 low byte first. */
        push    -80
        pop     r9
.Lcqr:  mov     eax, r9d
        and     eax, 7
        movzx   esi, word ptr [qr + rax*2]
        mov     ecx, 0x07080c10
.Lcstep:
        mov     eax, esi
        and     eax, 15                         /* a */
        mov     ebp, esi
        shr     ebp, 4
        and     ebp, 15                         /* b */
        mov     r10d, esi
        shr     r10d, 12                        /* d */
        mov     ebp, [rdi+rbp*4]
        add     ebp, [rdi+rax*4]
        mov     [rdi+rax*4], ebp
        xor     ebp, [rdi+r10*4]
        rol     ebp, cl
        mov     [rdi+r10*4], ebp
        ror     si, 8
        shr     ecx, 8
        jnz     .Lcstep
        inc     r9d
        jnz     .Lcqr
        /* keystream word = x + state; XOR up to 16 of them */
        mov     esi, edi
        pop     rdi
        pop     rbp
        xor     ecx, ecx
1:      lodsd
        add     eax, [rdx+rcx*4]
        xor     [rdi], eax
        scasd                                   /* rdi += 4 */
        sub     ebp, 4
        jbe     2f
        inc     ecx
        cmp     cl, 16
        jne     1b
        inc     dword ptr [rdx+48]
        push    rbp
        push    rdi
        jmp     .Lcblock
2:      pop     rbp
        ret

/* ---------------- Poly1305 ----------------
 * The tag over the ecx bytes at TMP with the key at PK, written behind
 * the message; ZF = it equals what was there (an OR of XORs: the time
 * does not depend on where they differ).  r8 = the address of 2^130-5.
 * h = (h + block || 01) r mod 2^130-5 runs as a two-instruction program
 * on the field arithmetic; that is big-endian, so the little-endian key
 * and blocks are stored reversed into the low end of their slots. */
#define cH C(sX2)
#define cR C(sY2)
#define cT C(sK)
poly:
        push    rcx                             /* bytes left */
        lea     edi, [cH]
        push    64
        pop     rcx
        lea     esi, [PK]
        push    16
        pop     rdx
        call    lecopy                          /* h = 0, r = key[0..15] */
        /* clamp r: bytes 3, 7, 11, 15 &= 15, bytes 4, 8, 12 &= 252 - one
         * mask on four unaligned words (the fourth's 252 hits a zero) */
        mov     cl, 4
1:      and     dword ptr [rdi+rcx*4-5], 0xffff0ffc
        loop    1b
        mov     esi, ebp
.Lpblock:
        pop     rax
        push    16
        pop     rdx
        cmp     eax, edx
        cmovb   edx, eax                        /* this block's bytes */
        sub     eax, edx
        push    rax
        lea     edi, [cT]
        mov     cl, 32                          /* rcx was 0 */
        call    lecopy
        mov     byte ptr [rdi-1], 1             /* the 1 above the block */
        push    rsi
        mov     si, LO(poly_prog)               /* rsi: the packet */
        call    run
        pop     rsi
        pop     rax
        push    rax
        test    eax, eax
        jnz     .Lpblock
        pop     rax
        /* tag = (h + key[16..31]) mod 2^128, little-endian, stored over
         * the old one a byte at a time; dl collects the differences.  The
         * carry of the addition sits in r9d while they are taken (rcl and
         * rcr leave ZF alone, so it is still the OR's on return). */
        cdq                                     /* eax was 0, CF is clear */
        mov     edi, esi                        /* behind the message */
        lea     esi, [PK + 16]
        mov     cl, 16                          /* rcx was 0 */
2:      lodsb
        adc     al, [cH + 15 + rcx]
        rcl     r9d, 1
        mov     ah, [rdi]
        stosb
        xor     ah, al
        or      dl, ah
        rcr     r9d, 1
        loop    2b
        ret

/* zero ecx bytes at rdi, then store the edx bytes at rsi reversed at the
 * end of them; rdi = the lowest byte stored, rsi advanced, rcx = 0 */
lecopy:
        xor     eax, eax
        rep stosb
        mov     ecx, edx
1:      dec     edi
        lodsb
        mov     [rdi], al
        loop    1b
        ret

/* ---------------- mpint ----------------
 * of the 32-byte number at rsi to rdi; both end behind it, rcx = 0.
 * Leading zero bytes are dropped (never the last byte), and a zero byte
 * is put back in front of a set top bit. */
mpint:
        push    32
        pop     rcx
1:      lodsb
        test    al, al
        loopz   1b
        dec     esi                             /* the first nonzero byte */
        inc     ecx                             /* and how many from it on */
        cmp     al, 0x80
        sbb     edx, edx
        inc     edx                             /* 1 if its top bit is set */
        lea     eax, [rcx+rdx]
        bswap   eax
        stosd
        mov     [rdi], dh                       /* 0, overwritten if unwanted */
        add     edi, edx
        rep movsb
        ret

/* ---------------- SHA-256 ----------------
 * of [rsi, rdi) to rdx, in one go: every message is assembled in a buffer
 * with 72 free bytes behind it, so it is padded in place.  The output may
 * overlap the padding.  Preserves rbx, r8, r12-r15; rdi ends behind the
 * digest, rcx = 0.
 *
 * Frame (W[i] is the message schedule, s the eight working variables
 * a..h as a window that slides down one word per round):
 *      rsp + 288       s at round 0
 *      rsp + 32 ..     W[63] .. W[0]
 *      rsp             the state
 * W is stored downwards so that round i finds W[i] just below the window
 * and overwrites it with the new a. */
sha256:
        push    rbp
        mov     r11d, edx
        mov     edx, edi
        sub     edx, esi                        /* length */
        mov     al, 0x80
        stosb
        push    71
        pop     rcx
        xor     eax, eax
        rep stosb
        lea     eax, [rdx+72]
        and     al, 0xc0
        add     eax, esi                        /* end of the padded message */
        shl     edx, 3
        bswap   edx
        mov     [rax-4], edx                    /* its length in bits */
        mov     r10d, eax
        mov     r9d, esi
        sub     esp, 320
        mov     edi, esp
        mov     si, LO(SHAH)                    /* rsi: the message */
        mov     cl, 8
        rep movsd
.Lsblock:
        lea     edi, [rsp+224]
        mov     cl, 16
1:      mov     eax, [r9+rcx*4-4]               /* W[15] first */
        bswap   eax
        stosd
        loop    1b
        add     r9d, 64
        mov     esi, esp
        mov     cl, 8
        rep movsd                               /* s = the state */
        lea     esi, [rdi-32]
        sub     edi, 100                        /* W[16] */
        std
        mov     cl, 48
2:      mov     eax, [rdi+8]                    /* W[i-2] */
        mov     edx, eax
        ror     eax, 2
        xor     eax, edx
        ror     eax, 17
        shr     edx, 10
        xor     eax, edx                        /* ror 17 ^ ror 19 ^ shr 10 */
        add     eax, [rdi+28]                   /* W[i-7] */
        add     eax, [rdi+64]                   /* W[i-16] */
        mov     edx, [rdi+60]                   /* W[i-15] */
        mov     ebp, edx
        ror     edx, 11
        xor     edx, ebp
        ror     edx, 7
        shr     ebp, 3
        xor     edx, ebp                        /* ror 7 ^ ror 18 ^ shr 3 */
        add     eax, edx
        stosd
        loop    2b
        cld
        mov     di, LO(SHAK)                    /* rdi: the stack */
        mov     cl, 64
3:      mov     eax, [rsi+16]                   /* e */
        mov     edx, eax
        ror     eax, 14
        xor     eax, edx
        ror     eax, 5
        xor     eax, edx
        ror     eax, 6                          /* ror 6 ^ ror 11 ^ ror 25 */
        mov     ebp, [rsi+20]
        xor     ebp, [rsi+24]
        and     ebp, edx
        xor     ebp, [rsi+24]                   /* ch(e, f, g) */
        add     eax, ebp
        add     eax, [rsi+28]                   /* h */
        add     eax, [rdi]                      /* K[i] */
        scasd                                   /* rdi += 4 */
        add     eax, [rsi-4]                    /* W[i] */
        add     [rsi+12], eax                   /* d += t1: the next e */
        mov     edx, [rsi]                      /* a */
        mov     ebp, edx
        ror     edx, 9
        xor     edx, ebp
        ror     edx, 11
        xor     edx, ebp
        ror     edx, 2                          /* ror 2 ^ ror 13 ^ ror 22 */
        add     eax, edx
        mov     edx, ebp
        or      edx, [rsi+4]
        and     edx, [rsi+8]
        and     ebp, [rsi+4]
        or      edx, ebp                        /* maj(a, b, c) */
        add     eax, edx
        sub     esi, 4
        mov     [rsi], eax                      /* the next a */
        loop    3b
        mov     cl, 8
4:      mov     eax, [rsi+rcx*4-4]
        add     [rsp+rcx*4-4], eax
        loop    4b
        cmp     r9d, r10d
        jb      .Lsblock
        mov     esi, esp
        mov     edi, r11d
        mov     cl, 8
5:      lodsd
        bswap   eax
        stosd
        loop    5b
        add     esp, 320
        pop     rbp
        ret

/* ---------------- P-256 ----------------
 * keygen: a fresh scalar in K and X2 || Y2 = [K] G.  The scalar is 32
 * random bytes, redrawn while their top word is 0 or ffffffff - both
 * read the same in either byte order - so it is uniform over [2^224,
 * 2^256 - 2^224), all below n: an ECDSA nonce must have no fixed bits,
 * and the lower bound is what lets the ladder use K + n.
 *
 * smult: X2 || Y2 = [K] P for the affine P at rsi (64 bytes, as on the
 * wire).  Double-and-add on K + n, which has bit 256 set: R = P stands
 * for that bit, then for each of the 256 bits below it R = 2R, S = R + P
 * (step_prog) and R = S if the bit is set - a copy either way.  The
 * prefixes of K + n never hit the formulas' exceptional cases (see
 * v31-trim/p256.c for the argument, which carries over unchanged).
 * Leaves r8 = the address of p and Z1 = 0. */
keygen:
        lea     edi, [C(sK)]
        push    32
        pop     rsi
        mov     eax, 318
        xor     edx, edx
        syscall                                 /* getrandom(K, 32, 0) */
        mov     eax, [rdi]
        inc     eax
        cmp     eax, 2
        jb      keygen
        mov     esi, offset gpoint
smult:
        lea     edi, [C(sX2)]
        push    64
        pop     rcx
        push    rsi
        rep movsb                               /* P */
        pop     rsi
        lea     edi, [C(sX1)]
        mov     cl, 64
        rep movsb                               /* R = P, */
        inc     byte ptr [rdi+31]               /* Z = 1 (it was 0) */
        lea     edi, [C(sKN)]
        lea     esi, [C(sK)]
        mov     r8d, offset PMOD
        lea     edx, [r8+32]
        call    radd                            /* K + n mod 2^256 */
1:      mov     si, LO(step_prog)               /* rsi: a slot */
        call    run
        movzx   eax, r12b                       /* bit 255 - r12b: bt counts */
        xor     al, 7                           /* a byte's bits from its low
                                                   end, hence the 7 */
        bt      [C(sKN)], eax
        lea     edi, [C(sX1)]
        mov     esi, edi
        lea     eax, [rdi+96]                   /* S */
        cmovc   esi, eax
        mov     cl, 96                          /* rcx was 0 */
        rep movsb
        inc     r12b
        jnz     1b
        mov     si, LO(affine_prog)             /* rsi: a slot */
        /* fall through */

/* Run the program at rsi on the slots, modulo [r8].  Wants rcx = 0 and
 * leaves it 0. */
run:
        lodsw
        cwde                                    /* the top bit is clear */
        test    al, al
        jz      2f
        push    rsi
        mov     cl, 3                           /* rcx was 0 */
1:      mov     edx, eax                        /* dst, src1, src2: */
        and     edx, 15
        shl     edx, 5
        lea     edx, [rbx+rdx-BXOFF]            /* the slot's address */
        push    rdx
        shr     eax, 4
        loop    1b
        pop     rdx
        pop     rsi
        pop     rdi
        mov     al, [rax + optab]               /* eax was the operation */
        add     eax, offset fp_add
        call    rax
        pop     rsi
        jmp     run
2:      ret

/* The field operations: [rdi] = [rsi] op [rdx] mod m, m at r8.  They
 * want rcx = 0 on entry and leave it 0; rdi and r8 are preserved.
 *
 * Add and subtract take three passes of a raw 256-bit add or subtract:
 *      add:  d = a + b,  d -= m,  d += m if that went below zero
 *      sub:  d = a - b,  d -= 0,  d += m if that went below zero
 * The carries of the first two passes differ exactly when the true
 * value is negative, so one test serves both.  The third pass always
 * runs (it adds zero otherwise): the time does not depend on the values.
 * a + b needs b <= m and a + b - m < 2^256; the result is below m if a
 * + b < 2m (so adding zero reduces an a < 2m). */
fp_addd:
        mov     esi, edi                        /* d += [rdx] */
fp_add:
        call    radd
        mov     edx, r8d
        jmp     1f
fp_sub:
        call    rsub
        lea     edx, [C(sZERO)]
1:      sbb     ah, ah                          /* the loops only use al */
        mov     esi, edi
        call    rsub
        sbb     ah, -1                          /* zero iff the carries differ */
        lea     edx, [C(sZERO)]
        cmovz   edx, r8d
        /* fall through */
/* [rdi] = [rsi] + [rdx] / [rsi] - [rdx] mod 2^256, CF = carry / borrow */
radd:
        clc
        mov     cl, 32
1:      mov     al, [rsi+rcx-1]
        adc     al, [rdx+rcx-1]
        mov     [rdi+rcx-1], al
        dec     ecx
        jnz     1b
        ret
rsub:
        clc
        mov     cl, 32
1:      mov     al, [rsi+rcx-1]
        sbb     al, [rdx+rcx-1]
        mov     [rdi+rcx-1], al
        dec     ecx
        jnz     1b
        ret

/* [rdi] = [rsi] * [rdx] mod m, for [rsi] < m and any 256-bit [rdx]; rdi
 * distinct from both.  Shift-and-add over the bits of [rdx], top down: r
 * = 2r + (bit ? a : 0), the addend picked with a cmov. */
fp_mul:
        mov     r9d, esi
        mov     r10d, edx
        push    rdi
        xor     eax, eax
        mov     cl, 32
        rep stosb
        pop     rdi
1:      mov     edx, edi
        call    fp_addd
        movzx   eax, r13b
        xor     al, 7
        bt      [r10], eax
        lea     edx, [C(sZERO)]
        cmovc   edx, r9d
        call    fp_addd
        inc     r13b
        jnz     1b
        ret

/* T0 = Z1^(m-2) mod m, the inverse of Z1 for a prime m, given T0 = Z1:
 * the top bit of m - 2 is set, and for each of the 255 below it T0 is
 * squared and, where the bit is set, multiplied by Z1.  The exponent is
 * public, so which program runs may depend on it.  It is read from a
 * copy of m with its last byte lowered by 2 (that byte is at least 2). */
fp_inv:
        lea     edi, [C(sKN)]
        mov     esi, r8d
        mov     cl, 32
        rep movsb
        sub     byte ptr [rdi-1], 2
        mov     r15b, 1
1:      movzx   eax, r15b
        xor     al, 7
        bt      [C(sKN)], eax
        mov     si, LO(inv_prog)                /* rsi: m, or the program */
        lea     eax, [rsi+5]
        cmovc   esi, eax
        call    run
        inc     r15b
        jnz     1b
        ret

#ifdef TEST
#include "test_main.S"
#endif

/* ---------------- data ---------------- */
inv_prog:
        MUL(sY2, sT0, sT0)                      /* bit clear: square */
        ADD(sT0, sY2, sZERO)
        END
        MUL(sY2, sT0, sT0)                      /* bit set: square, multiply */
        MUL(sT0, sY2, sZ1)
        END

/* ECDSA with [k] G in X2 || Y2, k in K, the hash z in X1 and the key in
 * D, all modulo n: r = x mod n, s = (z + r d) / k, left in X2 || Y2.
 * x and z are below 2^256 < 2n and go in as first operands or
 * multipliers, which may be unreduced. */
ecdsa_prog:
        ADD(sZ1, sK, sZERO)
        ADD(sT0, sK, sZERO)
        INV                                     /* T0 = 1/k */
        MUL(sT1, sD, sX2)                       /* r d */
        ADD(sT1, sX1, sT1)                      /* z + r d */
        MUL(sY2, sT0, sT1)                      /* s */
        ADD(sX2, sZERO, sX2)                    /* r */
        SUB(sZ1, sZ1, sZ1)                      /* Z = 0 again */
        END

/* One step of the ladder: R = 2R in place (dbl-1998-cmo-2 with a = -3,
 * projective), then S = R + P (add-1998-cmo-2 with Z2 = 1). */
step_prog:
        MUL(sT0, sX1, sX1)                      /* XX */
        MUL(sT1, sZ1, sZ1)                      /* ZZ */
        SUB(sT1, sT0, sT1)
        ADD(sT2, sT1, sT1)
        ADD(sT2, sT2, sT1)                      /* w = 3 (XX - ZZ) */
        MUL(sT3, sY1, sZ1)
        ADD(sT3, sT3, sT3)                      /* s = 2 Y Z */
        MUL(sT1, sT3, sT3)                      /* ss */
        MUL(sZ1, sT3, sT1)                      /* Z3 = s^3 */
        MUL(sT1, sY1, sT3)                      /* R = Y s */
        MUL(sSZ, sT1, sT1)                      /* RR */
        MUL(sT0, sX1, sT1)
        ADD(sT0, sT0, sT0)                      /* B = 2 X R */
        MUL(sT1, sT2, sT2)
        SUB(sT1, sT1, sT0)
        SUB(sT1, sT1, sT0)                      /* h = w^2 - 2B */
        MUL(sX1, sT1, sT3)                      /* X3 = h s */
        SUB(sT0, sT0, sT1)
        MUL(sT1, sT2, sT0)
        ADD(sSZ, sSZ, sSZ)
        SUB(sY1, sT1, sSZ)                      /* Y3 = w (B - h) - 2 RR */
        MUL(sT0, sY2, sZ1)
        SUB(sT0, sT0, sY1)                      /* u = y2 Z1 - Y1 */
        MUL(sT1, sT0, sT0)                      /* uu */
        MUL(sT2, sX2, sZ1)
        SUB(sT2, sT2, sX1)                      /* v = x2 Z1 - X1 */
        MUL(sT3, sT2, sT2)                      /* vv */
        MUL(sSX, sT2, sT3)                      /* vvv */
        MUL(sSY, sT3, sX1)                      /* R = vv X1 */
        MUL(sT3, sT1, sZ1)
        SUB(sT3, sT3, sSX)
        SUB(sT3, sT3, sSY)
        SUB(sT3, sT3, sSY)                      /* A = uu Z1 - vvv - 2R */
        MUL(sSZ, sSX, sZ1)                      /* Z3 = vvv Z1 */
        MUL(sT1, sSX, sY1)                      /* vvv Y1 */
        MUL(sSX, sT2, sT3)                      /* X3 = v A */
        SUB(sSY, sSY, sT3)                      /* R - A */
        MUL(sT2, sT0, sSY)
        SUB(sSY, sT2, sT1)                      /* Y3 = u (R - A) - vvv Y1 */
        END

nlow:   .byte   0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84
        .byte   0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51
gpoint: .byte   0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47
        .byte   0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2
        .byte   0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0
        .byte   0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
        .byte   0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b
        .byte   0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16
        .byte   0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce
        .byte   0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5

qr:     .word   0xc840, 0xd951, 0xea62, 0xfb73  /* columns */
        .word   0xfa50, 0xcb61, 0xd872, 0xe943  /* diagonals */
hello:  .ascii  "\0\0\0\015Hello World\r\n"
want:   .ascii  "\062\0\0\0\004user\0\0\0\016ssh-connection"
        .ascii  "\0\0\0\010password\0\0\0\0\013password123"
/* name-lists in wire form; the last three run on into bss as K_S =
 * string "ecdsa-sha2-nistp256" || string "nistp256" || string (04 || x ||
 * y), whose x || y is HOSTPUB */
encstr: .ascii  "\0\0\0\035chacha20-poly1305@openssh.com"
kexstr: .ascii  "\0\0\0\022ecdh-sha2-nistp256"
hkstr:  .ascii  "\0\0\0\023ecdsa-sha2-nistp256"
        .ascii  "\0\0\0\010nistp256"
        .ascii  "\0\0\0\101\004"
bss:
