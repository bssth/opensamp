#pragma once
//
// addresses.h — central offset map for GTA: San Andreas **US 1.0** (gta_sa.exe).
//
// This is the single source of truth for every hardcoded game address OpenSAMP
// touches. Do NOT scatter raw `0x........` literals through the codebase; add a
// named constant here and reference it.
//
// Attribution legend (see docs/offsets.md for the full table):
//   [SAMP]  — address the SA-MP client is known to use, as attested in public
//             SA-MP reverse-engineering references.
//   [MTA]   — matched in MTA:SA (mtasa-blue) named offset headers.
//   [SP]    — SilentPatch (SilentPatchSA) upstream.
//   [OWN]   — OpenSAMP's own reverse engineering; not yet cross-verified.
// Confidence is High unless marked (Med)/(Low).
//
// Scope note: the ~200 opaque "legacy" byte patches still living inline in
// patches.cpp (wholesale-imported NOP/RET anti-feature patches with no canonical
// symbol) are intentionally NOT enumerated here yet. See docs/offsets.md →
// "Legacy patch block" for tracking.
//
#include <cstdint>

namespace opensamp::addr
{
    using addr_t = std::uintptr_t;

    // ------------------------------------------------------------------
    // Game state & system
    // ------------------------------------------------------------------
    inline constexpr addr_t kGameState         = 0xC8D4C0; // gGameState / SystemState           [SAMP ADDR_ENTRY] [MTA gGameState]
    inline constexpr addr_t kGameStartedFlag   = 0xBA6831; // gbGameStarted                      [SAMP ADDR_GAME_STARTED]
    inline constexpr addr_t kMenuFlag          = 0xBA67A4; // "at menu" flag (cleared on start)  [OWN]
    inline constexpr addr_t kStartGameFlag     = 0xBA677B; // "start game" flag                  [OWN]
    inline constexpr addr_t kIsGameLoaded      = 0x86969C; // game-loaded BYTE                   [OWN]
    inline constexpr addr_t kGamePaused        = 0xB7CB49; // paused BYTE                        [OWN]
    inline constexpr addr_t kMenuOpenFlag      = 0xB6B964; // menu-open BYTE (is_menu_open)      [OWN]
    inline constexpr addr_t kFps               = 0xB7CB50; // float FPS                          [OWN]
    inline constexpr addr_t kTimeStep          = 0xB7CB5C; // float CTimer::ms_fTimeStep         [OWN]
    inline constexpr addr_t kTimeScale         = 0xB7CB64; // float CTimer::ms_fTimeScale        [OWN]
    inline constexpr addr_t kFramelimiter      = 0xC1704C; // DWORD framelimiter                 [OWN]

    // ------------------------------------------------------------------
    // Direct3D
    // ------------------------------------------------------------------
    inline constexpr addr_t kD3DDevicePtr      = 0xC97C28; // IDirect3DDevice9**                 [SAMP ADDR_ID3D9DEVICE]

    // ------------------------------------------------------------------
    // Local player
    // ------------------------------------------------------------------
    inline constexpr addr_t kLocalPlayerPedPtr = 0xB6F5F0; // CPed** local player ped            [SAMP] (Med)
    inline constexpr addr_t kPlayerPlaceablePtr= 0xB7CD98; // CPlaceable** local player          [OWN]

    // ------------------------------------------------------------------
    // Entity pool pointers  (CPool<T>*)
    // ------------------------------------------------------------------
    inline constexpr addr_t kPedPoolPtr        = 0xB74490; // CPool<CPed>*                       [MTA CLASS_CPedPool] [SAMP ADDR_PED_TABLE]
    inline constexpr addr_t kVehiclePoolPtr    = 0xB74494; // CPool<CVehicle>*                   [MTA] [SAMP ADDR_VEHICLE_TABLE]
    inline constexpr addr_t kBuildingPoolPtr   = 0xB74498; // CPool<CBuilding>*                  [MTA]
    inline constexpr addr_t kObjectPoolPtr     = 0xB7449C; // CPool<CObject>*                    [MTA]
    inline constexpr addr_t kDummyPoolPtr      = 0xB744A0; // CPool<CDummy>*                     [MTA]

