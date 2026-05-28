/* Nano SSH Server - v23-scratch: minimal from-scratch rewrite.
 * Single algorithm path: curve25519-sha256 / ssh-ed25519 / aes128-ctr / hmac-sha2-256.
 * No debug output, minimal error handling. */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "sodium_compat_production.h"
#include "aes128_minimal.h"
#include "sha256_minimal.h"

#define PORT 2222
#define V_S  "SSH-2.0-NanoSSH"

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

#define PUT32(b,v) do{uint32_t _v=(v);(b)[0]=_v>>24;(b)[1]=_v>>16;(b)[2]=_v>>8;(b)[3]=_v;}while(0)
#define GET32(b) (((uint32_t)(b)[0]<<24)|((uint32_t)(b)[1]<<16)|((uint32_t)(b)[2]<<8)|(b)[3])

typedef struct {
    aes128_ctr_ctx aes;
    uint8_t mac_key[32];
    uint32_t seq;
    int active;
} cstate_t;

static cstate_t c2s, s2c;

/* ---- I/O ---- */
static int xsend(int fd, const void *b, size_t n) {
    const uint8_t *p = b; size_t s = 0;
    while (s < n) { ssize_t r = send(fd, p + s, n - s, 0); if (r <= 0) return -1; s += r; }
    return 0;
}
static int xrecv(int fd, void *b, size_t n) {
    uint8_t *p = b; size_t s = 0;
    while (s < n) { ssize_t r = recv(fd, p + s, n - s, 0); if (r <= 0) return -1; s += r; }
    return 0;
}

/* ---- SSH string helper ---- */
static size_t put_str(uint8_t *b, const void *s, size_t n) {
    PUT32(b, (uint32_t)n); memcpy(b + 4, s, n); return 4 + n;
}

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

/* ---- HMAC over seq||packet ---- */
static void mac_compute(uint8_t *out, const uint8_t *key, uint32_t seq,
                        const uint8_t *pkt, size_t len) {
    hmac_sha256_ctx h; uint8_t sb[4];
    hmac_sha256_init(&h, key, 32);
    PUT32(sb, seq); hmac_sha256_update(&h, sb, 4);
    hmac_sha256_update(&h, pkt, len);
    hmac_sha256_final(&h, out);
}

/* ---- send one binary packet (encrypted if s2c.active) ---- */
static int send_packet(int fd, const uint8_t *payload, size_t plen) {
    uint8_t pkt[4096], mac[32];
    size_t bs = s2c.active ? 16 : 8;
    size_t total = 5 + plen;
    uint8_t pad = bs - (total % bs);
    if (pad < 4) pad += bs;
    uint32_t pktlen = 1 + plen + pad;
    PUT32(pkt, pktlen);
    pkt[4] = pad;
    memcpy(pkt + 5, payload, plen);
    randombytes_buf(pkt + 5 + plen, pad);
    total = 4 + pktlen;
    if (s2c.active) {
        mac_compute(mac, s2c.mac_key, s2c.seq, pkt, total);
        aes128_ctr_crypt(&s2c.aes, pkt, total);
        if (xsend(fd, pkt, total) || xsend(fd, mac, 32)) return -1;
        s2c.seq++;
    } else {
        if (xsend(fd, pkt, total)) return -1;
    }
    return 0;
}

/* ---- recv one binary packet, returns payload length or -1 ---- */
static ssize_t recv_packet(int fd, uint8_t *payload, size_t pmax) {
    uint8_t buf[4096];
    uint32_t pktlen;
    size_t total, pad;
    if (c2s.active) {
        if (xrecv(fd, buf, 16)) return -1;
        aes128_ctr_crypt(&c2s.aes, buf, 16);
        pktlen = GET32(buf);
        if (pktlen < 5 || pktlen + 4 > sizeof(buf)) return -1;
        total = 4 + pktlen;
        if (total > 16) {
            if (xrecv(fd, buf + 16, total - 16)) return -1;
            aes128_ctr_crypt(&c2s.aes, buf + 16, total - 16);
        }
        uint8_t mac[32], cmac[32];
        if (xrecv(fd, mac, 32)) return -1;
        mac_compute(cmac, c2s.mac_key, c2s.seq, buf, total);
        if (ct_verify_32(cmac, mac)) return -1;
        c2s.seq++;
    } else {
        if (xrecv(fd, buf, 4)) return -1;
        pktlen = GET32(buf);
        if (pktlen < 5 || pktlen + 4 > sizeof(buf)) return -1;
        if (xrecv(fd, buf + 4, pktlen)) return -1;
    }
    pad = buf[4];
    if (pad >= pktlen - 1) return -1;
    size_t plen = pktlen - 1 - pad;
    if (plen > pmax) return -1;
    memcpy(payload, buf + 5, plen);
    return (ssize_t)plen;
}

