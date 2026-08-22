#pragma once

// Encoding conversion helpers. The client runs entirely in UTF-8; SA-MP /
// open.mp servers historically transmit strings in Windows-1251 (Cyrillic codepage). 
// Convert on the I/O boundary — inside RPC read/write — and the
// rest of the codebase stays codepage-agnostic.

#include <string>
#include <string_view>
#include <Windows.h>

namespace opensamp::util
{
    namespace detail
    {
        inline std::string transcode(UINT src_cp, UINT dst_cp, std::string_view s)
        {
            if (s.empty()) return {};

            const int wlen = MultiByteToWideChar(
                src_cp, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            if (wlen <= 0) return std::string{s};

            std::wstring wide(static_cast<std::size_t>(wlen), L'\0');
            MultiByteToWideChar(
                src_cp, 0, s.data(), static_cast<int>(s.size()), wide.data(), wlen);

            const int olen = WideCharToMultiByte(
                dst_cp, 0, wide.data(), wlen, nullptr, 0, nullptr, nullptr);
            if (olen <= 0) return std::string{s};

            std::string out(static_cast<std::size_t>(olen), '\0');
            WideCharToMultiByte(
                dst_cp, 0, wide.data(), wlen, out.data(), olen, nullptr, nullptr);
            return out;
        }
    } // namespace detail

    // Inbound: bytes from the server → UTF-8.
    inline std::string cp1251_to_utf8(std::string_view s)
    {
        return detail::transcode(1251, CP_UTF8, s);
    }

    // Outbound: UTF-8 from our UI → bytes for the server.
    inline std::string utf8_to_cp1251(std::string_view s)
    {
        return detail::transcode(CP_UTF8, 1251, s);
    }
} // namespace opensamp::util
