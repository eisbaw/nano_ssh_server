/* Nano SSH Server - v31-trim: v30-chacha trimmed further - the same
 * algorithms (ecdh-sha2-nistp256, ecdsa-sha2-nistp256,
 * chacha20-poly1305@openssh.com), with the channel phase cut to what the
 * RFCs require, string instructions for the copies and byte-order loops,
 * and more of the constant data placed where it is used.  Single algorithm
 * path, no malloc or libc; fully static.  See optimization_log.txt for the
 * steps, their sizes and the tests. */

#include <stdint.h>
#include "nolibc.h"            /* mem/str, raw syscalls, sockets */
#include "random_minimal.h"     /* randombytes_buf() via getrandom(2) */
#include "fp.h"
#include "p256.h"
#include "chapoly.h"
#include "sha256.h"

/* memcpy() as an inline rep movsb that returns the end of the copy: the
 * count goes straight into rcx and no other register is lost, which at
 * most call sites is smaller than the call.  MOVSB() leaves the source
 * pointer behind the copy as well. */
#define MOVSB(d, s, n) \
    __asm__ volatile ("rep movsb" : "+D"(d), "+S"(s), "+c"(n) :: "memory")
static inline __attribute__((always_inline))
uint8_t *cpy(void *d, const void *s, size_t n) {
    MOVSB(d, s, n);
    return d;
}

#ifndef PORT
#define PORT 2222          /* make PORT=n builds a server on another port */
#endif
#define V_S  "SSH-2.0-NanoSSH"
/* V_S as the exchange hash wants it (SSH string) with the CR LF the version
 * line wants after it: one constant serves both.  tiny.ld puts it at
 * 0x40005d, overlapping the ELF header. */
__attribute__((section(".hdr.vs")))
static const uint8_t vs[4 + sizeof(V_S) + 1] = "\0\0\0\x0f" V_S "\r\n";

#define MSG_SERVICE_REQUEST 5
#define MSG_SERVICE_ACCEPT 6
#define MSG_KEXINIT 20
#define MSG_NEWKEYS 21
#define MSG_KEX_ECDH_INIT 30
#define MSG_KEX_ECDH_REPLY 31
#define MSG_USERAUTH_REQUEST 50
#define MSG_USERAUTH_SUCCESS 52
#define MSG_CHANNEL_OPEN 90
#define MSG_CHANNEL_OPEN_CONFIRMATION 91
#define MSG_CHANNEL_DATA 94
#define MSG_CHANNEL_CLOSE 97

/* Big-endian 32-bit wire fields: one bswap + one unaligned access (u32a). */
#define PUT32(b,v) (*(u32a *)(b) = __builtin_bswap32(v))
#define GET32(b) __builtin_bswap32(*(const u32a *)(b))

/* One direction of the connection: the ChaCha20 states of K_2 (payload)
 * and K_1 (length), derived per RFC 4253 7.2 straight into their key words
 * (chapoly.h fills in the rest), and the packet sequence number, set to 3
 * at NEWKEYS (three packets precede it in each direction).  c2s.seq
 * doubles as the "keys in use" flag of both directions, which switch on
 * together (the client's NEWKEYS is received and ours sent before the keys
 * are derived): serve() clears it, and it is never 0 again until the next
 * connection. */
typedef struct {
    uint32_t seq;               /* first: a one-byte displacement */
    uint32_t k2[16], k1[16];
} cstate_t;

static cstate_t cs[2];          /* [0] client->server, [1] server->client */
#define c2s cs[0]
#define s2c cs[1]

/* Host key, generated once at startup: the secret is P256_D (p256.h);
 * the public point lives inside rep[], the KEX_ECDH_REPLY message.  Its
 * constant head (type, the length of the host key blob K_S = string name
 * || string curve || string 04||x||y, up to and including the 04) is the
 * last 45 bytes of the image: tiny.ld defines rep there, so the rest of it
 * is bss (zero) and nothing has to copy the head in.  main() adds the host
 * point and the length prefix of Q_S; each connection only fills in Q_S
 * and the signature, and the exchange hash copies K_S and Q_S from here in
 * their wire form, length prefixes included. */
