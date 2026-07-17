/*
 * Butano fixed-8x8 sprite font built from the public-domain unscii-8 bitmap
 * font (http://viznut.fi/unscii/). Glyph layout mirrors Butano's common
 * fixed-8x8 font: ASCII '!'..'~' followed by 16 Latin-1 UTF-8 characters.
 */

#ifndef UNSCII_FONT_H
#define UNSCII_FONT_H

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_unscii_8_font.h"

namespace hc
{

constexpr bn::utf8_character unscii_8_sprite_font_utf8_characters[] = {
    "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ", "á", "é", "í", "ó", "ú", "ü", "ñ", "¡", "¿"
};

constexpr bn::span<const bn::utf8_character> unscii_8_sprite_font_utf8_characters_span(
        unscii_8_sprite_font_utf8_characters);

constexpr auto unscii_8_sprite_font_utf8_characters_map =
        bn::utf8_characters_map<unscii_8_sprite_font_utf8_characters_span>();

constexpr bn::sprite_font unscii_8_sprite_font(
        bn::sprite_items::unscii_8_font, unscii_8_sprite_font_utf8_characters_map.reference());

}

#endif
