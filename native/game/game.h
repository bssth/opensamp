#pragma once

// Thin C++ wrappers over gta_sa.exe (US 1.0) in-memory layout and functions.
// Offsets/addresses are facts from the known binary; code is our own.
//
// Convention: *Interface structs model memory layout so we can cast a raw
// pointer (e.g. *0xB6F5F0 → local player ped) to the right type and operate
// on it with normal C++. Methods that call into gta_sa do so through typed
// function-pointer thunks with the correct calling convention.

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

#include "../addresses.h"
#include "../gta/common.h"
#include "scm.h"

namespace gta::sa
{
    // Typed-usage aliases over the central offset map (native/addresses.h).
    // addresses.h is the single source of truth; these names exist so call
    // sites can keep using the FUNC_*/VAR_*/CLASS_* convention. Field offsets
    // (OFFS_*) are struct member offsets, NOT addresses, so they stay literal.
    namespace offsets
    {
        namespace addr = ::opensamp::addr;

        // system / CGame
        inline constexpr std::uintptr_t VAR_SystemState = addr::kGameState;
        inline constexpr std::uintptr_t VAR_IsAtMenu = addr::kStartGameFlag; // 0xBA677B — sa_state.h names this ADDR_STARTGAME
        inline constexpr std::uintptr_t VAR_IsGameLoaded = addr::kIsGameLoaded;
        inline constexpr std::uintptr_t VAR_GamePaused = addr::kGamePaused;
        inline constexpr std::uintptr_t VAR_FPS = addr::kFps;
        inline constexpr std::uintptr_t VAR_TimeStep = addr::kTimeStep;
        inline constexpr std::uintptr_t VAR_TimeScale = addr::kTimeScale;
        inline constexpr std::uintptr_t VAR_Framelimiter = addr::kFramelimiter;
        inline constexpr std::uintptr_t VAR_D3DDevice = addr::kD3DDevicePtr;

        // local player
        inline constexpr std::uintptr_t VAR_LocalPlayerPed = addr::kLocalPlayerPedPtr;
        inline constexpr std::uintptr_t VAR_PlayerPlaceable = addr::kPlayerPlaceablePtr;

        // pools (CPool<T>*)
        inline constexpr std::uintptr_t CLASS_CPedPool = addr::kPedPoolPtr;
        inline constexpr std::uintptr_t CLASS_CVehiclePool = addr::kVehiclePoolPtr;
        inline constexpr std::uintptr_t CLASS_CBuildingPool = addr::kBuildingPoolPtr;
        inline constexpr std::uintptr_t CLASS_CObjectPool = addr::kObjectPoolPtr;
        inline constexpr std::uintptr_t CLASS_CDummyPool = addr::kDummyPoolPtr;

        // streaming / bootstrap
        inline constexpr std::uintptr_t FUNC_RequestModel = addr::kRequestModel;
        inline constexpr std::uintptr_t FUNC_RequestSpecialModel = addr::kRequestSpecialModel;
        inline constexpr std::uintptr_t FUNC_LoadAllRequestedModels = addr::kLoadAllRequestedModels;
        inline constexpr std::uintptr_t FUNC_SetupPlayerPed = addr::kSetupPlayerPed;
        inline constexpr std::uintptr_t FUNC_HasModelLoaded = addr::kHasModelLoaded;

        // CPlaceable
        inline constexpr std::uintptr_t FUNC_Placeable_SetPosition = addr::kPlaceable_SetPosition;

        // CPed
        inline constexpr std::uintptr_t FUNC_Ped_SetModelIndex = addr::kPed_SetModelIndex;
        inline constexpr std::uintptr_t FUNC_Ped_SetCurrentWeapon = addr::kPed_SetCurrentWeapon;
        inline constexpr std::uintptr_t FUNC_Ped_GiveWeapon = addr::kPed_GiveWeapon;