extern uint8_t rep[512];
#define REP_KS   1      /* string K_S: 4 + 104 bytes */
#define REP_HOST 45     /* host public x || y */
#define REP_QS   109    /* string Q_S: 4 + 65 bytes */
#define REP_SIG  178    /* string signature */

/* KEXINIT name-lists (kex, hostkey, cipher, compression; the mac and
 * language lists are empty: an AEAD cipher carries its own MAC, and
 * OpenSSH skips MAC negotiation for it) in wire form, each with its 4-byte
 * length prefix, so a list is copied whole; the zero in front makes the
 * first four bytes an empty list.  They share their bytes with the
 * constant head of the KEX_ECDH_REPLY (from offset 64, which is rep[0]:
 * see there): type, length of K_S, then K_S up to and including the 04 of
 * its point, where the hostkey name is a string already; the signature
 * blob copies it from there too.  Sized, so no trailing NUL.  tiny.ld
 * places this last in the image. */
__attribute__((section(".tail.nl")))
static const char nl[109] =
    "\0" "\0\0\0\x1d" "chacha20-poly1305@openssh.com"             /* 0, 1 */
    "\0\0\0\x04" "none"                                          /* 34 */
    "\0\0\0\x12" "ecdh-sha2-nistp256"                            /* 42 */
    "\x1f" "\0\0\0\x68" "\0\0\0\x13" "ecdsa-sha2-nistp256"      /* 64 */
    "\0\0\0\x08" "nistp256" "\0\0\0\x41" "\x04";               /* 92 */
#define NL_HOSTKEY (nl + 73)    /* the name; its length prefix precedes */

/* Three constants that live in fields of the ELF header the kernel never
 * reads (tiny.ld): the listening address INADDR_ANY:PORT (p_paddr; bind()
 * reads 16 bytes but ignores the 8 of sin_zero), the integer 1 for
 * SO_REUSEADDR (the program header's PT_LOAD type word) and the
 * CHANNEL_OPEN_CONFIRMATION body - server channel 0, window 32768, max
 * packet 16384 (the top of e_phoff and e_shoff). */
extern const struct sockaddr hdr_sockaddr;
extern const int hdr_one;
extern const uint8_t hdr_win[12];

/* ---- I/O ----
 * The connection's socket. One connection at a time, so it is a global
 * rather than an argument threaded through every packet function.
 * read(2) and write(2) differ only in the syscall number, so one loop with
 * the number as a parameter serves both directions. */
static unsigned lfd, cfd;       /* unsigned: no sign extension to pass */

/* Abandon the connection - a failed read or write, a malformed packet -
 * from wherever it is noticed: close the socket and re-enter serve() on a
 * private stack in bss.  Nothing on the old stack is needed again, so there
 * is no unwinding, and the stack is reset to the same top each time.  The
 * I/O helpers die for their callers, so no call site tests a result. */
static uint8_t stk[32768] __attribute__((aligned(16)));
void serve(void);
__attribute__((noreturn)) static void die(void) {
    close(cfd);
    __asm__ volatile("mov %0, %%esp\n\tjmp serve"
                     : : "i"(stk + sizeof stk - 8) : "memory");
    __builtin_unreachable();
}

/* Every post-KEX packet is received into this one buffer, and every
 * packet is sent from it; tmp is the payload (5 bytes in, past the length
 * and padding-length bytes).  It is sized for the largest packet accepted,
 * as every buffer a packet is received into is (bss is free), so the
 * bound is one constant. */
static uint8_t tmpbuf[4096 + 16];
#define tmp (tmpbuf + 5)

static void xio(long sysno, void *b, size_t n) {
    do {
        long r;
        /* b and n are the syscall's own rsi and rdx, which it preserves:
         * they stay in those registers for the whole loop */
        __asm__ volatile ("syscall"
                          : "=a"(r), "+S"(b), "+d"(n)
                          : "a"(sysno), "D"(cfd)
                          : "rcx", "r11", "memory");
        if (r <= 0) die();
        b = (uint8_t *)b + r; n -= r;
    } while (n);
}
#define xsend(b, n) xio(SYS_write, (void *)(b), (n))
#define xrecv(b, n) xio(SYS_read, (b), (n))

