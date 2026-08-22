#include "input_guard.h"

#include <Windows.h>

#include "vendor/minhook.h"
#include "gui/chat.hpp"
#include "gui/dialog.hpp"

namespace
{
    using GetAsyncKeyState_t = SHORT(WINAPI*)(int);
    using SetCursorPos_t     = BOOL (WINAPI*)(int, int);

    GetAsyncKeyState_t o_GetAsyncKeyState = nullptr;
    SetCursorPos_t     o_SetCursorPos     = nullptr;

    bool modal_active()
    {
        // Any UI surface that wants exclusive input: the chat line editor or
        // a server-pushed dialog. Extend as we add class-select / spawn menu.
        return opensamp::gui::Dialog::Get().IsActive() || g_chat.IsActive();
    }

    SHORT WINAPI hk_GetAsyncKeyState(int vKey)
    {
        // Return "not pressed" so the game's input reader sees a clean state.
        // ImGui reads keyboard via WM_* messages, not this API, so our UI is
        // unaffected.
        if (modal_active()) return 0;
        return o_GetAsyncKeyState ? o_GetAsyncKeyState(vKey) : 0;
    }

    BOOL WINAPI hk_SetCursorPos(int X, int Y)
    {
        // GTA re-centers the cursor every frame to drive mouse-look camera.
        // Swallow those calls while a modal is up so the OS cursor stays
        // where the user put it — ImGui then tracks it naturally.
        if (modal_active()) return TRUE;
        return o_SetCursorPos ? o_SetCursorPos(X, Y) : FALSE;
    }
} // namespace

bool InstallInputGuards()
{
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) return false;

    auto* pGASK = reinterpret_cast<void*>(GetProcAddress(user32, "GetAsyncKeyState"));
    auto* pSCP  = reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos"));
    if (!pGASK || !pSCP) return false;

    if (MH_CreateHook(pGASK, &hk_GetAsyncKeyState,
                      reinterpret_cast<void**>(&o_GetAsyncKeyState)) != MH_OK)
        return false;

    if (MH_CreateHook(pSCP, &hk_SetCursorPos,
                      reinterpret_cast<void**>(&o_SetCursorPos)) != MH_OK)
        return false;

    if (MH_EnableHook(pGASK) != MH_OK) return false;
    if (MH_EnableHook(pSCP)  != MH_OK) return false;

    return true;
}