        // Player ped model swap helpers (touched only by SafeSetPlayerPedModel)
        inline constexpr std::uintptr_t FUNC_CAEPedSound_SetPed = addr::kAEPedSound_SetPed;
        inline constexpr std::uintptr_t FUNC_CTask_DestroyVdtor = addr::kTask_DestroyVdtor;
        inline constexpr std::uintptr_t FUNC_CClothes_RebuildPlayer = addr::kClothes_RebuildPlayer;
        inline constexpr std::size_t     OFFS_CPed_pTasks       = 0x47C;   // CPed::m_pTasks (PED_TASKS*)
        inline constexpr std::size_t     OFFS_PedTasks_pIK      = 0x2C;   // PED_TASKS::m_pIK (last task slot)
        inline constexpr std::size_t     OFFS_CPed_AudioSound   = 0x294;  // CAEPedSound sub-object inside CPed

        // CWorld
        inline constexpr std::uintptr_t FUNC_World_Add = addr::kWorld_Add;
        inline constexpr std::uintptr_t FUNC_World_Remove = addr::kWorld_Remove;
        inline constexpr std::uintptr_t VAR_CurrentArea  = addr::kCurrentArea;
        inline constexpr std::uintptr_t FUNC_RemoveBuildingsNotInArea = addr::kRemoveBuildingsNotInArea;

        // CClock
        inline constexpr std::uintptr_t VAR_TimeMinutes  = addr::kTimeMinutes;
        inline constexpr std::uintptr_t VAR_TimeHours    = addr::kTimeHours;

        // CWeather
        inline constexpr std::uintptr_t VAR_WeatherForced    = addr::kWeatherForced;
        inline constexpr std::uintptr_t VAR_WeatherOld       = addr::kWeatherOld;
        inline constexpr std::uintptr_t VAR_WeatherNew       = addr::kWeatherNew;

        // CGame::Gravity (single global float — m_pGame->SetGravity writes here)
        inline constexpr std::uintptr_t VAR_Gravity = addr::kGravity;

        // CCamera helpers
        inline constexpr std::uintptr_t FUNC_Cam_TakeControl         = addr::kCam_TakeControl;
        inline constexpr std::uintptr_t FUNC_Cam_TakeControlNoEntity = addr::kCam_TakeControlNoEntity;
        inline constexpr std::uintptr_t FUNC_Cam_PointCamera         = addr::kCam_PointCamera;
        inline constexpr std::uintptr_t FUNC_Cam_Restore_A           = addr::kCam_Restore_A;
        inline constexpr std::uintptr_t FUNC_Cam_Restore_B           = addr::kCam_Restore_B;
        inline constexpr std::uintptr_t VAR_TheCamera                = addr::kTheCamera;

        // CWorld z-finder
        inline constexpr std::uintptr_t FUNC_FindGroundZForCoord   = addr::kFindGroundZForCoord;
        inline constexpr std::uintptr_t FUNC_FindGroundZFor3DCoord = addr::kFindGroundZFor3DCoord;
        inline constexpr std::uintptr_t FUNC_CStreaming_LoadScene  = addr::kLoadScene;
        inline constexpr std::uintptr_t FUNC_CRenderer_RequestObjectsInDirection = addr::kRequestObjectsInDir;
        inline constexpr std::uintptr_t FUNC_CTimer_Stop           = addr::kTimer_Stop;
        inline constexpr std::uintptr_t FUNC_CTimer_Update         = addr::kTimer_Update;

        // Weapon helpers
        inline constexpr std::uintptr_t FUNC_Ped_ClearWeapons      = addr::kPed_ClearWeapons;

        // CExplosion
        inline constexpr std::uintptr_t FUNC_AddExplosion = addr::kAddExplosion;

        // Audio (CAudioEngine, simplified helpers)
        inline constexpr std::uintptr_t FUNC_PlayLoadedSound = addr::kPlayLoadedSound;
    } // namespace offsets

    using Vec3 = vec3;

