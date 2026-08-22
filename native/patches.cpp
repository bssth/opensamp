// gta_sa.exe US 1.0 binary patches applied once at DLL load.
// Most offsets are inherited from earlier reverse-engineering work.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <Windows.h>

#include "memory.h"

// ---------------- ScanList relocation ----------------
// Allows > 2 CPlayerInfo structures by pointing all "scan list" code sites
// to our own buffer. Classic SAMP trick. US 1.0 addresses only.

static constexpr std::size_t kScanListSize = 8 * 20000;
static unsigned char g_scan_list_memory[kScanListSize]{};

#pragma pack(push, 1)
struct PedModel
{
    DWORD func_tbl;
    BYTE data[64];
};
#pragma pack(pop)

static PedModel g_ped_models_memory[300]{};

static void RelocateScanListHack()
{
    const auto base = reinterpret_cast<DWORD>(&g_scan_list_memory[0]);
    const auto end = base + kScanListSize;

    // Direct DWORD writes (4-byte operand is the whole instruction)
    static constexpr DWORD kReloc1[] = {
        0x5DC7AA, 0x41A85D, 0x41A864, 0x408259, 0x711B32, 0x699CF8,
        0x4092EC, 0x40914E, 0x408702, 0x564220, 0x564172, 0x563845,
        0x84E9C2, 0x85652D
    };
    for (DWORD a : kReloc1)
        mem_put<DWORD>(a, base);

    // Writes at addr+3 (3-byte instruction prefix preserved)
    static constexpr DWORD kReloc2[] = {
        0x40D68C, 0x5664D7, 0x566586, 0x408706, 0x56B3B1, 0x56AD91, 0x56A85F, 0x5675FA,
        0x56CD84, 0x56CC79, 0x56CB51, 0x56CA4A, 0x56C664, 0x56C569, 0x56C445, 0x56C341,
        0x56BD46, 0x56BC53, 0x56BE56, 0x56A940, 0x567735, 0x546738, 0x54BB23, 0x6E31AA,
        0x40DC29, 0x534A09, 0x534D6B, 0x564B59, 0x564DA9, 0x67FF5D, 0x568CB9, 0x568EFB,
        0x569F57, 0x569537, 0x569127, 0x56B4B5, 0x56B594, 0x56B2C3, 0x56AF74, 0x56AE95,
        0x56BF4F, 0x56ACA3, 0x56A766, 0x56A685, 0x70B9BA, 0x56479D, 0x70ACB2, 0x6063C7,
        0x699CFE, 0x41A861, 0x40E061, 0x40DF5E, 0x40DDCE, 0x40DB0E, 0x40D98C, 0x1566855
        // NOTE: 0x1566855 is NOT a typo and is intentionally outside 0x400000-0xC00000.
        // The HOODLUM-cracked US 1.0 gta_sa.exe relocates this scanlist accessor
        // (`lea eax, [eax*8 + 0xB7D0B8]; ret`) into its appended .HOODLUM section
        // (0x1556000-0x1577000). The +3 write lands on the 0xB7D0B8 operand, exactly
        // like every other entry. Verified by disassembly; matches SA-MP US 1.0.
    };
    for (DWORD a : kReloc2)
        mem_put<DWORD>(a + 3, base);

    // Writes at addr+3, pointing to base+4
    static constexpr DWORD kReloc3[] = {
        0x4091C5, 0x409367, 0x40D9C5, 0x40DB47, 0x40DC61, 0x40DE07, 0x40DF97,
        0x40E09A, 0x534A98, 0x534DFA, 0x71CDB0
    };
    for (DWORD a : kReloc3)
        mem_put<DWORD>(a + 3, base + 4);

    // End-of-list pointer writes
    static constexpr DWORD kRelocEnd[] = {0x5634A6, 0x5638DF, 0x56420F, 0x564283};
    for (DWORD a : kRelocEnd)
        mem_put<DWORD>(a, end);

    // Misc one-off
    mem_put<DWORD>(0x40936A, base + 4);

    // Reset the original scanlist region (CPlayerInfo[14400] * 8)
    std::memset(reinterpret_cast<void*>(0xB7D0B8), 0, 8 * 14400);
}

static void RelocatePedsListHack()
{
    for (auto& m : g_ped_models_memory)
        m.func_tbl = 0x85BDC0;

    mem_put<DWORD>(0x4C67AD, reinterpret_cast<DWORD>(&g_ped_models_memory[0]));
}

// ---------------- pool/limit increases ----------------

static void ApplyPoolLimits()
{
    // Vehicle pool → 710: overwrite `push X; push imm32` with push 0; push 710
    static constexpr BYTE kVehiclePool[] = {0x6A, 0x00, 0x68, 0xC6, 0x02, 0x00, 0x00};
    std::memcpy(reinterpret_cast<void*>(0x551024), kVehiclePool, sizeof(kVehiclePool));

    mem_put<BYTE>(0x550FF2, 210); // ped pool
    mem_put<BYTE>(0x551283, 210); // ped intelligence pool
    mem_put<BYTE>(0x551140, 0x05); // task pool
    mem_put<BYTE>(0x551178, 0x01); // event pool → 456
    mem_put<BYTE>(0x551060, 0x42); // building pool → 17096

    // Collision model pool
    static constexpr BYTE kCollisionPool[] = {0x68, 0xFF, 0x7E, 0x00, 0x00};
    std::memcpy(reinterpret_cast<void*>(0x551106), kCollisionPool, sizeof(kCollisionPool));

    // Streaming memory budget: 128 MB
    mem_put<DWORD>(0x5B8E6A, 0x08000000u);

    // VehicleStruct count → 127 (full 7-byte replacement)
    mem_put<BYTE>(0x5B8FDE, 0x6A); // push 0
    mem_put<BYTE>(0x5B8FDF, 0x00);
    mem_put<BYTE>(0x5B8FE0, 0x68); // push imm32
    mem_put<BYTE>(0x5B8FE1, 127);
    mem_put<BYTE>(0x5B8FE2, 0x00);
    mem_put<BYTE>(0x5B8FE3, 0x00);
    mem_put<BYTE>(0x5B8FE4, 0x00);
}

// ---------------- crash fixes ----------------

