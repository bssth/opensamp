#include "diag.h"

#include <cstdint>

#include "addresses.h"
#include "game/game.h"
#include "gta/sa_state.h"
#include "net/netgame.h"
#include "sampraknet_bridge.h"

// Defined in sampraknet_bridge.cpp — mirrors to chat + OpenSamp.log.
extern void Log(const char* fmt, ...);

namespace opensamp::diag
{
    namespace
    {
        std::uint32_t ReadU32(std::uintptr_t addr)
        {
            return *reinterpret_cast<const volatile std::uint32_t*>(addr);
        }

        std::uint8_t ReadU8(std::uintptr_t addr)
        {
            return *reinterpret_cast<const volatile std::uint8_t*>(addr);
        }

        std::uint16_t ReadU16(std::uintptr_t addr)
        {
            return *reinterpret_cast<const volatile std::uint16_t*>(addr);
        }

        std::int16_t ReadS16(std::uintptr_t addr)
        {
            return *reinterpret_cast<const volatile std::int16_t*>(addr);
        }

        float ReadF32(std::uintptr_t addr)
        {
            return *reinterpret_cast<const volatile float*>(addr);
        }
    }

    void DumpBootState()
    {
        namespace a = opensamp::addr;

        // Two state deltas between our bootstrap and MTA's, both worth watching:
        //
        // gbGameStarted (0xBA6831) — kick_game_start() writes 1 here, mirroring
        // SA-MP. But 1 is the *frontend* value: SA-MP's own IsGameLoaded() reads
        // it inverted (`if (!*(BYTE*)ADDR_GAME_STARTED) return TRUE`), i.e. the
        // engine clears it to 0 once the world is up. CStreaming reads it at
        // 0x4D6E07 and 0x4D76E4 to zero the streaming look-ahead and skip a
        // streaming update step — so if it is still 1 here, that is a direct
        // candidate for "LODs everywhere instead of full models".
        //
        // m_UserPause (0xB7CB49) — MTA clears this as part of starting the game
        // and we never touch it. A stuck pause suppresses per-frame updates.
        Log("STATE systemState=%u gbGameStarted=%u userPause=%u loadedFlag=%u",
            (unsigned)get_system_state(),
            (unsigned)ReadU8(a::kGameStartedFlag),
            (unsigned)ReadU8(a::kGamePaused),
            (unsigned)ReadU8(a::kIsGameLoaded));

        // Two one-run questions.
        //
        // lodMult: the global draw-distance multiplier. If this reads 0.0, every
        // model's effective draw distance is zero and only LOD proxies render —
        // which is exactly the reported symptom, and would make the LOD bug a
        // single missing initialisation rather than a streaming problem.
        //
        // visPluginTable: [0x8D6094], the CVisibilityPlugins clump dispatch
        // table. The tree contradicts itself about whether our bootstrap runs
        // CGame::Initialise — hooks.cpp says it is skipped, patches.cpp says
        // disassembly proves otherwise — and several workarounds rest on the
        // answer. A real heap pointer here means the engine initialised it and
        // the "truncated bootstrap" theory is dead; 0x34 or 0xFFFFFFFF means it
        // is alive.
        Log("STATE lodMult=%.3f visPluginTable=0x%08X",
            *reinterpret_cast<const volatile float*>(a::kLodDistanceMultiplier),
            ReadU32(a::kVisPlugins_ClumpTable));
    }

