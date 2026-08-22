#include <imgui.h>
#include <d3d9.h>
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx9.h"

#include "gui.h"
#include "chat.hpp"
#include "dialog.hpp"
#include "../net/netgame.h"
#include "../sampraknet_bridge.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND g_hwnd = nullptr;
IDirect3DDevice9* g_dev = nullptr;

void im_gui_init(HWND hwnd, IDirect3DDevice9* dev)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(dev);

    io.Fonts->Clear();

    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0400, 0x052F, 0};

    char win_dir[MAX_PATH]{};
    GetWindowsDirectoryA(win_dir, MAX_PATH);
    std::string fontPath = std::string(win_dir) + "\\Fonts\\arial.ttf";
    ImFont* f = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, nullptr, ranges);

    if (!f)
    {
        OutputDebugStringA("[ImGui] Failed to load font C:\\Windows\\Fonts\\arial.ttf\n");
        io.Fonts->AddFontDefault();
    }

    ImGui_ImplDX9_InvalidateDeviceObjects();
    ImGui_ImplDX9_CreateDeviceObjects();

    g_imgui_init = true;
}


static void im_gui_shutdown() // @todo
{
    if (!g_imgui_init) return;

    SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig_wnd_proc));
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    g_imgui_init = false;
}

LRESULT CALLBACK HookWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_imgui_init)
    {
        const bool dialog_active = opensamp::gui::Dialog::Get().IsActive();

        if (msg == WM_KEYDOWN)
        {
            // Toggle chat with T — suppressed while a server dialog is up
            // so typing into the dialog input doesn't leak into chat.
            if (!dialog_active && !g_chat.IsActive() && wParam == 'T')
            {
                g_chat.SetActive(true);
                g_chat.RequestFocus();
                return 0;
            }

            if (g_chat.IsActive() && wParam == VK_ESCAPE)
            {
                g_chat.SetActive(false);
                return 0;
            }
        }

        const bool imgui_handled = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

        // While chat or dialog is taking input, swallow keyboard/IME messages
        // so the game doesn't also react to them (movement, weapons, etc.).
        if (g_chat.IsActive() || dialog_active)
        {
            switch (msg)
            {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
            case WM_SYSCHAR:
            case WM_IME_STARTCOMPOSITION:
            case WM_IME_COMPOSITION:
            case WM_IME_ENDCOMPOSITION:
                return imgui_handled ? TRUE : 0;
            default: break;
            }

            // Dialogs take mouse too — block game camera/fire.
            if (dialog_active && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)
                return imgui_handled ? TRUE : 0;
        }
    }

    return CallWindowProc(g_orig_wnd_proc, hWnd, msg, wParam, lParam);
}


static void draw_net_debug()
{
    using namespace opensamp;

    const auto& ng = net::CNetGame::Get();

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 260.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240.0f, 0.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 140));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::Begin("##opensamp_net_debug", nullptr, flags);

    ImGui::Text("OpenSamp — net");
    ImGui::Separator();
    ImGui::Text("state   : %s", net::ToString(ng.State()));
    if (!ng.Server().host.empty())
        ImGui::Text("server  : %s:%u", ng.Server().host.c_str(), static_cast<unsigned>(ng.Server().port));
    if (!ng.Local().nickname.empty())
        ImGui::Text("nick    : %s", ng.Local().nickname.c_str());
    ImGui::Text("pid     : %d", bridge::MyPlayerId());
    ImGui::Text("rak up  : %s", bridge::IsConnected()  ? "yes" : "no");
    ImGui::Text("ingame  : %s", bridge::IsGameInited() ? "yes" : "no");

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void draw_ui()
{
    // GTA hides the system cursor in-game. When a modal (chat input or
    // server dialog) is up we need SOMETHING to click with, so ask ImGui to
    // render a software cursor for the duration.
    const bool modal_active =
        g_chat.IsActive() || opensamp::gui::Dialog::Get().IsActive();
    ImGui::GetIO().MouseDrawCursor = modal_active;

    g_chat.Draw();
    opensamp::gui::Dialog::Get().Draw();
    draw_net_debug();
}