static void ApplyCrashFixes()
{
    // 0x609C08 NOP 39 — labelled "CPlayerPed::ProcessControl crash fix" in
    // SAMP, but on our build re-enabling it kills movement: ped rotates
    // toward camera direction but never starts a walk task. Suspicion:
    // the 39 bytes contain locomotion task creation, not just a null-check
    // bypass. Leave disabled until we know exactly what they do.
    //
    // mem_set(reinterpret_cast<void*>(0x609C08), 0x90, 39);

    // TEMPORARY STOPGAP — restored 2026-08-09, to be deleted by the MTA port.
    //
    // Removing this RET produced a naked CJ instead of the requested skin, and
    // gating our own RebuildPlayer call on model==0 did not help, which means
    // the engine reaches CClothes::RebuildPlayer from inside SetModelIndex —
    // i.e. from the hand-rolled swap in SafeSetPlayerPedModel, not from us. The
    // real fix is MTA's _ChangeModel sequence (blocking model ref, verify
    // GetRwObject, Respawn to settle clothes/tasks, plain SetModelIndex), not a
    // byte patch. Until that lands, keep the RET so skins work at all.
    mem_put<BYTE>(0x5A82C0, 0xC3);

    // The old justification for the RET was that our bootstrap leaves the CVisibilityPlugins
    // clump table at [0x8D6094] uninitialised, so the rebuild walks junk. That
    // premise is false: disassembly of the retail exe shows the state-8 handler
    // does call CGame::Initialise (jump table 0x748EF8 entry [8] -> 0x748CF1 ->
    // 0x748CFB -> 0x53E580 -> 0x53BC80), and MTA relies on exactly that same
    // engine path.
    //
    // MTA never patches this function. It calls it, deferred to a safe point in
    // the frame and at most once per frame — see
    // gta::sa::ProcessPendingPlayerRebuild in game/game.h, ported from
    // CClientPed::ProcessRebuildPlayer (MTA:SA, GPL-3.0).
    //
    // Keeping it RETed left the local player's clothes textures unbound, which
    // is the leading explanation for the green placeholder in the water.
    // REMOVED: mem_set(0x53EA08, 0x90, 10);
    //
    // Was commented "ped shadow rendering". Disassembly of the retail exe says
    // otherwise — those ten bytes are exactly:
    //
    //     0x53EA08  mov ecx, 0xC40350   ; this = CRealTimeShadowManager
    //     0x53EA0D  call 0x706AB0       ; CRealTimeShadowManager::Update
    //
    // i.e. the shadow manager's *per-frame update*, not the rendering. The call
    // immediately before it, 0x53EA03 -> CWorld::ProcessPedsAfterPreRender, is
    // untouched (MTA hooks that one and keeps the original call intact:
    // HOOKPOS_Idle_CWorld_ProcessPedsAfterPreRender, CMultiplayerSA.cpp:298).
    //
    // Meanwhile the legacy block still applies MTA's four patches that *improve*
    // real-time shadow casting (CRealTimeShadowManager::GetRealTimeShadow,
    // CShadows::CastRealTimeShadowSectorList). So shadows were still being
    // projected, from a buffer that was never refreshed — which is a good
    // explanation for the smeared coloured patches that trail the player across
    // water and terrain. Restored so the manager updates again.
    mem_set(reinterpret_cast<void*>(0x542485), 0x90, 11); // CPhysical destructor (0x705B3B)
    mem_set(reinterpret_cast<void*>(0x434272), 0x90, 5); // SomeCarGenerator (0x41A8B3)
    mem_put<BYTE>(0x6D5410, 0xFF); // train entry on track 2

    // drown-in-vehicle: mov al, 0; nop
    mem_put<BYTE>(0x4BC6C1, 0xB0);
    mem_put<BYTE>(0x4BC6C2, 0x00);
    mem_put<BYTE>(0x4BC6C3, 0x90);

    // 0x540040 crash: test ecx, ecx; jz ... instead of test [ecx+270]
    mem_set(reinterpret_cast<void*>(0x540040), 0x90, 6);
    mem_put<BYTE>(0x540046, 0x85); // test ecx, ecx
    mem_put<BYTE>(0x540047, 0xC9);
    mem_put<BYTE>(0x540048, 0x74); // jz

    // ---------------- SilentPatch byte-level crash fixes ----------------
    // Cherry-picked from SilentPatchSA (MIT, by Adrian Zdanowicz / Silent).
    // Only the surgical byte-only patches are ported here — the rest of
    // SilentPatch requires its full Memory.h / Trampoline.h infrastructure
    // and a per-feature C++ stub library which isn't worth integrating
    // piecemeal. If we ever want the full set, build SilentPatchSA.asi
    // separately and load it from DllMain.

    // Heap corruption — the function at 0x4A9D50 trampled the heap on certain
    // code paths. Silent's accepted fix is to RET it; the function had no
    // observable gameplay effect. SilentPatchSA.cpp ~L8028.
    mem_put<BYTE>(0x4A9D50, 0xC3);

    // Mirrors crash — original sequence at 0x7271CB had a wrong branch target
    // that AVed when minimising the game next to a mirror. Replace with
    // `test eax, eax; je 0x727203; add esp, 4`. SilentPatchSA.cpp ~L6821.
    {
        static constexpr BYTE kMirrors[] = { 0x85, 0xC0, 0x74, 0x34, 0x83, 0xC4, 0x04 };
        std::memcpy(reinterpret_cast<void*>(0x7271CB), kMirrors, sizeof(kMirrors));
    }
}

// ---------------- intro skip ----------------
// Mirrors SAMP's `ApplyPreGamePatches`: NOPs the 6-byte sequence that gates
// past the logo videos, and replaces the two intro movie filenames in
// .rdata with "title" so the streamer doesn't stall waiting for them.
// Lets the engine progress naturally through gs_init_once (5) →
// gs_init_frontend (6) → gs_frontend (7), which gives CGame::Initialise the
// chance to populate CVisibilityPlugins / streaming / collision tables.
static void ApplyIntroSkipPatches()
{
    // 0x747483: 6-byte sequence inside `CMovieClip::Init` that decides whether
    // to play the GTA logo / R* logo. Replacing with NOPs forces the bypass.
    mem_set(reinterpret_cast<void*>(0x747483), 0x90, 6);

    // Logo movie filenames in .rdata. Both reused as "title" — a tiny
    // existing video file that finishes near-instantly. Slots are 10 bytes
    // each (movie name max length); zero the tail before writing the new
    // shorter name so .rdata stays clean.
    static constexpr std::uintptr_t kMovieNameA = 0x866CD8;
    static constexpr std::uintptr_t kMovieNameB = 0x866CCC;
    mem_set(reinterpret_cast<void*>(kMovieNameA), 0, 10);
    mem_set(reinterpret_cast<void*>(kMovieNameB), 0, 10);
    std::memcpy(reinterpret_cast<void*>(kMovieNameA), "title", 5);
    std::memcpy(reinterpret_cast<void*>(kMovieNameB), "title", 5);
}

