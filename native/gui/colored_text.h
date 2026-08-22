#pragma once

// Reusable SAMP-style `{RRGGBB}` color tag rendering with proper newline
// handling. Server text in chat / dialogs / 3D labels / textdraws / death feed
// all use the same tag syntax. We parse it once here and let every UI surface
// call `DrawColoredText` instead of a raw `ImGui::TextWrapped`.
//
// Layout note: ImGui's `TextColored` with embedded `\n` does emit multi-line
// text, but our chunked rendering uses `SameLine(0, 0)` between chunks — and
// `SameLine` against a multi-line widget puts the next chunk at the *bottom-
// right* of the previous one, which produces the "Введите ↓ пароль ↓ для…"
// column-wrap glitch. So we split lines ourselves and render them as
// independent rows; chunks within a row use `SameLine(0, 0)`.

#include <cstdint>
#include <string_view>

#include "imgui.h"

namespace opensamp::gui::color
{
    // Parse a `{RRGGBB}` tag at `s` (8 chars total: `{` + 6 hex + `}`).
    // Alpha is forced to 0xFF since SAMP/open.mp tags don't carry it.
    // On success returns true and writes the consumed length (always 8).
    inline bool ParseHexTag(const char* s, std::uint32_t& outRgba, int& outLen)
    {
        if (!s || s[0] != '{') return false;
        for (int i = 1; i <= 6; ++i)
            if (!s[i]) return false;
        if (s[7] != '}') return false;

        const auto hex = [](char c) -> int
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };

        const int n[6] = { hex(s[1]), hex(s[2]), hex(s[3]),
                           hex(s[4]), hex(s[5]), hex(s[6]) };
        for (int v : n) if (v < 0) return false;

        const auto r = static_cast<std::uint8_t>((n[0] << 4) | n[1]);
        const auto g = static_cast<std::uint8_t>((n[2] << 4) | n[3]);
        const auto b = static_cast<std::uint8_t>((n[4] << 4) | n[5]);
        outRgba = (static_cast<std::uint32_t>(r) << 24) |
                  (static_cast<std::uint32_t>(g) << 16) |
                  (static_cast<std::uint32_t>(b) << 8)  |
                  0xFFu;
        outLen = 8;
        return true;
    }

    namespace detail
    {
        // Render a single line (no `\n` inside `line`), splitting by `{RRGGBB}`
        // tags. `cur` is the current color — updated in place as tags are
        // consumed so subsequent lines inherit the last set color (matches
        // SAMP behaviour where `{RRGGBB}` "sticks" until overridden).
        inline void DrawLine(std::string_view line, ImVec4& cur, float alphaMul)
        {
            const char* p   = line.data();
            const char* end = p + line.size();

            bool first = true;
            const auto emit = [&](std::string_view chunk)
            {
                if (chunk.empty()) return;
                if (!first) ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(cur, "%.*s",
                                   static_cast<int>(chunk.size()),
                                   chunk.data());
                first = false;
            };

            while (p < end)
            {
                std::uint32_t rgba   = 0;
                int           tagLen = 0;
                if (*p == '{' && ParseHexTag(p, rgba, tagLen))
                {
                    cur.x = ((rgba >> 24) & 0xFF) / 255.0f;
                    cur.y = ((rgba >> 16) & 0xFF) / 255.0f;
                    cur.z = ((rgba >> 8)  & 0xFF) / 255.0f;
                    cur.w = ((rgba)       & 0xFF) / 255.0f * alphaMul;
                    p += tagLen;
                    continue;
                }

                const char* nextBrace = p;
                while (nextBrace < end && *nextBrace != '{') ++nextBrace;

                if (nextBrace == p && *p == '{')
                {
                    // Lone '{' that didn't parse as a tag — render as literal
                    // and advance one byte to avoid an infinite loop.
                    emit(std::string_view(p, 1));
                    ++p;
                    continue;
                }

                if (nextBrace > p)
                {
                    emit(std::string_view(p, static_cast<std::size_t>(nextBrace - p)));
                    p = nextBrace;
                }
            }

            // Empty line → still emit a blank row so vertical spacing matches
            // the server's intent. `TextUnformatted("")` advances the cursor
            // by one font height.
            if (first) ImGui::TextUnformatted("");
        }
    } // namespace detail

    // Draw a single-line colored text directly onto a draw list at `pos`.
    // No cursor manipulation, no item registration — safe to overlay on top
    // of an existing widget (e.g. a Selectable) without tripping ImGui's
    // "SetCursorPos extended bounds without a follow-up item" assertion.
    // `\n` inside `line` is rendered as a literal (callers should split first).
    inline void DrawColoredTextOnDrawList(ImDrawList* dl,
                                          ImVec2 pos,
                                          std::string_view line,
                                          ImVec4 baseColor,
                                          float  alphaMul = 1.0f)
    {
        if (!dl || line.empty()) return;

        ImVec4 cur = baseColor;
        cur.w *= alphaMul;

        const char* p   = line.data();
        const char* end = p + line.size();
        float x = pos.x;

        const auto emit = [&](const char* a, const char* b)
        {
            if (a >= b) return;
            const ImU32 col = ImGui::GetColorU32(cur);
            dl->AddText(ImVec2(x, pos.y), col, a, b);
            x += ImGui::CalcTextSize(a, b).x;
        };

        while (p < end)
        {
            std::uint32_t rgba   = 0;
            int           tagLen = 0;
            if (*p == '{' && ParseHexTag(p, rgba, tagLen))
            {
                cur.x = ((rgba >> 24) & 0xFF) / 255.0f;
                cur.y = ((rgba >> 16) & 0xFF) / 255.0f;
                cur.z = ((rgba >> 8)  & 0xFF) / 255.0f;
                cur.w = ((rgba)       & 0xFF) / 255.0f * alphaMul;
                p += tagLen;
                continue;
            }

            const char* nextBrace = p;
            while (nextBrace < end && *nextBrace != '{') ++nextBrace;

            if (nextBrace == p && *p == '{')
            {
                emit(p, p + 1);
                ++p;
                continue;
            }

            if (nextBrace > p)
            {
                emit(p, nextBrace);
                p = nextBrace;
            }
        }
    }

    // Render `text` as a sequence of inline-colored chunks, line by line.
    // `baseColor` is the color used for any text outside of a `{RRGGBB}` tag.
    // `alphaMul` multiplies the alpha of every chunk (used by chat to fade
    // old lines). `wrap` enables ImGui's auto-wrapping at the current content
    // region — applied per line so each line wraps independently.
    inline void DrawColoredText(std::string_view text,
                                ImVec4 baseColor,
                                float  alphaMul = 1.0f,
                                bool   wrap     = false)
    {
        ImVec4 cur = baseColor;
        cur.w *= alphaMul;

        if (wrap) ImGui::PushTextWrapPos(0.0f);

        const char* p   = text.data();
        const char* end = p + text.size();

        while (p <= end)
        {
            const char* lineEnd = p;
            while (lineEnd < end && *lineEnd != '\n') ++lineEnd;

            detail::DrawLine(std::string_view(p, static_cast<std::size_t>(lineEnd - p)),
                             cur, alphaMul);

            if (lineEnd < end && *lineEnd == '\n')
                p = lineEnd + 1;
            else
                break;
        }

        if (wrap) ImGui::PopTextWrapPos();
    }
} // namespace opensamp::gui::color
