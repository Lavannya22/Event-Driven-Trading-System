#pragma once

#include <string>
#include <cstdint>

namespace trading::dashboard {

inline std::string base64_encode(const uint8_t* data, std::size_t len) {
    static constexpr char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (std::size_t i = 0; i < len; i += 3) {
        uint32_t b = (static_cast<uint32_t>(data[i]) << 16)
                   | (i + 1 < len ? static_cast<uint32_t>(data[i + 1]) << 8 : 0u)
                   | (i + 2 < len ? static_cast<uint32_t>(data[i + 2])      : 0u);
        out += tbl[(b >> 18) & 0x3F];
        out += tbl[(b >> 12) & 0x3F];
        out += (i + 1 < len) ? tbl[(b >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? tbl[(b >> 0) & 0x3F] : '=';
    }
    return out;
}

} // namespace trading::dashboard
