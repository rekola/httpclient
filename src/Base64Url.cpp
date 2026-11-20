// Base64url (RFC 4648 section 5), without padding.
//
// Part of httpclient, MIT licensed, see LICENSE.

#include "Base64Url.h"

#include <cstdint>
#include <string>

using namespace std;
using namespace httpclient;

static const char encTable[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Returns the 6-bit value of a base64url character, or -1.
static int decodeChar(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '-') return 62;
  if (c == '_') return 63;
  return -1;
}

std::string
Base64Url::encode(const void *input, size_t inputLen) {
  std::string output;
  if (!input) return output;

  const uint8_t *p = (const uint8_t *)input;
  output.reserve((inputLen * 4 + 2) / 3);

  size_t i = 0;
  for (; i + 3 <= inputLen; i += 3) {
    uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i + 1] << 8) | p[i + 2];
    output += encTable[(v >> 18) & 0x3F];
    output += encTable[(v >> 12) & 0x3F];
    output += encTable[(v >> 6) & 0x3F];
    output += encTable[v & 0x3F];
  }
  size_t rest = inputLen - i;
  if (rest == 1) {
    output += encTable[p[i] >> 2];
    output += encTable[(p[i] & 0x03) << 4];
  } else if (rest == 2) {
    output += encTable[p[i] >> 2];
    output += encTable[((p[i] & 0x03) << 4) | (p[i + 1] >> 4)];
    output += encTable[(p[i + 1] & 0x0F) << 2];
  }
  return output;
}

// Decodes unpadded base64url. If output is null only the length is computed.
bool
Base64Url::decode(const char *input, size_t inputLen, void *output, size_t *outputLen) {
  if (input == nullptr && inputLen != 0) return false;
  if (outputLen == nullptr) return false;
  if ((inputLen % 4) == 1) return false;

  uint8_t *p = (uint8_t *)output;
  size_t n = 0;
  uint32_t value = 0;
  bool error = false;

  for (size_t i = 0; i < inputLen && !error; i++) {
    int d = decodeChar((unsigned char)input[i]);
    if (d < 0) {
      error = true;
      break;
    }
    value = (value << 6) | (uint32_t)d;
    if ((i % 4) == 3) {
      if (p) {
        p[n] = (value >> 16) & 0xFF;
        p[n + 1] = (value >> 8) & 0xFF;
        p[n + 2] = value & 0xFF;
      }
      n += 3;
      value = 0;
    }
  }

  if (!error) {
    if ((inputLen % 4) == 2) {
      if (p) p[n] = (value >> 4) & 0xFF;
      n += 1;
    } else if ((inputLen % 4) == 3) {
      if (p) {
        p[n] = (value >> 10) & 0xFF;
        p[n + 1] = (value >> 2) & 0xFF;
      }
      n += 2;
    }
  }

  *outputLen = n;
  return !error;
}

long long
Base64Url::decode_id(const std::string &s) {
  // Zero-initialized so that short or invalid input cannot read garbage.
  std::string bytes(s.size() + 8, '\0');
  size_t len = 0;
  decode(s.c_str(), s.size(), &bytes[0], &len);

  uint64_t id = 0;
  for (int i = 7; i >= 0; i--) id = (id << 8) | (unsigned char)bytes[i];
  return (long long)id;
}

std::string
Base64Url::encode_id(long long id) {
  unsigned char bytes[8];
  for (int i = 0; i < 8; i++) bytes[i] = (unsigned char)(((uint64_t)id >> (8 * i)) & 0xff);
  return encode(bytes, 8);
}

std::string
Base64Url::encode_id_bigendian(long long id) {
  unsigned char bytes[8];
  for (int i = 0; i < 8; i++) bytes[7 - i] = (unsigned char)(((uint64_t)id >> (8 * i)) & 0xff);
  return encode(bytes, 8);
}