    // ------------------------------------------------------------------
    // Pool-size / streaming-budget PATCH SITES (immediate-operand rewrites
    // inside CPools::Initialise / CStreaming). Patched at boot.        [SAMP][MTA]
    // ------------------------------------------------------------------
    inline constexpr addr_t kPatchVehiclePoolSize   = 0x551024; // -> 710
    inline constexpr addr_t kPatchPedPoolSize       = 0x550FF2; // -> 210
    inline constexpr addr_t kPatchPedIntelPoolSize  = 0x551283; // -> 210
    inline constexpr addr_t kPatchTaskPoolSize      = 0x551140; //
    inline constexpr addr_t kPatchEventPoolSize     = 0x551178; // -> 456
    inline constexpr addr_t kPatchBuildingPoolSize  = 0x551060; // -> 17096
    inline constexpr addr_t kPatchColModelPoolSize  = 0x551106; //
    inline constexpr addr_t kPatchMatrixPoolSize    = 0x54F3A2; // -> 4228
    inline constexpr addr_t kPatchStreamingBudget   = 0x5B8E6A; // CStreaming::ms_memoryAvailable -> 128 MB
    inline constexpr addr_t kPatchVehicleStructCount= 0x5B8FDE; // VehicleStruct count -> 127

    // ------------------------------------------------------------------
    // Streaming / model loading  (functions we call)
    // ------------------------------------------------------------------
    inline constexpr addr_t kRequestModel           = 0x4087E0; // CStreaming::RequestModel(int,int)        [MTA][SAMP]
    inline constexpr addr_t kRequestSpecialModel    = 0x409D10; // CStreaming::RequestSpecialModel          [OWN]
    inline constexpr addr_t kLoadAllRequestedModels = 0x40EA10; // CStreaming::LoadAllRequestedModels(int)  [MTA][SAMP]
    inline constexpr addr_t kHasModelLoaded         = 0x4044C0; // CStreaming::HasModelLoaded(int)          [OWN]
    inline constexpr addr_t kLoadScene              = 0x40EB70; // CStreaming::LoadScene(CVector*)          [OWN]
    inline constexpr addr_t kRequestObjectsInDir    = 0x555CB0; // CRenderer::RequestObjectsInDirection     [OWN]
    inline constexpr addr_t kSetupPlayerPed         = 0x60D790; // CPlayerPed::SetupPlayerPed(int)          [MTA]

    // ------------------------------------------------------------------
    // Ped functions  (__thiscall on CPed unless noted)
    // ------------------------------------------------------------------
    inline constexpr addr_t kPed_SetModelIndex      = 0x5E4880; // CPed::SetModelIndex(int)                 [MTA FUNC_SetModelIndex]
    inline constexpr addr_t kPed_SetCurrentWeapon   = 0x5E61F0; // CPed::SetCurrentWeapon(int)              [MTA]
    inline constexpr addr_t kPed_GiveWeapon         = 0x5E6080; // CPed::GiveWeapon(int,int,bool)           [MTA][SAMP]
    inline constexpr addr_t kPed_ClearWeapons       = 0x4FF740; // CPed::ClearWeapons()                     [OWN]
    inline constexpr addr_t kAEPedSound_SetPed      = 0x4E68D0; // CAEPedSound::SetPed(CPed*)               [MTA][SAMP]
    inline constexpr addr_t kTask_DestroyVdtor      = 0x639330; // CTask IK-slot vdtor                      [OWN]
    inline constexpr addr_t kPlaceable_SetPosition  = 0x420B80; // CPlaceable::SetPosn(f,f,f)               [OWN]

