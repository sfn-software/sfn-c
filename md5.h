/*
 * MD5 hash (RFC 1321). Streaming API: init, update zero or more times, then final.
 * Self-contained, no external dependencies.
 */

#ifndef SIPHON_MD5_H
#define SIPHON_MD5_H

#include <stddef.h>
#include <stdint.h>

#define MD5_DIGEST_SIZE 16
#define MD5_HEX_SIZE 32

typedef struct {
    uint32_t state[4];   /* a, b, c, d */
    uint64_t count;      /* total bytes fed so far */
    uint8_t buffer[64];  /* leftover bytes from last update */
} md5_ctx;

void md5_init(md5_ctx *ctx);
void md5_update(md5_ctx *ctx, const void *data, size_t len);
void md5_final(md5_ctx *ctx, uint8_t digest[MD5_DIGEST_SIZE]);

/* Write digest as lowercase hex into out (32 chars + trailing NUL). */
void md5_to_hex(const uint8_t digest[MD5_DIGEST_SIZE], char out[MD5_HEX_SIZE + 1]);

#endif
