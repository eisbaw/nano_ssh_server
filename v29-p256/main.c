/* Nano SSH Server - v29-p256: v28-chapoly with the key exchange and host
 * key moved from Curve25519/Ed25519 to NIST P-256 (ecdh-sha2-nistp256 /
 * ecdsa-sha2-nistp256), so that one curve, one modular multiplier and one
 * hash (SHA-256) serve the whole handshake; the packet cipher stays
 * chacha20-poly1305@openssh.com with Poly1305 on the same multiplier.
 * Single algorithm path, no debug output, no malloc, no libc.  Fully
 * static/self-contained.  See optimization_log.txt for the size steps. */

#include <stdint.h>
#include "nolibc.h"            /* mem/str, raw syscalls, sockets */
#include "random_minimal.h"     /* randombytes_buf() via getrandom(2) */
#include "fp.h"
#include "p256.h"
#include "chapoly.h"
#include "sha256.h"

#ifndef PORT
#define PORT 2222          /* make PORT=n builds a server on another port */
#endif
#define V_S  "SSH-2.0-NanoSSH"
/* V_S as the exchange hash wants it (SSH string) with the CR LF the version
 * line wants after it: one constant serves both.  tiny.ld puts it at
 * 0x40005d, overlapping the ELF header. */
__attribute__((section(".hdr.vs")))
static const uint8_t vs[4 + sizeof(V_S) + 1] = "\0\0\0\x0f" V_S "\r\n";

#define MSG_DISCONNECT 1
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
#define MSG_CHANNEL_EOF 96
#define MSG_CHANNEL_CLOSE 97
#define MSG_CHANNEL_REQUEST 98
#define MSG_CHANNEL_SUCCESS 99

/* Big-endian 32-bit wire fields: one bswap + one unaligned access (u32a). */
#define PUT32(b,v) (*(u32a *)(b) = __builtin_bswap32(v))
#define GET32(b) __builtin_bswap32(*(const u32a *)(b))

/* One direction of the connection. key = K_2 (payload) || K_1 (length) as
 * derived per RFC 4253 7.2; seq is the packet sequence number and doubles as
 * the "keys in use" flag: it stays 0 until NEWKEYS and is set to 3 there
 * (three packets precede it in each direction), so it is never 0 afterwards. */
typedef struct {
    uint8_t key[64];
    uint32_t seq;
} cstate_t;

static cstate_t cs[2];          /* [0] client->server, [1] server->client */
#define c2s cs[0]
#define s2c cs[1]

/* Host key, generated once at startup: the secret is P256_D (p256.h);
 * the public point lives
 * inside rep[], the KEX_ECDH_REPLY message, whose constant part (type,
 * host key blob K_S = string name || string curve || string 04||x||y, and
 * the length prefix of Q_S) is built once at startup; each connection only
 * fills in Q_S (the scalar multiplier writes x || y straight after the 04)
 * and the signature, and the exchange hash reads K_S and Q_S from here in
 * their wire form, length prefixes included. */
static uint8_t rep[512];
#define REP_KS   1      /* string K_S: 4 + 104 bytes */
#define REP_HOST 45     /* host public x || y */
#define REP_QS   109    /* string Q_S: 4 + 65 bytes */
#define REP_SIG  178    /* string signature */

/* KEXINIT name-lists (kex, hostkey, cipher, compression; the mac and
 * language lists are empty: an AEAD cipher carries its own MAC, and
 * OpenSSH skips MAC negotiation for it), each preceded by its length byte
 * (strlen() is a real call in a freestanding build), sharing their bytes
 * with the constant head of the KEX_ECDH_REPLY: type, length of K_S, then
 * K_S up to and including the 04 of its point.  The hostkey name's length
 * byte is the last byte of its length prefix there, and the signature blob
 * copies it with that prefix.  Sized, so no trailing NUL. */
static const char nl[99] =
    "\x1d" "chacha20-poly1305@openssh.com" "\x04" "none"        /* 0, 30 */
    "\x12" "ecdh-sha2-nistp256"                                 /* 35 */
    "\x1f" "\0\0\0\x68" "\0\0\0\x13" "ecdsa-sha2-nistp256"      /* 54 */
    "\0\0\0\x08" "nistp256" "\0\0\0\x41" "\x04";               /* 82 */
#define NL_HOSTKEY (nl + 63)    /* the name; its length prefix precedes */
#define NL_REP     (nl + 54)    /* 45-byte constant head of the reply */

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

