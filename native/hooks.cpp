#include <Windows.h>
#include <atomic>
#include <cstdio>

#include "diag.h"
#include "memory.h"
#include "sampraknet_bridge.h"
#include "game/game.h"
#include "gta/sa_state.h"
#include "gui/chat.hpp"
#include "gui/gui.h"
#include "input_guard.h"
#include "net/netgame.h"
#include "vendor/minhook.h"

// ---------------- debug log ----------------

static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCs;
static bool g_logInitialized = false;

static void InitDebugLog()
{
    if (g_logInitialized)
        return;

    InitializeCriticalSection(&g_logCs);

    g_logFile = CreateFileA(
        "GameReady.log",
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (g_logFile != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(g_logFile, 0, nullptr, FILE_END);
        const auto hdr = "---- GameReady session start ----\r\n";
        DWORD written;
        WriteFile(g_logFile, hdr, static_cast<DWORD>(strlen(hdr)), &written, nullptr);
    }

    g_logInitialized = true;
}

static void LogLine(const char* text)
{
    if (g_logFile == INVALID_HANDLE_VALUE)
        return;

    EnterCriticalSection(&g_logCs);
    DWORD written;
    WriteFile(g_logFile, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    LeaveCriticalSection(&g_logCs);
}

// ---------------- state helpers ----------------

static constexpr std::uintptr_t kVAR_IsAtMenu = 0xBA677B;
static constexpr std::uintptr_t kVAR_IsGameLoaded = 0x86969C;

static inline bool IsAtMenu()
{
    return *reinterpret_cast<volatile std::uint8_t*>(kVAR_IsAtMenu) != 0;
}

static inline bool IsGameLoaded()
{
    return *reinterpret_cast<volatile std::uint8_t*>(kVAR_IsGameLoaded) != 0;
}

// ---------------- GameReady tick ----------------

static int g_totalFrames = 0;
static std::atomic_bool g_kickedToPlaying{false};

// Externally-defined Log() from sampraknet_bridge.cpp — mirrors into chat
// and OpenSamp.log. Declared here so we can surface state during boot.
extern void Log(const char* fmt, ...);

// ---------------- CVisibilityPlugins safety net ----------------
//
// `[0x8D6094]` is the CVisibilityPlugins clump dispatch table. Real game
// boot allocates it inside CGame::Initialise; our bootstrap skips that
// step, leaving the global at a tiny junk value (e.g. 0x34) and turning
// every read at 0x732B20 / write at 0x732B00 into an AV.
//
// While we don't have a proper init for the table, hook these two
// thiscall-flavoured-but-actually-cdecl helpers and short-circuit when
// the base looks invalid. Returning 0 / no-op matches what the rest of
// the engine does when the plugin slot has never been written for an
// entity — game continues, just without per-clump alpha/visibility data.

static inline bool ClumpTableValid()
{
    const auto v = *reinterpret_cast<volatile std::uint32_t*>(0x8D6094);
    return v > 0x10000 && v != 0xFFFFFFFFu;
}

using vis_get_t = std::uintptr_t(__cdecl*)(std::uintptr_t);
using vis_set_t = void(__cdecl*)(std::uintptr_t, std::uintptr_t);

static vis_get_t o_VisGet = nullptr;
static vis_set_t o_VisSet = nullptr;

static std::uintptr_t __cdecl hk_VisGet(std::uintptr_t key)
{
    if (!ClumpTableValid()) return 0;
    return o_VisGet(key);
}

static void __cdecl hk_VisSet(std::uintptr_t key, std::uintptr_t value)
{
    if (!ClumpTableValid()) return;
    o_VisSet(key, value);
}

static bool InstallClumpTableGuards()
{
    auto* pGet = reinterpret_cast<void*>(0x732B20);
    auto* pSet = reinterpret_cast<void*>(0x732B00);

    if (MH_CreateHook(pGet, &hk_VisGet,
                      reinterpret_cast<void**>(&o_VisGet)) != MH_OK) return false;
    if (MH_CreateHook(pSet, &hk_VisSet,
                      reinterpret_cast<void**>(&o_VisSet)) != MH_OK) return false;

    if (MH_EnableHook(pGet) != MH_OK) return false;
    if (MH_EnableHook(pSet) != MH_OK) return false;
    return true;
}

static void EnsurePedPoolsInitialised()
{
    // Three ped-related pools are allocated by gta_sa.exe at 0x732E30 and
    // their addresses stored in 0x8D608C / 0x8D6090 / 0x8D6094. Normal game
    // boot reaches this via CGame::Initialise → 0x53D8A0 → 0x732E30. Our
    // bootstrap sidesteps a large chunk of the menu/SCM path; dumps show
    // 0x8D6094 holding garbage (0x34) at crash time, which means either the
    // init ran with wrong state or didn't run at all.
    //
    // Force it here. If already initialised (pointer looks like a heap VA,
    // not -1 or something tiny) we skip so we don't leak the original alloc.
    const auto val = *reinterpret_cast<std::uint32_t*>(0x8D6094);
    Log("[Boot] ped pools: 0x8D608C=%08x  0x8D6090=%08x  0x8D6094=%08x",
        *reinterpret_cast<std::uint32_t*>(0x8D608C),
        *reinterpret_cast<std::uint32_t*>(0x8D6090), val);

    const bool looks_initialised = val > 0x10000 && val != 0xFFFFFFFF;
    if (looks_initialised) return;

    using init_fn_t = int(__cdecl*)();
    const int rc = reinterpret_cast<init_fn_t>(0x732E30)();
    Log("[Boot] ped pools init rc=%d new 0x8D6094=%08x", rc,
        *reinterpret_cast<std::uint32_t*>(0x8D6094));
}

static void OnGameReadyOnce()
{
    static bool done = false;
    if (done) return;
    done = true;

    auto* ped = gta::sa::GetLocalPlayerPed();
    if (!ped)
    {
        LogLine("[GameReady] ERROR: no ped pointer\r\n");
        ExitProcess(1);
        return;
    }

    // First-time skin swap. Without this the bootstrap "player" special slot
    // doesn't fully bind textures and the ped renders as untextured green.
    // Mid-game swaps (server-pushed SetPlayerSkin) still need to be skipped —
    // they leave m_rwObject NULL and crash the next frame inside the ~45
    // inlined CVisibilityPlugins lookups. The boot swap is OK because the
    // ped state is fresh and the hooks at 0x732B20 / 0x732B00 + the
    // 0x5A82C0 NOP keep the model-swap path from AVing.
    gta::sa::SafeSetPlayerPedModel(ped, 7);

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "[GameReady] OnGameReadyOnce: state=%u atMenu=%d loaded=%d\r\n",
                  static_cast<unsigned>(get_system_state()),
                  IsAtMenu() ? 1 : 0,
                  IsGameLoaded() ? 1 : 0);
    LogLine(buf);

    // EnsurePedPoolsInitialised();  // disabled: 0x732E30 returns rc=0 but
    // leaves 0x8D6094 = 0xFFFFFFFF (diag's own looks_ok check rejects it).
    // Trying without it to see if a different crash surfaces.

    LogLine("[GameReady] Game ready\r\n");

    allow_chat = true;
}

void TickGameReady()
{
    ++g_totalFrames;

    static int hb = 0;
    if ((++hb % 300) == 0)
        LogLine("[HB] TickGameReady alive\r\n");

    if (g_totalFrames < 5)
        return;

    // Если мы ещё во фронтенде — кикаем игру один раз
    if (!g_kickedToPlaying.load(std::memory_order_acquire) && get_system_state() == gs_frontend)
    {
        LogLine("[KICK] kick_game_start()\r\n");
        kick_game_start();
        g_kickedToPlaying.store(true, std::memory_order_release);
        return;
    }

    if (!gta::sa::GetLocalPlayerPed())
        return;

    static int settle = 0;
    if (settle++ < 30)
        return;

    OnGameReadyOnce();

    // @todo remove once the flat-sky bug is closed. Periodic timecycle dump —
    // the sky gradient, fog and far clip come out of one struct that is rebuilt
    // every frame, so a snapshot over time is what distinguishes "the update
    // produces bad numbers" from "the renderer never draws the gradient".
    // Run any pending CClothes::RebuildPlayer here rather than inline in the
    // model-swap path — MTA does the same from its per-frame ped pulse.
    gta::sa::ProcessPendingPlayerRebuild(g_totalFrames);

    if ((g_totalFrames % 120) == 0)
    {
        opensamp::diag::DumpSkyState();
        opensamp::diag::DumpBootState();
    }
}

// ---------------- CGame::Process hook -> netgame tick ----------------
//
// SA-MP's main-loop integration point. The game calls CGame::Process() once
// per frame from 0x53C095 (CALL rel32). We resolve that call target and hook
// it with MinHook; our detour runs the original and then ticks the netgame
// state machine. This is the only reliable "once per game frame" anchor that
// isn't rendering-bound.

using CGameProcess_t = void(__cdecl*)();
static CGameProcess_t oCGameProcess = nullptr;

static void __cdecl hkCGameProcess()
{
    if (oCGameProcess) oCGameProcess();
    opensamp::net::CNetGame::Get().Tick();
}

static bool Install_CGameProcess_Hook()
{
    // Decode the CALL rel32 at 0x53C095 to find CGame::Process's real address.
    auto* p = reinterpret_cast<std::uint8_t*>(0x53C095);
    if (p[0] != 0xE8)
        return false;

    const std::int32_t rel    = *reinterpret_cast<std::int32_t*>(p + 1);
    const auto         target = reinterpret_cast<void*>(0x53C095u + 5u + rel);

    if (MH_CreateHook(target, &hkCGameProcess,
                      reinterpret_cast<void**>(&oCGameProcess)) != MH_OK)
        return false;

    return MH_EnableHook(target) == MH_OK;
}

// ---------------- RunningScriptProcess hook -> bootstrap player ----------------

static std::atomic_bool g_bootDone{false};

static void BootstrapStart_NoScm()
{
    static bool once = false;
    if (once) return;
    once = true;

    gta::sa::RequestSpecialModel("player", 26);
    gta::sa::LoadAllRequestedModels(1);
    gta::sa::SetupPlayerPed(0);
    gta::sa::PlaceableSetPosition(gta::sa::GetPlayerPlaceable(), 0.0f, 0.0f, 3.1172f);

    g_bootDone.store(true, std::memory_order_release);
}

using RunningScriptProcess_t = void(__fastcall*)(void* self, void* edx);
static RunningScriptProcess_t oRunningScriptProcess = nullptr;

static void __fastcall hkRunningScriptProcess(void* /*self*/, void* /*edx*/)
{
    if (g_bootDone.load(std::memory_order_acquire))
        return;

    if (get_system_state() < gs_init_playing_game)
        return;

    BootstrapStart_NoScm();
}

// ---------------- install ----------------

bool GameReady_Install()
{
    InitDebugLog();

    const auto p_rs = reinterpret_cast<void*>(0x469F00);
    if (MH_CreateHook(p_rs, &hkRunningScriptProcess,
                      reinterpret_cast<void**>(&oRunningScriptProcess)) != MH_OK)
        return false;

    if (MH_EnableHook(p_rs) != MH_OK)
        return false;

    if (!Install_CGameProcess_Hook())
    {
        LogLine("[GameReady] WARNING: CGame::Process hook failed — netgame will not tick\r\n");
        // Non-fatal: boot sequence still works; only net state machine is stalled.
    }

    if (!InstallInputGuards())
    {
        LogLine("[GameReady] WARNING: user32 input guards failed — UI may leak input to game\r\n");
    }

    if (!InstallClumpTableGuards())
    {
        LogLine("[GameReady] WARNING: CVisibilityPlugins guard hooks failed — model swaps will crash\r\n");
    }

    opensamp::net::CNetGame::Get().Initialize();
    return true;
}