/* ---- chacha20-poly1305@openssh.com on one packet ----
 * Poly1305 one-time key = ChaCha20(K_2, seq, counter 0)[0..31]; the tag covers
 * the encrypted length and the encrypted payload, and goes right behind
 * them, where a received packet's tag is: poly1305() returns whether the
 * tag it writes differs from what was there. */
static unsigned aead_tag(cstate_t *c, uint8_t *pkt, size_t pktlen) {
    uint8_t pk[32] = { 0 };
    chacha_xor(c->k2, pk, c->seq, 0, 32);
    return poly1305(pkt + 4 + pktlen, pk, pkt, 4 + pktlen);
}

/* Encrypt (dec = 0) or decrypt (dec = 1) pkt = length || payload in place
 * and write the tag behind it, returning nonzero if that replaced a
 * different one (only a received packet's difference is looked at).
 * Sending encrypts and then tags; receiving tags the ciphertext and then
 * decrypts - the length word is XORed first in both cases, which on
 * receive re-encrypts the copy recv_packet() decrypted to learn how much
 * to read, so the tag sees the bytes that were on the wire.  One copy of
 * each call for both directions: the payload is XORed in pass dec of two,
 * the tag taken in pass 0. */
__attribute__((noinline))
static unsigned aead(cstate_t *c, uint8_t *pkt, size_t pktlen, int dec) {
    unsigned d = 0;
    chacha_xor(c->k1, pkt, c->seq, 0, 4);
    for (int i = 0; i < 2; i++) {
        if (i == dec) chacha_xor(c->k2, pkt + 4, c->seq, 1, pktlen);
        if (!i) d = aead_tag(c, pkt, pktlen);
    }
    c->seq++;
    return d;
}

/* ---- send the binary packet whose payload (plen bytes) is at tmp ----
 * Encrypted once c2s.seq is set (see cstate_t).  The packet is built
 * around the payload in place: length and padding length in front of it,
 * padding and the tag behind it - a reply to a received packet is built
 * where the request was, so nothing is copied. */
static void send_packet(size_t plen) {
    /* Padding is over packet_length||padding_length||payload before
     * NEWKEYS; the AEAD leaves the length word out (OpenSSH aadlen = 4).
     * Block size is 8 either way. */
    /* the smallest length >= plen + 5 (padding of at least 4) that is 0
     * mod 8 when encrypted and 4 mod 8 before (the length word counted) */
    unsigned a = c2s.seq ? 0 : 4;
    uint32_t pktlen = ((plen + 12 + a) & ~7u) - a;
    unsigned pad = pktlen - 1 - plen;   /* 4..11: no zero extension */
    size_t total;
    PUT32(tmpbuf, pktlen);
    tmpbuf[4] = pad;
    randombytes_buf(tmp + plen, pad);
    total = 4 + pktlen;
    if (c2s.seq) {
        aead(&s2c, tmpbuf, pktlen, 0);
        total += 16;
    }
    xsend(tmpbuf, total);
}

/* Send [type][recipient channel][body]: the shape of every channel-level
 * message this server emits (open confirmation, data, close), and of the
 * userauth failure. */
static void send_chan(uint8_t type, const void *body, size_t n,
                      uint32_t chan) {
    /* body and n arrive where memcpy() wants them */
    tmp[0] = type;
    *(u32a *)(tmp + 1) = chan;      /* in wire order already */
    memcpy(tmp + 5, body, n);
    send_packet(5 + n);
}

/* ---- recv one binary packet into buf (sizeof tmpbuf bytes of room) ----
 * The packet is read in place - length, padding length, payload, padding,
 * tag - and the payload length is returned; the payload itself starts at
 * buf + 5, which callers address through a second pointer, so there is no
 * copy out of a private buffer. Dies on any error. */
