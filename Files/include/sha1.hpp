#pragma once
// SHA-1 (RFC 3174) plus hex helpers. Used for content addressing, not security.
#include <string>

std::string sha1Raw(const std::string& data);  // 20 raw bytes
std::string sha1Hex(const std::string& data);  // 40 lowercase hex chars

std::string toHex(const std::string& raw);     // raw bytes -> hex
std::string fromHex(const std::string& hex);   // hex -> raw bytes (throws on bad input)
bool isValidHash(const std::string& s);        // exactly 40 lowercase hex chars