static void xio(void *b, size_t n, long sysno) {
    uint8_t *p = b;
    do {
        long r = __syscall3(sysno, cfd, p, n);
        if (r <= 0) die();
        p += r; n -= r;
    } while (n);
}
#define xsend(b, n) xio((void *)(b), (n), SYS_write)
#define xrecv(b, n) xio((b), (n), SYS_read)

/* ---- SSH string helper ---- */
static size_t put_str(uint8_t *b, const void *s, size_t n) {
    PUT32(b, (uint32_t)n); memcpy(b + 4, s, n); return 4 + n;
}

/* Does an SSH string field equal this literal? Compared in place, with the
 * literal's length known at compile time (strlen() is a real call in a
 * freestanding build). */
#define fld_is(d, n, s) ((n) == sizeof(s) && !memcmp((d), (s), (n)))

/* ---- chacha20-poly1305@openssh.com on one packet ----
 * Poly1305 one-time key = ChaCha20(K_2, seq, counter 0)[0..31]; the tag covers
 * the encrypted length and the encrypted payload. */
static void aead_tag(const cstate_t *c, const uint8_t *pkt, size_t pktlen,
                     uint8_t *tag) {
    uint8_t pk[32] = { 0 };
    chacha_xor(c->key, c->seq, 0, pk, 32);
    poly1305(tag, pk, pkt, 4 + pktlen);
}

/* Encrypt (dec = 0) or decrypt (dec = 1) pkt = length || payload in place
 * and produce the tag. Sending encrypts and then tags; receiving tags the
 * ciphertext and then decrypts - the length word is XORed first in both
 * cases, which on receive re-encrypts the copy recv_packet() decrypted to
 * learn how much to read, so the tag sees the bytes that were on the wire.
 * One copy of each call for both directions: the payload is XORed in
 * pass dec of two, the tag taken in pass 0. */
__attribute__((noinline))
static void aead(cstate_t *c, uint8_t *pkt, size_t pktlen, uint8_t *tag,
                 int dec) {
    chacha_xor(c->key + 32, c->seq, 0, pkt, 4);
    for (int i = 0; i < 2; i++) {
        if (i == dec) chacha_xor(c->key, c->seq, 1, pkt + 4, pktlen);
        if (!i) aead_tag(c, pkt, pktlen, tag);
    }
    c->seq++;
}

/* ---- send the binary packet whose payload (plen bytes) is at tmp ----
 * Encrypted once s2c.seq is set.  The packet is built around the payload
 * in place: length and padding length in front of it, padding and the tag
 * behind it - a reply to a received packet is built where the request
 * was, so nothing is copied. */
static void send_packet(size_t plen) {
    /* Padding is over packet_length||padding_length||payload before
     * NEWKEYS; the AEAD leaves the length word out (OpenSSH aadlen = 4).
     * Block size is 8 either way. */
    size_t total = (s2c.seq ? 1 : 5) + plen;
    /* at least 4, and the packet a multiple of 8: 4 + (4 padded up to the
     * next multiple of 8) - total */
    uint8_t pad = 4 + (-(total + 4) & 7);
    uint32_t pktlen = 1 + plen + pad;
    PUT32(tmpbuf, pktlen);
    tmpbuf[4] = pad;
    randombytes_buf(tmp + plen, pad);
    total = 4 + pktlen;
    if (s2c.seq) {
        aead(&s2c, tmpbuf, pktlen, tmpbuf + total, 0);
        total += 16;
    }
    xsend(tmpbuf, total);
}

/* Send [type][recipient channel][body]: the shape of every channel-level
 * message this server emits (open confirmation, success, data, eof, close). */
static void send_chan(uint8_t type, uint32_t chan, const void *body,
                      size_t n) {
    tmp[0] = type;
    PUT32(tmp + 1, chan);
    memcpy(tmp + 5, body, n);
    send_packet(5 + n);
}

/* ---- recv one binary packet into buf (sizeof tmpbuf bytes of room) ----
 * The packet is read in place - length, padding length, payload, padding,
 * tag - and the payload length is returned; the payload itself starts at
 * buf + 5, which callers address through a second pointer, so there is no
 * copy out of a private buffer. Dies on any error. */
