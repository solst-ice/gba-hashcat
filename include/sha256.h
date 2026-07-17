/*
 * Minimal SHA-256 implementation for GBA-Hashcat.
 *
 * Self-contained, no dynamic allocation. Suitable for the GBA.
 */

#ifndef GBA_HASHCAT_SHA256_H
#define GBA_HASHCAT_SHA256_H

#include <cstdint>

#include "bn_string.h"
#include "bn_string_view.h"

namespace hc
{
    // Raw 32-byte (256-bit) SHA-256 digest.
    struct sha256_digest
    {
        uint8_t bytes[32];
    };

    // Computes the SHA-256 digest of the given byte buffer.
    sha256_digest sha256(const uint8_t* data, uint32_t length);

    // Convenience overload for text input.
    sha256_digest sha256(const bn::string_view& text);

    // Formats a digest as a lowercase 64-character hex string.
    bn::string<64> to_hex(const sha256_digest& digest);

    // Computes and formats the SHA-256 of text in one call.
    inline bn::string<64> sha256_hex(const bn::string_view& text)
    {
        return to_hex(sha256(text));
    }
}

#endif
