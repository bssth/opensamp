# GTA SA US 1.0 offset map

Canonical addresses OpenSAMP uses live in [`native/addresses.h`](../native/addresses.h).
This document records, for each address, where the name/attribution came from
and any caveats. **Target build: `gta_sa.exe` US 1.0 only.**

## Attribution sources

| Tag | Source |
|---|---|
| `[SAMP]` | Address the SA-MP client is known to use, as attested in public SA-MP reverse-engineering references. |
| `[MTA]`  | MTA:SA (mtasa-blue) named offset headers (`CPedSA.h`, `CPoolsSA.h`, `CMultiplayerSA.cpp` HOOKPOS/FUNC, …). |
| `[SP]`   | SilentPatch (SilentPatchSA) upstream. |
| `[OWN]`  | OpenSAMP's own reverse engineering; not yet cross-verified against a second source. |

Confidence is **High** unless tagged (Med)/(Low) in `addresses.h`.

## Named map

See `native/addresses.h` for the authoritative list (grouped by subsystem:
game state, D3D, local player, entity pools, pool-size patch sites, streaming,
ped functions, world/area, timers, camera, weather/time/gravity, explosions/SCM,
boot hooks, crash-fix patch sites, intro/startup). Every constant carries its
attribution tag inline.

### High-confidence highlights (exact match in a reference tree)

- **Entity pools**: `CPedPool 0xB74490`, `CVehiclePool 0xB74494` — `[MTA CLASS_*]` + `[SAMP ADDR_*_TABLE]`.
- **Pool-size patches** (`0x551024` veh / `0x550FF2` ped / `0x551178` event / `0x551060` building / …) — `[SAMP patches.cpp]` confirmed by `[MTA CPoolsSA]`.
- **Ped functions**: `SetModelIndex 0x5E4880`, `SetCurrentWeapon 0x5E61F0`, `GiveWeapon 0x5E6080`, `CAEPedSound::SetPed 0x4E68D0` — `[MTA FUNC_*]`.
- **Streaming**: `RequestModel 0x4087E0`, `LoadAllRequestedModels 0x40EA10` — `[MTA][SAMP]`.
- **Boot hooks**: `CGame::Process 0x53C095` `[MTA HOOKPOS]`, `CRunningScript::Process 0x469F00` `[MTA]`, `ProcessOneCommand 0x469EB0` `[SAMP]`.
- **Crash fixes**: `CPlayerPed::ProcessControl 0x609C08`, `CPhysical::~CPhysical 0x542485`, `drown-in-vehicle 0x4BC6C1`, `CPlayerPed ctor task 0x60D64E` — `[SAMP]`/`[MTA]`.

### Caveats / disagreements (verified in Phase 2)

- **`0x53EA08`** — our code calls it "ped shadow rendering" (inherited comment). MTA's
  precise symbol is **`CWorld::ProcessPedsAfterPreRender`**; the shadow effect is
  a side effect of the patched function. `addresses.h` uses the MTA name.
- **`0x5A82C0`** — MTA: `CClothes::RebuildPlayer`. SA-MP-lineage offset lists alias
  the same address as `CPlayerPed::UpdateAfterPhysicalChange`. Kept MTA name;
  alias noted.
- **`0x50AD60` (`Cam_PointCamera`)** — confirmed to be a `CCamera` member (MTA
  brackets it: `0x50AD40 Find3rdPersonQuickAimPitch`, `0x50ADF0 GetFadingDirection`),
  but the exact name "PointCamera" is **unconfirmed**. Marked (Low).
- **TheCamera `0xB6F028` vs SAMP `ADDR_CAMERA 0xB6F99C`** — different addresses;
  `0xB6F99C` is a field *inside* TheCamera, not the object base. Do not conflate.
- **Not found in any local tree** (kept as `[OWN]`/(Low)): `0x8D6094`
  (CVisibilityPlugins clump table), `0x732B20` (visibility helper sibling),
  `0x732E30` (ped-pool init fn). SilentPatch addresses `0x4A9D50` / `0x7271CB`
  are `[SP]` upstream (already cited in `patches.cpp`).

## Legacy patch block (NOT yet enumerated in addresses.h)

`native/patches.cpp` contains **~200 opaque byte patches** — mostly
`fn -> RET` / `NOP N` anti-feature patches — that appear to have been imported
wholesale from an older SA-MP / trainer base. They have no canonical symbol and
low individual value to name. They remain inline in `patches.cpp` pending a
dedicated attribution pass. **Tracking item, not a blocker.**

## Anomalies flagged during inventory (worth fixing separately)

- **`0x1566855`** (`patches.cpp:49`) — out of the `0x400000–0xC00000` range;
  sits in a cluster of `0x56xxxx` scanlist-relocation writes. Almost certainly a
  **typo for `0x566855`** — writes to the wrong address today. **Likely bug.**
- **`0x074872D`** (`patches.cpp:514`) — written with a stray leading zero
  (`0x074872D` == `0x74872D`, which is in range and matches neighbouring
  `0x748xxx` patches). Harmless numerically but should be normalised.
- **`0x349B7B`** (`sampraknet_bridge.cpp:2757`, in a comment) — a **module-relative
  RVA** (`gta_sa.exe+0x349b7b`), not an absolute VA. Excluded from the absolute map.

## Provenance

This map was produced by a parallel multi-agent inventory + reference-tree
attribution pass (2026-05). Inventory covered `patches.cpp`, `game.h`,
`hooks.cpp`, `sampraknet_bridge.cpp`, `gta/sa_state.h`, `gta/common.h`.
