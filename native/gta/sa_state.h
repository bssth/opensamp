#pragma once
#include "common.h"
#include "../addresses.h"

enum e_system_state : std::uint8_t
{
    gs_start_up = 0,
    gs_init_logo_mpeg,
    gs_logo_mpeg,
    gs_init_intro_mpeg,
    gs_intro_mpeg,
    gs_init_once,
    gs_init_frontend,
    gs_frontend,
    gs_init_playing_game,
    gs_playing_game
};

inline e_system_state get_system_state()
{
    DWORD s{};
    gta::read_memory(opensamp::addr::kGameState, sizeof(s), &s);
    return static_cast<e_system_state>(s);
}

inline void set_system_state(const e_system_state s)
{
    const DWORD v = s;
    gta::write_memory(opensamp::addr::kGameState, sizeof(v), &v);
}

inline void kick_game_start()
{
    // Mirror SAMP's `CGame::StartGame()` exactly. Order and full set of
    // writes matter — several game subsystems gate their first-frame init on
    // `ADDR_GAME_STARTED == 1` (specifically subsystems that finalise
    // CVisibilityPlugins bindings and streaming sector setup), and we were
    // previously missing that write.
    //
    //   ADDR_ENTRY        = 0xC8D4C0  →  8 (gs_init_playing_game)
    //   ADDR_GAME_STARTED = 0xBA6831  →  1   (was missing — likely cause of
    //                                         half-init streaming/visibility)
    //   ADDR_MENU         = 0xBA67A4  →  0
    //   ADDR_STARTGAME    = 0xBA677B  →  0

    set_system_state(gs_init_playing_game);

    constexpr BYTE one  = 1;
    constexpr BYTE zero = 0;
    gta::write_memory(opensamp::addr::kGameStartedFlag, 1, &one);   // ADDR_GAME_STARTED
    gta::write_memory(opensamp::addr::kMenuFlag,        1, &zero);  // ADDR_MENU
    gta::write_memory(opensamp::addr::kStartGameFlag,   1, &zero);  // ADDR_STARTGAME
}