static size_t recv_packet(uint8_t *buf) {
    uint32_t pktlen;
    size_t pad, taglen = 0;
    xrecv(buf, 4);
    if (c2s.seq) {
        chacha_xor(c2s.k1, buf, c2s.seq, 0, 4);
        taglen = 16;
    }
    pktlen = GET32(buf);
    /* compared as size_t: a 32-bit sum would wrap for lengths near 2^32 */
    if (pktlen > sizeof tmpbuf - 20) die();
    xrecv(buf + 4, pktlen + taglen);
    if (taglen && aead(&c2s, buf, pktlen, 1)) die();
    pad = buf[4];
    /* signed: this also rejects pktlen 0 and 1 (a stale padding-length
     * byte is then compared, and loses to -1 or 0) */
    if ((int)pad >= (int)pktlen - 1) die();
    return pktlen - 1 - pad;
}

/* Receive the next post-KEX packet into tmpbuf. */
static size_t rp(void) { return recv_packet(tmpbuf); }

/* Receive a packet that must be of type t (dies otherwise); returns its
 * payload length. */
static size_t rexp(uint8_t t) {
    size_t n = rp();
    if (tmp[0] != t) die();
    return n;
}

/* ---- KEXINIT payload ---- */
static size_t build_kexinit(uint8_t *p) {
    /* KEXINIT carries ten name-lists: kex, hostkey, enc c2s/s2c, mac
     * c2s/s2c, comp c2s/s2c, lang c2s/s2c. Six of them repeat, so the four
     * distinct names are stored once (in nl, in wire form) and indexed by
     * offset; the four empty lists are nl's first four bytes, and an
     * eleventh empty list writes four of the five zero bytes that follow
     * (first_kex_packet_follows and the reserved word). */
    static const uint8_t off[11] = { 42, 69, 1, 1, 0, 0, 34, 34, 0, 0, 0 };
    uint8_t *q = p;
    *q++ = MSG_KEXINIT;
    randombytes_buf(q, 16); q += 16;
    /* i runs up to 0, so the loop ends on the inc (opaque, or GCC counts
     * 0..11 again); a list's length is read through the pointer that the
     * copy then takes, and the copy leaves q behind the list */
    long i = -11;
    __asm__("" : "+r"(i));
    do {
        const char *s = nl + off[11 + i];
        __asm__("" : "+S"(s));
        size_t n = 4 + (uint8_t)s[3];
        MOVSB(q, s, n);
    } while (++i);
    *q = 0;
    return q + 1 - p;
}

/* ---- mpint (shared secret K, ECDSA r and s) ----
 * Leading zero bytes are dropped; a value with its top bit set gets one
 * zero byte back so it stays positive.  That byte is written unconditionally
 * (the copy overwrites it when it is not wanted).  The last byte is never
 * dropped, so the top-bit test needs no bound (the value 0, which would
 * want an empty encoding, is not a P-256 coordinate or signature half).
 * Returns the end of the mpint, where the copy left it. */
static uint8_t *put_mpint(uint8_t *b, const uint8_t *d, size_t n) {
    /* repe scasb stops one past the first nonzero byte, the count one
     * short of what is left from it (or on the last byte if all are 0) */
    __asm__ ("repe scasb" : "+D"(d), "+c"(n)
                          : "a"(0), "m"(*(const uint8_t (*)[32])d));
    d--, n++;
    size_t pad = *d;            /* in a size_t first: no byte shift */
    pad >>= 7;
    uint8_t *e = b + 4 + pad;
    b[4] = 0;
    PUT32(b, (uint32_t)(n + pad));
    __asm__ volatile ("rep movsb" : "+D"(e), "+S"(d), "+c"(n) :: "memory");
    return e;
}

/* ---- derive one 64-byte key per RFC4253 7.2 ----
 * K1 = HASH(K || H || id || session_id), K2 = HASH(K || H || K1); the only
 * keys this cipher needs are the two 64-byte encryption keys, so the length
 * is fixed and both hashes are always taken.  mp holds K || H with room
 * behind it: id || H goes there for K1, and K1 replaces it for K2 (the
 * session id is H on a server that never rekeys).  The two halves go to
 * out and out + 64: the key words of the two ChaCha20 states. */
