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
 * line wants after it: one constant serves both. */
static const uint8_t vs[] = "\0\0\0\x0f" V_S "\r\n";

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
 * OpenSSH skips MAC negotiation for it) sharing their bytes with the
 * constant head of the KEX_ECDH_REPLY: type, length of K_S, then K_S up
 * to and including the 04 of its point.  The hostkey name inside it is
 * NUL-terminated for the name-list by the first byte of the next length
 * prefix, and the signature blob copies it with that prefix. */
static const char nl[] =
    "ecdh-sha2-nistp256\0"                                   /* 0  */
    "\x1f" "\0\0\0\x68" "\0\0\0\x13" "ecdsa-sha2-nistp256"   /* 19 */
    "\0\0\0\x08" "nistp256" "\0\0\0\x41" "\x04"             /* 47 */
    "chacha20-poly1305@openssh.com\0" "none";                 /* 64 */
#define NL_HOSTKEY (nl + 28)    /* the name; its length prefix precedes */
#define NL_REP     (nl + 19)    /* 45-byte constant head of the reply */

/* ---- I/O ----
 * The connection's socket. One connection at a time, so it is a global
 * rather than an argument threaded through every packet function.
 * read(2) and write(2) differ only in the syscall number, so one loop with
 * the number as a parameter serves both directions. */
static int cfd;

