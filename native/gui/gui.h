#pragma once

#include <imgui.h>
#include <d3d9.h>

void im_gui_init(HWND hwnd, IDirect3DDevice9* dev);
LRESULT CALLBACK HookWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void draw_ui();

inline WNDPROC g_orig_wnd_proc = nullptr;
inline bool g_imgui_init = false;
inline bool allow_chat = false;