static void derive(uint8_t *out, uint8_t *mp, const uint8_t *H, char id) {
    uint8_t *t = (uint8_t *)H + 32;
    t[0] = (uint8_t)id;
    memcpy(t + 1, H, 32);
    sha256(out, mp, t + 33 - mp);
    cpy(t, out, 32);
    sha256(out + 64, mp, t + 32 - mp);
}

/* A fresh P-256 scalar in the multiplier's scalar slot: 32 random bytes,
 * redrawn while their top word is 0 or 0xffffffff - one compare, as both
 * values read the same in either byte order.  n's top word is 0xffffffff,
 * so what is kept is below n: uniform over [2^224, 2^256 - 2^224), one
 * part in 2^32 short of uniform over [1, n).  An ECDSA nonce must have no
 * fixed bits - a lattice attack turns a fixed top bit into the host key -
 * and the lower bound is what lets the ladder in p256.c use k + n as its
 * scalar without a second case. */
static void rand_scalar(void) {
    do randombytes_buf(P256_K, 32);
    while ((uint32_t)(*(u32a *)P256_K + 1) < 2);
}

/* A key pair: a fresh scalar, and [k]G at p256_w[0..1] - the host key,
 * the ephemeral key and the signature's nonce point. */
static void keygen(void) {
    rand_scalar();
    p256_smult(p256_g);
}

