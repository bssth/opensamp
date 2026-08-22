#pragma once

// SA-MP-style dialog box rendered in ImGui. One dialog active at a time
// (matches server-side semantics). Server pushes via Show(), user interaction
// routes back via opensamp::bridge::SendDialogResponse.

#include <cstdint>
#include <string>
#include <vector>

namespace opensamp::gui
{
    enum class DialogStyle : std::uint8_t
    {
        MsgBox          = 0,
        Input           = 1,
        List            = 2,
        Password        = 3,
        // Newer SAMP / open.mp adds tab-formatted list dialogs. Items are
        // tab-separated rows; we render them as plain list rows for now.
        TabList         = 4,
        TabListHeaders  = 5,
    };

    class Dialog
    {
    public:
        static Dialog& Get();

        // Called from the RPC handler once the server requests a dialog.
        // `info` for LIST style is newline-separated items.
        void Show(std::uint16_t id, DialogStyle style,
                  const char* title,
                  const char* btn1, const char* btn2,
                  const char* info);

        // Dismiss without responding. Server also dismisses by sending style 255.
        void Hide();

        bool IsActive() const { return m_active; }

        // Called from draw_ui() every frame.
        void Draw();

    private:
        Dialog() = default;

        void Respond(std::uint8_t button);

        bool          m_active        = false;
        std::uint16_t m_id            = 0;
        DialogStyle   m_style         = DialogStyle::MsgBox;
        std::string   m_title;
        std::string   m_btn1;
        std::string   m_btn2;
        std::string   m_info;
        std::vector<std::string> m_listItems; // parsed from m_info for LIST style
        int           m_selectedItem  = -1;
        char          m_input[256]{};
        bool          m_wantFocus     = false;
    };
} // namespace opensamp::gui