    // ------------------------------------------------------------------
    // World / area
    // ------------------------------------------------------------------
    inline constexpr addr_t kWorld_Add              = 0x563220; // CWorld::Add(CEntity*)                    [OWN]
    inline constexpr addr_t kWorld_Remove           = 0x563280; // CWorld::Remove(CEntity*)                 [OWN]
    inline constexpr addr_t kRemoveBuildingsNotInArea = 0x4094B0; // CWorld::RemoveBuildingsNotInArea(int)  [OWN]
    inline constexpr addr_t kCurrentArea            = 0xB72914; // DWORD current world area/interior        [OWN]
    inline constexpr addr_t kFindGroundZForCoord    = 0x569660; // CWorld::FindGroundZForCoord(x,y)         [OWN]
    inline constexpr addr_t kFindGroundZFor3DCoord  = 0x5696C0; // CWorld::FindGroundZFor3DCoord(x,y,z)     [OWN]

    // ------------------------------------------------------------------
    // Timers
    // ------------------------------------------------------------------
    inline constexpr addr_t kTimer_Stop             = 0x561AA0; // CTimer::Stop()                           [OWN]
    inline constexpr addr_t kTimer_Update           = 0x561B10; // CTimer::Update()                         [OWN]

    // ------------------------------------------------------------------
    // Camera
    // ------------------------------------------------------------------
    inline constexpr addr_t kTheCamera              = 0xB6F028; // CCamera TheCamera (object base)          [MTA CLASS_CCamera]
    inline constexpr addr_t kCam_TakeControl        = 0x50C7C0; // CCamera::TakeControl(CEntity*,...)       [OWN]
    inline constexpr addr_t kCam_TakeControlNoEntity= 0x50C8B0; // CCamera::TakeControlNoEntity(CVector*,..)[OWN]
    inline constexpr addr_t kCam_PointCamera        = 0x50AD60; // CCamera::PointCamera (name unconfirmed)  [MTA] (Low)
    inline constexpr addr_t kCam_Restore_A          = 0x50BD40; // CCamera::Restore step 1                  [OWN]
    inline constexpr addr_t kCam_Restore_B          = 0x50BAB0; // CCamera::Restore step 2                  [OWN]
    // NOTE: SAMP's ADDR_CAMERA (0xB6F99C) is a *field inside* TheCamera, NOT the
    //       object base above. Do not conflate them.

    // ------------------------------------------------------------------
    // Weather / time / gravity / physics globals
    // ------------------------------------------------------------------
    inline constexpr addr_t kTimeMinutes            = 0xB70152; // BYTE CClock minutes                      [OWN]
    inline constexpr addr_t kTimeHours              = 0xB70153; // BYTE CClock hours                        [OWN]
    inline constexpr addr_t kClockSpeed             = 0xB7014C; // CClock speed region (ToggleClock @todo)  [OWN] (Med)
    // CWeather. All three are SHORTs, not bytes — CWeather::Update reads them
    // with `mov ax, word [..]`. Naming and widths cross-checked against MTA's
    // CWeatherSA (Client/game_sa/CWeatherSA.h, GPL-3.0).             [MTA] (High)
    inline constexpr addr_t kWeatherForced          = 0xC81318; // short ForcedWeatherType; 0xFFFF = "not forced"
    inline constexpr addr_t kWeatherOld             = 0xC81320; // short OldWeatherType (blend source)
    inline constexpr addr_t kWeatherNew             = 0xC8131C; // short NewWeatherType (blend target)
    inline constexpr addr_t kGravity                = 0x863984; // float CGame gravity                      [OWN]