static size_t recv_packet(uint8_t *buf) {
    static uint8_t mac[16];
    uint32_t pktlen;
    size_t pad, taglen = 0;
    xrecv(buf, 4);
    if (c2s.seq) {
        chacha_xor(c2s.key + 32, c2s.seq, 0, buf, 4);
        taglen = 16;
    }
    pktlen = GET32(buf);
    /* compared as size_t: a 32-bit sum would wrap for lengths near 2^32 */
    if (pktlen < 5 || pktlen > sizeof tmpbuf - 20) die();
    xrecv(buf + 4, pktlen + taglen);
    if (taglen) {
        aead(&c2s, buf, pktlen, mac, 1);
        if (ct_diff(mac, buf + 4 + pktlen, 16)) die();
    }
    pad = buf[4];
    if (pad >= pktlen - 1) die();
    return pktlen - 1 - pad;
}

/* Receive the next post-KEX packet into tmpbuf. */
static size_t rp(void) { return recv_packet(tmpbuf); }

/* Receive a packet that must be of type t (dies otherwise); returns its
 * payload length. */
static size_t rexp(unsigned t) {
    size_t n = rp();
    if (tmp[0] != t) die();
    return n;
}

/* ---- KEXINIT payload ---- */
static size_t build_kexinit(uint8_t *p) {
    /* KEXINIT carries ten name-lists: kex, hostkey, enc c2s/s2c, mac
     * c2s/s2c, comp c2s/s2c, lang c2s/s2c. Six of them repeat, so the four
     * distinct names are stored once and indexed by their length byte; the
     * four empty lists point at a zero byte of a length prefix, and an
     * eleventh empty list writes four of the five zero bytes that follow
     * (first_kex_packet_follows and the reserved word). */
    static const uint8_t off[11] = { 35, 62, 0, 0, 55, 55, 30, 30, 55, 55, 55 };
    size_t o = 0;
    p[o++] = MSG_KEXINIT;
    randombytes_buf(p + o, 16); o += 16;
    for (int i = 0; i < 11; i++) {
        const char *s = nl + off[i];
        o += put_str(p + o, s + 1, (uint8_t)*s);
    }
    p[o] = 0;
    return o + 1;
}

/* ---- mpint (shared secret K, ECDSA r and s) ----
 * Leading zero bytes are dropped; a value with its top bit set gets one
 * zero byte back so it stays positive.  That byte is written unconditionally
 * (the copy overwrites it when it is not wanted).  The last byte is never
 * dropped, so the top-bit test needs no bound (the value 0, which would
 * want an empty encoding, is not a P-256 coordinate or signature half). */
static size_t put_mpint(uint8_t *b, const uint8_t *d, size_t n) {
    size_t i = 0;
    while (i < n - 1 && d[i] == 0) i++;
    size_t pad = d[i] >> 7;
    b[4] = 0;
    memcpy(b + 4 + pad, d + i, n - i);
    n = n - i + pad;
    PUT32(b, (uint32_t)n);
    return 4 + n;
}

/* ---- derive one 64-byte key per RFC4253 7.2 ----
 * K1 = HASH(K || H || id || session_id), K2 = HASH(K || H || K1); the only
 * keys this cipher needs are the two 64-byte encryption keys, so the length
 * is fixed and both hashes are always taken.  mp holds K || H with room
 * behind it: id || H goes there for K1, and K1 replaces it for K2 (the
 * session id is H on a server that never rekeys). */
static void derive(uint8_t *out, uint8_t *mp, const uint8_t *H, char id) {
    uint8_t *t = (uint8_t *)H + 32;
    t[0] = (uint8_t)id;
    memcpy(t + 1, H, 32);
    sha256(out, mp, t + 33 - mp);
    memcpy(t, out, 32);
    sha256(out + 32, mp, t + 32 - mp);
}

/* A fresh P-256 scalar in the multiplier's scalar slot: 32 random bytes,
 * redrawn while they are not below n (big-endian, so memcmp() orders them
 * numerically) or their top word is zero.  An ECDSA nonce must have no
 * fixed bits - a lattice attack turns a fixed top bit into the host key -
 * and this is uniform over [2^224, n), one part in 2^32 short of uniform
 * over [1, n); the lower bound is what lets the ladder in p256.c use k + n
 * as its scalar without a second case. */
static void rand_scalar(void) {
    do randombytes_buf(P256_K, 32);
    while (memcmp(P256_K, p256_n, 32) >= 0 || !*(u32a *)P256_K);
}

