/*
 * SHA-1 (FIPS 180-1) and HMAC-SHA-1 (RFC 2104).
 *
 * Part of httpclient, MIT licensed, see LICENSE.
 */
#include "SHA1.h"

#include <string.h>

static inline uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_process(sha1_context *ctx, const unsigned char *p) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
           ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
  }
  for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

  uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3],
           e = ctx->state[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
    else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
    else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
    else { f = b ^ c ^ d; k = 0xCA62C1D6; }
    uint32_t t = rol(a, 5) + f + e + k + w[i];
    e = d; d = c; c = rol(b, 30); b = a; a = t;
  }
  ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c;
  ctx->state[3] += d; ctx->state[4] += e;
}

void sha1_starts(sha1_context *ctx) {
  ctx->total[0] = ctx->total[1] = 0;
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->state[4] = 0xC3D2E1F0;
}

void sha1_update(sha1_context *ctx, const unsigned char *input, size_t ilen) {
  if (!ilen) return;
  size_t left = ctx->total[0] & 0x3F;
  size_t fill = 64 - left;

  ctx->total[0] += (uint32_t)ilen;
  if (ctx->total[0] < (uint32_t)ilen) ctx->total[1]++;
  ctx->total[1] += (uint32_t)((uint64_t)ilen >> 32);

  if (left && ilen >= fill) {
    memcpy(ctx->buffer + left, input, fill);
    sha1_process(ctx, ctx->buffer);
    input += fill;
    ilen -= fill;
    left = 0;
  }
  while (ilen >= 64) {
    sha1_process(ctx, input);
    input += 64;
    ilen -= 64;
  }
  if (ilen) memcpy(ctx->buffer + left, input, ilen);
}

void sha1_finish(sha1_context *ctx, unsigned char output[20]) {
  uint32_t high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
  uint32_t low = ctx->total[0] << 3;

  unsigned char msglen[8];
  for (int i = 0; i < 4; i++) {
    msglen[i] = (unsigned char)(high >> (24 - 8 * i));
    msglen[4 + i] = (unsigned char)(low >> (24 - 8 * i));
  }

  unsigned char pad[64] = {0x80};
  size_t last = ctx->total[0] & 0x3F;
  size_t padn = (last < 56) ? (56 - last) : (120 - last);
  sha1_update(ctx, pad, padn);
  sha1_update(ctx, msglen, 8);

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 4; j++) output[4 * i + j] = (unsigned char)(ctx->state[i] >> (24 - 8 * j));
  }
}

void sha1(const unsigned char *input, size_t ilen, unsigned char output[20]) {
  sha1_context ctx;
  sha1_starts(&ctx);
  sha1_update(&ctx, input, ilen);
  sha1_finish(&ctx, output);
  memset(&ctx, 0, sizeof(ctx));
}

void sha1_hmac_starts(sha1_context *ctx, const unsigned char *key, size_t keylen) {
  unsigned char sum[20];
  if (keylen > 64) {
    sha1(key, keylen, sum);
    key = sum;
    keylen = 20;
  }
  memset(ctx->ipad, 0x36, 64);
  memset(ctx->opad, 0x5C, 64);
  for (size_t i = 0; i < keylen; i++) {
    ctx->ipad[i] ^= key[i];
    ctx->opad[i] ^= key[i];
  }
  memset(sum, 0, sizeof(sum));
  sha1_starts(ctx);
  sha1_update(ctx, ctx->ipad, 64);
}

void sha1_hmac_update(sha1_context *ctx, const unsigned char *input, size_t ilen) {
  sha1_update(ctx, input, ilen);
}

void sha1_hmac_finish(sha1_context *ctx, unsigned char output[20]) {
  unsigned char tmp[20];
  sha1_finish(ctx, tmp);
  sha1_starts(ctx);
  sha1_update(ctx, ctx->opad, 64);
  sha1_update(ctx, tmp, 20);
  sha1_finish(ctx, output);
  memset(tmp, 0, sizeof(tmp));
}

void sha1_hmac_reset(sha1_context *ctx) {
  sha1_starts(ctx);
  sha1_update(ctx, ctx->ipad, 64);
}

void sha1_hmac(const unsigned char *key, size_t keylen,
               const unsigned char *input, size_t ilen, unsigned char output[20]) {
  sha1_context ctx;
  sha1_hmac_starts(&ctx, key, keylen);
  sha1_hmac_update(&ctx, input, ilen);
  sha1_hmac_finish(&ctx, output);
  memset(&ctx, 0, sizeof(ctx));
}