__attribute__((noreturn, always_inline)) static inline void handle(void) {
    /* Every string the exchange hash covers is hashed from a buffer that
     * holds its 4-byte length prefix in front of it, so there is no
     * separate "hash an SSH string" step.  hb collects the first four,
     * V_C, V_S, I_C and I_S, back to back so they are hashed as one piece:
     * the client's KEXINIT packet is received so that its payload's string
     * prefix (written over the packet header once that is parsed) follows
     * V_S directly, and the server's KEXINIT is built right after it (both
     * sides send KEXINIT without waiting for the other's, so the client's
     * can come first).  Room for the longest version line, a full-size
     * packet, the server's list, another full-size packet (KEX_ECDH_INIT
     * is received in place) and the strings behind it. */
    static uint8_t hb[4 + 256 + 4 + 15 + 4096 + 16 + 256 + 4096 + 16 + 512]
        __attribute__((aligned(256)));  /* see the version line's bound */
    uint8_t *cver = hb + 4;
    /* version exchange */
    xsend(vs + 4, sizeof(V_S) + 1);
    /* up to the LF, at most 252 bytes: the line ends where hb's next
     * 256-byte boundary is, a test of the pointer's low byte */
    uint8_t *v = cver;
    do xrecv(v, 1);
    while (*v != '\n' && (uint8_t)(uintptr_t)++v);
    /* V_C for the exchange hash is the line without its CR LF (RFC 4253
     * 4.2: the line MUST end in CR LF); we stopped on the LF.  Signed: a
     * bare LF gives -1, which keeps every pointer below inside hb (and a
     * transcript no client can verify). */
    ptrdiff_t vl = v - cver - 1;
    PUT32(hb, (uint32_t)vl);

    /* KEXINIT: the payload of a received packet sits 5 bytes in; the
     * packet's first byte (the length's high byte) lands where V_S's last
     * byte goes, so V_S is copied in after the receive. */
    uint8_t *ckexbuf = cver + vl + 4 + sizeof(V_S) - 2, *ckex = ckexbuf + 5;
    size_t ckexl = recv_packet(ckexbuf);
    cpy(cver + vl, vs, 4 + sizeof(V_S) - 1);
    PUT32(ckex - 4, (uint32_t)ckexl);
    uint8_t *skex = ckex + ckexl;
    size_t skexl = build_kexinit(skex + 4);
    PUT32(skex, (uint32_t)skexl);
    cpy(tmp, skex + 4, skexl);
    send_packet(skexl);

    /* ECDH: ephemeral key pair (the scalar in the P256_K slot, which the
     * signature's nonce takes over afterwards), public point Q_S written
     * into the reply in its wire form 04 || x || y */
    keygen();
    cpy(rep + REP_QS + 5, p256_w, 64);

    /* exchange hash H = SHA256(V_C||V_S||I_C||I_S||K_S||Q_C||Q_S||K): the
     * remaining four strings go behind I_S with their length prefixes.
     * KEX_ECDH_INIT, the peer's point Q_C as 0x04 || x || y, is received
     * so that its string (payload + 1) lands where the hash wants it and
     * is used from there; K_S from the reply is then copied in front of
     * it, over the packet's header and type byte, and Q_S from the reply
     * behind it - one pair of copies from rep, the second going on from
     * where the first left the source.  K's mpint is written after them
     * and H itself follows it, as the key derivation hashes K || H four
     * times.  The message is 70 bytes (type 30, then a 65-byte string
     * 04 || x || y): only its length is checked.  Its content is the
     * client's point, which the multiplier uses as it is (see README). */
    uint8_t *t = skex + 4 + skexl, *kinit = t + REP_QS - REP_KS - 1;
    if (recv_packet(kinit - 5) != 70) die();
    p256_smult(kinit + 6);                  /* K = x coordinate, p256_w[0] */
    {
        const uint8_t *s = rep + REP_KS;
        size_t n = REP_QS - REP_KS;
        MOVSB(t, s, n);
        t += 69;
        n = 69;
        MOVSB(t, s, n);
    }
    uint8_t *mp = t;
    uint8_t *H = put_mpint(mp, *p256_w, 32);
    sha256(H, hb, H - hb);
    /* ECDSA signs SHA-256(H), hashed straight into its input slot */
    sha256(P256_Z, H, 32);
    keygen();
    p256_run(p256_ecdsa);
    const uint8_t *rs = *p256_w;            /* r || s */

    /* KEX_ECDH_REPLY: K_S and Q_S are in place; the signature blob =
     * string name || string (mpint r || mpint s) follows.  The inner
     * string's length depends on the leading bits of r and s, so the two
     * lengths are filled in last. */
    size_t rl;
    {
        uint8_t *sig = rep + REP_SIG;
        cpy(sig + 4, NL_HOSTKEY - 4, 23);   /* string name, prefix in nl */
        uint8_t *e = put_mpint(put_mpint(sig + 31, rs, 32), rs + 32, 32);
        PUT32(sig + 27, (uint32_t)(e - sig - 31));
        PUT32(sig, (uint32_t)(e - sig - 4));
        rl = e - rep;
    }
    cpy(tmp, rep, rl);
    send_packet(rl);

    /* NEWKEYS: the client sends its own as soon as it has the reply, so it
     * is received first and, being a bare type byte, sent straight back. */
    rexp(MSG_NEWKEYS);
    send_packet(1);

    /* Key derivation. RFC 4253 7.2 letters 'C' and 'D' are the client-to-
     * server and server-to-client encryption keys; the AEAD needs no IVs
     * (the nonce is the sequence number) and no integrity keys, so the two
     * 64-byte keys go straight into the direction states, and each
     * direction's sequence number (3 packets so far) marks them in use. */
    cstate_t *c = cs;
    char id = 'C';
    do {
        c->seq = 3;             /* before: a short displacement */
        derive((uint8_t *)(c->k2 + 4), mp, H, id);
        c++;
    } while (id++ == 'C');

    /* SERVICE_REQUEST -> ACCEPT: the accept message is the request with
     * its type byte changed (both carry just the service name), so the
     * received payload is sent back in place. */
    size_t srl = rexp(MSG_SERVICE_REQUEST);
    tmp[0] = MSG_SERVICE_ACCEPT;
    send_packet(srl);

    /* USERAUTH. Exactly one request is acceptable - user "user", service
     * "ssh-connection", method "password", no change flag, password
     * "password123" - and it is a fixed 55-byte message, so it is matched
     * whole instead of being parsed field by field. */
    static const uint8_t want[55] =
        "\x32" "\0\0\0\x04" "user" "\0\0\0\x0e" "ssh-connection"
        "\0\0\0\x08" "password" "\0" "\0\0\0\x0b" "password123";
    for (;;) {
        size_t n = rexp(MSG_USERAUTH_REQUEST);
        if (n == sizeof(want) && memeq(tmp, want, sizeof(want))) break;
        /* USERAUTH_FAILURE: name-list "password", partial success FALSE,
         * which is want[] from its method's length prefix on (the change
         * flag being the FALSE): send_chan()'s shape, with the string's
         * length as the channel */
        send_chan(51, want + 31, 9, 0x08000000);
    }
    /* USERAUTH_SUCCESS is a bare type byte: reuse the matched request's */
    tmp[0] = MSG_USERAUTH_SUCCESS;
    send_packet(1);

    /* CHANNEL_OPEN -> CONFIRMATION.  The one channel type served is
     * "session", so the sender's channel is at a fixed offset: type,
     * string "session", uint32 sender channel (a shorter payload leaves
     * stale bytes of the buffer there, which is harmless) */
    rexp(MSG_CHANNEL_OPEN);
    uint32_t cchan = *(u32a *)(tmp + 12);   /* kept in wire order */
    send_chan(MSG_CHANNEL_OPEN_CONFIRMATION, hdr_win, 12, cchan);

    /* CHANNEL_DATA "Hello World" (the string, length prefix included),
     * then CLOSE */
    static const uint8_t hello[17] = "\0\0\0\x0d" "Hello World\r\n";
    send_chan(MSG_CHANNEL_DATA, hello, sizeof(hello), cchan);
    send_chan(MSG_CHANNEL_CLOSE, 0, 0, cchan);
    for (;;) rp();
}

