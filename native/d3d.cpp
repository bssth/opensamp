#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>

#include "imgui.h"
#include "memory.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"
#include "gui/chat.hpp"
#include "gui/gui.h"

using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using Reset_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

static Reset_t g_oReset = nullptr;
static Present_t g_oPresent = nullptr;

static IDirect3DDevice9* g_dev = nullptr;
static HWND g_hwnd = nullptr;

static bool g_hooked = false;

static bool g_borderlessEnabled = true;
static bool g_borderlessApplied = false;

static bool g_forceResEnabled = true;
static int g_forceW = 0;
static int g_forceH = 0;

static bool PatchPtr(void** slot, void* val)
{
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old))
        return false;
    *slot = val;
    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}

static void ApplyBorderless(HWND hwnd)
{
    if (!hwnd) return;

    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);

    // remove border/caption
    style &= ~(WS_CAPTION | WS_THICKFRAME);
    // remove extra edges
    ex &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

    SetWindowLongW(hwnd, GWL_STYLE, style);
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex);

    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);

    SetWindowPos(hwnd, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);
}

static void GetMonitorSizeForWindow(HWND hwnd, int& outW, int& outH)
{
    outW = 0;
    outH = 0;
    HMONITOR mon = MonitorFromWindow(hwnd ? hwnd : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(mon, &mi)) return;
    outW = (mi.rcMonitor.right - mi.rcMonitor.left);
    outH = (mi.rcMonitor.bottom - mi.rcMonitor.top);
}

static void FixupPresentParamsForBorderless(HWND hwnd, D3DPRESENT_PARAMETERS* pp)
{
    if (!pp) return;

    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;

    int w = g_forceW, h = g_forceH;
    if (w <= 0 || h <= 0)
        GetMonitorSizeForWindow(hwnd, w, h);

    if (w > 0 && h > 0)
    {
        pp->BackBufferWidth = static_cast<UINT>(w);
        pp->BackBufferHeight = static_cast<UINT>(h);
    }

    if (hwnd)
        pp->hDeviceWindow = hwnd;
}

static HRESULT STDMETHODCALLTYPE hkPresent(IDirect3DDevice9* dev,
                                           const RECT* src, const RECT* dst, HWND hWnd, const RGNDATA* dirty);

static HRESULT STDMETHODCALLTYPE hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp);

static bool CaptureOriginals(IDirect3DDevice9* dev)
{
    auto pVT = (void***)dev;
    void** vtbl = *pVT;
    if (!vtbl) return false;

    if (!g_oReset && vtbl[16] && vtbl[16] != (void*)hkReset)
        g_oReset = static_cast<Reset_t>(vtbl[16]);
    if (!g_oPresent && vtbl[17] && vtbl[17] != (void*)hkPresent)
        g_oPresent = static_cast<Present_t>(vtbl[17]);

    return g_oReset && g_oPresent;
}

static void HookDevice(IDirect3DDevice9* dev)
{
    if (!dev) return;
    auto pVT = (void***)dev;
    void** vtbl = *pVT;
    if (!vtbl) return;

    CaptureOriginals(dev);

    bool ok1 = (vtbl[16] == (void*)hkReset) ? true : PatchPtr(&vtbl[16], (void*)hkReset);
    bool ok2 = (vtbl[17] == (void*)hkPresent) ? true : PatchPtr(&vtbl[17], (void*)hkPresent);

    g_hooked = ok1 && ok2;
}

static void ForceViewportToBackbuffer(IDirect3DDevice9* dev, const D3DPRESENT_PARAMETERS* pp)
{
    if (!dev || !pp) return;
    if (!pp->BackBufferWidth || !pp->BackBufferHeight) return;

    D3DVIEWPORT9 vp{};
    vp.X = 0;
    vp.Y = 0;
    vp.Width = pp->BackBufferWidth;
    vp.Height = pp->BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    SUCCEEDED(dev->SetViewport(&vp));
}