    void DumpSkyState()
    {
        namespace a = opensamp::addr;

        const auto tR = ReadU16(a::kSkyTopRed),    tG = ReadU16(a::kSkyTopGreen),    tB = ReadU16(a::kSkyTopBlue);
        const auto bR = ReadU16(a::kSkyBottomRed), bG = ReadU16(a::kSkyBottomGreen), bB = ReadU16(a::kSkyBottomBlue);

        // Top == bottom means the gradient itself is degenerate, i.e. a data
        // problem in the timecycle update rather than a rendering problem —
        // and since the sky-bottom colour doubles as the world fog colour,
        // that one condition explains both the flat sky and the washed horizon.
        const bool flat = (tR == bR && tG == bG && tB == bB);

        Log("SKY   top=(%u,%u,%u) bottom=(%u,%u,%u) %s",
            (unsigned)tR, (unsigned)tG, (unsigned)tB,
            (unsigned)bR, (unsigned)bG, (unsigned)bB,
            flat ? "<<< DEGENERATE: top == bottom" : "gradient present");

        // Three float triples at +0x00 / +0x0C / +0x18. The first is ambient and
        // is confirmed correct (it tracks timecyc.dat exactly). The other two
        // are believed to be ambient-for-objects and directional, but +0x18 was
        // observed reading back identical to ambient while timecyc.dat says
        // directional is 255,255,255 — so they are logged raw by offset rather
        // than labelled with a guess.
        Log("SKY   farClip=%.1f fogStart=%.1f amb=(%.2f,%.2f,%.2f) +0x0C=(%.2f,%.2f,%.2f) +0x18=(%.2f,%.2f,%.2f)",
            ReadF32(a::kFarClip), ReadF32(a::kFogStart),
            ReadF32(a::kCurrentColours + 0x00), ReadF32(a::kCurrentColours + 0x04),
            ReadF32(a::kCurrentColours + 0x08),
            ReadF32(a::kCurrentColours + 0x0C), ReadF32(a::kCurrentColours + 0x10),
            ReadF32(a::kCurrentColours + 0x14),
            ReadF32(a::kCurrentColours + 0x18), ReadF32(a::kCurrentColours + 0x1C),
            ReadF32(a::kCurrentColours + 0x20));

        // Water colour also lives in m_CurrentColours, so it is measurable the
        // same way as the sky was: if these match timecyc.dat's WaterRGBA for
        // the current weather and hour, then green water is not a colour
        // problem and the search moves to textures / reflections.
        Log("SKY   water=(%.0f,%.0f,%.0f,%.0f)",
            ReadF32(a::kWaterRed), ReadF32(a::kWaterGreen),
            ReadF32(a::kWaterBlue), ReadF32(a::kWaterAlpha));

        // Read the weather globals as SHORTS. Any nonzero high byte is the
        // signature of something having written them a byte at a time.
        Log("SKY   clock=%02u:%02u weather forced=%d old=%d new=%d  extraColourOn=%u extraColour=%d",
            (unsigned)ReadU8(a::kTimeHours), (unsigned)ReadU8(a::kTimeMinutes),
            (int)ReadS16(a::kWeatherForced),
            (int)ReadS16(a::kWeatherOld),
            (int)ReadS16(a::kWeatherNew),
            ReadU32(a::kExtraColourOn),
            (int)ReadU32(a::kExtraColour));
    }

    void RunCompatDump()
    {
        Log("=== OpenSamp compat dump ===");

        // ---------------- NetGame ----------------
        {
            const auto& ng = net::CNetGame::Get();
            Log("NET   state=%s server=%s:%u nick='%s'",
                net::ToString(ng.State()),
                ng.Server().host.empty() ? "-" : ng.Server().host.c_str(),
                (unsigned)ng.Server().port,
                ng.Local().nickname.empty() ? "-" : ng.Local().nickname.c_str());
        }

        // ---------------- Bridge ----------------
        Log("BRIDGE init=%d connected=%d game_inited=%d pid=%d",
            bridge::IsInitialized() ? 1 : 0,
            bridge::IsConnected()   ? 1 : 0,
            bridge::IsGameInited()  ? 1 : 0,
            bridge::MyPlayerId());

        // ---------------- Game state ----------------
        const auto st  = get_system_state();
        const auto atm = ReadU8(0xBA677B);
        const auto ldr = ReadU8(0x86969C);
        const auto psd = ReadU8(0xB7CB49);
        Log("GAME  system_state=%u at_menu=%u loaded=%u paused=%u",
            (unsigned)st, (unsigned)atm, (unsigned)ldr, (unsigned)psd);

        // ---------------- Local ped ----------------
        if (auto* ped = gta::sa::GetLocalPlayerPed())
        {
            const auto& p = ped->m_position;
            Log("PED   ptr=0x%08x model=%u health=%.1f armour=%.1f pos=(%.1f, %.1f, %.1f)",
                (unsigned)reinterpret_cast<std::uintptr_t>(ped),
                (unsigned)ped->m_modelIndex,
                ped->Health(), ped->Armour(),
                p.x, p.y, p.z);
        }
        else
        {
            Log("PED   ptr=NULL");
        }

        // ---------------- Suspicious globals ----------------
        const auto p8c = ReadU32(0x8D608C);
        const auto p90 = ReadU32(0x8D6090);
        const auto p94 = ReadU32(0x8D6094);
        const bool ok  = (p94 > 0x10000 && p94 != 0xFFFFFFFFu);
        Log("POOLS 0x8D608C=%08x 0x8D6090=%08x 0x8D6094=%08x (looks_ok=%d)",
            p8c, p90, p94, ok ? 1 : 0);

        // ---------------- Sky / timecycle ----------------
        DumpSkyState();

        Log("===========================");
    }

    void ForceInitPedPools()
    {
        const auto before = ReadU32(0x8D6094);
        Log("[compat] ped-pools reinit: before 0x8D6094=%08x", before);

        using init_fn_t = int(__cdecl*)();
        const int rc = reinterpret_cast<init_fn_t>(0x732E30)();

        Log("[compat] ped-pools reinit: rc=%d after 0x8D6094=%08x",
            rc, ReadU32(0x8D6094));
    }
} // namespace opensamp::diag
