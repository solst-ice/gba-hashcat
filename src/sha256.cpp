/*
 * Minimal SHA-256 implementation for GBA-Hashcat.
 *
 * Follows FIPS 180-4. Operates on a fixed-size internal state, so it never
 * allocates and is safe to call from the GBA main loop.
 */

#include "sha256.h"

namespace hc
{
    // Both SHA-256 overloads are implemented entirely in src/sha256_arm.s.
    bn::string<64> to_hex(const sha256_digest& digest)
    {
        constexpr char hex_chars[] = "0123456789abcdef";

        bn::string<64> result;
        for(uint8_t byte : digest.bytes)
        {
            result.push_back(hex_chars[byte >> 4]);
            result.push_back(hex_chars[byte & 0x0f]);
        }

        return result;
    }
}
