/*
 * GBA-Hashcat: a joke password "cracker" for the Game Boy Advance.
 *
 * Boot flow:
 *   1. Splash: black -> logo fades in (3s) -> neon title glitches in on top of
 *      the logo (3s) -> everything fades out (1s).
 *   2. Terminal boot scene: system info + target hash printed line by line.
 */

#include "bn_core.h"
#include "bn_array.h"
#include "bn_color.h"
#include "bn_random.h"
#include "bn_vector.h"
#include "bn_keypad.h"
#include "bn_bpp_mode.h"
#include "bn_display.h"
#include "bn_green_swap.h"
#include "bn_bgs_mosaic.h"
#include "bn_timer.h"
#include "bn_timers.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_ptr.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_text_generator.h"
#include "bn_regular_bg_position_hbe_ptr.h"

#include "bn_regular_bg_items_logo.h"
#include "bn_regular_bg_items_title.h"

#include "unscii_font.h"
#include "sha256.h"

// Embedded wordlist. assets/min-rockyou.txt is the ~1,000,000-entry list fetched
// from https://github.com/ignis-sec/Pwdb-Public/blob/master/wordlists/ignis-1M.txt
// (kept under the min-rockyou.txt name); the Makefile copies it to
// data/min_rockyou.bin, which bin2s exposes as this header.
#include "min_rockyou_bin.h"

namespace
{
    // Frame counts at 60 FPS.
    constexpr int fade_in_frames = 180;   // 3s logo fade-in
    constexpr int glitch_frames = 24;     // brief glitch-in of the title
    constexpr int title_hold_frames = 120; // static title display (2s)
    constexpr int fade_out_frames = 60;   // 1s fade-out to black

    // Background priorities: lower value draws in front. The neon title sits on
    // top of the logo, with its transparent (index 0 / black) pixels letting the
    // logo show through.
    constexpr int logo_priority = 2;
    constexpr int title_priority = 1;

    // Fades the background palettes toward black. intensity 1 == fully black.
    void set_black_fade(bn::fixed intensity)
    {
        bn::bg_palettes::set_fade(bn::color(0, 0, 0), intensity);
    }

    // Clears the per-scanline horizontal offsets back to a clean (non-glitched)
    // image and pushes the update to the hardware.
    void clear_glitch(bn::fixed (&deltas)[bn::display::height()],
                      bn::regular_bg_position_hbe_ptr& hbe,
                      bn::regular_bg_ptr& title)
    {
        for(bn::fixed& delta : deltas)
        {
            delta = 0;
        }

        hbe.reload_deltas_ref();
        bn::green_swap::set_enabled(false);
        bn::bgs_mosaic::set_stretch(0);
        title.set_position(0, 0);
    }

    // Glitches the title in for a few frames, snaps it to a clean static image,
    // holds it, then fades the whole scene out to black.
    void glitch_and_fade(bn::regular_bg_ptr& title)
    {
        bn::random random;

        // Per-scanline horizontal offsets, referenced (not copied) by the HBE,
        // so this array must outlive it.
        bn::fixed h_deltas[bn::display::height()] = {};
        bn::regular_bg_position_hbe_ptr h_hbe =
                bn::regular_bg_position_hbe_ptr::create_horizontal(title, h_deltas);

        // Brief, intense glitch-in.
        for(int frame = 0; frame < glitch_frames; ++frame)
        {
            int y = 0;
            while(y < bn::display::height())
            {
                int band = random.get_int(3, 18);
                bn::fixed offset = random.get_int(-9, 10);

                for(int i = 0; i < band && y < bn::display::height(); ++i, ++y)
                {
                    h_deltas[y] = offset;
                }
            }

            h_hbe.reload_deltas_ref();

            // Chromatic-aberration flicker and occasional mosaic/jitter.
            bn::green_swap::set_enabled(random.get_int(3) == 0);
            bn::bgs_mosaic::set_stretch(bn::fixed(random.get_int(0, 4)) / 10);
            title.set_y(random.get_int(-2, 3));

            bn::core::update();
        }

        // Snap to a clean, static title and hold it.
        clear_glitch(h_deltas, h_hbe, title);

        for(int frame = 0; frame < title_hold_frames; ++frame)
        {
            bn::core::update();
        }

        // Fade the whole scene out to black.
        for(int frame = 0; frame < fade_out_frames; ++frame)
        {
            set_black_fade(bn::fixed(frame + 1) / fade_out_frames);
            bn::core::update();
        }
    }

