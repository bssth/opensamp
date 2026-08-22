#include "dialog.hpp"

#include <cstring>
#include <algorithm>
#include <imgui.h>

#include "../sampraknet_bridge.h"
#include "colored_text.h"

namespace opensamp::gui
{
    namespace
    {
        constexpr float kDialogWidth = 480.0f;

        // Split `s` by '\n' into m_listItems, trimming trailing '\r'.
        std::vector<std::string> SplitLines(const std::string& s)
        {
            std::vector<std::string> out;
            std::size_t start = 0;
            while (start <= s.size())
            {
                std::size_t end = s.find('\n', start);
                if (end == std::string::npos) end = s.size();
                std::string line = s.substr(start, end - start);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                out.emplace_back(std::move(line));
                if (end == s.size()) break;
                start = end + 1;
            }
            // Drop a trailing empty line if present.
            if (!out.empty() && out.back().empty()) out.pop_back();
            return out;
        }
    }

    Dialog& Dialog::Get()
    {
        static Dialog instance;
        return instance;
    }

    void Dialog::Show(std::uint16_t id, DialogStyle style,
                      const char* title,
                      const char* btn1, const char* btn2,
                      const char* info)
    {
        m_id    = id;
        m_style = style;
        m_title = title ? title : "";
        m_btn1  = btn1  ? btn1  : "";
        m_btn2  = btn2  ? btn2  : "";
        m_info  = info  ? info  : "";

        m_listItems.clear();
        m_selectedItem = -1;
        if (style == DialogStyle::List ||
            style == DialogStyle::TabList ||
            style == DialogStyle::TabListHeaders)
        {
            m_listItems = SplitLines(m_info);
            if (!m_listItems.empty()) m_selectedItem = 0;
        }

        std::memset(m_input, 0, sizeof(m_input));
        m_wantFocus = (style == DialogStyle::Input || style == DialogStyle::Password);
        m_active    = true;
    }

    void Dialog::Hide()
    {
        m_active = false;
    }

    void Dialog::Respond(std::uint8_t button)
    {
        // Pick payload based on style. Server expects a listItem + input text.
        std::uint16_t listItem = 0xFFFF;
        const char*   text     = "";

        switch (m_style)
        {
        case DialogStyle::List:
            if (m_selectedItem >= 0)
            {
                listItem = static_cast<std::uint16_t>(m_selectedItem);
                text     = m_listItems[m_selectedItem].c_str();
            }
            break;
        case DialogStyle::Input:
        case DialogStyle::Password:
            text = m_input;
            break;
        case DialogStyle::MsgBox:
        default:
            break;
        }

        bridge::SendDialogResponse(m_id, button, listItem, text);
        Hide();
    }

    void Dialog::Draw()
    {
        if (!m_active) return;

        const ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(kDialogWidth, 0.0f), ImGuiCond_Always);

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(25, 25, 30, 235));

        const char* title = m_title.empty() ? "Dialog" : m_title.c_str();
        ImGui::Begin(title, nullptr, flags);

        bool submitMain   = false;
        bool submitCancel = false;

        constexpr ImVec4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        switch (m_style)
        {
        case DialogStyle::MsgBox:
            opensamp::gui::color::DrawColoredText(m_info, baseColor, 1.0f, true);
            break;

        case DialogStyle::Input:
        case DialogStyle::Password:
            opensamp::gui::color::DrawColoredText(m_info, baseColor, 1.0f, true);
            ImGui::Dummy(ImVec2(0, 4));
            if (m_wantFocus)
            {
                ImGui::SetKeyboardFocusHere();
                m_wantFocus = false;
            }
            {
                const ImGuiInputTextFlags inputFlags =
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    (m_style == DialogStyle::Password
                         ? ImGuiInputTextFlags_Password
                         : ImGuiInputTextFlags_None);
                if (ImGui::InputText("##dlg_input", m_input, sizeof(m_input), inputFlags))
                    submitMain = true;
            }
            break;

        case DialogStyle::List:
        case DialogStyle::TabList:
        case DialogStyle::TabListHeaders:
            {
                // For TabListHeaders, the first row is the column header — render it
                // as plain text above the scroll region instead of as a selectable.
                const bool hasHeader = (m_style == DialogStyle::TabListHeaders) && !m_listItems.empty();
                const int  firstItem = hasHeader ? 1 : 0;
                if (hasHeader)
                {
                    opensamp::gui::color::DrawColoredText(m_listItems[0], baseColor, 1.0f, true);
                    ImGui::Separator();
                }

                const std::size_t rowCount =
                    m_listItems.size() > static_cast<std::size_t>(firstItem)
                        ? m_listItems.size() - static_cast<std::size_t>(firstItem)
                        : std::size_t{ 1 };
                const float listH = std::min(
                    260.0f,
                    ImGui::GetFontSize() * static_cast<float>(rowCount) + 20.0f);
                if (ImGui::BeginChild("##dlg_list", ImVec2(0, listH), true))
                {
                    for (int i = firstItem; i < static_cast<int>(m_listItems.size()); ++i)
                    {
                        const bool selected = (m_selectedItem == i);
                        // Selectable provides hit-testing + highlight; we paint
                        // the colored text on top via the window's draw list so
                        // `{RRGGBB}` tags work inside list items too. Drawing
                        // through the draw list avoids any cursor manipulation
                        // that could trip ImGui's bounds-tracking assertion.
                        ImGui::PushID(i);
                        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                        const float  rowH   = ImGui::GetFontSize() + 2.0f;
                        if (ImGui::Selectable("##row", selected,
                                              ImGuiSelectableFlags_AllowDoubleClick,
                                              ImVec2(0, rowH)))
                        {
                            m_selectedItem = i;
                            if (ImGui::IsMouseDoubleClicked(0))
                                submitMain = true;
                        }
                        // Vertically center the text inside the selectable row.
                        const float textY = rowPos.y + (rowH - ImGui::GetFontSize()) * 0.5f;
                        opensamp::gui::color::DrawColoredTextOnDrawList(
                            ImGui::GetWindowDrawList(),
                            ImVec2(rowPos.x, textY),
                            m_listItems[i], baseColor);
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
            break;
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        // Buttons. Always draw at least one. Right-align them.
        const float btnW = 100.0f;
        const float spacing = 8.0f;
        const bool hasBtn2 = !m_btn2.empty();
        const float rowW  = hasBtn2 ? (btnW * 2 + spacing) : btnW;

        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - rowW);

        if (ImGui::Button(m_btn1.empty() ? "OK" : m_btn1.c_str(), ImVec2(btnW, 0)))
            submitMain = true;

        if (hasBtn2)
        {
            ImGui::SameLine(0.0f, spacing);
            if (ImGui::Button(m_btn2.c_str(), ImVec2(btnW, 0)))
                submitCancel = true;
        }

        // ESC cancels. Only effective when a second button exists; otherwise
        // ESC still dismisses the dialog to avoid deadlock.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            if (hasBtn2) submitCancel = true;
            else         submitMain   = true;
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (submitMain)   Respond(1);
        else if (submitCancel) Respond(0);
    }
} // namespace opensamp::gui
