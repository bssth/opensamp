#pragma once

// On-demand diagnostic helpers. Called from the `/compat` chat command so
// whoever is testing can dump everything we can observe about the current
// client/server state into OpenSamp.log with one line of input.

namespace opensamp::diag
{
    // Print NetGame state, bridge flags, game state, local ped info and the
    // ped-pool globals we suspect when things crash. One call per line so the
    // output survives in both the in-game chat and OpenSamp.log.
    void RunCompatDump();

    // Dump CTimeCycle::m_CurrentColours (sky gradient, fog start, far clip,
    // ambient), the clock, the weather globals read as shorts, and the
    // extra-colour override. Part of RunCompatDump, and also logged
    // periodically while the flat-sky bug is open.
    void DumpSkyState();

    // Log the bootstrap-state globals that differ between our boot path and
    // MTA's: gbGameStarted, CTimer::m_UserPause, and the game-loaded flag.
    void DumpBootState();

    // Force-call gta_sa.exe+0x732E30 (ped task/anim pool init). Mostly for
    // experimenting when we suspect those pools weren't populated during our
    // abbreviated boot. Logs before/after values.
    void ForceInitPedPools();
} // namespace opensamp::diag