// ---------------- multiplayer behavior ----------------

static void ApplyMultiplayerPatches()
{
    // Kill CPopulation::AddPed: xor eax, eax; ret
    mem_put<BYTE>(0x612710, 0x33);
    mem_put<BYTE>(0x612711, 0xC0);
    mem_put<BYTE>(0x612712, 0xC3);

    mem_set(reinterpret_cast<void*>(0x613BA7), 0x90, 5); // AddPed call in create_train
    mem_set(reinterpret_cast<void*>(0x53C1C1), 0x90, 5); // CCarCtrl::GenerateRandomCars
    mem_set(reinterpret_cast<void*>(0x56E0FA), 0x90, 18); // FindPlayerVehicle → always nPlayerPed

    mem_set(reinterpret_cast<void*>(0x561AF0), 0x90, 7); // anti-pause / run in background

    mem_put<BYTE>(0x47C477, 0xEB); // camera_on_actor fallback

    // CPlayerPed::CPlayerPed — task system corruption fix (SAMP: JNZ → JZ)
    mem_put<BYTE>(0x60D64E, 0x84);

    // Matrix pool → 4228 (SAMP: single-byte imm8 change)
    mem_put<BYTE>(0x54F3A2, 0x10);

    // CPushBike fires patch: jmp +0x77 (SAMP)
    mem_put<BYTE>(0x53A984, 0xEB);
    mem_put<BYTE>(0x53A985, 0x77);

    // @todo install SCM events processor hook at 0x47BF54 (SAMP onscmevent).
    //       Needs C++ handler — patching the prologue without one is unsafe.
}

// ---------------- behavior tweaks (SAMP-style) ----------------

static void ApplyBehaviorPatches()
{
    mem_set(reinterpret_cast<void*>(0x440833), 0x90, 8); // no interior peds
    mem_set(reinterpret_cast<void*>(0x53C06A), 0x90, 5); // no IPL vehicles
    mem_set(reinterpret_cast<void*>(0x53C090), 0x90, 5); // prevent replays
    mem_put<BYTE>(0x86D1EC, 0x00); // no playidles anim loading
    mem_put<BYTE>(0x588BE0, 0xC3); // MessagePrint → ret
    mem_set(reinterpret_cast<void*>(0x56E5AD), 0x90, 5); // no "wasted" screen
    mem_set(reinterpret_cast<void*>(0x609A4E), 0x90, 6); // use ped IDE anims, not player
    // @todo 0x6884C4 NOP 6 — "don't rotate ped from camera" (SAMP). Disabled for now:
    // without a network input module driving facing, the ped stays in a fixed orientation
    // and WASD only pushes along its local axis. Re-enable once local player control is in.

    // goggles (NV/thermal) manual control
    mem_set(reinterpret_cast<void*>(0x5E3AD1), 0x90, 3);
    mem_set(reinterpret_cast<void*>(0x5DF1EE), 0x90, 3);
    mem_set(reinterpret_cast<void*>(0x634F6C), 0x90, 16);

    // sniper click sound
    mem_set(reinterpret_cast<void*>(0x60F289), 0x90, 8);
    mem_set(reinterpret_cast<void*>(0x60F29D), 0x90, 19);

    mem_set(reinterpret_cast<void*>(0x6BC9EB), 0x90, 2); // motorbike input fix
    // Removes the blue-ish fog on the PAUSE MENU MAP SCREEN — this is inside
    // CMenuManager, not the world renderer. The old comment said "remove blue
    // fog" without qualification and cost an afternoon of chasing it as the
    // cause of the washed-out horizon. It is not related. SA-MP ships the
    // identical patch.
    mem_set(reinterpret_cast<void*>(0x575B0E), 0x90, 5); // pause-menu map fog
}

// ---------------- filesystem tweaks ----------------

static void ApplyFilesystemPatches()
{
    // Rename Documents folder: "GTA San Andreas User Files" → "OpenSamp Files"
    // String lives at 0x849AB4 in .rdata (27-byte slot including terminator).
    // "OpenSamp Files" is 14 chars; we zero the tail to keep .rdata clean.
    static constexpr std::uintptr_t kUserFilesString = 0x849AB4;
    static constexpr std::size_t kUserFilesSlot = 27;
    static constexpr char kNewName[] = "OpenSamp Files";

    mem_set(reinterpret_cast<void*>(kUserFilesString), 0, kUserFilesSlot);
    std::memcpy(reinterpret_cast<void*>(kUserFilesString), kNewName, sizeof(kNewName) - 1);
}