/* ---- KEXINIT payload ---- */
static size_t build_kexinit(uint8_t *p) {
    static const char *nl[] = {
        "curve25519-sha256", "ssh-ed25519",
        "aes128-ctr", "aes128-ctr",
        "hmac-sha2-256", "hmac-sha2-256",
        "none", "none", "", "" };
    size_t o = 0;
    p[o++] = MSG_KEXINIT;
    randombytes_buf(p + o, 16); o += 16;
    for (int i = 0; i < 10; i++) o += put_str(p + o, nl[i], strlen(nl[i]));
    p[o++] = 0;            /* first_kex_packet_follows */
    PUT32(p + o, 0); o += 4;
    return o;
}

/* ---- mpint (for shared secret K) ---- */
static size_t put_mpint(uint8_t *b, const uint8_t *d, size_t n) {
    size_t i = 0;
    while (i < n && d[i] == 0) i++;
    if (i < n && (d[i] & 0x80)) {
        PUT32(b, (uint32_t)(n - i + 1)); b[4] = 0;
        memcpy(b + 5, d + i, n - i); return 4 + 1 + (n - i);
    }
    PUT32(b, (uint32_t)(n - i));
    memcpy(b + 4, d + i, n - i); return 4 + (n - i);
}

/* ---- derive key material per RFC4253 7.2 ---- */
static void derive(uint8_t *out, size_t need, const uint8_t *K,
                   const uint8_t *H, char id, const uint8_t *sid) {
    uint8_t mp[64], km[64]; size_t mlen = put_mpint(mp, K, 32);
    sha256_ctx h;
    sha256_init(&h);
    sha256_update(&h, mp, mlen);
    sha256_update(&h, H, 32);
    sha256_update(&h, (uint8_t *)&id, 1);
    sha256_update(&h, sid, 32);
    sha256_final(&h, km);
    if (need > 32) {
        sha256_init(&h);
        sha256_update(&h, mp, mlen);
        sha256_update(&h, H, 32);
        sha256_update(&h, km, 32);
        sha256_final(&h, km + 32);
    }
    memcpy(out, km, need);
}

