/*
 * SHA-256 / SHA-224 (FIPS 180-4) and HMAC (RFC 2104).
 *
 * Part of httpclient, MIT licensed, see LICENSE.
 */
#include "sha2.h"

#include <string.h>

static const uint32_t K[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha2_process(sha2_context *ctx, const unsigned char *p) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
           ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
  }
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t v[8];
  for (int i = 0; i < 8; i++) v[i] = ctx->state[i];
  for (int i = 0; i < 64; i++) {
    uint32_t S1 = ror(v[4], 6) ^ ror(v[4], 11) ^ ror(v[4], 25);
    uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
    uint32_t t1 = v[7] + S1 + ch + K[i] + w[i];
    uint32_t S0 = ror(v[0], 2) ^ ror(v[0], 13) ^ ror(v[0], 22);
    uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
    uint32_t t2 = S0 + maj;
    v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
    v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
  }
  for (int i = 0; i < 8; i++) ctx->state[i] += v[i];
}

void sha2_starts(sha2_context *ctx, int is224) {
  ctx->total[0] = ctx->total[1] = 0;
  ctx->is224 = is224;
  static const uint32_t iv256[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  static const uint32_t iv224[8] = {0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
                                    0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4};
  memcpy(ctx->state, is224 ? iv224 : iv256, sizeof(ctx->state));
}

void sha2_update(sha2_context *ctx, const void *input0, size_t ilen) {
  const unsigned char *input = (const unsigned char *)input0;
  if (!ilen) return;
  size_t left = ctx->total[0] & 0x3F;
  size_t fill = 64 - left;

  ctx->total[0] += (uint32_t)ilen;
  if (ctx->total[0] < (uint32_t)ilen) ctx->total[1]++;
  ctx->total[1] += (uint32_t)((uint64_t)ilen >> 32);

  if (left && ilen >= fill) {
    memcpy(ctx->buffer + left, input, fill);
    sha2_process(ctx, ctx->buffer);
    input += fill;
    ilen -= fill;
    left = 0;
  }
  while (ilen >= 64) {
    sha2_process(ctx, input);
    input += 64;
    ilen -= 64;
  }
  if (ilen) memcpy(ctx->buffer + left, input, ilen);
}

void sha2_finish(sha2_context *ctx, unsigned char output[32]) {
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
  sha2_update(ctx, pad, padn);
  sha2_update(ctx, msglen, 8);

  int words = ctx->is224 ? 7 : 8;
  for (int i = 0; i < words; i++) {
    for (int j = 0; j < 4; j++) output[4 * i + j] = (unsigned char)(ctx->state[i] >> (24 - 8 * j));
  }
}

void sha2(const void *input, size_t ilen, unsigned char output[32], int is224) {
  sha2_context ctx;
  sha2_starts(&ctx, is224);
  sha2_update(&ctx, input, ilen);
  sha2_finish(&ctx, output);
  memset(&ctx, 0, sizeof(ctx));
}

void sha2_hmac_starts(sha2_context *ctx, const unsigned char *key, size_t keylen, int is224) {
  unsigned char sum[32];
  if (keylen > 64) {
    sha2(key, keylen, sum, is224);
    key = sum;
    keylen = is224 ? 28 : 32;
  }
  memset(ctx->ipad, 0x36, 64);
  memset(ctx->opad, 0x5C, 64);
  for (size_t i = 0; i < keylen; i++) {
    ctx->ipad[i] ^= key[i];
    ctx->opad[i] ^= key[i];
  }
  memset(sum, 0, sizeof(sum));
  sha2_starts(ctx, is224);
  sha2_update(ctx, ctx->ipad, 64);
}

void sha2_hmac_update(sha2_context *ctx, const unsigned char *input, size_t ilen) {
  sha2_update(ctx, input, ilen);
}

void sha2_hmac_finish(sha2_context *ctx, unsigned char output[32]) {
  int is224 = ctx->is224;
  size_t hlen = is224 ? 28 : 32;
  unsigned char tmp[32];
  sha2_finish(ctx, tmp);
  sha2_starts(ctx, is224);
  sha2_update(ctx, ctx->opad, 64);
  sha2_update(ctx, tmp, hlen);
  sha2_finish(ctx, output);
  memset(tmp, 0, sizeof(tmp));
}

void sha2_hmac_reset(sha2_context *ctx) {
  sha2_starts(ctx, ctx->is224);
  sha2_update(ctx, ctx->ipad, 64);
}

void sha2_hmac(const unsigned char *key, size_t keylen,
               const unsigned char *input, size_t ilen,
               unsigned char output[32], int is224) {
  sha2_context ctx;
  sha2_hmac_starts(&ctx, key, keylen, is224);
  sha2_hmac_update(&ctx, input, ilen);
  sha2_hmac_finish(&ctx, output);
  memset(&ctx, 0, sizeof(ctx));
}
