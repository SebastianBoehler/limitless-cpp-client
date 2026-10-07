// sha3.h
// 19-Nov-11  Markku-Juhani O. Saarinen <mjos@iki.fi>
// Public domain. Vendored from https://github.com/mjosaarinen/tiny_sha3

#ifndef LIMITLESS_TINY_SHA3_H
#define LIMITLESS_TINY_SHA3_H

#include <stddef.h>
#include <stdint.h>

#ifndef KECCAKF_ROUNDS
#define KECCAKF_ROUNDS 24
#endif

#ifndef ROTL64
#define ROTL64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))
#endif

typedef struct {
    union {
        uint8_t b[200];
        uint64_t q[25];
    } st;
    int pt, rsiz, mdlen;
} sha3_ctx_t;

void sha3_keccakf(uint64_t st[25]);
int sha3_init(sha3_ctx_t *c, int mdlen);
int sha3_update(sha3_ctx_t *c, const void *data, size_t len);
int sha3_final(void *md, sha3_ctx_t *c);

#endif