static void handle(int fd, const uint8_t *hpk, const uint8_t *hsk) {
    char cver[256];
    /* version exchange */
    if (xsend(fd, V_S "\r\n", strlen(V_S) + 2)) return;
    int i;
    for (i = 0; i < (int)sizeof(cver) - 1; i++) {
        if (xrecv(fd, cver + i, 1)) return;
        if (cver[i] == '\n') break;
    }
    cver[i + 1] = 0;
    int vl = i + 1;
    while (vl > 0 && (cver[vl - 1] == '\n' || cver[vl - 1] == '\r')) cver[--vl] = 0;

    uint8_t skex[512], ckex[4096];
    size_t skexl = build_kexinit(skex);
    if (send_packet(fd, skex, skexl)) return;
    ssize_t ckexl = recv_packet(fd, ckex, sizeof(ckex));
    if (ckexl <= 0 || ckex[0] != MSG_KEXINIT) return;

    /* ECDH */
    uint8_t epriv[32], epub[32], cpub[32], shared[32], H[32], sid[32];
    randombytes_buf(epriv, 32);
    crypto_scalarmult_base(epub, epriv);

    uint8_t ks[128]; size_t ksl = 0;
    ksl += put_str(ks, "ssh-ed25519", 11);
    ksl += put_str(ks + ksl, hpk, 32);

    uint8_t kinit[256];
    ssize_t kinitl = recv_packet(fd, kinit, sizeof(kinit));
    if (kinitl <= 0 || kinit[0] != MSG_KEX_ECDH_INIT) return;
    if (GET32(kinit + 1) != 32) return;
    memcpy(cpub, kinit + 5, 32);
    if (crypto_scalarmult(shared, epriv, cpub)) return;

    /* exchange hash H = SHA256(V_C||V_S||I_C||I_S||K_S||Q_C||Q_S||K) */
    {
        sha256_ctx h; uint8_t t[8];
        sha256_init(&h);
        PUT32(t, (uint32_t)vl); sha256_update(&h, t, 4); sha256_update(&h, (uint8_t *)cver, vl);
        PUT32(t, strlen(V_S)); sha256_update(&h, t, 4); sha256_update(&h, (uint8_t *)V_S, strlen(V_S));
        PUT32(t, (uint32_t)ckexl); sha256_update(&h, t, 4); sha256_update(&h, ckex, ckexl);
        PUT32(t, (uint32_t)skexl); sha256_update(&h, t, 4); sha256_update(&h, skex, skexl);
        PUT32(t, (uint32_t)ksl); sha256_update(&h, t, 4); sha256_update(&h, ks, ksl);
        PUT32(t, 32); sha256_update(&h, t, 4); sha256_update(&h, cpub, 32);
        PUT32(t, 32); sha256_update(&h, t, 4); sha256_update(&h, epub, 32);
        uint8_t mp[64]; size_t mpl = put_mpint(mp, shared, 32); sha256_update(&h, mp, mpl);
        sha256_final(&h, H);
    }
    memcpy(sid, H, 32);

    uint8_t sig[64]; unsigned long long sl;
    crypto_sign_detached(sig, &sl, H, 32, hsk);

    /* KEX_ECDH_REPLY */
    uint8_t rep[512]; size_t rl = 0;
    rep[rl++] = MSG_KEX_ECDH_REPLY;
    rl += put_str(rep + rl, ks, ksl);
    rl += put_str(rep + rl, epub, 32);
    {
        uint8_t sb[128]; size_t sbl = 0;
        sbl += put_str(sb, "ssh-ed25519", 11);
        sbl += put_str(sb + sbl, sig, 64);
        rl += put_str(rep + rl, sb, sbl);
    }
    if (send_packet(fd, rep, rl)) return;

    /* key derivation */
    uint8_t ivc[16], ivs[16], kc[16], ksc[16], ikc[32], iks[32];
    derive(ivc, 16, shared, H, 'A', sid);
    derive(ivs, 16, shared, H, 'B', sid);
    derive(kc, 16, shared, H, 'C', sid);
    derive(ksc, 16, shared, H, 'D', sid);
    derive(ikc, 32, shared, H, 'E', sid);
    derive(iks, 32, shared, H, 'F', sid);

    /* NEWKEYS */
    uint8_t nk = MSG_NEWKEYS;
    if (send_packet(fd, &nk, 1)) return;
    memcpy(s2c.mac_key, iks, 32);
    aes128_ctr_init(&s2c.aes, ksc, ivs);
    s2c.seq = 3; s2c.active = 1;

    uint8_t tmp[256];
    if (recv_packet(fd, tmp, sizeof(tmp)) <= 0 || tmp[0] != MSG_NEWKEYS) return;
    memcpy(c2s.mac_key, ikc, 32);
    aes128_ctr_init(&c2s.aes, kc, ivc);
    c2s.seq = 3; c2s.active = 1;

    /* SERVICE_REQUEST -> ACCEPT */
    if (recv_packet(fd, tmp, sizeof(tmp)) <= 0 || tmp[0] != MSG_SERVICE_REQUEST) return;
    uint8_t sa[64]; size_t sal = 0;
    sa[sal++] = MSG_SERVICE_ACCEPT;
    sal += put_str(sa + sal, "ssh-userauth", 12);
    if (send_packet(fd, sa, sal)) return;

    /* USERAUTH loop */
    for (;;) {
        ssize_t n = recv_packet(fd, tmp, sizeof(tmp));
        if (n <= 0 || tmp[0] != MSG_USERAUTH_REQUEST) return;
        uint8_t *p = tmp + 1, *end = tmp + n, *fld;
        char user[64], meth[32];
        uint32_t ul, svl, ml;
        fld = rd_field(&p, end, &ul); if (!fld || ul >= sizeof(user)) return;
        memcpy(user, fld, ul); user[ul] = 0;
        if (!rd_field(&p, end, &svl)) return;          /* service (skipped) */
        (void)svl;
        fld = rd_field(&p, end, &ml); if (!fld || ml >= sizeof(meth)) return;
        memcpy(meth, fld, ml); meth[ml] = 0;
        if (!strcmp(meth, "password")) {
            char pass[64]; uint32_t pl;
            if (p >= end) return;
            p += 1;                                    /* change flag */
            fld = rd_field(&p, end, &pl); if (!fld || pl >= sizeof(pass)) return;
            memcpy(pass, fld, pl); pass[pl] = 0;
            if (!strcmp(user, "user") && !strcmp(pass, "password123")) {
                uint8_t ok = MSG_USERAUTH_SUCCESS;
                if (send_packet(fd, &ok, 1)) return;
                break;
            }
        }
        uint8_t f[32]; size_t fl = 0;
        f[fl++] = 51;                                  /* USERAUTH_FAILURE */
        fl += put_str(f + fl, "password", 8);
        f[fl++] = 0;
        if (send_packet(fd, f, fl)) return;
    }

    /* CHANNEL_OPEN -> CONFIRMATION */
    ssize_t con = recv_packet(fd, tmp, sizeof(tmp));
    if (con <= 0 || tmp[0] != MSG_CHANNEL_OPEN) return;
    uint8_t *p = tmp + 1, *end = tmp + con;
    uint32_t ctl;
    if (!rd_field(&p, end, &ctl)) return;              /* channel type (skipped) */
    (void)ctl;
    if (end - p < 4) return;
    uint32_t cchan = GET32(p);
    uint8_t cc[32]; size_t ccl = 0;
    cc[ccl++] = MSG_CHANNEL_OPEN_CONFIRMATION;
    PUT32(cc + ccl, cchan); ccl += 4;
    PUT32(cc + ccl, 0); ccl += 4;                      /* server channel */
    PUT32(cc + ccl, 32768); ccl += 4;                  /* window */
    PUT32(cc + ccl, 16384); ccl += 4;                  /* max packet */
    if (send_packet(fd, cc, ccl)) return;

    /* channel requests until shell/exec */
    int ready = 0;
    while (!ready) {
        ssize_t n = recv_packet(fd, tmp, sizeof(tmp));
        if (n <= 0) return;
        if (tmp[0] != MSG_CHANNEL_REQUEST) break;
        uint8_t *q = tmp + 1, *qend = tmp + n, *rf;
        char rt[32]; uint32_t rtl;
        if (qend - q < 4) return;
        q += 4;                                        /* recipient */
        rf = rd_field(&q, qend, &rtl); if (!rf || rtl >= sizeof(rt)) return;
        memcpy(rt, rf, rtl); rt[rtl] = 0;
        if (q >= qend) return;
        uint8_t want = *q;
        if (!strcmp(rt, "shell") || !strcmp(rt, "exec")) ready = 1;
        if (want) {
            uint8_t r[8]; r[0] = MSG_CHANNEL_SUCCESS; PUT32(r + 1, cchan);
            if (send_packet(fd, r, 5)) return;
        }
    }

    /* CHANNEL_DATA "Hello World" */
    if (ready) {
        const char *msg = "Hello World\r\n";
        uint8_t d[64]; size_t dl = 0;
        d[dl++] = MSG_CHANNEL_DATA;
        PUT32(d + dl, cchan); dl += 4;
        dl += put_str(d + dl, msg, strlen(msg));
        if (send_packet(fd, d, dl)) return;
    }

    /* EOF + CLOSE */
    uint8_t e[8]; e[0] = MSG_CHANNEL_EOF; PUT32(e + 1, cchan); send_packet(fd, e, 5);
    e[0] = MSG_CHANNEL_CLOSE; send_packet(fd, e, 5);
    recv_packet(fd, tmp, sizeof(tmp));
}

int main(void) {
    uint8_t hpk[32], hsk[64];
    crypto_sign_keypair(hpk, hsk);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) return 1;
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(PORT);
    if (bind(lfd, (struct sockaddr *)&a, sizeof(a)) < 0) return 1;
    if (listen(lfd, 5) < 0) return 1;

    for (;;) {
        int cfd = accept(lfd, 0, 0);
        if (cfd < 0) continue;
        memset(&c2s, 0, sizeof(c2s));
        memset(&s2c, 0, sizeof(s2c));
        handle(cfd, hpk, hsk);
        close(cfd);
    }
}