__attribute__((noreturn)) static void handle(void) {
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
    static uint8_t hb[4 + 256 + 4 + 15 + 4096 + 16 + 256 + 4096 + 16 + 512];
    uint8_t *cver = hb + 4;
    /* version exchange */
    xsend(vs + 4, sizeof(V_S) + 1);
    uint8_t *v;
    for (v = cver; v < cver + 255; v++) {
        xrecv(v, 1);
        if (*v == '\n') break;
    }
    /* V_C for the exchange hash is the line without its CR LF; we stopped
     * on the LF, so at most one CR precedes it. */
    size_t vl = v - cver;
    if (vl > 0 && v[-1] == '\r') vl--;
    PUT32(hb, (uint32_t)vl);

    /* KEXINIT: the payload of a received packet sits 5 bytes in; the
     * packet's first byte (the length's high byte) lands where V_S's last
     * byte goes, so V_S is copied in after the receive. */
    uint8_t *ckexbuf = cver + vl + 4 + sizeof(V_S) - 2, *ckex = ckexbuf + 5;
    size_t ckexl = recv_packet(ckexbuf);
    if (ckex[0] != MSG_KEXINIT) die();
    memcpy(cver + vl, vs, 4 + sizeof(V_S) - 1);
    PUT32(ckex - 4, (uint32_t)ckexl);
    uint8_t *skex = ckex + ckexl;
    size_t skexl = build_kexinit(skex + 4);
    PUT32(skex, (uint32_t)skexl);
    memcpy(tmp, skex + 4, skexl);
    send_packet(skexl);

    /* ECDH: ephemeral key pair (the scalar in the P256_K slot, which the
     * signature's nonce takes over afterwards), public point Q_S written
     * into the reply in its wire form 04 || x || y */
    rand_scalar();
    p256_smult(p256_g);
    memcpy(rep + REP_QS + 5, p256_w, 64);

    /* exchange hash H = SHA256(V_C||V_S||I_C||I_S||K_S||Q_C||Q_S||K): the
     * remaining four strings go behind I_S with their length prefixes.
     * KEX_ECDH_INIT, the peer's point Q_C as 0x04 || x || y, is received
     * so that its string (payload + 1) lands where the hash wants it and
     * is used from there; K_S from the reply is then copied in front of
     * it, over the packet's header and type byte, and Q_S from the reply
     * behind it.  K's mpint is written after them and H itself follows
     * it, as the key derivation hashes K || H four times.
     * The message is exactly 70 bytes: type 30, then a 65-byte string
     * starting with 04 - checked as one compare of its first six bytes
     * (little-endian 1e 00 00 00 41 04, shifted up past the two bytes of
     * x that follow). */
    uint8_t *t = skex + 4 + skexl, *kinit = t + REP_QS - REP_KS - 1;
    if (recv_packet(kinit - 5) != 70 ||
        *(u64a *)kinit << 16 != 0x04410000001e0000u) die();
    p256_smult(kinit + 6);                  /* K = x coordinate, p256_w[0] */
    memcpy(t, rep + REP_KS, REP_QS - REP_KS); t += REP_QS - REP_KS + 69;
    memcpy(t, rep + REP_QS, 69); t += 69;
    uint8_t *mp = t;
    uint8_t *H = mp + put_mpint(mp, *p256_w, 32);
    sha256(H, hb, H - hb);
    /* ECDSA signs SHA-256(H), hashed straight into its input slot */
    sha256(P256_Z, H, 32);
    rand_scalar();
    ecdsa_sign();
    const uint8_t *rs = *p256_w;            /* r || s */

    /* KEX_ECDH_REPLY: K_S and Q_S are in place; the signature blob =
     * string name || string (mpint r || mpint s) follows.  The inner
     * string's length depends on the leading bits of r and s, so the two
     * lengths are filled in last. */
    size_t rl;
    {
        uint8_t *sig = rep + REP_SIG;
        memcpy(sig + 4, NL_HOSTKEY - 4, 23);   /* string name, prefix in nl */
        size_t sl = 4 + 23;
        size_t il = put_mpint(sig + sl + 4, rs, 32);
        il += put_mpint(sig + sl + 4 + il, rs + 32, 32);
        PUT32(sig + sl, (uint32_t)il);
        sl += 4 + il;
        PUT32(sig, (uint32_t)(sl - 4));
        rl = REP_SIG + sl;
    }
    memcpy(tmp, rep, rl);
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
    for (int i = 0; i < 2; i++) {
        derive(cs[i].key, mp, H, (char)('C' + i));
        cs[i].seq = 3;
    }

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
        if (n == sizeof(want) && !memcmp(tmp, want, n)) break;
        /* USERAUTH_FAILURE: name-list "password", partial success FALSE,
         * which is want[] from its method's length prefix on (the change
         * flag being the FALSE): send_chan()'s shape, with the string's
         * length as the channel */
        send_chan(51, 8, want + 31, 9);
    }
    /* USERAUTH_SUCCESS is a bare type byte: reuse the matched request's */
    tmp[0] = MSG_USERAUTH_SUCCESS;
    send_packet(1);

    /* CHANNEL_OPEN -> CONFIRMATION */
    size_t con = rexp(MSG_CHANNEL_OPEN);
    /* string channel type (skipped), then the sender's channel: one bound
     * covers the string and the 4 bytes behind it (a length word read
     * from a payload shorter than 5 bytes is stale buffer data, and any
     * value of it fails the bound) */
    uint32_t ctl = GET32(tmp + 1);
    if (con < (size_t)ctl + 9) die();
    uint32_t cchan = GET32(tmp + 5 + ctl);
    send_chan(MSG_CHANNEL_OPEN_CONFIRMATION, cchan, hdr_win, 12);

    /* channel requests until shell/exec */
    int ready = 0;
    while (!ready) {
        size_t n = rp();
        if (tmp[0] != MSG_CHANNEL_REQUEST) break;
        /* recipient channel, string request type, boolean want_reply:
         * one bound covers the string and the byte behind it (as above) */
        uint32_t rtl = GET32(tmp + 5);
        if (n < (size_t)rtl + 10) die();
        uint8_t *rt = tmp + 9, want = rt[rtl];
        static const uint8_t shell[5] = "shell", exec[4] = "exec";
        if (fld_is(rt, rtl, shell) || fld_is(rt, rtl, exec)) ready = 1;
        if (want) send_chan(MSG_CHANNEL_SUCCESS, cchan, 0, 0);
    }

    /* CHANNEL_DATA "Hello World" (the string, length prefix included) */
    static const uint8_t hello[17] = "\0\0\0\x0d" "Hello World\r\n";
    if (ready) send_chan(MSG_CHANNEL_DATA, cchan, hello, sizeof(hello));

    /* EOF + CLOSE */
    send_chan(MSG_CHANNEL_EOF, cchan, 0, 0);
    send_chan(MSG_CHANNEL_CLOSE, cchan, 0, 0);
    rp();
    die();
}

