/**
 * \file SHA1.h
 *
 * \brief SHA-1 hash function and HMAC-SHA-1 (FIPS 180-1, RFC 2104).
 *
 * Part of httpclient, MIT licensed, see LICENSE.
 */
#ifndef _SHA1_H_
#define _SHA1_H_

#include <stddef.h>
#include <stdint.h>

/** SHA-1 context. The pads are only used by the HMAC functions. */
typedef struct {
  uint32_t total[2];        /* number of bytes processed (low, high) */
  uint32_t state[5];        /* intermediate digest state */
  unsigned char buffer[64]; /* data block being processed */
  unsigned char ipad[64];   /* HMAC: inner padding */
  unsigned char opad[64];   /* HMAC: outer padding */
} sha1_context;

void sha1_starts(sha1_context *ctx);
void sha1_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
void sha1_finish(sha1_context *ctx, unsigned char output[20]);

/** output = SHA-1(input) */
void sha1(const unsigned char *input, size_t ilen, unsigned char output[20]);

void sha1_hmac_starts(sha1_context *ctx, const unsigned char *key, size_t keylen);
void sha1_hmac_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
void sha1_hmac_finish(sha1_context *ctx, unsigned char output[20]);
void sha1_hmac_reset(sha1_context *ctx);

/** output = HMAC-SHA-1(key, input) */
void sha1_hmac(const unsigned char *key, size_t keylen,
               const unsigned char *input, size_t ilen,
               unsigned char output[20]);

#endif /* _SHA1_H_ */
