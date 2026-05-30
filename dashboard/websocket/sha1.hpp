#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cstring>

namespace trading::dashboard {

// SHA-1 (FIPS 180-4) — used only for the WebSocket handshake accept key.
inline std::array<uint8_t, 20> sha1(const uint8_t* data, std::size_t len) noexcept {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    auto rotl = [](uint32_t x, int n) -> uint32_t {
        return (x << n) | (x >> (32 - n));
    };

    uint64_t bit_len  = static_cast<uint64_t>(len) * 8;
    std::size_t padded = len + 1;
    while (padded % 64 != 56) ++padded;
    padded += 8;

    std::vector<uint8_t> msg(padded, 0);
    std::memcpy(msg.data(), data, len);
    msg[len] = 0x80;
    for (int i = 0; i < 8; ++i)
        msg[padded - 8 + i] = static_cast<uint8_t>(bit_len >> (56 - i * 8));

    for (std::size_t off = 0; off < padded; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[off + i*4])     << 24)
                 | (static_cast<uint32_t>(msg[off + i*4 + 1]) << 16)
                 | (static_cast<uint32_t>(msg[off + i*4 + 2]) <<  8)
                 |  static_cast<uint32_t>(msg[off + i*4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            uint32_t t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = rotl(t, 1);
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if      (i < 20) { f = (b & c) | (~b & d);           k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
            uint32_t tmp = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = tmp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    std::array<uint8_t, 20> digest{};
    for (int i = 0; i < 5; ++i) {
        digest[i*4]   = (h[i] >> 24) & 0xFF;
        digest[i*4+1] = (h[i] >> 16) & 0xFF;
        digest[i*4+2] = (h[i] >>  8) & 0xFF;
        digest[i*4+3] = (h[i]      ) & 0xFF;
    }
    return digest;
}

} // namespace trading::dashboard