    // ------------------------------------------------------------------
    // CTimeCycle::m_CurrentColours — the single CColourSet (0xAC bytes) that
    // the renderer actually reads. Rebuilt every frame by the timecycle update
    // called from CGame::Process at 0x53C0DA.
    //
    // Base and field offsets verified three independent ways: plugin-sdk's
    // CColourSet layout, the absolute addresses MTA pokes in
    // CMultiplayerSA.cpp (DoEndWorldColorsPokes, and its far-clip/fog
    // accessors, GPL-3.0), and disassembly of the shipped gta_sa.exe — where
    // Idle at 0x53EA41 loads exactly these six words and passes them to
    // DoRWStuffStartOfFrame (0x53D7A0) -> CClouds::RenderSkyPolys (0x714650).
    //                                                              [MTA][SP] (High)
    //
    // Note the sky-bottom colour is ALSO the world fog colour (set by the
    // render-state setup at 0x734650), which is why one bad pair of words
    // produces both a flat sky and a washed-out horizon.
    inline constexpr addr_t kCurrentColours         = 0xB7C4A0; // CColourSet m_CurrentColours
    inline constexpr addr_t kSkyTopRed              = 0xB7C4C4; // u16  (+0x24)
    inline constexpr addr_t kSkyTopGreen            = 0xB7C4C6; // u16
    inline constexpr addr_t kSkyTopBlue             = 0xB7C4C8; // u16
    inline constexpr addr_t kSkyBottomRed           = 0xB7C4CA; // u16  (+0x2A)
    inline constexpr addr_t kSkyBottomGreen         = 0xB7C4CC; // u16
    inline constexpr addr_t kSkyBottomBlue          = 0xB7C4CE; // u16
    inline constexpr addr_t kFarClip                = 0xB7C4F0; // float (+0x50) -> RwCameraSetFarClipPlane
    inline constexpr addr_t kFogStart               = 0xB7C4F4; // float (+0x54)
    inline constexpr addr_t kWaterRed               = 0xB7C508; // float (+0x68)
    inline constexpr addr_t kWaterGreen             = 0xB7C50C; // float (+0x6C)
    inline constexpr addr_t kWaterBlue              = 0xB7C510; // float (+0x70)
    inline constexpr addr_t kWaterAlpha             = 0xB7C514; // float (+0x74)

    // Extra-colour (interior tint) override. A stuck-on extra colour is the
    // classic cause of a uniform wash over everything.                [SP] (Med)
    // Global draw-distance multiplier applied to every
    // CBaseModelInfo::fLodDistanceUnscaled. Lives in BSS with an initial 0.0
    // and has no absolute-addressed writer anywhere in the image, so if our
    // bootstrap skips whatever normally sets it, every model's effective draw
    // distance is zero and only LOD proxies ever render.        [MTA] (Med)
    inline constexpr addr_t kLodDistanceMultiplier  = 0xB6F118; // float

    inline constexpr addr_t kExtraColourOn          = 0xB7C484; // u32 m_bExtraColourOn
    inline constexpr addr_t kExtraColour            = 0xB79E44; // int m_ExtraColour
    inline constexpr addr_t kStopExtraColourFn      = 0x55FF20; // CTimeCycle::StopExtraColour

    // ------------------------------------------------------------------
    // Explosions / SCM scripting
    // ------------------------------------------------------------------
    inline constexpr addr_t kAddExplosion           = 0x736A50; // CExplosion::AddExplosion(...)            [MTA]
    inline constexpr addr_t kProcessOneCommand      = 0x469EB0; // CRunningScript::ProcessOneCommand        [SAMP]
    inline constexpr addr_t kPlayLoadedSound        = 0x506EA0; // CAudioEngine play-sound (placeholder, unused) [OWN] (Low)

