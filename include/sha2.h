/**
 * \file sha2.h
 *
 * \brief SHA-256 / SHA-224 hash functions and HMAC (FIPS 180-4, RFC 2104).
 *
 * Part of httpclient, MIT licensed, see LICENSE.
 */
#ifndef _SHA2_H_
#define _SHA2_H_

#include <stddef.h>
#include <stdint.h>

/** SHA-256/224 context. The pads are only used by the HMAC functions. */
typedef struct {
  uint32_t total[2];        /* number of bytes processed (low, high) */
  uint32_t state[8];        /* intermediate digest state */
  unsigned char buffer[64]; /* data block being processed */
  unsigned char ipad[64];   /* HMAC: inner padding */
  unsigned char opad[64];   /* HMAC: outer padding */
  int is224;                /* 0 => SHA-256, else SHA-224 */
} sha2_context;

/** is224: 0 = SHA-256 (32 byte output), 1 = SHA-224 (28 byte output). */
void sha2_starts(sha2_context *ctx, int is224);
void sha2_update(sha2_context *ctx, const void *input, size_t ilen);
void sha2_finish(sha2_context *ctx, unsigned char output[32]);

/** output = SHA-2(input) */
void sha2(const void *input, size_t ilen, unsigned char output[32], int is224);

void sha2_hmac_starts(sha2_context *ctx, const unsigned char *key, size_t keylen, int is224);
void sha2_hmac_update(sha2_context *ctx, const unsigned char *input, size_t ilen);
void sha2_hmac_finish(sha2_context *ctx, unsigned char output[32]);
void sha2_hmac_reset(sha2_context *ctx);

/** output = HMAC-SHA-2(key, input) */
void sha2_hmac(const unsigned char *key, size_t keylen,
               const unsigned char *input, size_t ilen,
               unsigned char output[32], int is224);

#endif /* _SHA2_H_ */