static HRESULT STDMETHODCALLTYPE hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    thread_local bool inReset = false;
    if (inReset)
        return g_oReset ? g_oReset(dev, pp) : D3DERR_INVALIDCALL;
    inReset = true;

    if (g_imgui_init)
        ImGui_ImplDX9_InvalidateDeviceObjects();

    if (!g_oReset) CaptureOriginals(dev);

    if (!g_hwnd)
    {
        D3DDEVICE_CREATION_PARAMETERS cp{};
        if (SUCCEEDED(dev->GetCreationParameters(&cp)) && cp.hFocusWindow)
        {
            g_hwnd = cp.hFocusWindow;
        }
    }

    if (g_hwnd && IsIconic(g_hwnd))
    {
        inReset = false;
        return D3DERR_DEVICELOST;
    }

    if (g_borderlessEnabled && g_forceResEnabled && pp)
        FixupPresentParamsForBorderless(g_hwnd, pp);

    HRESULT hr = g_oReset ? g_oReset(dev, pp) : D3DERR_INVALIDCALL;

    if (SUCCEEDED(hr))
    {
        if (g_imgui_init)
        {
            ImGui_ImplDX9_CreateDeviceObjects();
        }

        HookDevice(dev);

        if (pp)
            ForceViewportToBackbuffer(dev, pp);

        g_borderlessApplied = false;
    }

    inReset = false;
    return hr;
}

static HRESULT STDMETHODCALLTYPE hkPresent(IDirect3DDevice9* dev,
                                           const RECT* src, const RECT* dst, HWND hWnd, const RGNDATA* dirty)
{
    if (!g_oPresent) CaptureOriginals(dev);
    g_dev = dev;
    if (!g_hwnd && hWnd) g_hwnd = hWnd;


    if (g_borderlessEnabled && !g_borderlessApplied && g_hwnd && !IsIconic(g_hwnd))
    {
        ApplyBorderless(g_hwnd);
        g_borderlessApplied = true;
    }

    if (!g_imgui_init)
    {
        if (allow_chat)
        {
            if (!g_hwnd)
            {
                D3DDEVICE_CREATION_PARAMETERS cp{};
                if (SUCCEEDED(dev->GetCreationParameters(&cp)) && cp.hFocusWindow)
                    g_hwnd = cp.hFocusWindow;
            }

            if (g_hwnd)
            {
                g_orig_wnd_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hwnd, GWLP_WNDPROC,
                                                                             reinterpret_cast<LONG_PTR>(HookWndProc)));
                im_gui_init(g_hwnd, dev);
            }
        }
    }
    else
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        draw_ui();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    TickGameReady();
    return g_oPresent ? g_oPresent(dev, src, dst, hWnd, dirty) : D3DERR_INVALIDCALL;
}

static DWORD WINAPI HookThread(LPVOID)
{
    IDirect3DDevice9* dev = nullptr;
    for (int i = 0; i < 400; ++i) // ~10 sec
    {
        dev = gta::get_d3d_device();
        if (dev) break;
        Sleep(25);
    }

    if (!dev)
    {
        return 0;
    }

    HookDevice(dev);

    while (true)
    {
        IDirect3DDevice9* cur = gta::get_d3d_device();
        if (cur && cur != g_dev)
        {
            g_dev = cur;
            HookDevice(cur);
            g_borderlessApplied = false;
        }

        if (g_dev)
        {
            auto pVT = (void***)g_dev;
            void** vtbl = *pVT;
            if (vtbl)
            {
                if (vtbl[16] != (void*)hkReset) PatchPtr(&vtbl[16], (void*)hkReset);
                if (vtbl[17] != (void*)hkPresent) PatchPtr(&vtbl[17], (void*)hkPresent);
            }
        }

        Sleep(250);
    }
}

void D3D_Start()
{
    static bool started = false;
    if (started) return;
    started = true;

    // Fire and forget: the thread outlives this call, and closing the handle
    // only drops our reference to it. Not closing it leaks a kernel handle for
    // the lifetime of the process.
    if (HANDLE h = CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr))
        CloseHandle(h);
}