/* Entered by falling through from _start (nolibc.c, tiny.ld); never
 * returns.  used/externally_visible: nothing in C calls it. */
__attribute__((used, externally_visible, noreturn))
int main(void) {
    sha_gentables();
    fp_one[FP_SIZE - 1]++;      /* bss: an inc is a byte shorter than a store */
    /* the host key: its point goes into the KEX_ECDH_REPLY (see rep[])
     * behind the constant head, and the interpreter moves the secret from
     * the scalar slot to the signature's d slot (p256.h) - after the copy,
     * as the conversion to affine that program shares zeroes the point */
    keygen();
    cpy(rep + REP_HOST, p256_w, 64);
    p256_run(p256_hostkey);
    /* string(Q_S) head "\0\0\0\x41\x04": rep is bss, so one 32-bit store of
     * the last four bytes (little-endian 00 00 41 04) does it */
    *(u32a *)(rep + REP_QS + 1) = 0x04410000;

    /* also in a local: the syscall asm's memory clobber would otherwise
     * reload the global before each of the three calls below */
    unsigned fd = lfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &hdr_one, sizeof(int));
    /* bind() fails on a bad socket as well, so one sign test on the OR of
     * the two results covers socket(), bind() and listen(). */
    /* listen()'s backlog: any positive value does, the kernel caps it
     * at somaxconn - and bind() has just left the address in its register */
    int r = bind(fd, &hdr_sockaddr, sizeof(struct sockaddr_in)) |
            __syscall2(SYS_listen, fd, &hdr_sockaddr);
    if (r < 0)                      /* nonzero: fd is 3, or -errno if */
        __syscall1(SYS_exit, fd);   /* socket() failed; it is in rdi */

    serve();
}

/* One connection, then the next: handle() ends in die(), which re-enters
 * here (see there), as it does from wherever handle() gives up part way;
 * handle() is inlined here (always_inline), so this is its frame too.
 * A failed accept() leaves a bad descriptor in cfd, whose first write dies
 * at once: same as skipping it.
 * used/externally_visible/noinline: die() jumps here from inline asm. */
__attribute__((used, externally_visible, noinline, noreturn))
void serve(void) {
    cfd = accept(lfd, 0, 0);
    c2s.seq = 0;                /* keys off, both directions */
    handle();
}