    // ------------------------------------------------------------------
    // Boot hooks / visibility plugins
    // ------------------------------------------------------------------
    inline constexpr addr_t kHook_CGame_Process     = 0x53C095; // CGame::Process call-site (boot hook)     [MTA HOOKPOS_CGame_Process]
    inline constexpr addr_t kHook_CRunningScript_Process = 0x469F00; // CRunningScript::Process (boot anchor)[MTA]
    inline constexpr addr_t kVisPlugins_SetClumpAlpha = 0x732B00; // CVisibilityPlugins::SetClumpAlpha      [MTA]
    inline constexpr addr_t kVisPlugins_Helper      = 0x732B20; // CVisibilityPlugins sibling helper        [OWN] (Low)
    inline constexpr addr_t kVisPlugins_ClumpTable  = 0x8D6094; // CVisibilityPlugins clump dispatch table  [OWN] (Low)
    inline constexpr addr_t kPedPoolInitFn          = 0x732E30; // ped-pool allocator (CGame::Initialise)   [OWN] (Low)

    // ------------------------------------------------------------------
    // Named crash-fix / behaviour PATCH SITES (rewritten at boot)
    // ------------------------------------------------------------------
    inline constexpr addr_t kPatch_ProcessControlCrash = 0x609C08; // CPlayerPed::ProcessControl NOP 39       [SAMP] (currently disabled — breaks movement)
    inline constexpr addr_t kPatch_PlayerPedCtorTask   = 0x60D64E; // CPlayerPed::CPlayerPed jnz->jz          [SAMP][MTA]
    inline constexpr addr_t kPatch_ProcessPedsPreRender= 0x53EA08; // CWorld::ProcessPedsAfterPreRender       [MTA] (effect: ped shadow)
    inline constexpr addr_t kPatch_CPhysicalDtor       = 0x542485; // CPhysical::~CPhysical NOP 11            [SAMP]
    inline constexpr addr_t kPatch_CarGenerator        = 0x434272; // SomeCarGenerator NOP 5                  [SAMP]
    inline constexpr addr_t kPatch_540040Crash         = 0x540040; // "540040 bug" test ecx,ecx              [SAMP]
    inline constexpr addr_t kPatch_DrownInVehicle      = 0x4BC6C1; // drown-in-vehicle crash fix             [SAMP]
    inline constexpr addr_t kPatch_SilentPatchHeap     = 0x4A9D50; // SilentPatch heap-corruption fn -> RET  [SP] (Low)
    inline constexpr addr_t kPatch_SilentPatchMirrors  = 0x7271CB; // SilentPatch mirrors AV fix             [SP] (Low)
    inline constexpr addr_t kClothes_RebuildPlayer     = 0x5A82C0; // CClothes::RebuildPlayer (NOP->RET)     [MTA] (SAMP alias: CPlayerPed::UpdateAfterPhysicalChange)

    // ------------------------------------------------------------------
    // Behaviour-null patch sites (call-sites nulled from CGame::Process etc.)
    // ------------------------------------------------------------------
    inline constexpr addr_t kPatch_AddPed              = 0x612710; // CPopulation::AddPed nulled              [SAMP]
    inline constexpr addr_t kPatch_GenerateRandomCars  = 0x53C1C1; // CCarCtrl::GenerateRandomCars nulled     [SAMP]
    inline constexpr addr_t kPatch_FindPlayerVehicle   = 0x56E0FA; // FindPlayerVehicle -> always nPlayerPed  [SAMP]

    // ------------------------------------------------------------------
    // Intro / startup patches & .rdata strings
    // ------------------------------------------------------------------
    inline constexpr addr_t kPatch_IntroLogoBypass     = 0x747483; // intro/logo video bypass (US 1.0)       [SAMP ADDR_BYPASS_VIDS_USA10]
    inline constexpr addr_t kStr_IntroMovieNameA       = 0x866CD8; // .rdata intro movie filename A           [SAMP]
    inline constexpr addr_t kStr_IntroMovieNameB       = 0x866CCC; // .rdata intro movie filename B           [SAMP]
    inline constexpr addr_t kStr_UserFilesDir          = 0x849AB4; // "GTA San Andreas User Files" .rdata     [OWN]

} // namespace opensamp::addr