    // CEntitySAInterface — base in-memory layout.
    //
    // Transcribed from MTA:SA, Client/game_sa/CEntitySA.h and CPlaceableSA.h
    // (GPL-3.0, same licence as OpenSAMP).
    //
    // This struct was previously WRONG from +0x1C onward: it declared three
    // dwords of entity flags where GTA SA has one, shifting every later field
    // 8 bytes too high and putting m_modelIndex at +0x2A instead of +0x22.
    // Three independent confirmations of the correct layout:
    //   * MTA's own field comments;
    //   * the retail binary — _LinkLods at 0x5B52F8 nulls the LOD pointer via
    //     [esi+0x30], matching m_pLod here and not the old +0x38;
    //   * a scan of .text for 16-bit accesses: 309 sites read [reg+0x22],
    //     2 read [reg+0x2A].
    //
    // The damage this did: writing `ped->m_modelIndex = id` actually wrote the
    // skin id into the high half of m_pLastRenderedLink — never setting the
    // model index and corrupting a render-list pointer — and every read of it,
    // including the diag dump and the "is this CJ" gate, tested that pointer's
    // high half instead of a model id.
    struct CEntityInterface
    {
        void* vtbl;          // +0x00
        // CPlaceable: inline position/heading, then the matrix pointer.
        Vec3  m_position;    // +0x04  m_transform.m_translate
        float m_heading;     // +0x10
        void* m_matrix;      // +0x14  CMatrix*
        void* m_rwObject;    // +0x18  RpClump*

        // +0x1C — one dword of entity flags. Named because several matter:
        // bIsVisible gates rendering, and bDontCastShadowsOn is directly
        // relevant to the real-time shadow work.
        std::uint32_t bUsesCollision : 1;
        std::uint32_t bCollisionProcessed : 1;
        std::uint32_t bIsStatic : 1;
        std::uint32_t bHasContacted : 1;
        std::uint32_t bIsStuck : 1;
        std::uint32_t bIsInSafePosition : 1;
        std::uint32_t bWasPostponed : 1;
        std::uint32_t bIsVisible : 1;
        std::uint32_t bIsBIGBuilding : 1;
        std::uint32_t bRenderDamaged : 1;
        std::uint32_t bStreamingDontDelete : 1;
        std::uint32_t bRemoveFromWorld : 1;
        std::uint32_t bHasHitWall : 1;
        std::uint32_t bImBeingRendered : 1;
        std::uint32_t bDrawLast : 1;
        std::uint32_t bDistanceFade : 1;
        std::uint32_t bDontCastShadowsOn : 1;
        std::uint32_t bOffscreen : 1;
        std::uint32_t bIsStaticWaitingForCollision : 1;
        std::uint32_t bDontStream : 1;
        std::uint32_t bUnderwater : 1;
        std::uint32_t bHasPreRenderEffects : 1;
        std::uint32_t bIsTempBuilding : 1;
        std::uint32_t bDontUpdateHierarchy : 1;
        std::uint32_t bHasRoadsignText : 1;
        std::uint32_t bDisplayedSuperLowLOD : 1;
        std::uint32_t bIsProcObject : 1;
        std::uint32_t bBackfaceCulled : 1;
        std::uint32_t bLightObject : 1;
        std::uint32_t bUnimportantStream : 1;
        std::uint32_t bTunnel : 1;
        std::uint32_t bTunnelTransition : 1;

        std::uint16_t m_randomSeed;          // +0x20
        std::uint16_t m_modelIndex;          // +0x22
        void*         m_pReferences;         // +0x24
        void*         m_pLastRenderedLink;   // +0x28
        std::uint16_t m_scanCode;            // +0x2C
        std::uint8_t  m_iplIndex;            // +0x2E
        std::uint8_t  m_areaCode;            // +0x2F
        void*         m_pLod;                // +0x30
        std::uint8_t  m_numLodChildren;      // +0x34
        std::int8_t   m_numLodChildrenRendered; // +0x35
        std::uint8_t  m_typeStatus;          // +0x36  low 3 bits type, high 5 status
        std::uint8_t  _pad_37;               // +0x37
    };

    static_assert(sizeof(CEntityInterface) == 0x38, "CEntityInterface layout mismatch");
    static_assert(offsetof(CEntityInterface, m_modelIndex) == 0x22, "m_modelIndex offset");
    static_assert(offsetof(CEntityInterface, m_pLod) == 0x30, "m_pLod offset");

    // CVehicleSAInterface — shares the CEntity/CPlaceable base layout (world
    // matrix at +0x14, inline translate at +0x04). Vehicle-specific fields are
    // addressed by absolute byte offset. Offsets verified for GTA SA US 1.0
    // against SA-MP (saco/game/common.h) and mtasa-blue (CPhysicalSA/CVehicleSA);
    // see docs/offsets.md.
    struct CVehicleInterface : CEntityInterface
    {
        // CPhysical::m_vecMoveSpeed — linear velocity (world units / frame).
        Vec3& MoveSpeed() { return field<Vec3>(0x44); }
        // CVehicle::m_fHealth — 1000.0 == full.
        float& Health()   { return field<float>(0x4C0); }
        // CVehicle::pDriver — ped in the driver seat (null if none).
        void*  Driver()   { return field<void*>(0x460); }