    void splash_scene()
    {
        bn::bg_palettes::set_transparent_color(bn::color(0, 0, 0));
        set_black_fade(1); // start fully black

        // Logo fades in from black over 3s.
        bn::regular_bg_ptr logo = bn::regular_bg_items::logo.create_bg(0, 0);
        logo.set_priority(logo_priority);

        for(int frame = 0; frame < fade_in_frames; ++frame)
        {
            set_black_fade(bn::fixed(fade_in_frames - 1 - frame) / fade_in_frames);
            bn::core::update();
        }

        set_black_fade(0);

        // As soon as the logo is in, layer the neon title on top of it and let
        // it glitch in, then fade everything out together.
        bn::regular_bg_ptr title = bn::regular_bg_items::title.create_bg(0, 0);
        title.set_priority(title_priority);

        glitch_and_fade(title);
    }

    // --- Terminal boot scene --------------------------------------------------

    // The unscii-8 font is 1-bit: index 0 is transparent, index 1 is the glyph.
    // Recoloring the text is just a matter of setting index 1 to the wanted color.
    constexpr bn::array<bn::color, 16> glyph_palette(bn::color core_color)
    {
        bn::array<bn::color, 16> colors{}; // index 0 is transparent
        colors[1] = core_color;
        return colors;
    }

    constexpr bn::array<bn::color, 16> white_colors = glyph_palette(bn::color(31, 31, 31));
    constexpr bn::array<bn::color, 16> green_colors = glyph_palette(bn::color(0, 31, 0));
    constexpr bn::array<bn::color, 16> fuchsia_colors = glyph_palette(bn::color(31, 0, 31));

    constexpr bn::sprite_palette_item white_palette(white_colors, bn::bpp_mode::BPP_4);
    constexpr bn::sprite_palette_item green_palette(green_colors, bn::bpp_mode::BPP_4);
    constexpr bn::sprite_palette_item fuchsia_palette(fuchsia_colors, bn::bpp_mode::BPP_4);

    // Left edge for the terminal text, a few pixels in from the screen border.
    constexpr bn::fixed left_x = -(bn::display::width() / 2) + 4;
    constexpr bn::fixed line_height = 10;
    constexpr bn::fixed top_y = -76;
    // Lowest line position that still fits on screen; below this we scroll.
    constexpr bn::fixed bottom_y = 68;

    const bn::string_view target_hash =
            "f52fbd32b2b3b86ff88ef6c490628285f482af15ddcb29541f94bcf526a3f6c7";

    // A scrolling terminal: prints lines top-down and, once the screen is full,
    // scrolls everything up so the newest line stays visible.
    class terminal
    {

    public:
        terminal(bn::sprite_text_generator& generator, bn::vector<bn::sprite_ptr, 128>& sprites) :
            _generator(generator),
            _sprites(sprites)
        {
        }

        // Prints a whole line in a single color, then waits.
        void print(const bn::sprite_palette_item& palette, const bn::string_view& text, int wait_frames)
        {
            bn::fixed y = _next_line_y();
            _generator.set_palette_item(palette);
            _generator.generate(left_x, y, text, _sprites);
            _wait(wait_frames);
        }

        // Advances by one empty line (no sprites generated), then waits.
        void blank(int wait_frames)
        {
            _next_line_y();
            _wait(wait_frames);
        }

