#pragma once
#include <string>
#include <deque>
#include <mutex>
#include <cstdint>
#include <vector>

#include "imgui.h"

struct ChatLine
{
    std::string textUtf8;
    uint64_t tMs;
};

class ImGuiChat
{
public:
    bool PopCommand(std::string& out);

    void PushLineAsync(std::string s)
    {
        std::lock_guard lg(m_pendingMx);
        m_pending.emplace_back(std::move(s));
    }

    void SetActive(bool active)
    {
        if (m_active == active) return;
        if (active) m_scrollToBottom = true;
        m_active = active;
        m_wantFocus = active;

        if (!active)
        {
            m_input[0] = '\0';
            ImGui::SetWindowFocus(nullptr);
        }
    }

    void RequestFocus()
    {
        m_wantFocus = true;
    }

    bool IsActive() const { return m_active; }

    void Draw();

    void SetMaxLines(size_t n) { m_maxLines = n; }
    void SetVisibleLines(int n) { m_visibleLines = n; }

private:
    std::mutex m_pendingMx;
    std::deque<ChatLine> m_lines;
    std::deque<std::string> m_outCommands;
    std::vector<std::string> m_pending;

    bool m_active = false;
    bool m_scrollToBottom = false;
    bool m_wantFocus = false;

    char m_input[256]{};

    size_t m_maxLines = 200; // @todo use
    int m_visibleLines = 15; // @todo change

    static uint64_t NowMs();
    static bool ParseHexColorTag(const char* s, uint32_t& outRGBA, int& outLen);
    static ImU32 ToImU32(uint32_t rgba);
    void DrawLineColored(const std::string& s, float alphaMul);
};

inline ImGuiChat g_chat;