        // Build a normalised quaternion (w,x,y,z) from the vehicle's world
        // matrix basis vectors (right / up / at). Falls back to identity when
        // the matrix is not yet allocated (sleeping/un-streamed vehicle).
        // NOTE: the GTA matrix is column-basis right/up/at/pos at +0x00/+0x10/
        // +0x20/+0x30; handedness/sign convention needs in-game confirmation.
        void GetQuaternion(float& w, float& x, float& y, float& z)
        {
            const auto* m = static_cast<const float*>(m_matrix);
            if (!m) { w = 1.0f; x = y = z = 0.0f; return; }

            // columns: right=m[0..2], up=m[4..6], at=m[8..10]
            const float rx = m[0], ry = m[1], rz = m[2];
            const float ux = m[4], uy = m[5], uz = m[6];
            const float ax = m[8], ay = m[9], az = m[10];

            // clamp-to-zero helper (avoid std::max — windows.h defines a `max` macro)
            const auto nn = [](float v) { return v < 0.0f ? 0.0f : v; };
            w = std::sqrt(nn(1.0f + rx + uy + az)) * 0.5f;
            x = std::sqrt(nn(1.0f + rx - uy - az)) * 0.5f;
            y = std::sqrt(nn(1.0f - rx + uy - az)) * 0.5f;
            z = std::sqrt(nn(1.0f - rx - uy + az)) * 0.5f;
            x = std::copysign(x, uz - ay);
            y = std::copysign(y, ax - rz);
            z = std::copysign(z, ry - ux);
        }

