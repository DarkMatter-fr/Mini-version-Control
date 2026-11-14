#include "sha1.hpp"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

inline uint32_t rol32(uint32_t value, size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

void sha1Block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (size_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (size_t i = 16; i < 80; ++i) {
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (size_t i = 0; i < 80; ++i) {
        uint32_t f = 0;
        uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol32(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}  // namespace

std::string sha1Raw(const std::string& data) {
    uint32_t state[5] = {
        0x67452301,
        0xEFCDAB89,
        0x98BADCFE,
        0x10325476,
        0xC3D2E1F0
    };

    uint64_t totalBits = static_cast<uint64_t>(data.size()) * 8;
    size_t offset = 0;
    while (offset + 64 <= data.size()) {
        sha1Block(state, reinterpret_cast<const uint8_t*>(data.data() + offset));
        offset += 64;
    }

    // Padding
    uint8_t buffer[128] = {0};
    size_t rem = data.size() - offset;
    if (rem > 0) {
        std::memcpy(buffer, data.data() + offset, rem);
    }
    buffer[rem] = 0x80;

    size_t padLen = (rem < 56) ? 64 : 128;
    for (size_t i = 0; i < 8; ++i) {
        buffer[padLen - 8 + i] = static_cast<uint8_t>((totalBits >> ((7 - i) * 8)) & 0xFF);
    }

    sha1Block(state, buffer);
    if (padLen == 128) {
        sha1Block(state, buffer + 64);
    }

    std::string raw(20, '\0');
    for (size_t i = 0; i < 5; ++i) {
        raw[i * 4]     = static_cast<char>((state[i] >> 24) & 0xFF);
        raw[i * 4 + 1] = static_cast<char>((state[i] >> 16) & 0xFF);
        raw[i * 4 + 2] = static_cast<char>((state[i] >> 8) & 0xFF);
        raw[i * 4 + 3] = static_cast<char>(state[i] & 0xFF);
    }
    return raw;
}

std::string toHex(const std::string& raw) {
    static const char hexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(raw.size() * 2);
    for (unsigned char c : raw) {
        hex.push_back(hexDigits[(c >> 4) & 0x0F]);
        hex.push_back(hexDigits[c & 0x0F]);
    }
    return hex;
}

std::string sha1Hex(const std::string& data) {
    return toHex(sha1Raw(data));
}

std::string fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("Hex string must have an even length");
    }
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("Invalid hex character: " + std::string(1, c));
    };

    std::string raw;
    raw.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = hexVal(hex[i]);
        int lo = hexVal(hex[i + 1]);
        raw.push_back(static_cast<char>((hi << 4) | lo));
    }
    return raw;
}

bool isValidHash(const std::string& s) {
    if (s.size() != 40) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