        // Prints two differently-colored segments on the same line, inline.
        void print2(const bn::sprite_palette_item& palette1, const bn::string_view& text1,
                    const bn::sprite_palette_item& palette2, const bn::string_view& text2,
                    int wait_frames)
        {
            bn::fixed y = _next_line_y();
            _generator.set_palette_item(palette1);
            _generator.generate(left_x, y, text1, _sprites);
            _generator.set_palette_item(palette2);
            _generator.generate(left_x + _generator.width(text1), y, text2, _sprites);
            _wait(wait_frames);
        }

        static void wait(int frames)
        {
            _wait(frames);
        }

    private:
        bn::sprite_text_generator& _generator;
        bn::vector<bn::sprite_ptr, 128>& _sprites;
        bn::fixed _cursor_y = top_y;

        // Returns the y for the next line, scrolling the screen up if needed.
        bn::fixed _next_line_y()
        {
            if(_cursor_y > bottom_y)
            {
                for(bn::sprite_ptr& sprite : _sprites)
                {
                    sprite.set_y(sprite.y() - line_height);
                }

                return bottom_y;
            }

            bn::fixed y = _cursor_y;
            _cursor_y += line_height;
            return y;
        }

        static void _wait(int frames)
        {
            for(int frame = 0; frame < frames; ++frame)
            {
                bn::core::update();
            }
        }
    };

    void terminal_scene()
    {
        // The splash left the background palettes faded to black; clear the fade
        // so the (black) backdrop is clean for the terminal.
        set_black_fade(0);
        bn::bg_palettes::set_transparent_color(bn::color(0, 0, 0));

        bn::sprite_text_generator generator(hc::unscii_8_sprite_font);
        generator.set_left_alignment();

        // Text sprites must stay alive while on screen. This is a local (not
        // static) vector, so returning from this scene frees the sprites and
        // clears the screen before the cracking scene draws.
        bn::vector<bn::sprite_ptr, 128> sprites;

        terminal term(generator, sprites);

        constexpr int line_delay = 10; // frames between printed lines
        constexpr int blank_delay = 5; // shorter pause for blank lines

        term.print(white_palette, "GBA-HASHCAT v 1.0.6", line_delay);
        term.print(white_palette, "By solst/ICE of Astarte", line_delay);
        term.print(white_palette, "===============", line_delay);
        term.blank(blank_delay);
        term.print(white_palette, "CPU: 16.78 MHz ARM7TDMI", line_delay);
        term.print(white_palette, "Cores: 1", line_delay);
        term.print(white_palette, "RAM: 288 KB", line_delay);
        term.print(white_palette, "Storage: 32 MB", line_delay);
        // The wordlist line is too wide for the 8px monospace font, so it wraps;
        // [LOADED] stays inline and green on the continuation line.
        term.print(white_palette, "Wordlist:", line_delay);
        term.print2(white_palette, " min-rockyou.txt ",
                    green_palette, "[LOADED]", line_delay);
        term.blank(blank_delay);
        term.print(fuchsia_palette, "TARGET HASH:", line_delay);
        // 64-char hash wrapped across lines that fit the screen width.
        constexpr int hash_chunk = 22;
        for(int i = 0; i < 64; i += hash_chunk)
        {
            int len = (64 - i < hash_chunk) ? (64 - i) : hash_chunk;
            term.print(fuchsia_palette, bn::string_view(target_hash.data() + i, len), line_delay);
        }

        // Hold on the target hash for a second before the cracking scene.
        terminal::wait(60);
    }

    // --- Cracking scene -------------------------------------------------------

    constexpr bn::array<bn::color, 16> red_colors = glyph_palette(bn::color(31, 4, 4));
    constexpr bn::sprite_palette_item red_palette(red_colors, bn::bpp_mode::BPP_4);

    constexpr int hex_value(char c)
    {
        return (c <= '9') ? (c - '0') : (c - 'a' + 10);
    }

    // Parses the 64-char lowercase target hex string into a 32-byte digest.
    hc::sha256_digest parse_target()
    {
        hc::sha256_digest digest;
        for(int i = 0; i < 32; ++i)
        {
            digest.bytes[i] = uint8_t((hex_value(target_hash[i * 2]) << 4) | hex_value(target_hash[i * 2 + 1]));
        }
        return digest;
    }