    private:
        template <class T>
        T& field(std::size_t byte_offset)
        {
            return *reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(this) + byte_offset);
        }
    };

    // CPhysicalSAInterface is large; skip modelling and leave as opaque bytes
    // until we need specific fields. CPed extends CPhysical starting somewhere
    // around offset 0x48; we expose only the ped-specific fields we use,
    // addressed by absolute byte offset from the ped start.
    struct CPedInterface : CEntityInterface
    {
        // Opaque CPhysical + CPed header. We touch fields through typed helpers.

        void SetModelIndex(int model_id)
        {
            using fn_t = void(__thiscall*)(void*, int);
            reinterpret_cast<fn_t>(offsets::FUNC_Ped_SetModelIndex)(this, model_id);
        }

        void SetCurrentWeapon(int weapon_slot)
        {
            using fn_t = void(__thiscall*)(void*, int);
            reinterpret_cast<fn_t>(offsets::FUNC_Ped_SetCurrentWeapon)(this, weapon_slot);
        }

        void GiveWeapon(int weapon_id, int ammo, bool set_as_current = true)
        {
            using fn_t = void(__thiscall*)(void*, int, int, bool);
            reinterpret_cast<fn_t>(offsets::FUNC_Ped_GiveWeapon)(this, weapon_id, ammo, set_as_current);
        }

        // Drop every weapon currently held. Mirrors `CPed::ClearWeapons` in
        // gta_sa.exe; address from mod_s0beit_sa.
        void ClearWeapons()
        {
            using fn_t = void(__thiscall*)(void*);
            reinterpret_cast<fn_t>(offsets::FUNC_Ped_ClearWeapons)(this);
        }

        void SetPosition(float x, float y, float z)
        {
            using fn_t = void(__thiscall*)(void*, float, float, float);
            reinterpret_cast<fn_t>(offsets::FUNC_Placeable_SetPosition)(this, x, y, z);
        }

        void SetPosition(const Vec3& p) { SetPosition(p.x, p.y, p.z); }

        // Snap yaw to `radians`. Writes both current and target so the
        // game's rotate-towards logic does not smooth back to the old angle.
        void SetHeading(float radians)
        {
            CurrentRotation() = radians;
            TargetRotation()  = radians;
        }

        // Field accessors by absolute offset. Values verified against
        // mod_s0beit_sa game_sa headers.
        float& Health() { return field<float>(0x540); }
        float& Armour() { return field<float>(0x548); }
        float& CurrentRotation() { return field<float>(0x558); }
        float& TargetRotation() { return field<float>(0x55C); }
        std::uint8_t& CurrentWeaponSlot() { return field<std::uint8_t>(0x718); }

        // CPed::m_pVehicle (+0x58C) — the vehicle this ped currently occupies,
        // or null when on foot. Verified for US 1.0 (SA-MP common.h / MTA CPedSA).
        CVehicleInterface* CurrentVehicle() { return field<CVehicleInterface*>(0x58C); }
        bool IsInVehicle() { return CurrentVehicle() != nullptr; }
        // True only when this ped sits in the driver seat of its vehicle.
        bool IsDriver()
        {
            auto* v = CurrentVehicle();
            return v != nullptr && v->Driver() == this;
        }

    private:
        template <class T>
        T& field(std::size_t byte_offset)
        {
            return *reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(this) + byte_offset);
        }
    };

    using CPlayerPedInterface = CPedInterface;

    // ---------------- accessors ----------------

    inline CPedInterface* GetLocalPlayerPed()
    {
        return *reinterpret_cast<CPedInterface**>(offsets::VAR_LocalPlayerPed);
    }

    // The vehicle the local player currently occupies, or null when on foot.
    inline CVehicleInterface* GetLocalVehicle()
    {
        auto* ped = GetLocalPlayerPed();
        return ped ? ped->CurrentVehicle() : nullptr;
    }

    // Pointer used by the original bootstrap asm for CPlaceable::SetPosition.
    // During early init this can differ from GetLocalPlayerPed(); we preserve
    // the legacy source to keep the spawn sequence identical.
    inline void* GetPlayerPlaceable()
    {
        return *reinterpret_cast<void**>(offsets::VAR_PlayerPlaceable);
    }

    inline void PlaceableSetPosition(void* placeable, float x, float y, float z)
    {
        if (!placeable) return;
        using fn_t = void(__thiscall*)(void*, float, float, float);
        reinterpret_cast<fn_t>(offsets::FUNC_Placeable_SetPosition)(placeable, x, y, z);
    }

    // ---------------- streaming / bootstrap ----------------

    inline void RequestModel(int model_id, int flags = 2)
    {
        using fn_t = void(__cdecl*)(int, int);
        reinterpret_cast<fn_t>(offsets::FUNC_RequestModel)(model_id, flags);
    }

    inline bool HasModelLoaded(int model_id)
    {
        using fn_t = bool(__cdecl*)(int);
        return reinterpret_cast<fn_t>(offsets::FUNC_HasModelLoaded)(model_id);
    }

    inline void RequestSpecialModel(const char* name, int flags = 26)
    {
        using fn_t = void(__cdecl*)(int, const char*, int);
        reinterpret_cast<fn_t>(offsets::FUNC_RequestSpecialModel)(0, name, flags);
    }

    inline void LoadAllRequestedModels(int only_priority = 1)
    {
        using fn_t = void(__cdecl*)(int);
        reinterpret_cast<fn_t>(offsets::FUNC_LoadAllRequestedModels)(only_priority);
    }

    inline void SetupPlayerPed(int player_id = 0)
    {
        using fn_t = void(__cdecl*)(int);
        reinterpret_cast<fn_t>(offsets::FUNC_SetupPlayerPed)(player_id);
    }

    // Blocking: request a model and wait until streaming has it ready.
    // Safe to call before SetModelIndex to avoid the "swap to unloaded model"
    // crash. Idempotent — returns immediately if the model is already loaded.
    //
    // Mirrors SAMP's `CEntity::SetModelIndex` preload: Request → LoadAll(0) →
    // busy-wait IsModelLoaded(). Loading with arg=0 (all pending, not just
    // priority) matches what SAMP's `pGame->LoadRequestedModels()` does and
    // is what's needed for arbitrary skin IDs which aren't priority models.
    inline bool EnsureModelLoaded(int model_id, int max_iterations = 200)
    {
        if (model_id <= 0) return false;
        if (HasModelLoaded(model_id)) return true;

        RequestModel(model_id, 2);
        LoadAllRequestedModels(0); // load *all* pending, not just priority

        // Streaming may finish asynchronously — busy-wait with a short Sleep.
        // 200 * 1ms = 200ms cap; in practice loads in 1–10 iterations.
        for (int i = 0; i < max_iterations; ++i)
        {
            if (HasModelLoaded(model_id)) return true;
            ::Sleep(1);
        }
        return false;
    }

    // ---------------- world / time / weather ----------------

    // Set the in-game clock (24h hour, 0..59 minute). Mirrors CClock::Set.
    inline void SetWorldTime(int hour, int minute)
    {
        if (hour   < 0 || hour   > 23) return;
        if (minute < 0 || minute > 59) return;
        *reinterpret_cast<std::uint8_t*>(offsets::VAR_TimeHours)   = static_cast<std::uint8_t>(hour);
        *reinterpret_cast<std::uint8_t*>(offsets::VAR_TimeMinutes) = static_cast<std::uint8_t>(minute);
    }

    // Set the active weather id.
    //
    // These are SHORTs. Writing them as bytes was a real bug: CWeather::Update
    // reads ForcedWeatherType with `mov ax, word [0xC81318]` and treats it as
    // signed, with 0xFFFF meaning "not forced". A byte write can never clear
    // the sign bit, so the write was either a silent no-op (sentinel intact) or
    // — if the high byte happened to be zero — a permanent weather lock. The
    // stale high byte also broke CClouds' clear-weather test at 0x7143B6, which
    // compares `word [0xC81320]` against {0, 2, 6, 0x0B, 0x0D, 0x11} to decide
    // whether to draw the sun, moon and stars at all.
    //
    // ForcedWeatherType is deliberately NOT written here. It is a *lock*, not a
    // selection: MTA writes 0xFF to it only to release a lock, and never puts a
    // weather id in it (Client/game_sa/CWeatherSA.cpp, GPL-3.0). Setting the
    // blend pair to the same value is the correct way to snap weather.
    inline void SetWeather(std::uint8_t weather_id)
    {
        const auto w = static_cast<std::int16_t>(weather_id);
        *reinterpret_cast<std::int16_t*>(offsets::VAR_WeatherOld) = w;
        *reinterpret_cast<std::int16_t*>(offsets::VAR_WeatherNew) = w;
    }

    // Release any forced-weather lock, restoring the engine's "not forced"
    // sentinel. Mirrors MTA's CWeatherSA::Release.
    inline void ReleaseForcedWeather()
    {
        *reinterpret_cast<std::int16_t*>(offsets::VAR_WeatherForced) =
            static_cast<std::int16_t>(0xFFFF);
    }

    inline void SetGravity(float g)
    {
        *reinterpret_cast<float*>(offsets::VAR_Gravity) = g;
    }

    // ---------------- ground-Z finder ----------------

    inline float FindGroundZ(float x, float y)
    {
        using fn_t = float(__cdecl*)(float, float);
        return reinterpret_cast<fn_t>(offsets::FUNC_FindGroundZForCoord)(x, y);
    }

    // Force the streaming pipeline to pull every IPL/model around `(x, y, z)`
    // in the current interior synchronously. Use after a teleport / interior
    // swap so the player doesn't fall through unloaded geometry.
    //
    // Mirrors `CWorldSA::LoadMapAroundPoint` from mod_s0beit_sa: stops CTimer
    // first so the synchronous streamer doesn't register a giant dt, requests
    // surrounding objects, calls CStreaming::LoadScene, then resumes CTimer.
    // Calling LoadScene without the CTimer pause AVs in our bootstrap state.
    inline void StreamingLoadScene(float x, float y, float z, float radius = 100.0f)
    {
        struct V3 { float x, y, z; } pos = { x, y, z };

        using fn_void = void(__cdecl*)();
        using fn_load = void(__cdecl*)(void*);
        using fn_req  = void(__cdecl*)(void* pos, float radius, int dir);

        reinterpret_cast<fn_void>(offsets::FUNC_CTimer_Stop)();
        reinterpret_cast<fn_req >(offsets::FUNC_CRenderer_RequestObjectsInDirection)(&pos, radius, 32);
        reinterpret_cast<fn_load>(offsets::FUNC_CStreaming_LoadScene)(&pos);
        reinterpret_cast<fn_void>(offsets::FUNC_CTimer_Update)();
    }

    // ---------------- camera ----------------

    // Move the in-game camera to `(x, y, z)` with jump-cut. Equivalent to
    // SAMP's `pGame->GetCamera()->SetPosition` for our purposes.
    inline void CameraSetPosition(float x, float y, float z)
    {
        struct V3 { float x, y, z; } pos = { x, y, z };

        // CCamera::TakeControlNoEntity is __thiscall(CVector*, switchStyle, extra).
        // Extra=1, switchStyle=2 (jump-cut). Matches mod_s0beit_sa's asm.
        using fn_t = void(__thiscall*)(void*, V3*, int, int);
        reinterpret_cast<fn_t>(offsets::FUNC_Cam_TakeControlNoEntity)(
            reinterpret_cast<void*>(offsets::VAR_TheCamera), &pos, 2, 1);
    }

    // Snap camera back to following the local player. Two-step "restore with
    // jump cut" sequence verified in mod_s0beit_sa.
    inline void CameraRestore()
    {
        using fn_t = void(__thiscall*)(void*);
        auto* cam = reinterpret_cast<void*>(offsets::VAR_TheCamera);
        reinterpret_cast<fn_t>(offsets::FUNC_Cam_Restore_A)(cam);
        reinterpret_cast<fn_t>(offsets::FUNC_Cam_Restore_B)(cam);
    }

    // Move the local player into a different interior. Mirrors SAMP's
    // CPlayerPed::SetInterior — fires the `select_interior` SCM opcode which
    // triggers GTA's *full* interior switch (CCullZones, building visibility,
    // alpha lists, audio area code, etc.) — far more than what direct global
    // pokes accomplish. Direct writes to 0xB72914 + RemoveBuildingsNotInArea
    // are kept as a fallback / belt-and-braces.
    inline void SetCurrentInterior(CPedInterface* ped, std::uint8_t area)
    {
        *reinterpret_cast<std::uint32_t*>(offsets::VAR_CurrentArea) = area;

        using fn_t = void(__cdecl*)(int);
        reinterpret_cast<fn_t>(offsets::FUNC_RemoveBuildingsNotInArea)(area);

        if (ped)
            *(reinterpret_cast<std::uint8_t*>(ped) + 0x37) = area; // CEntity::m_areaCode

        // SCM-driven interior switch. After this returns the streamer is
        // primed for the new cell — refresh_streaming_at(x,y) will then drag
        // in the collision/IPLs for the destination.
        scm::select_interior(area);
    }

    // Synchronously load collision + IPLs around (x, y) for the current
    // interior. Use after a SetPosition that crosses an interior boundary.
    inline void RefreshStreamingAt(float x, float y)
    {
        scm::refresh_streaming_at(x, y);
    }

    // Safely swap the local player ped's skin. The naive `ped->SetModelIndex(N)`
    // path on a CPlayerPed in our partially-bootstrapped state crashes deep
    // inside `CClothes::RebuildPlayer` (0x5A82C0) because it walks the
    // CVisibilityPlugins clump table at [0x8D6094] which we never populated.
    //
    // The dance below mirrors what the binary does for vehicles / safe entity
    // model swaps — addresses verified in mod_s0beit_sa:
    //   1. Tear down the IK task slot (vdtor at 0x639330) so the swap doesn't
    //      walk a stale pointer.
    //   2. Call vtable[8] (DestroyRW) to release the old RpClump.
    //   3. Write `m_modelIndex` directly so vtable[5] (SetModelIndex) sees the
    //      target id when it allocates the replacement clump.
    //   4. Re-bind the embedded CAEPedSound (audio sub-object at +0x294) via
    //      `CAEPedSound::SetPed(ped)` (0x4E68D0).
    //
    // Defined below, next to the rest of the deferred-rebuild machinery.
    inline void RequestPlayerRebuild();

    // `CClothes::RebuildPlayer` used to be NOPed to RET at boot to keep this
    // path from crashing. That is no longer done — see RequestPlayerRebuild
    // below and patches.cpp — so the rebuild is requested here and executed at
    // a safe point in the frame instead of being disabled outright.
    inline void SafeSetPlayerPedModel(CPedInterface* ped, int model_id)
    {
        if (!ped) return;
        EnsureModelLoaded(model_id);

        auto* base = reinterpret_cast<std::uint8_t*>(ped);

        // 1. IK task slot — release if any.
        if (auto* tasks = *reinterpret_cast<std::uint8_t**>(base + offsets::OFFS_CPed_pTasks))
        {
            auto*& ik_slot = *reinterpret_cast<void**>(tasks + offsets::OFFS_PedTasks_pIK);
            if (ik_slot)
            {
                using vdtor_t = void(__thiscall*)(void*, int);
                reinterpret_cast<vdtor_t>(offsets::FUNC_CTask_DestroyVdtor)(ik_slot, 1);
                ik_slot = nullptr;
            }
        }

        // 2-3. CEntity-style swap: vtable[8] DestroyRW, write modelIndex, vtable[5] SetModelIndex.
        auto** vtbl = *reinterpret_cast<void***>(base);

        using thiscall_void = void(__thiscall*)(void*);
        reinterpret_cast<thiscall_void>(vtbl[8])(ped);

        ped->m_modelIndex = static_cast<std::uint16_t>(model_id);

        using thiscall_int = void(__thiscall*)(void*, int);
        reinterpret_cast<thiscall_int>(vtbl[5])(ped, model_id);

        // 4. Audio rebind — `this` is the embedded CAEPedSound, arg is the ped.
        using audio_setped_t = void(__thiscall*)(void*, void*);
        reinterpret_cast<audio_setped_t>(offsets::FUNC_CAEPedSound_SetPed)(
            base + offsets::OFFS_CPed_AudioSound, ped);

        // 5. The clump now exists but its clothes/textures have not been bound.
        //    Ask for a rebuild rather than doing it inline — see below.
        RequestPlayerRebuild();
    }

    // ---------------- deferred player rebuild ----------------
    //
    // Borrowed from MTA:SA (GPL-3.0, same licence as OpenSAMP), specifically
    // CClientPed::ProcessRebuildPlayer in
    // Client/mods/deathmatch/logic/CClientPed.cpp.
    //
    // OpenSAMP used to `mem_put<BYTE>(0x5A82C0, 0xC3)` — permanently RET out of
    // CClothes::RebuildPlayer — because calling it inline during a model swap
    // crashed. MTA never patches that function. It calls it, but defers the
    // call to a safe point in the frame and runs it at most once per frame:
    //
    //     if (m_bPendingRebuildPlayer && m_uiFrameLastRebuildPlayer != frame)
    //     { ...; m_pPlayerPed->RebuildPlayer(); }
    //
    // Disabling the rebuild is what leaves the local player's clothes textures
    // unbound, which is the likely source of the green placeholder in the
    // water reflection: the main render pass works because the model swap bound
    // something, while passes that need the rebuilt clump get nothing.

    inline bool g_rebuildPending = false;
    inline unsigned g_rebuildLastFrame = 0xFFFFFFFFu;

    inline void RequestPlayerRebuild() { g_rebuildPending = true; }

    // Call once per frame from the game tick, passing the frame counter.
    inline void ProcessPendingPlayerRebuild(unsigned frame)
    {
        if (!g_rebuildPending || g_rebuildLastFrame == frame) return;

        auto* ped = GetLocalPlayerPed();
        if (!ped) return;

        g_rebuildPending   = false;
        g_rebuildLastFrame = frame;

        // ONLY for CJ (model 0). CClothes::RebuildPlayer does not "refresh" an
        // arbitrary ped — it *assembles* the player out of clothing components,
        // so calling it on a normal skin replaces that skin with a naked CJ.
        // MTA gates it the same way, in CClientPed::RebuildModel:
        //     if (m_ulModel == 0) { RefreshClothes(); AddAllToModel(); ... }
        // and again in _ChangeModel. Learned the hard way.
        if (ped->m_modelIndex != 0) return;

        // void __cdecl CClothes::RebuildPlayer(CPed* ped, bool bIgnoreFatAndMuscle)
        using rebuild_t = void(__cdecl*)(void*, bool);
        reinterpret_cast<rebuild_t>(offsets::FUNC_CClothes_RebuildPlayer)(ped, false);
    }
} // namespace gta::sa