// ---------------- base MTA-style patches (legacy block) ----------------
//
// 261 byte patches inherited from earlier work, originally with no comments at
// all. A cross-reference pass against MTA:SA (GPL-3.0, named code) matched 251
// of them one-for-one with a documented patch line upstream: this block is
// essentially MTA's InitHooks_Others plus SA-MP's patches.cpp, in MTA's order.
//
// An earlier version of this comment claimed a third of the addresses fell
// inside CTimeCycle / CRenderer / CClouds / CVisibilityPlugins / CWaterLevel,
// and that one of them was the likely cause of the flat sky. That was inferred
// from numeric ranges and it was wrong. Counted properly, CTimeCycle
// (0x55FEC0-0x5617A0), CColourSet (0x55F4B0-0x55F900), CClouds
// (0x712FA0-0x716D00) and CWeather (0x72A480-0x72C800) contain *zero* entries
// from this block, and neither CTimeCycle::Initialise, CClouds::RenderSkyPolys,
// DoRWStuffStartOfFrame nor any of their call sites are patched anywhere in
// this file.
//
// CAUTION before deleting this function: the VirtualProtect below covers
// 0x401000..0x8A4000, and ApplyCrashFixes / ApplyIntroSkipPatches /
// ApplyPoolLimits / ApplyFilesystemPatches all write into .text and .rdata via
// raw memcpy/memset with no page-protection handling of their own. They work
// only because this runs first. Route those through mem_put/mem_set before
// removing anything here.
//
// The bisect harness is kept so the block can be cleared with a single boot,
// not because it is still a strong suspect:
//
//     opensamp_patchlimit.txt   (next to gta_sa.exe, one decimal number)
//        file absent, or -1  -> apply all 261 (normal operation)
//        0                   -> apply none
//        N                   -> apply only the first N, skip the rest
//
// Each boot appends the count and the last address applied to
// opensamp_patches.log.
//
// WARNING: LP() gates each individual *write*, not each logical patch, and a
// few patches are written as consecutive single-byte writes. A limit landing
// inside one of those groups leaves a half-written instruction and corrupts the
// code stream. Safe cutoffs are group boundaries; when in doubt use 0 or -1.
//
// Delete this harness once the block is enumerated and documented.

static int            g_legacyLimit   = -1;
static int            g_legacyApplied = 0;
static std::uintptr_t g_legacyLast    = 0;

static void LoadLegacyPatchLimit()
{
    std::FILE* f = std::fopen("opensamp_patchlimit.txt", "r");
    if (!f) return;
    int v = -1;
    if (std::fscanf(f, "%d", &v) == 1) g_legacyLimit = v;
    std::fclose(f);
}

// Returns false once the configured limit is reached, which makes the caller
// bail out of the rest of the block.
static bool legacy_gate(std::uintptr_t addr)
{
    if (g_legacyLimit >= 0 && g_legacyApplied >= g_legacyLimit) return false;
    ++g_legacyApplied;
    g_legacyLast = addr;
    return true;
}

#define LP(addr) if (!legacy_gate(addr)) return

