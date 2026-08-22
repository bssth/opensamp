#include "gui.h"
#include "chat.hpp"
#include <imgui.h>
#include <cctype>
#include <charconv>
#include <chrono>
#include <algorithm>
#include <utility>

#include "../diag.h"
#include "../net/netgame.h"
#include "colored_text.h"
#include "../sampraknet_bridge.h"

using namespace std;
using namespace std::chrono;

uint64_t ImGuiChat::NowMs()
{
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

ImU32 ImGuiChat::ToImU32(uint32_t rgba)
{
    const uint8_t r = (rgba >> 24) & 0xFF;
    const uint8_t g = (rgba >> 16) & 0xFF;
    const uint8_t b = (rgba >> 8) & 0xFF;
    const uint8_t a = (rgba) & 0xFF;
    return IM_COL32(r, g, b, a);
}

bool ImGuiChat::ParseHexColorTag(const char* s, uint32_t& outRGBA, int& outLen)
{
    if (!s || s[0] != '{') return false;
    if (!(s[1] && s[2] && s[3] && s[4] && s[5] && s[6] && s[7])) return false;
    if (s[8] != '}') return false;

    auto hex = [](char c)-> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    int r1 = hex(s[1]), r2 = hex(s[2]), g1 = hex(s[3]), g2 = hex(s[4]), b1 = hex(s[5]), b2 = hex(s[6]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) return false;

    uint8_t r = static_cast<uint8_t>((r1 << 4) | r2);
    uint8_t g = static_cast<uint8_t>((g1 << 4) | g2);
    uint8_t b = static_cast<uint8_t>((b1 << 4) | b2);

    outRGBA = (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(b) << 8) |
        0xFF;
    outLen = 9;
    return true;
}

bool ImGuiChat::PopCommand(string& out)
{
    if (m_outCommands.empty()) return false;
    out = move(m_outCommands.front());
    m_outCommands.pop_front();
    return true;
}

namespace
{
    std::vector<std::string> SplitWhitespace(std::string_view s)
    {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < s.size())
        {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            const size_t start = i;
            while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (start < i) out.emplace_back(s.substr(start, i - start));
        }
        return out;
    }

    bool ParsePort(const std::string& s, std::uint16_t& out)
    {
        unsigned v = 0;
        const auto* first = s.data();
        const auto* last  = s.data() + s.size();
        const auto  res   = std::from_chars(first, last, v);
        if (res.ec != std::errc{} || res.ptr != last || v == 0 || v > 65535) return false;
        out = static_cast<std::uint16_t>(v);
        return true;
    }

    // Route a committed input line. Slash-commands handled locally; plain
    // text goes to the server as chat. Returns true if the line was eaten
    // (caller should not re-queue it).
    bool RouteInput(const std::string& line)
    {
        if (line.empty()) return true;

        if (line == "/q" || line == "//q")
        {
            ExitProcess(0);
        }

        if (line.rfind("//connect", 0) == 0)
        {
            const auto parts = SplitWhitespace(line);
            if (parts.size() < 3)
            {
                g_chat.PushLineAsync("//connect <ip> <port> [nick]");
                return true;
            }

            opensamp::net::ServerAddress addr;
            addr.host = parts[1];
            if (!ParsePort(parts[2], addr.port))
            {
                g_chat.PushLineAsync("Invalid port.");
                return true;
            }

            opensamp::net::LocalInfo info;
            info.nickname = parts.size() >= 4 ? parts[3] : "OpenSamp";

            opensamp::net::CNetGame::Get().Connect(std::move(addr), std::move(info));
            return true;
        }

        if (line == "//disconnect")
        {
            opensamp::net::CNetGame::Get().Disconnect("user");
            return true;
        }

        if (line == "//compat")
        {
            opensamp::diag::RunCompatDump();
            return true;
        }

        if (line == "//compat reinit" || line == "//compat pools")
        {
            opensamp::diag::ForceInitPedPools();
            return true;
        }

        // Anything else starting with `/` (single slash) → server command.
        // We reserve `//` for local UI commands above.
        if (!line.empty() && line[0] == '/' && line.size() > 1 && line[1] != '/')
        {
            opensamp::bridge::SendCommand(line.c_str());
            return true;
        }

        // Plain text → chat to server. Silently drops if not connected.
        opensamp::bridge::SendChat(line.c_str());
        return true;
    }
}

void ImGuiChat::DrawLineColored(const std::string& s, float alphaMul)
{
    // Force pure white as the default — vanilla ImGui dark theme `Text` is
    // close to white but slightly off, which made server lines look dim.
    constexpr ImVec4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    opensamp::gui::color::DrawColoredText(s, baseColor, alphaMul, /*wrap=*/true);
}

void ImGuiChat::Draw()
{
    {
        std::lock_guard lg(m_pendingMx);
        if (!m_pending.empty())
        {
            for (auto& s : m_pending)
                m_lines.push_back(ChatLine{.textUtf8 = std::move(s), .tMs = NowMs()});
            m_pending.clear();
            m_scrollToBottom = true;
        }
    }

    std::vector<ChatLine> lines;
    {
        lines.assign(m_lines.begin(), m_lines.end());
    }

    const ImGuiIO& io = ImGui::GetIO();
    constexpr float pad = 20.0f;
    const float width = min(720.0f, io.DisplaySize.x - pad * 2.0f);

    const float lineH = ImGui::GetFontSize() + 2.0f;
    const int linesToShow = m_visibleLines;
    const float height = lineH * static_cast<float>(m_visibleLines) + lineH * 2.2f;

    ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse;

    if (!m_active) flags |= ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, m_active ? 120 : 0));

    ImGui::Begin("##samp_chat", nullptr, flags);

    const float input_h = m_active ? lineH * 1.6f : 0.0f;
    ImGui::BeginChild("##chat_lines", ImVec2(0, -input_h), false);

    std::vector<int> idx;
    idx.reserve(static_cast<size_t>(linesToShow));

    for (int i = static_cast<int>(lines.size()) - 1; i >= 0 && std::cmp_less(idx.size(), linesToShow); --i)
    {
        idx.push_back(i);
    }
    ranges::reverse(idx);

    for (const int i : idx)
    {
        DrawLineColored(lines[i].textUtf8, 1.0f);
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        m_scrollToBottom = true;

    if (m_scrollToBottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    if (m_active)
    {
        ImGui::Separator();
        ImGui::PushItemWidth(-1);

        if (m_wantFocus)
        {
            ImGui::SetKeyboardFocusHere();
            m_wantFocus = false;
        }

        if (ImGui::InputText("##chat_input", m_input, sizeof(m_input),
                             ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::string cmd(m_input);
                while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == ' ' || cmd.back() ==
                    '\t'))
                    cmd.pop_back();

                if (!cmd.empty())
                {
                    RouteInput(cmd);
                }

                g_chat.SetActive(false);
                m_scrollToBottom = true;
            }
        }
        ImGui::PopItemWidth();
    }

    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