    // A view into one wordlist line, excluding its CR/LF terminator.
    struct candidate
    {
        const char* data;
        int size;
    };

    // Reads the next line from `pos`, strips a trailing '\r' (the wordlist is
    // CRLF), and advances `pos` past the '\n'. Returns false when exhausted.
    bool next_candidate(const uint8_t* list, int list_size, int& pos, candidate& out)
    {
        if(pos >= list_size)
        {
            return false;
        }

        int start = pos;
        while(pos < list_size && list[pos] != '\n')
        {
            ++pos;
        }

        int end = pos; // index of '\n' or list_size
        if(end > start && list[end - 1] == '\r')
        {
            --end; // drop the CR of a CRLF pair
        }

        out.data = reinterpret_cast<const char*>(list + start);
        out.size = end - start;

        if(pos < list_size)
        {
            ++pos; // skip the '\n'
        }

        return true;
    }

    // Estimates the total line count by sampling only the top of the list.
    // Scanning the whole multi-megabyte wordlist from ROM to get an exact count
    // takes several seconds, and the total is only used for the progress bar, so
    // a fast estimate from the first chunk is good enough.
    int estimate_total(const uint8_t* list, int list_size)
    {
        if(list_size <= 0)
        {
            return 1;
        }

        constexpr int max_sample = 128 * 1024;
        int sample = (list_size < max_sample) ? list_size : max_sample;

        int lines = 0;
        for(int i = 0; i < sample; ++i)
        {
            if(list[i] == '\n')
            {
                ++lines;
            }
        }

        if(sample == list_size)
        {
            return lines > 0 ? lines : 1; // whole list fit in the sample
        }

        // Scale the sampled line count up to the full list size.
        int estimate = int((int64_t(lines) * list_size) / sample);
        return estimate > 0 ? estimate : 1;
    }

    // Builds a "[####......]" style progress bar string.
    bn::string<26> build_bar(int processed, int total)
    {
        constexpr int cells = 22;
        int filled = (total > 0) ? (processed * cells) / total : cells;
        if(filled > cells)
        {
            filled = cells; // total is an estimate; never overflow the bar
        }

        bn::string<26> bar;
        bar.push_back('[');
        for(int i = 0; i < cells; ++i)
        {
            bar.push_back(i < filled ? '#' : '.');
        }
        bar.push_back(']');
        return bar;
    }

    // Copies a candidate view into a short display string (truncated to fit).
    bn::string<24> candidate_text(const candidate& c)
    {
        bn::string<24> text;
        for(int i = 0; i < c.size && i < 23; ++i)
        {
            text.push_back(c.data[i]);
        }
        return text;
    }