/* Entered by falling through from _start (nolibc.c, tiny.ld); never
 * returns.  used/externally_visible: nothing in C calls it. */
__attribute__((used, externally_visible, noreturn))
int main(void) {
    sha_gentables();
    fp_one[FP_SIZE - 1]++;      /* bss: an inc is a byte shorter than a store */
    /* the multiplier's scalar slot serves the host key once; the secret
     * is moved to the signature's d slot by the interpreter (p256.h) */
    rand_scalar();
    p256_run(p256_hostkey);
    /* constant part of the KEX_ECDH_REPLY (see rep[]), then the host
     * point behind its 04 */
    memcpy(rep, NL_REP, REP_HOST);
    p256_smult(p256_g);
    memcpy(rep + REP_HOST, p256_w, 64);
    /* string(Q_S) head "\0\0\0\x41\x04": rep is bss, so one 32-bit store of
     * the last four bytes (little-endian 00 00 41 04) does it */
    *(u32a *)(rep + REP_QS + 1) = 0x04410000;

    /* also in a local: the syscall asm's memory clobber would otherwise
     * reload the global before each of the three calls below */
    unsigned fd = lfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &hdr_one, sizeof(int));
    /* bind() fails on a bad socket as well, so one sign test on the OR of
     * the two results covers socket(), bind() and listen(). */
    int r = bind(fd, &hdr_sockaddr, sizeof(struct sockaddr_in)) | listen(fd, 5);
    if (r < 0) _exit(r);            /* exit status -errno & 0xff: nonzero */

    serve();
}

/* One connection, then the next: handle() ends in die(), which re-enters
 * here (see there), as it does from wherever handle() gives up part way.
 * A failed accept() leaves a bad descriptor in cfd, whose first write dies
 * at once: same as skipping it.
 * used/externally_visible/noinline: die() jumps here from inline asm. */
__attribute__((used, externally_visible, noinline, noreturn))
void serve(void) {
    cfd = accept(lfd, 0, 0);
    memset(cs, 0, sizeof(cs));
    handle();
}