static int xio(void *b, size_t n, long sysno) {
    uint8_t *p = b; size_t s = 0;
    while (s < n) {
        long r = __syscall3(sysno, cfd, p + s, n - s);
        if (r <= 0) return -1;
        s += (size_t)r;
    }
    return 0;
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
#define fld_is(d, n, s) ((n) == sizeof(s) - 1 && !memcmp((d), (s), (n)))

/* Bounds-checked read of an SSH length-prefixed field within [*pp, end).
 * Advances *pp past the field; returns a pointer to its data, or NULL on
 * overrun. *len receives the field length. */
static uint8_t *rd_field(uint8_t **pp, uint8_t *end, uint32_t *len) {
    if (end - *pp < 4) return 0;
    uint32_t l = GET32(*pp); *pp += 4;
    if ((uint32_t)(end - *pp) < l) return 0;
    uint8_t *d = *pp; *pp += l; *len = l;
    return d;
}

/* ---- chacha20-poly1305@openssh.com on one packet ----
 * Poly1305 one-time key = ChaCha20(K_2, seq, counter 0)[0..31]; the tag covers
 * the encrypted length and the encrypted payload. */
static void aead_tag(const cstate_t *c, const uint8_t *pkt, size_t pktlen,
                     uint8_t *tag) {
    uint8_t pk[32];
    memset(pk, 0, 32);
    chacha_xor(c->key, c->seq, 0, pk, 32);
    poly1305(tag, pk, pkt, 4 + pktlen);
}

/* Encrypt (dec = 0) or decrypt (dec = 1) pkt = length || payload in place
 * and produce the tag. Sending encrypts and then tags; receiving tags the
 * ciphertext and then decrypts - the length word is XORed first in both
 * cases, which on receive re-encrypts the copy recv_packet() decrypted to
 * learn how much to read, so the tag sees the bytes that were on the wire. */
static void aead(cstate_t *c, uint8_t *pkt, size_t pktlen, uint8_t *tag,
                 int dec) {
    chacha_xor(c->key + 32, c->seq, 0, pkt, 4);
    if (dec) aead_tag(c, pkt, pktlen, tag);
    chacha_xor(c->key, c->seq, 1, pkt + 4, pktlen);
    if (!dec) aead_tag(c, pkt, pktlen, tag);
    c->seq++;
}

/* ---- send one binary packet (encrypted once s2c.seq is set) ---- */
static int send_packet(const uint8_t *payload, size_t plen) {
    uint8_t pkt[4096 + 16];
    /* Padding is over packet_length||padding_length||payload before
     * NEWKEYS; the AEAD leaves the length word out (OpenSSH aadlen = 4).
     * Block size is 8 either way. */
    size_t total = (s2c.seq ? 1 : 5) + plen;
    uint8_t pad = 8 - (total % 8);
    if (pad < 4) pad += 8;
    uint32_t pktlen = 1 + plen + pad;
    PUT32(pkt, pktlen);
    pkt[4] = pad;
    memcpy(pkt + 5, payload, plen);
    randombytes_buf(pkt + 5 + plen, pad);
    total = 4 + pktlen;
    if (s2c.seq) {
        aead(&s2c, pkt, pktlen, pkt + total, 0);
        total += 16;
    }
    return xsend(pkt, total);
}

/* Send [type][recipient channel][body]: the shape of every channel-level
 * message this server emits (open confirmation, success, data, eof, close). */
static int send_chan(uint8_t type, uint32_t chan, const void *body,
                     size_t n) {
    uint8_t b[32];
    b[0] = type;
    PUT32(b + 1, chan);
    memcpy(b + 5, body, n);
    return send_packet(b, 5 + n);
}

/* ---- recv one binary packet into buf (max bytes) ----
 * The packet is read in place - length, padding length, payload, padding,
 * tag - and the payload length is returned; the payload itself starts at
 * buf + 5, which callers address through a second pointer, so there is no
 * copy out of a private buffer. Returns -1 on any error. */
static ssize_t recv_packet(uint8_t *buf, size_t max) {
    uint8_t mac[16];
    uint32_t pktlen;
    size_t pad;
    if (xrecv(buf, 4)) return -1;
    if (c2s.seq) chacha_xor(c2s.key + 32, c2s.seq, 0, buf, 4);
    pktlen = GET32(buf);
    /* compared as size_t: a 32-bit sum would wrap for lengths near 2^32 */
    if (pktlen < 5 || pktlen > max - 20) return -1;
    if (xrecv(buf + 4, pktlen + (c2s.seq ? 16 : 0))) return -1;
    if (c2s.seq) {
        aead(&c2s, buf, pktlen, mac, 1);
        if (ct_diff(mac, buf + 4 + pktlen, 16)) return -1;
    }
    pad = buf[4];
    if (pad >= pktlen - 1) return -1;
    return (ssize_t)(pktlen - 1 - pad);
}

/* ---- KEXINIT payload ---- */
static size_t build_kexinit(uint8_t *p) {
    /* KEXINIT carries ten name-lists: kex, hostkey, enc c2s/s2c, mac
     * c2s/s2c, comp c2s/s2c, lang c2s/s2c. Six of them repeat, so the four
     * distinct names are stored once and indexed; the four empty lists
     * point at the trailing NUL. */
    static const uint8_t off[10] = { 0, 28, 64, 64, 98, 98, 94, 94, 98, 98 };
    size_t o = 0;
    p[o++] = MSG_KEXINIT;
    randombytes_buf(p + o, 16); o += 16;
    for (int i = 0; i < 10; i++) {
        const char *s = nl + off[i];
        o += put_str(p + o, s, strlen(s));
    }
    memset(p + o, 0, 5);   /* first_kex_packet_follows + reserved */
    return o + 5;
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

/* ---- hash an SSH string that sits in memory with its length prefix ---- */
static void sha_pstr(const uint8_t *p) {
    sha256_update(p, 4 + GET32(p));
}

/* ---- derive one 64-byte key per RFC4253 7.2 ----
 * K1 = HASH(K || H || id || session_id), K2 = HASH(K || H || K1); the only
 * keys this cipher needs are the two 64-byte encryption keys, so the length
 * is fixed and both hashes are always taken. */
static void derive(uint8_t *out, const uint8_t *mp, const uint8_t *H,
                   char id) {
    for (int j = 0; j < 2; j++) {
        sha256_update(mp, H + 32 - mp);   /* K || H: H follows the mpint */
        if (j) {
            sha256_update(out, 32);
        } else {
            /* the letter is parked in the key's first byte, which the
             * hash output overwrites below */
            out[0] = (uint8_t)id;
            sha256_update(out, 1);
            sha256_update(H, 32);   /* session id == H: never rekeys */
        }
        sha256_final(out + 32 * j);
    }
}

/* A fresh P-256 scalar in the multiplier's scalar slot, uniform in
 * [0, n): 32 random bytes, redrawn while they are not below n (big-endian,
 * so memcmp() orders them numerically).  An ECDSA nonce must be uniform -
 * any fixed bits leak the key to a lattice attack - and the ladder in
 * p256.c takes care of its leading-bit requirement itself. */
static void rand_scalar(void) {
    do randombytes_buf(P256_K, 32); while (memcmp(P256_K, p256_n, 32) >= 0);
}

static void handle(void) {
    /* Every string the exchange hash covers is hashed from a buffer that
     * holds its 4-byte length prefix in front of it, so there is no
     * separate "hash an SSH string" step.  hb collects the first four,
     * V_C, V_S, I_C and I_S, back to back so they are hashed as one piece:
     * the client's KEXINIT packet is received so that its payload's string
     * prefix (written over the packet header once that is parsed) follows
     * V_S directly, and the server's KEXINIT is built right after it (both
     * sides send KEXINIT without waiting for the other's, so the client's
     * can come first).  Room for the longest version line, a full-size
     * packet and the server's list. */
    static uint8_t hb[4 + 256 + 4 + 15 + 4096 + 16 + 256];
    uint8_t *cver = hb + 4;
    /* version exchange */
    if (xsend(vs + 4, sizeof(V_S) + 1)) return;
    int i;
    for (i = 0; i < 255; i++) {
        if (xrecv(cver + i, 1)) return;
        if (cver[i] == '\n') break;
    }
    /* V_C for the exchange hash is the line without its CR LF; we stopped
     * on the LF, so at most one CR precedes it. */
    int vl = i;
    if (vl > 0 && cver[vl - 1] == '\r') vl--;
    PUT32(hb, (uint32_t)vl);

    /* KEXINIT: the payload of a received packet sits 5 bytes in; the
     * packet's first byte (the length's high byte) lands where V_S's last
     * byte goes, so V_S is copied in after the receive. */
    uint8_t *ckexbuf = cver + vl + 4 + sizeof(V_S) - 2, *ckex = ckexbuf + 5;
    ssize_t ckexl = recv_packet(ckexbuf, 4096 + 16);
    if (ckexl <= 0 || ckex[0] != MSG_KEXINIT) return;
    memcpy(cver + vl, vs, 4 + sizeof(V_S) - 1);
    PUT32(ckex - 4, (uint32_t)ckexl);
    uint8_t *skex = ckex + ckexl;
    size_t skexl = build_kexinit(skex + 4);
    PUT32(skex, (uint32_t)skexl);
    if (send_packet(skex + 4, skexl)) return;

    /* ECDH: ephemeral key pair (the scalar in the P256_K slot, which the
     * signature's nonce takes over afterwards), public point Q_S written
     * into the reply in its wire form 04 || x || y */
    static uint8_t shared[65];
    rand_scalar();
    p256_smult(rep + REP_QS + 4, p256_g);

    /* KEX_ECDH_INIT: the peer's point Q_C, 0x04 || x || y, used in place
     * (tmpbuf receives every later packet as well).
     * The message is exactly 70 bytes: type 30, then a 65-byte string
     * starting with 04 - checked as one compare of its first six bytes
     * (little-endian 1e 00 00 00 41 04, shifted up past the two bytes of
     * x that follow). */
    static uint8_t tmpbuf[512];
    uint8_t *tmp = tmpbuf + 5, *kinit = tmp;
    if (recv_packet(tmpbuf, sizeof(tmpbuf)) != 70 ||
        *(u64a *)kinit << 16 != 0x04410000001e0000u) return;
    p256_smult(shared, kinit + 6);          /* K = x coordinate, at +1 */

    /* exchange hash H = SHA256(V_C||V_S||I_C||I_S||K_S||Q_C||Q_S||K) */
    /* K as an SSH string, with H written right after it: the key
     * derivation hashes K || H four times */
    static uint8_t mp[4 + 33 + 32];
    uint8_t *H = mp + put_mpint(mp, shared + 1, 32);
    sha256_init();
    sha256_update(hb, skex + 4 + skexl - hb);   /* V_C || V_S || I_C || I_S */
    sha_pstr(rep + REP_KS);
    sha_pstr(kinit + 1);
    sha_pstr(rep + REP_QS);
    sha_pstr(mp);
    sha256_final(H);
    /* ECDSA signs SHA-256(H), hashed straight into its input slot */
    sha256_update(H, 32);
    sha256_final(P256_Z);
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
    if (send_packet(rep, rl)) return;

    /* Key derivation. RFC 4253 7.2 letters 'C' and 'D' are the client-to-
     * server and server-to-client encryption keys; the AEAD needs no IVs
     * (the nonce is the sequence number) and no integrity keys, so the two
     * 64-byte keys go straight into the direction states. */
    for (int i = 0; i < 2; i++)
        derive(cs[i].key, mp, H, (char)('C' + i));

    /* NEWKEYS: the client sends its own as soon as it has the reply, so it
     * is received first and, being a bare type byte, sent straight back.
     * After it, each direction's sequence number (3 so far) also marks its
     * keys as in use. */
    if (recv_packet(tmpbuf, sizeof(tmpbuf)) <= 0 || tmp[0] != MSG_NEWKEYS) return;
    if (send_packet(tmp, 1)) return;
    c2s.seq = s2c.seq = 3;

    /* SERVICE_REQUEST -> ACCEPT: the accept message is the request with
     * its type byte changed (both carry just the service name), so the
     * received payload is sent back in place. */
    ssize_t srl = recv_packet(tmpbuf, sizeof(tmpbuf));
    if (srl <= 0 || tmp[0] != MSG_SERVICE_REQUEST) return;
    tmp[0] = MSG_SERVICE_ACCEPT;
    if (send_packet(tmp, srl)) return;

    /* USERAUTH. Exactly one request is acceptable - user "user", service
     * "ssh-connection", method "password", no change flag, password
     * "password123" - and it is a fixed 55-byte message, so it is matched
     * whole instead of being parsed field by field. */
    static const uint8_t want[] =
        "\x32" "\0\0\0\x04" "user" "\0\0\0\x0e" "ssh-connection"
        "\0\0\0\x08" "password" "\0" "\0\0\0\x0b" "password123";
    for (;;) {
        ssize_t n = recv_packet(tmpbuf, sizeof(tmpbuf));
        if (n <= 0 || tmp[0] != MSG_USERAUTH_REQUEST) return;
        if (n == sizeof(want) - 1 && !memcmp(tmp, want, n)) break;
        /* USERAUTH_FAILURE: name-list "password", partial success FALSE */
        static const uint8_t fail[] = "\x33" "\0\0\0\x08" "password" "\0";
        if (send_packet(fail, sizeof(fail) - 1)) return;
    }
    /* USERAUTH_SUCCESS is a bare type byte: reuse the matched request's */
    tmp[0] = MSG_USERAUTH_SUCCESS;
    if (send_packet(tmp, 1)) return;

    /* CHANNEL_OPEN -> CONFIRMATION */
    ssize_t con = recv_packet(tmpbuf, sizeof(tmpbuf));
    if (con <= 0 || tmp[0] != MSG_CHANNEL_OPEN) return;
    uint8_t *p = tmp + 1, *end = tmp + con;
    uint32_t ctl;
    if (!rd_field(&p, end, &ctl)) return;              /* channel type (skipped) */
    (void)ctl;
    if (end - p < 4) return;
    uint32_t cchan = GET32(p);
    /* server channel 0, window 32768, max packet 16384 */
    static const uint8_t win[12] = { 0, 0, 0, 0, 0, 0, 0x80, 0, 0, 0, 0x40, 0 };
    if (send_chan(MSG_CHANNEL_OPEN_CONFIRMATION, cchan, win, 12)) return;

    /* channel requests until shell/exec */
    int ready = 0;
    while (!ready) {
        ssize_t n = recv_packet(tmpbuf, sizeof(tmpbuf));
        if (n <= 0) return;
        if (tmp[0] != MSG_CHANNEL_REQUEST) break;
        uint8_t *q = tmp + 1, *qend = tmp + n, *rt;
        uint32_t rtl = 0;
        if (qend - q < 4) return;
        q += 4;                                        /* recipient */
        rt = rd_field(&q, qend, &rtl); if (!rt) return;
        if (q >= qend) return;
        uint8_t want = *q;
        if (fld_is(rt, rtl, "shell") || fld_is(rt, rtl, "exec")) ready = 1;
        if (want && send_chan(MSG_CHANNEL_SUCCESS, cchan, 0, 0)) return;
    }

    /* CHANNEL_DATA "Hello World" (the string, length prefix included) */
    if (ready && send_chan(MSG_CHANNEL_DATA, cchan,
                           "\0\0\0\x0d" "Hello World\r\n", 17)) return;

    /* EOF + CLOSE */
    send_chan(MSG_CHANNEL_EOF, cchan, 0, 0);
    send_chan(MSG_CHANNEL_CLOSE, cchan, 0, 0);
    recv_packet(tmpbuf, sizeof(tmpbuf));
}

/* used/externally_visible: nothing in C calls main() - _start reaches it
 * from inline asm, which LTO cannot see. */
__attribute__((used, externally_visible))
int main(void) {
    sha_gentables();
    fp_one[FP_SIZE - 1] = 1;
    /* the multiplier's scalar slot serves the host key once; the secret
     * then moves to the signature's d slot */
    rand_scalar();
    /* constant part of the KEX_ECDH_REPLY (see rep[]); the multiplier
     * rewrites the 04 that precedes the host point */
    memcpy(rep, NL_REP, REP_HOST);
    p256_smult(rep + REP_HOST - 1, p256_g);
    memcpy(P256_D, P256_K, 32);
    /* string(Q_S) head "\0\0\0\x41\x04": rep is bss, so one 32-bit store of
     * the last four bytes (little-endian 00 00 41 04) does it */
    *(u32a *)(rep + REP_QS + 1) = 0x04410000;

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) return 1;
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    /* INADDR_ANY:PORT as 16 bytes of rodata rather than a memset and stores;
     * the port is byte-swapped by the preprocessor. */
    static const struct sockaddr_in a = {
        AF_INET, (uint16_t)((PORT << 8) | (PORT >> 8)), { INADDR_ANY }, { 0 }
    };
    if (bind(lfd, (const struct sockaddr *)&a, sizeof(a)) < 0) return 1;
    if (listen(lfd, 5) < 0) return 1;

    for (;;) {
        cfd = accept(lfd, 0, 0);
        if (cfd < 0) continue;
        memset(cs, 0, sizeof(cs));
        handle();
        close(cfd);
    }
}