    void cracking_scene()
    {
        set_black_fade(0);
        bn::bg_palettes::set_transparent_color(bn::color(0, 0, 0));

        bn::sprite_text_generator generator(hc::unscii_8_sprite_font);
        generator.set_left_alignment();

        const hc::sha256_digest target = parse_target();
        const uint8_t* list = min_rockyou_bin;
        const int list_size = int(min_rockyou_bin_size);

        // Static header + target (drawn once and kept for the whole scene).
        static bn::vector<bn::sprite_ptr, 32> static_sprites;
        generator.set_palette_item(white_palette);
        generator.generate(left_x, -70, "GBA-HASHCAT :: SHA256", static_sprites);
        generator.set_palette_item(fuchsia_palette);
        {
            bn::string<32> tgt;
            tgt.append("TGT ");
            for(int i = 0; i < 20; ++i)
            {
                tgt.push_back(target_hash[i]);
            }
            tgt.append("..");
            generator.generate(left_x, -56, tgt, static_sprites);
        }

        // Estimate the list size from the top of the file (fast) instead of
        // scanning all 8 MB from ROM. Corrected to the exact count if the scan
        // runs to the end without a match.
        int total = estimate_total(list, list_size);

        // Dynamic UI, cleared and regenerated on each refresh.
        static bn::vector<bn::sprite_ptr, 64> dyn_sprites;

        constexpr bn::fixed bar_y = -24;
        constexpr bn::fixed stats_y = -10;
        constexpr bn::fixed rate_y = 4;
        constexpr bn::fixed try_y = 20;
        constexpr bn::fixed result_y = 44;

        int pos = 0;
        int processed = 0;
        int hashes_in_interval = 0;
        int rate = 0;
        bool found = false;
        candidate last_tried = { nullptr, 0 };

        auto redraw = [&](bool finished)
        {
            dyn_sprites.clear();

            generator.set_palette_item(green_palette);
            generator.generate(left_x, bar_y, build_bar(processed, total), dyn_sprites);

            generator.set_palette_item(white_palette);
            int percent = (total > 0) ? processed * 100 / total : 100;
            if(percent > 100)
            {
                percent = 100; // total is an estimate
            }
            bn::string<40> stats;
            stats.append(bn::to_string<12>(percent));
            stats.append("%  ");
            stats.append(bn::to_string<12>(processed));
            stats.push_back('/');
            stats.append(bn::to_string<12>(total));
            generator.generate(left_x, stats_y, stats, dyn_sprites);

            generator.set_palette_item(green_palette);
            bn::string<32> rate_line;
            rate_line.append("RATE: ");
            rate_line.append(bn::to_string<12>(rate));
            rate_line.append(" H/s");
            generator.generate(left_x, rate_y, rate_line, dyn_sprites);

            if(! finished)
            {
                generator.set_palette_item(white_palette);
                bn::string<32> try_line;
                try_line.append("TRY: ");
                try_line.append(candidate_text(last_tried));
                generator.generate(left_x, try_y, try_line, dyn_sprites);
            }
            else if(found)
            {
                generator.set_palette_item(green_palette);
                generator.generate(left_x, try_y, "*** MATCH FOUND ***", dyn_sprites);
                bn::string<40> pw;
                pw.append("PLAINTEXT: ");
                pw.append(candidate_text(last_tried));
                generator.generate(left_x, result_y, pw, dyn_sprites);
            }
            else
            {
                generator.set_palette_item(red_palette);
                generator.generate(left_x, try_y, "*** CRACKING FAILED ***", dyn_sprites);
                generator.generate(left_x, result_y, "No match in wordlist.", dyn_sprites);
            }
        };

        redraw(false); // initial 0%

        // Run the real SHA-256 scan as fast as the GBA can, hashing candidates
        // for a slice of each frame and yielding so the UI stays responsive; the
        // rate reflects the hardware's actual throughput.
        const int frame_budget = bn::timers::ticks_per_second() / 80; // ~12.5 ms
        constexpr int refresh_period = 15;                            // 4 refreshes/sec
        bn::timer rate_timer;
        int frame = 0;
        bool done = false; // reached the actual end of the wordlist

        while(! found && ! done)
        {
            bn::timer frame_timer;

            while(! found && ! done && frame_timer.elapsed_ticks() < frame_budget)
            {
                candidate c;
                if(! next_candidate(list, list_size, pos, c))
                {
                    done = true;
                    break;
                }

                last_tried = c;
                ++processed;
                ++hashes_in_interval;

                if(hc::sha256_matches(reinterpret_cast<const uint8_t*>(c.data),
                                      uint32_t(c.size), target))
                {
                    found = true;
                }
            }

            // Refresh the display and rate estimate 4 times per second.
            if(frame % refresh_period == 0)
            {
                int ticks = rate_timer.elapsed_ticks_with_restart();
                if(frame > 0 && ticks > 0)
                {
                    rate = int((int64_t(hashes_in_interval) * bn::timers::ticks_per_second()) / ticks);
                }

                hashes_in_interval = 0;
                redraw(false);
            }

            ++frame;
            bn::core::update();
        }

        if(done && ! found)
        {
            total = processed; // whole list scanned: exact count is now known
        }

        redraw(true); // final success / failure state

        while(true)
        {
            bn::core::update();
        }
    }
}

int main()
{
    bn::core::init();

    splash_scene();
    terminal_scene();
    cracking_scene();
}