static void ApplyLegacyBasePatches()
{
    DWORD oldProt;
    VirtualProtect((LPVOID)0x401000, 0x4A3000, PAGE_EXECUTE_READWRITE, &oldProt);

    LP(0x748EF8);
    mem_put<DWORD>(0x748EF8, 0x748AE7);
    LP(0x748EFC);
    mem_put<DWORD>(0x748EFC, 0x748B08);
    LP(0x748B0E);
    mem_put<BYTE>(0x748B0E, 5);
    LP(0x748C2B);
    mem_set(reinterpret_cast<void*>(0x748C2B), 0x90, 5);
    LP(0x748C9A);
    mem_set(reinterpret_cast<void*>(0x748C9A), 0x90, 5);
    LP(0x748CF6);
    mem_set(reinterpret_cast<void*>(0x748CF6), 0x90, 5);
    LP(0x590D7C);
    mem_set(reinterpret_cast<void*>(0x590D7C), 0x90, 5);
    LP(0x590DB3);
    mem_set(reinterpret_cast<void*>(0x590DB3), 0x90, 5);
    LP(0x590D9F);
    std::memcpy(reinterpret_cast<void*>(0x590D9F), "\xC3\x90\x90\x90\x90", 5);
    LP(0x7459E1);
    mem_put<WORD>(0x7459E1, 0x9090);
    LP(0x748054);
    std::memset(reinterpret_cast<void*>(0x748054), 0x90, 10);
    LP(0x00550F82);
    mem_put<int>(0x00550F82, 8000);
    LP(0x8a5a84);
    mem_put<int>(0x8a5a84, 127);
    LP(0x442AD0);
    mem_set(reinterpret_cast<void*>(0x442AD0), 0xC3, 1);
    LP(0x731AB5);
    std::memset(reinterpret_cast<void*>(0x731AB5), 0x90, 4);

    // radio disable
    LP(0x4E9820);
    mem_put<BYTE>(0x4E9820, 0xC2);
    LP(0x4E9821);
    mem_put<BYTE>(0x4E9821, 0x08);
    LP(0x4E9822);
    mem_put<BYTE>(0x4E9822, 0x00);
    LP(0x4DBEC0);
    mem_put<BYTE>(0x4DBEC0, 0xC2);
    LP(0x4DBEC1);
    mem_put<BYTE>(0x4DBEC1, 0x00);
    LP(0x4DBEC2);
    mem_put<BYTE>(0x4DBEC2, 0x00);
    LP(0x4EB3C0);
    mem_put<BYTE>(0x4EB3C0, 0xC2);
    LP(0x4EB3C1);
    mem_put<BYTE>(0x4EB3C1, 0x10);
    LP(0x4EB3C2);
    mem_put<BYTE>(0x4EB3C2, 0x00);

    LP(0x52A535);
    mem_put<BYTE>(0x52A535, 0);
    LP(0x72DF0D);
    mem_put<BYTE>(0x72DF0D, 0xEB);
    LP(0x742685);
    mem_put<BYTE>(0x742685, 0x90);
    LP(0x742686);
    mem_put<BYTE>(0x742686, 0xE9);
    LP(0x7399B0);
    mem_put<BYTE>(0x7399B0, 0xC3);
    LP(0x4629E0);
    mem_put<BYTE>(0x4629E0, 0xC3);
    LP(0x4E8410);
    mem_put<BYTE>(0x4E8410, 0xC3);
    LP(0x614720);
    mem_put<BYTE>(0x614720, 0x32);
    LP(0x614721);
    mem_put<BYTE>(0x614721, 0xC0);
    LP(0x614722);
    mem_put<BYTE>(0x614722, 0xC3);
    LP(0x620607);
    mem_put<unsigned char>(0x620607, 0x33);
    LP(0x620608);
    mem_put<unsigned char>(0x620608, 0xC0);
    LP(0x620618);
    mem_put<unsigned char>(0x620618, 0x33);
    LP(0x620619);
    mem_put<unsigned char>(0x620619, 0xC0);
    LP(0x62061A);
    mem_put<unsigned char>(0x62061A, 0x90);
    LP(0x62061B);
    mem_put<unsigned char>(0x62061B, 0x90);
    LP(0x62061C);
    mem_put<unsigned char>(0x62061C, 0x90);
    LP(0x61EFFE);
    mem_put<BYTE>(0x61EFFE, 0xEB);
    LP(0x441770);
    mem_put<BYTE>(0x441770, 0xC3);
    LP(0x532010);
    mem_put<BYTE>(0x532010, 0xC3);
    LP(0x4C01F0);
    mem_put<BYTE>(0x4C01F0, 0xB0);
    LP(0x4C01F1);
    mem_put<BYTE>(0x4C01F1, 0x00);
    LP(0x4C01F2);
    mem_put<BYTE>(0x4C01F2, 0x90);
    LP(0x4C01F3);
    mem_put<BYTE>(0x4C01F3, 0x90);
    LP(0x4C01F4);
    mem_put<BYTE>(0x4C01F4, 0x90);
    LP(0x5B8342);
    mem_put<BYTE>(0x5B8342 + 0, 0x33);
    LP(0x5B8342);
    mem_put<BYTE>(0x5B8342 + 1, 0xC0);
    LP(0x5B8342);
    mem_put<BYTE>(0x5B8342 + 2, 0xB0);
    LP(0x5B8342);
    mem_put<BYTE>(0x5B8342 + 3, 0xFF);
    LP(0x5B8342);
    mem_put<BYTE>(0x5B8342 + 4, 0x8B);
    LP(0x5B8342);
    mem_put<BYTE>(0x5B8342 + 5, 0xF8);
    LP(0x56E870);
    mem_put<BYTE>(0x56E870, 0xC2);
    LP(0x56E871);
    mem_put<BYTE>(0x56E871, 0x08);
    LP(0x56E872);
    mem_put<BYTE>(0x56E872, 0x00);
    // REMOVED: mem_set(0x4DCF87, 0x90, 6);
    //
    // Copied in from MTA, but MTA itself reverted it — CMultiplayerSA.cpp:899
    // carries it commented out with "mantis#8590, gh#124, see c20d2adc5". The
    // NOP kills `push eax; call FxSystem_c::GetCompositeMatrix` while leaving
    // the preceding `lea eax,[esp+8]` in place, so everything from 0x4DCF8D on
    // consumes an uninitialised matrix buffer off the stack. We inherited dead
    // upstream code as live code. Do not re-enable.
    LP(0x6A436C);
    mem_put<BYTE>(0x6A436C, 0x90);
    LP(0x6A436D);
    mem_put<BYTE>(0x6A436D, 0x90);
    LP(0x621983);
    mem_put<BYTE>(0x621983, 0xEB);
    LP(0x627E01);
    mem_set((LPVOID)0x627E01, 0x90, 6);
    LP(0x62840D);
    mem_set((LPVOID)0x62840D, 0x90, 6);
    LP(0x738F3A);
    mem_set((LPVOID)0x738F3A, 0x90, 83);
    LP(0x6B5B17);
    mem_set((LPVOID)0x6B5B17, 0x90, 6);
    LP(0x460390);
    mem_set(reinterpret_cast<void*>(0x460390), 0xC3, 1);
    LP(0x4600F0);
    mem_set(reinterpret_cast<void*>(0x4600F0), 0xC3, 1);
    LP(0x45F050);
    mem_set(reinterpret_cast<void*>(0x45F050), 0xC3, 1);
    LP(0x439AF0);
    mem_set(reinterpret_cast<void*>(0x439AF0), 0xC3, 1);
    LP(0x438370);
    mem_set(reinterpret_cast<void*>(0x438370), 0xC3, 1);
    LP(0x44AA89);
    mem_put<BYTE>(0x44AA89 + 0, 0xE9);
    LP(0x44AA89);
    mem_put<BYTE>(0x44AA89 + 1, 0x28);
    LP(0x44AA89);
    mem_put<BYTE>(0x44AA89 + 2, 0x01);
    LP(0x44AA89);
    mem_put<BYTE>(0x44AA89 + 3, 0x00);
    LP(0x44AA89);
    mem_put<BYTE>(0x44AA89 + 4, 0x00);
    LP(0x44AA89);
    mem_put<BYTE>(0x44AA89 + 5, 0x90);

    // vtable of something → 0x44C7C4
    LP(0x44C7E0);
    static constexpr DWORD kVtableRedirects[] = {
        0x44C7E0, 0x44C7E4, 0x44C7F8, 0x44C7FC, 0x44C804, 0x44C808,
        0x44C83C, 0x44C840, 0x44C850, 0x44C854, 0x44C864, 0x44C868,
        0x44C874, 0x44C878, 0x44C88C, 0x44C890, 0x44C89C, 0x44C8A0,
        0x44C8AC, 0x44C8B0
    };
    // Gated as one unit by the LP() above — the whole table is a single patch.
    for (DWORD a : kVtableRedirects)
        mem_put<DWORD>(a, 0x44C7C4);

    LP(0x44C39A);
    mem_put<BYTE>(0x44C39A + 0, 0x0F);
    LP(0x44C39A);
    mem_put<BYTE>(0x44C39A + 1, 0x84);
    LP(0x44C39A);
    mem_put<BYTE>(0x44C39A + 2, 0x24);
    LP(0x44C39A);
    mem_put<BYTE>(0x44C39A + 3, 0x04);
    LP(0x44C39A);
    mem_put<BYTE>(0x44C39A + 4, 0x00);
    LP(0x44C39A);
    mem_put<BYTE>(0x44C39A + 5, 0x00);
    LP(0x4486F7);
    mem_set((LPVOID)0x4486F7, 0x90, 4);
    LP(0x55C180);
    mem_put<BYTE>(0x55C180, 0xC3);
    LP(0x559FD5);
    mem_set(reinterpret_cast<void*>(0x559FD5), 0x90, 7);
    LP(0x559FEB);
    mem_set(reinterpret_cast<void*>(0x559FEB), 0x90, 7);
    LP(0x55B980);
    mem_set(reinterpret_cast<void*>(0x55B980), 0xC3, 1);
    LP(0x559760);
    mem_set(reinterpret_cast<void*>(0x559760), 0xC3, 1);
    LP(0x5FBA26);
    mem_put<BYTE>(0x5FBA26, 0xEB);
    LP(0x522423);
    mem_put<BYTE>(0x522423, 0x90);
    LP(0x522424);
    mem_put<BYTE>(0x522424, 0x90);
    LP(0x748A8D);
    mem_set(reinterpret_cast<void*>(0x748A8D), 0x90, 6);
    LP(0x58B0AE);
    mem_put<BYTE>(0x58B0AE, 0x00);
    LP(0x58AD56);
    mem_put<BYTE>(0x58AD56, 0x00);
    LP(0x85953C);
    mem_put<float>(0x85953C, 320.0f);
    LP(0x58B149);
    mem_put<BYTE>(0x58B149, 0x3C);
    LP(0x58AE52);
    mem_put<BYTE>(0x58AE52, 0x3C);
    LP(0x5A07D0);
    mem_put<BYTE>(0x5A07D0, 0xC3);
    LP(0x6F7900);
    mem_put<BYTE>(0x6F7900, 0xC3);
    LP(0x6F7865);
    mem_put<BYTE>(0x6F7865, 0xEB);
    LP(0x6CD2F0);
    mem_put<BYTE>(0x6CD2F0, 0xC3);
    LP(0x42B7D0);
    mem_put<BYTE>(0x42B7D0, 0xC3);
    LP(0x6F3F40);
    mem_put<BYTE>(0x6F3F40, 0xC3);
    LP(0x440D10);
    mem_put<BYTE>(0x440D10, 0xC3);
    LP(0x53BC78);
    mem_put<BYTE>(0x53BC78, 0x00);
    LP(0x56E740);
    mem_set((LPVOID)0x56E740, 0x90, 5);
    LP(0x6B0BC2);
    mem_set((LPVOID)0x6B0BC2, 0xEB, 1);
    LP(0x53C017);
    mem_put<BYTE>(0x53C017, 0x90);
    LP(0x53C018);
    mem_put<BYTE>(0x53C018, 0x90);
    LP(0x6F2089);
    mem_put<BYTE>(0x6F2089, 0x58);
    LP(0x6F208A);
    mem_set(reinterpret_cast<void*>(0x6F208A), 0x90, 4);
    LP(0x5B47B0);
    mem_put<BYTE>(0x5B47B0, 0xC3);
    LP(0x42CD10);
    mem_put<BYTE>(0x42CD10, 0xC3);
    LP(0x5E68A0);
    mem_put<BYTE>(0x5E68A0, 0xEB);
    LP(0x542483);
    mem_put<BYTE>(0x542483, 0xEB);
    LP(0x550FBA);
    mem_put<BYTE>(0x550FBA, 0x00);
    LP(0x550FBB);
    mem_put<BYTE>(0x550FBB, 0x10);
    LP(0x561FA4);
    mem_put<BYTE>(0x561FA4, 0x90);
    LP(0x561FA5);
    mem_put<BYTE>(0x561FA5, 0x90);
    LP(0x53BFF6);
    mem_set(reinterpret_cast<void*>(0x53BFF6), 0x90, 5);
    LP(0x60EBCC);
    mem_set(reinterpret_cast<void*>(0x60EBCC), 0x90, 5);
    LP(0x6D189B);
    mem_put<BYTE>(0x6D189B, 0x06);
    LP(0x591F90);
    mem_put<BYTE>(0x591F90, 0xC3);
    LP(0x4418E0);
    mem_put<BYTE>(0x4418E0, 0xC3);
    LP(0x632140);
    mem_put<BYTE>(0x632140, 0xB0);
    LP(0x632141);
    mem_put<BYTE>(0x632141, 0x01);
    LP(0x632142);
    mem_put<BYTE>(0x632142, 0xC3);
    LP(0x644C18);
    mem_put<BYTE>(0x644C18, 0x90);
    LP(0x644C19);
    mem_put<BYTE>(0x644C19, 0xE9);
    LP(0x5E8E84);
    mem_set(reinterpret_cast<void*>(0x5E8E84), 0x90, 5);
    LP(0x6D29CB);
    mem_set(reinterpret_cast<void*>(0x6D29CB), 0x90, 5);
    LP(0x741FD0);
    mem_set(reinterpret_cast<void*>(0x741FD0), 0x90, 3);
    LP(0x741FD0);
    mem_put<BYTE>(0x741FD0, 0xC3);
    LP(0x6872C0);
    mem_put<BYTE>(0x6872C0, 0xC2);
    LP(0x6872C1);
    mem_put<BYTE>(0x6872C1, 0x04);
    LP(0x6872C2);
    mem_put<BYTE>(0x6872C2, 0x00);
    LP(0x55E870);
    mem_put<DWORD>(0x55E870, 0xC2C03366);
    LP(0x55E874);
    mem_put<WORD>(0x55E874, 0x0004);
    LP(0x59FAA3);
    mem_put<BYTE>(0x59FAA3, 0x00);
    LP(0x6D19CD);
    mem_put<BYTE>(0x6D19CD, 0xEB);
    LP(0x6D1A1A);
    mem_put<BYTE>(0x6D1A1A, 0xEB);
    LP(0x6D1762);
    mem_put<BYTE>(0x6D1762, 0x00);
    LP(0x6F701D);
    mem_set(reinterpret_cast<void*>(0x6F701D), 0x90, 6);
    LP(0x6F7069);
    mem_put<BYTE>(0x6F7069, 0xEB);
    LP(0x73FDF9);
    mem_put<BYTE>(0x73FDF9, 0xEB);
    LP(0x6E1DBC);
    mem_set(reinterpret_cast<void*>(0x6E1DBC), 0x90, 8);
    LP(0x6E1D4F);
    mem_put<BYTE>(0x6E1D4F, 2);
    LP(0x5E1E72);
    mem_put<BYTE>(0x5E1E72, 0xE9);
    LP(0x5E1E73);
    mem_put<BYTE>(0x5E1E73, 0xB9);
    LP(0x5E1E74);
    mem_put<BYTE>(0x5E1E74, 0x00);
    LP(0x5E1E77);
    mem_put<BYTE>(0x5E1E77, 0x90);
    LP(0x6D65C5);
    mem_set(reinterpret_cast<void*>(0x6D65C5), 0x90, 11);
    LP(0x522C80);
    mem_put<BYTE>(0x522C80, 0xC3);
    LP(0x53E9C6);
    mem_set(reinterpret_cast<void*>(0x53E9C6), 0x90, 6);
    LP(0x745BC9);
    mem_put<WORD>(0x745BC9, 0x9090);
    LP(0x58FC3E);
    mem_set(reinterpret_cast<void*>(0x58FC3E), 0x90, 14);
    LP(0x633695);
    mem_set(reinterpret_cast<void*>(0x633695), 0x90, 6);
    LP(0x633720);
    mem_put<BYTE>(0x633720, 0);
    LP(0x53A459);
    mem_put<BYTE>(0x53A459, 0x33);
    LP(0x53A568);
    mem_put<BYTE>(0x53A568, 0x8B);
    LP(0x53A4A9);
    mem_put<BYTE>(0x53A4A9, 0x33);
    LP(0x53A55F);
    mem_put<WORD>(0x53A55F, 0x9090);
    LP(0x73EC06);
    mem_put<BYTE>(0x73EC06, 0x85);
    LP(0x52A2BB);
    mem_put<BYTE>(0x52A2BB, 0);
    LP(0x52A4F8);
    mem_put<BYTE>(0x52A4F8, 0);
    LP(0x685DFB);
    mem_set(reinterpret_cast<void*>(0x685DFB), 0x90, 5);
    LP(0x685DFB);
    mem_put<BYTE>(0x685DFB, 0x33);
    LP(0x685DFC);
    mem_put<BYTE>(0x685DFC, 0xC0);
    LP(0x685C3E);
    mem_set(reinterpret_cast<void*>(0x685C3E), 0x90, 5);
    LP(0x685C3E);
    mem_put<BYTE>(0x685C3E, 0x33);
    LP(0x685C3F);
    mem_put<BYTE>(0x685C3F, 0xC0);
    LP(0x685DC4);
    mem_set(reinterpret_cast<void*>(0x685DC4), 0x90, 5);
    LP(0x685DC4);
    mem_put<BYTE>(0x685DC4, 0x33);
    LP(0x685DC5);
    mem_put<BYTE>(0x685DC5, 0xC0);
    LP(0x685DE6);
    mem_set(reinterpret_cast<void*>(0x685DE6), 0x90, 5);
    LP(0x685DE6);
    mem_put<BYTE>(0x685DE6, 0x33);
    LP(0x685DE7);
    mem_put<BYTE>(0x685DE7, 0xC0);
    LP(0x62E63F);
    mem_set(reinterpret_cast<void*>(0x62E63F), 0x90, 6);
    LP(0x62E63F);
    mem_put<BYTE>(0x62E63F, 0xDD);
    LP(0x62E640);
    mem_put<BYTE>(0x62E640, 0xD8);
    LP(0x62E659);
    mem_set(reinterpret_cast<void*>(0x62E659), 0x90, 6);
    LP(0x62E659);
    mem_put<BYTE>(0x62E659, 0xDD);
    LP(0x62E65A);
    mem_put<BYTE>(0x62E65A, 0xD8);
    LP(0x62E692);
    mem_set(reinterpret_cast<void*>(0x62E692), 0x90, 6);
    LP(0x62E692);
    mem_put<BYTE>(0x62E692, 0xDD);
    LP(0x62E693);
    mem_put<BYTE>(0x62E693, 0xD8);
    LP(0x4F9CCE);
    mem_put<BYTE>(0x4F9CCE, 0xCE);
    LP(0x576CCC);
    mem_put<BYTE>(0x576CCC, 0xEB);
    LP(0x576EBA);
    mem_put<BYTE>(0x576EBA, 0xEB);
    LP(0x576F8A);
    mem_put<BYTE>(0x576F8A, 0xEB);
    LP(0x53E94C);
    mem_put<BYTE>(0x53E94C, 0x00);
    // mem_set ( (void *)0x57BA57, 0x90, 6 ); @todo disable main menu, use own implementation
    LP(0x53C127);
    mem_set(reinterpret_cast<void*>(0x53C127), 0x90, 10);
    LP(0x41AD12);
    mem_set(reinterpret_cast<void*>(0x41AD12), 0x90, 2);
    LP(0x41ADA7);
    mem_set(reinterpret_cast<void*>(0x41ADA7), 0x90, 2);
    LP(0x41ADF3);
    mem_set(reinterpret_cast<void*>(0x41ADF3), 0x90, 2);
    LP(0x5FFAEE);
    mem_set(reinterpret_cast<void*>(0x5FFAEE), 0x90, 2);
    LP(0x5FFB4B);
    mem_set(reinterpret_cast<void*>(0x5FFB4B), 0x90, 2);
    LP(0x5FFBA2);
    mem_set(reinterpret_cast<void*>(0x5FFBA2), 0x90, 5);
    LP(0x5FFC00);
    mem_set(reinterpret_cast<void*>(0x5FFC00), 0x90, 5);
    LP(0x7361BF);
    mem_set(reinterpret_cast<void*>(0x7361BF), 0x90, 6);
    LP(0x7361D4);
    mem_set(reinterpret_cast<void*>(0x7361D4), 0x90, 6);
    LP(0x7361E9);
    mem_set(reinterpret_cast<void*>(0x7361E9), 0x90, 6);
    LP(0x7361FE);
    mem_set(reinterpret_cast<void*>(0x7361FE), 0x90, 6);
    LP(0x6E2FBC);
    mem_set(reinterpret_cast<void*>(0x6E2FBC), 0x90, 2);
    LP(0x6E301C);
    mem_set(reinterpret_cast<void*>(0x6E301C), 0x90, 2);
    LP(0x6E3075);
    mem_set(reinterpret_cast<void*>(0x6E3075), 0x90, 2);
    LP(0x6E30D6);
    mem_set(reinterpret_cast<void*>(0x6E30D6), 0x90, 2);
    LP(0x44C6FA);
    mem_set(reinterpret_cast<void*>(0x44C6FA), 0x90, 4);
    LP(0x40E7DF);
    mem_put<BYTE>(0x40E7DF, 0xEB);
    LP(0x611FC0);
    mem_put<BYTE>(0x611FC0, 0xC3);
    LP(0x616698);
    mem_put<BYTE>(0x616698, 0x5E);
    LP(0x616699);
    mem_put<BYTE>(0x616699, 0xC3);
    LP(0x460500);
    mem_put<BYTE>(0x460500, 0xC3);
    LP(0x605A30);
    mem_put<BYTE>(0x605A30, 0xC3);
    LP(0x446610);
    mem_put<BYTE>(0x446610, 0xC3);
    LP(0x43C590);
    mem_put<BYTE>(0x43C590, 0xC3);
    LP(0x43B0F0);
    mem_put<BYTE>(0x43B0F0, 0xC3);
    LP(0x4322B0);
    mem_put<BYTE>(0x4322B0, 0xC3);
    LP(0x40B650);
    mem_put<BYTE>(0x40B650, 0xC3);
    LP(0x468EB5);
    mem_put<BYTE>(0x468EB5, 0xEB);
    LP(0x468EB6);
    mem_put<BYTE>(0x468EB6, 0x32);
    LP(0x406946);
    std::memcpy(reinterpret_cast<void*>(0x406946), "\x00\x00\x00\x00", 4);
    LP(0x074872D);
    std::memcpy(reinterpret_cast<void*>(0x074872D), "\x90\x90\x90\x90\x90\x90\x90\x90\x90", 9);
    LP(0x56A404);
    mem_set(reinterpret_cast<void*>(0x56A404), 0x90, 0x56A446 - 0x56A404);
    LP(0x53A651);
    mem_set(reinterpret_cast<void*>(0x53A651), 0x90, 0xA);
    LP(0x6D1741);
    mem_set(reinterpret_cast<void*>(0x6D1741), 0x90, 0x6D175F - 0x6D1741);
    LP(0x6E1A22);
    mem_put<BYTE>(0x6E1A22, 0xF0);
    LP(0x6D6517);
    mem_set(reinterpret_cast<void*>(0x6D6517), 0x90, 2);
    LP(0x6D0E43);
    mem_set(reinterpret_cast<void*>(0x6D0E43), 0x90, 2);
    LP(0x63F576);
    mem_put<BYTE>(0x63F576, 0xEB);
    LP(0x5023B2);
    std::memset(reinterpret_cast<void*>(0x5023B2), 0x90, 6);
    LP(0x5023E1);
    std::memset(reinterpret_cast<void*>(0x5023E1), 0x90, 5);
    LP(0x502341);
    std::memset(reinterpret_cast<void*>(0x502341), 0x90, 5);
    LP(0x60D861);
    std::memset(reinterpret_cast<void*>(0x60D861), 0x90, 14);
    LP(0x72925D);
    mem_set(reinterpret_cast<void*>(0x72925D), 0x1, 1);
    LP(0x729263);
    mem_set(reinterpret_cast<void*>(0x729263), 0x1, 1);
    LP(0x6FB9A0);
    mem_put<BYTE>(0x6FB9A0, 0x1C);
    LP(0x58FBC4);
    mem_set(reinterpret_cast<void*>(0x58FBC4), 0x90, 9);
    LP(0x61ECD2);
    mem_set(reinterpret_cast<void*>(0x61ECD2), 0x90, 20);
    //mem_set(reinterpret_cast<void*>(0x705331), 0x90, 0x7053AF - 0x705331); // @todo disable camera photo, use own implementation
    LP(0x7069F5);
    mem_put<BYTE>(0x7069F5, 0xEB);
    LP(0x7069FE);
    mem_put<BYTE>(0x7069FE, 0x08);
    LP(0x70A83B);
    mem_set(reinterpret_cast<void*>(0x70A83B), 0x90, 6);
    LP(0x70A4CB);
    mem_set(reinterpret_cast<void*>(0x70A4CB), 0x90, 6);
    LP(0x524084);
    mem_put<BYTE>(0x524084, 0xFF);
    LP(0x524089);
    mem_put<BYTE>(0x524089, 0xFF);
    LP(0x7225F5);
    mem_set(reinterpret_cast<void*>(0x7225F5), 0x90, 4);
    LP(0x725DDE);
    std::memcpy(reinterpret_cast<void*>(0x725DDE), "\xFF\x76\xB\x90\x90", 5);
    LP(0x60D86F);
    std::memset(reinterpret_cast<void*>(0x60D86F), 0x90, 19);
    LP(0x6E1425);
    mem_put<BYTE>(0x6E1425, 1);
    LP(0x6C444B);
    mem_set(reinterpret_cast<void*>(0x6C444B), 0x90, 6);
    LP(0x6C4453);
    mem_set(reinterpret_cast<void*>(0x6C4453), 0x90, 0x68);
    LP(0x725844);
    std::memcpy(reinterpret_cast<void*>(0x725844), "\xDD\xD8\x90", 3);
    LP(0x725619);
    std::memcpy(reinterpret_cast<void*>(0x725619), "\xDD\xD8\x90", 3);
    LP(0x72565A);
    std::memcpy(reinterpret_cast<void*>(0x72565A), "\xDD\xD8\x90", 3);
    LP(0x7259B0);
    std::memcpy(reinterpret_cast<void*>(0x7259B0), "\xDD\xD8\x90", 3);
    LP(0x7258B8);
    mem_set(reinterpret_cast<void*>(0x7258B8), 0x90, 6);
    LP(0x53A23F);
    std::memcpy(reinterpret_cast<void*>(0x53A23F), "\x33\xC0\x90\x90\x90", 5);
    LP(0x53A00A);
    std::memcpy(reinterpret_cast<void*>(0x53A00A), "\x33\xC0\x90\x90\x90", 5);
    LP(0xBAB318);
    mem_put<BYTE>(0xBAB318, 0);
}

// ---------------- entry point ----------------

void ApplyBaseMemoryPatches()
{
    LoadLegacyPatchLimit();
    ApplyLegacyBasePatches();

    if (std::FILE* f = std::fopen("opensamp_patches.log", "a"))
    {
        std::fprintf(f, "[patches] legacy limit=%d applied=%d last=0x%06X\n",
                     g_legacyLimit, g_legacyApplied,
                     static_cast<unsigned>(g_legacyLast));
        std::fclose(f);
    }

    ApplyPoolLimits();
    ApplyCrashFixes();
    ApplyIntroSkipPatches();
    ApplyMultiplayerPatches();
    ApplyBehaviorPatches();
    ApplyFilesystemPatches();
    RelocatePedsListHack();
    RelocateScanListHack();
}
