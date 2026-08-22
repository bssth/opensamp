# TODO

Roadmap and outstanding work that needs to be closed before the repository
can go public and accept third-party contributions.

Priorities:
- **P0** — must-do before going public. Either embarrassing or legally
  risky to ship without.
- **P1** — needs to land before the first public alpha build.
- **P2** — quality-of-life and code-health improvements; can be done
  incrementally.

---

## Recently shipped

Quick log of what's landed. Detailed status lives in the priority sections
below; this is a high-level overview for context.

### Bootstrap & game state
- [x] **Pre-game patches mirroring SAMP's `ApplyPreGamePatches`** — intro
      logo bypass (NOP 6 @ `0x747483`), replace logo movie filenames
      (`0x866CD8`, `0x866CCC`) with `"title"` so the streamer doesn't
      stall on the .mpg files.
- [x] **`kick_game_start` now matches `CGame::StartGame()` exactly** —
      added the missing `*(BYTE*)0xBA6831 = 1` (`ADDR_GAME_STARTED`)
      write that several first-frame init paths gate on. Previously
      half-init streaming/visibility was a side-effect of skipping it.
- [x] **CVisibilityPlugins clump-table guards** at `0x732B00` / `0x732B20`
      to keep model swaps from AVing on the still-uninitialised plugin
      table at `[0x8D6094]` in our bootstrap state.
- [x] **`EnsurePedPoolsInitialised` removed from boot** — directly
      forcing `0x732E30` was the root cause of the original "crash after
      enter server" report.

### Crash fixes
- [x] **SilentPatch byte-only fixes ported**: heap-corruption RET on
      `0x4A9D50`, mirrors AV fix at `0x7271CB`. Wholesale SilentPatch
      integration (Memory.h / Trampoline.h) deliberately not pursued.
- [x] **Local SEH wrap around `CStreaming::LoadScene`** so a fault in
      streaming pre-load doesn't abort the surrounding RPC handler
      (was killing every `ScrSetPlayerPos`).
- [x] **`StreamingLoadScene` rewritten** to match the verified
      `mod_s0beit_sa::CWorldSA::LoadMapAroundPoint` pattern: stop CTimer
      → request objects in direction → LoadScene → resume CTimer. The
      naked LoadScene call AVed without the timer pause.

### Skin / model swap
- [x] **`SafeSetPlayerPedModel`** dance for player-ped model swaps:
      tear down IK task, vtable[8] DestroyRW, write `m_modelIndex`,
      vtable[5] SetModelIndex, audio rebind via
      `CAEPedSound::SetPed (0x4E68D0)`. Plus `mem_put<BYTE>(0x5A82C0,
      0xC3)` to RET out of `CClothes::RebuildPlayer`.
- [x] **`EnsureModelLoaded` strengthened** — now mirrors SAMP's
      preload: `RequestModel` → `LoadAllRequestedModels(0)` (load *all*
      pending, not just priority) → busy-wait `IsModelLoaded` with
      Sleep(1) and 200 ms cap. Fixes the "swap to unloaded model"
      green-untextured bug.
- [x] **Mid-game `ScrSetPlayerSkin` re-enabled** with the strengthened
      preload. Boot-time skin swap kept (initial `player` → `7`).

### RPC layer
- [x] **`SetPlayerInterior` handler** (RPC 156) — sets `CWorld`
      currArea, removes buildings not in area, writes `m_areaCode` on
      ped, fires `select_interior` SCM opcode for the full GTA-side
      interior switch.
- [x] **SCM dispatcher infrastructure** (`native/game/scm.h`): fake
      `GAME_SCRIPT_THREAD` (0xE0 bytes, `pack(1)`) + opcode buffer +
      `ProcessOneCommand` (`0x469EB0`) invoker. Wrappers for
      `select_interior (0x4BB)`, `link_actor_to_interior (0x860)`,
      `refresh_streaming_at (0x4E4)`. Lets us drive subsystems that
      have no clean C ABI.
- [x] **Many missing world / player RPCs wired**: weather, world time,
      gravity, camera (set / look-at / behind-player / restore),
      weapons (give / clear), world bounds, player armour / health /
      facing-angle, position-find-Z, player-pos-with-streaming-refresh.
- [x] **On-foot sync at 20 Hz** — fixes the server-side AFK timer.
      Reads live ped pos / rot / HP / armour / current weapon; vehicle
      sync paths still pending.
- [x] **Local clock pinned to server time** when
      `g_world.serverTimeKnown` — vanilla CClock no longer overrides
      what the server pushes.
- [x] **Single-slash `/cmd` routes to RPC `ServerCommand`**, double-slash
      `//cmd` reserved for local/client UI commands.
- [x] **CP-1251 ↔ UTF-8 conversion** on the wire so Cyrillic names /
      chat / dialogs round-trip correctly.

### UI
- [x] **`{RRGGBB}` color-tag parser fix** — was reading 8 hex chars
      (`{XXXXXXX}` length 9), SAMP format is 6 hex chars (`{RRGGBB}`
      length 8). One byte off; broke every server's coloured text.
- [x] **Shared color renderer** (`gui/colored_text.h`) used by chat,
      dialog body, and dialog list items. Per-line rendering avoids the
      `SameLine(0,0)`-against-multi-line-widget column-wrap glitch.
- [x] **Dialog list-item rendering** — was tripping ImGui's
      "SetCursorPos extended bounds without a follow-up item" assertion.
      Fixed by painting coloured text via `ImDrawList::AddText` over a
      hit-testing `Selectable`, removing the cursor-bounce.
- [x] **Dialog styles 4 / 5** (TabList, TabListHeaders) added — both
      previously fell through to default (rendered nothing).
- [x] **Dialog body buffer** bumped from 257 to 4097 (matches SAMP
      Pawn-string limit). Modern open.mp servers exceed 256 routinely.

---

## P0 — Repository hygiene and legal

- [x] **LICENSE.** GPL-3.0 chosen and committed (`LICENSE`, canonical FSF
      text). OpenSAMP is a clean-room reimplementation against
      [open.mp](https://www.open.mp/)'s documented wire protocol — **not** a
      fork of SAMP — so we are not bound by SAMP's license. README license
      section updated to match.
- [x] **Vendored third-party licenses inventoried** in
      [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). Findings:
      `vendor/sampraknet` is **RakNet 3.x** (Kevin Jenkins / Rakkarsoft,
      2003), not the 2014 BSD RakNet 4 — its headers offer CC BY-NC 2.5 /
      commercial / **GPL v2-or-later**, and the GPL option is the only free
      one. That is what makes GPL-3.0 mandatory rather than optional, and it
      means OpenSAMP **cannot** be relicensed permissively while it links
      this library. `vendor/imgui` is MIT; MinHook is BSD-2-Clause and its
      text (plus the bundled Hacker Disassembler Engine notice, required
      because we ship a prebuilt `.lib`) now lives at
      `native/vendor/MinHook.LICENSE.txt`.
- [ ] **Close the three licensing gaps** listed under "Open questions" in
      THIRD-PARTY-NOTICES.md:
  - The SA-MP files (`SAMPAuth`, `SAMPNetEncr`, `SAMPRPC`) carry no licence
    headers, and the auth key table is SA-MP-derived. Now vendored, with the
    project's position stated in `vendor/sampraknet/OPENSAMP-NOTICE.md`.
    See the P0 item on `SAMPAuth.cpp` below.
  - `rijndael.h` points at a `LICENSE-EST` file that is not in the tree.

  Resolved by vendoring: `sampraknet` had no `LICENSE` file of its own, which
  no longer matters now that OpenSAMP has elected the GPL option explicitly and
  in writing, in-tree.
- [x] **`.gitignore` rewritten.** Now covers `Debug/`/`Release/`/`x64/`,
      all VS/MSBuild intermediates (`*.obj`, `*.pdb`, `*.ilk`, `*.idb`,
      `*.tlog`, `*.lastbuildstate`, `*.exp`, …), `*.user`/`*.suo`, `.vs/`,
      `.idea/`, `.run/`, `Backup/`. `*.lib` deliberately NOT ignored
      (vendored import libs under `native/lib/`).
- [x] **Purged build junk from the index.** 214 tracked files removed via
      `git rm -r --cached` (`native/Debug`, `native/Release`,
      `launcher/{Debug,Release,x64}`, `.vs/`, `.idea/`, `Backup/`, `.run/`,
      `*.user`). Tracked file count dropped from 258 to 38.
- [x] **`redist/` untracked.** 62 MB removed from the index
      (`VC_redist.x86.exe` 6.9 MB + `windowsdesktop-runtime-9.0.13` 55 MB +
      `comhost`/`ijwhost`/`nethost`). The .NET pieces were dead weight: the
      solution builds two native C++ projects and nothing referenced them.
      The VC++ runtime is now a documented download in `docs/build.md`.
      `redist/` is `.gitignore`d; the files stay on disk locally.
- [x] **`bin/` guard.** `tools/git-hooks/pre-commit` rejects staged
      `gta_sa.exe` / `*.img` / `*.dff` / `*.txd` / `*.ide` / `*.ipl` /
      `*.col` / `*.ifp` / `*.scm` / `*.gxt`, anything under `bin/` or
      `redist/`, and any file over 5 MiB. Enable with
      `git config core.hooksPath tools/git-hooks` (documented in
      CONTRIBUTING.md and docs/build.md).
- [x] **CI-side version of the same guard.** `tools/ci/check-tree.sh` runs
      the hook's checks over the whole tracked tree in CI, plus two the hook
      does not do: local workstation paths / leaked-source-tree references,
      and unfilled placeholders. Run it locally with
      `bash tools/ci/check-tree.sh`.
- [x] **README.md** — build/run section written, status corrected (on-foot
      sync works; vehicle sync partial), license and contributing sections
      point at the new documents.
- [x] **CONTRIBUTING.md** — clean-room rule, the no-game-assets rule, the
      GPL rationale, offset-map provenance policy, code style, PR and bug
      report expectations.
- [x] **CODE_OF_CONDUCT.md** — Contributor Covenant 2.1, enforcement contact
      filled in.

## P0 — Security and reputation

- [x] **Removed the hardcoded test server.** The `/test` block in
      `native/gui/chat.cpp` is gone. It went in two rounds: first the
      credentials (IP `198.13.187.116`, password, a real nickname), then the
      block itself, which had been left behind pointing at a third-party
      production server. `//connect <ip> <port> [nick]` is the only
      supported path.
- [x] **History audit clean for that leak.** Neither the credentials nor the
      later hostname ever reached a commit that is published — the public
      history is built fresh from the audited tree.
- [x] **No references to leaked SA-MP sources.** Offset provenance is stated
      in terms of public reverse-engineering references; no source tree is
      named and no local paths remain in the tree.
- [x] **Bounded three server-controlled buffer lengths.** `ScrGameText` wrote
      one byte past a 400-byte stack buffer and accepted a negative length;
      `ScrShowTextDraw` and `ScrEditTextDraw` accepted up to 65535 bytes into
      1 KiB. All reachable from any server the user connects to.
- [ ] **Audit the rest of the RPC surface the same way.** The three above
      were found by following one cppcheck hit to its neighbours, not by a
      systematic pass. Every handler that reads a length or an index off the
      wire needs the same read: is the bound checked, and is it checked
      against `sizeof` rather than a copied literal? Player-id indexing is
      already bounds-checked everywhere; lengths were not.
- [ ] **`BitStream::Read` results are almost never checked.** One call out of
      210 in `sampraknet_bridge.cpp` looks at the return value. `Read` returns
      false on a short stream and leaves the destination untouched, so every
      unchecked call is a handler that proceeds on whatever was in that
      variable — stale globals, or stack garbage for locals. Three locals
      where the garbage reached a consumer are fixed; the systematic pass is
      not done. The shape to copy:

      ```cpp
      PICKUP Pickup{};
      if (!bsData.Read((PCHAR)&Pickup, sizeof(PICKUP))) return;
      ```

      Worth considering a small checked-read helper rather than 209 hand-written
      `if`s, since the failure action is `return` in nearly every case.

## P0 — Reproducible dependencies

- [x] **Submodules replaced by vendored copies.** Both dependencies were
      unusable by anyone other than this workstation:
  - `vendor/imgui` was pinned to `3835a9832`, a *local, unpushed* commit
    that renamed `imgui.h` to `imgui_manager.h`. It does not exist on
    `github.com/ocornut/imgui`, so `git submodule update --init` failed for
    every cloner.
  - `vendor/sampraknet/src/RakPeer.cpp` carried an *uncommitted* patch that
    existed in one working tree and nowhere else.

      Both are now vendored as ordinary tracked files. `git clone` alone
      produces a tree that builds. imgui is byte-identical to upstream
      `eaa32bb7` (v1.92.5+) and the rename is reverted, so updating it is a
      file copy; provenance in `vendor/imgui/UPSTREAM.md`. sampraknet is the
      full fork at `e891acf` plus the RakPeer patch, which is now a reviewable
      diff carrying the GPL §5(a) modification notice; provenance and the
      licence election in `vendor/sampraknet/OPENSAMP-NOTICE.md`.

      This also removes the supply-chain dependency on third-party GitHub
      accounts, which is why forking them under the OpenSamp org is no longer
      needed.
- [ ] **Decide the long-term position on `SAMPAuth.cpp`.** 512 SA-MP 0.3.7
      challenge/response key pairs, used by the connect handshake at
      [sampraknet_bridge.cpp:382](native/sampraknet_bridge.cpp:382). Vendoring
      moved them into our own tree, which is documented honestly in
      `OPENSAMP-NOTICE.md` as an interoperability artefact rather than SA-MP
      logic. If that position ever needs to change, the fallback is to load
      the table from a user-supplied file at runtime instead of compiling it
      in. Worth checking first whether open.mp servers require the SA-MP
      handshake at all — if not, the table is only needed for classic 0.3.7
      servers and can become optional.

## P1 — Builds out of the box

- [ ] **CMake alongside `.sln`.** Right now the project only builds via
      Visual Studio 2022 on Windows. That's fine for the target platform
      but CMake makes CI and clang-cl experiments much cheaper.
- [x] **GitHub Actions CI.** `.github/workflows/ci.yml`: tree guard, a
      Debug + Release Win32 build that fails on any warning in first-party
      code and uploads both binaries, and cppcheck in two passes — blocking
      on definite defects, advisory on style/performance/portability. The
      first cppcheck run found a real off-by-one in `ScrGameText`.
- [ ] **clang-tidy and a formatting check.** Neither is wired up. Formatting
      has nothing to check against: the tree has no agreed style, and mixes
      RakSAMP-era hungarian notation with modern C++ in the same file.
      Agree a `.clang-format` before adding the gate, or it will just churn.
- [x] **`docs/build.md`** — toolchain, the `bin/` layout, the `SolutionDir`
      trap when building a `.vcxproj` directly, running, crash dumps, and why
      the DLL fails to load.
- [x] **`imgui` updated** from v1.62 (2018) to v1.92.5 and vendored at
      upstream `eaa32bb7`. Re-check against upstream periodically; the
      procedure is in `vendor/imgui/UPSTREAM.md`.
- [ ] **Move log files out of `CWD`.** `GameReady.log` and `OpenSamp.log`
      are written next to `gta_sa.exe`. Move them to
      `%LOCALAPPDATA%\OpenSAMP\logs\` (or alongside `crash/`) so we
      don't pollute the game directory.
- [ ] **Game install path in config.** Correction to an earlier note: the
      injector does *not* hardcode a path — `run_game()` resolves
      `gta_sa.exe` and `Client.Native.dll` relative to the launcher's own
      directory. That works, but it forces the launcher to live inside the
      game folder. A config file (or a registry/Steam lookup) would let the
      client be installed separately.

## P1 — Architectural debt

- [ ] **Break up `native/sampraknet_bridge.cpp`** (3204 lines!).
      Suggested split:
  - `bridge/init.cpp` — `Initialize`, `Shutdown`, `Connect`, `Disconnect`.
  - `bridge/sync_onfoot.cpp` — `Send/HandleOnFootSync`.
  - `bridge/sync_incar.cpp` — `Send/HandleInCarSync`, passenger.
  - `bridge/sync_aim.cpp`, `sync_unoccupied.cpp`, `sync_trailer.cpp`.
  - `bridge/rpc/*.cpp` — one file per RPC family (server, world,
    scr_player, scr_vehicle, scr_object, dialog, textdraw…).
  - `bridge/rpc_table.h` — all IDs in a single `constexpr` namespace
    instead of ~140 global `static int RPC_*`.
- [ ] **Drop the ~340 lines of commented-out code** in the bridge —
      git history has it; in-source it's just noise.
- [ ] **`addresses.h`** — a single offset map for GTA SA US 1.0. The
      codebase currently has ~422 magic addresses (`0x53C095`,
      `0xBA677B`, `0x732E30`, …) scattered across 23 files. Without a
      central offset table no contributor can tell what is being
      hooked or patched. Reference public reverse-engineering work
      (e.g. [plugin-sdk](https://github.com/DK22Pac/plugin-sdk)) but
      list **every address we use**.
- [ ] **One naming style.** Currently mixed:
      `kick_game_start`, `get_system_state`, `gta::sa::GetLocalPlayerPed`,
      `TickGameReady`, `g_szNickName`, `iAreWeConnected`. Hungarian
      notation (`g_sz`, `iAre`, `dwLast`) is inherited from the original
      SAMP — decide whether to drop it, then enforce via
      `.clang-format` + `.editorconfig`.
- [ ] **`using namespace std`** at
      [native/gui/chat.cpp:14](native/gui/chat.cpp:14)
      and `using namespace std::chrono` next to it — drop them.
- [ ] **Hardcoded server netcode version.** `#define NETGAME_VERSION 4057`
      at [native/sampraknet_bridge.cpp:29](native/sampraknet_bridge.cpp:29)
      means we support exactly one server version. Move to a config /
      per-server selection.
- [ ] **`int g_myPlayerID = -1;`** plus dozens of other globals in the
      bridge — fold them into a client state struct.
- [ ] **One stray Russian comment** at
      [native/hooks.cpp:151](native/hooks.cpp:151) among the English
      ones — translate it.

## P1 — Functional gaps

These are what makes the client **not yet playable**.

- [ ] **Bridge sync senders are commented out:**
  - [x] `SendOnFootFullSyncData` — wired at 20 Hz.
  - [~] `SendInCarFullSyncData` — **driver in-car sync implemented**
        (`inCarUpdate()`, mirrors `Packet_VehicleSync` RX wire format;
        `CVehicleInterface` + `GetLocalVehicle` in game.h; VehicleID from
        `ScrPutPlayerInVehicle` RPC 70). Compiles; **needs in-game test**
        (matrix→quaternion handedness is the main risk). Limitation:
        VehicleID only known for server-seated entry — player-driven entry
        needs a `CVehicle*<->VEHICLEID` registry (WorldVehicleAdd) and falls
        back to on-foot sync for now.
  - [ ] `SendPassengerFullSyncData` — passenger sync still missing.
  - [ ] `SendUnoccupiedSyncData` — empty vehicles still don't sync.
  - [ ] `SendTrailerSyncData` — trailers still don't sync.
- [ ] **`TickRestarting()`** at
      [net/netgame.cpp:209](native/net/netgame.cpp:209) — empty.
      Implement exponential backoff + reconnect.
- [ ] **`OnEnter` / `OnExit`** in the FSM — empty. Transitions only
      log; no side effects.
- [ ] **`CONNECTION_FAILED` / timeout in `TickConnecting`** — no
      handling. Bridge needs to surface those events.
- [ ] **RPC handlers — done / partial / missing matrix.** Big
      progress this milestone:
  - [x] `ScrSetInterior` (156), `ScrSetPlayerPos` (12) +
        `ScrSetPlayerPosFindZ` (13), `ScrSetPlayerFacingAngle`,
        `ScrSetPlayerHealth`, `ScrSetPlayerArmour`, `ScrSetPlayerSkin`,
        `ScrSetSpawnInfo`, `Spawn`, dialog box + response, server
        command echo, weather, world time (and clock-pin), gravity,
        camera set / look-at / behind-player / restore, world bounds,
        give-weapon / clear-weapons, gametext, init-game.
  - [ ] `ScrSetCameraLookAt` is a stub — needs a real
        `CCamera::PointCamera` call.
  - [ ] `AddExplosion` — stub (`@todo` in handler).
  - [ ] `ScrSetWeaponAmmo` — partial; needs proper `CPedWeapon`
        traversal and weapon-slot lookup.
  - [ ] **3D text labels** — handlers exist but nothing is drawn in
        the world.
  - [ ] **Textdraws** — handlers exist but nothing is drawn.
  - [ ] **Pickups / objects / gangzones / menus** — out of scope per
        non-goals, but list them here for tracking.
- [ ] **`@todo` patches** in [native/patches.cpp](native/patches.cpp):
  - `0x609C08` NOP 39 — CPlayerPed::ProcessControl crash fix.
        **Investigated, intentionally kept disabled**: re-enabling kills
        movement (ped rotates but won't walk). Needs deeper analysis
        before applying.
  - `0x47BF54` — SCM events processor hook.
  - `0x6884C4` NOP 6 — "don't rotate ped from camera".
  - `0x57BA57` — disable the original main menu (we draw our own).
  - `0x705331..0x7053AF` — disable the in-game photo camera.
- [ ] **`m_maxLines = 200` / `m_visibleLines = 15`** in
      [native/gui/chat.hpp](native/gui/chat.hpp:65) marked `@todo
      use/change` — the chat buffer isn't actually capped to that limit.

### P1 — Known-broken visuals (post-bootstrap)

Symptoms of our partial `CGame::Initialise` skip. The
`ApplyPreGamePatches` + `ADDR_GAME_STARTED=1` improvements made these
better but didn't fully close them.

- [x] **Flat cyan sky / washed-out distance — NOT A BUG. Timecycle exonerated
      by measurement.** `diag::DumpSkyState()` was compared field-by-field
      against `data/timecyc.dat` for the observed conditions (weather 13 =
      EXTRASUNNY_COUNTRYSIDE, 14:00, interpolating Midday→7PM at f=0.286).
      Every value matches to within rounding:

      | field | expected from timecyc.dat | logged |
      | --- | --- | --- |
      | SkyTop | 95, 169, 227 | 95, 169, 227 |
      | SkyBottom | 88, 162, 219 | 87, 162, 219 |
      | Ambient | 22, 9.9, 9.9 | 23, 10, 10 |
      | FarClip | 1500.0 | 1500.0 |
      | FogStart | 47.9 | 47.9 |

      So `CTimeCycle::Update` runs, interpolates correctly, and produces exactly
      what Rockstar's data prescribes. Two consequences worth writing down:
      the near-flat sky is *authored* — top and bottom differ by only ~8 for
      this weather and hour; and the heavy haze is authored too — `FogSt` falls
      from 65 at midday to 5 by 7PM while `FarClp` stays at 1500.
      `ForcedWeatherType` reads `-1` and the extra-colour override is off, so
      neither is interfering.

      What remains genuinely wrong is the **distant terrain rendering as flat
      untextured silhouettes** — that is the LOD/streaming item below, a
      geometry problem, not a colour one. Do not re-open this as a sky bug
      without new evidence.

      (Diagnostic caveat: the float triple at `m_CurrentColours+0x18` reads back
      identical to ambient, while timecyc.dat gives directional as 255,255,255.
      Either the field is not directional or the engine stores it scaled. The
      dump logs the three triples by raw offset instead of asserting labels.)

- [ ] ~~Flat cyan sky~~ — superseded by the entry above. Retained context:
  - The renderer reads exactly one struct, `CTimeCycle::m_CurrentColours`
    (`CColourSet`, 0xAC bytes) at `0xB7C4A0`, rebuilt each frame from
    `CGame::Process` at `0x53C0DA`. `Idle` at `0x53EA41` loads the six sky
    words and passes them to `DoRWStuffStartOfFrame` (`0x53D7A0`) →
    `CClouds::RenderSkyPolys` (`0x714650`). Field addresses are now in
    `addresses.h`.
  - **The sky-bottom colour is also the world fog colour.** One bad pair of
    words produces both halves of the symptom, which is why they should be
    treated as a single fault, not two.
  - Near geometry and lighting are correct, so ambient/directional are sane —
    which means `timecyc.dat` loaded and the update runs. That rules out the
    "table never loaded" and "update never called" families.
  - **The 261-patch legacy block is almost certainly innocent**: it contains
    zero entries in `CTimeCycle`, `CColourSet`, `CClouds` or `CWeather`. See
    [docs/legacy-patches.md](docs/legacy-patches.md).
  - Remaining hypotheses: the sky fields specifically not being refreshed
    (`CColourSet::Interpolate` at `0x55F870` takes a `bIgnoreSky` flag), a
    stuck-on extra-colour interior tint (`0xB7C484`), or fog/far-clip making
    the sky quad — which sits on the far plane — resolve to a flat fill.
  - `diag::DumpSkyState()` logs all of it every 120 frames. Read the log
    before theorising further.

- [ ] **Interior teleport drops the player into the void / "seventh
      sky".** SCM `select_interior` + `refresh_streaming_at` are now
      wired; user testing shows it's still imperfect. Likely missing
      pieces: `link_actor_to_interior` with the *real* SCM actor handle
      (we currently skip it), and possibly a `CColStore` request for
      the destination cell.
- [ ] **Local player water reflection / mirror is the green
      placeholder.** Reads CVisibilityPlugins entries we never
      populated for the local clump. Either init the plugin table
      properly, or seed the player clump's plugin entries by hand at
      boot.
- [ ] **LODs everywhere instead of full models.** Streaming radius /
      sector flags not initialised by the natural state path. Suspect
      a missing `CStreaming::ms_streamingBufferSize` or the LOD-priority
      cull ring being uninitialised.

## P2 — Code quality and tooling

- [ ] **`.editorconfig`** — indent, line endings, encodings.
- [ ] **`.clang-format`** — single formatting style.
- [ ] **`.clang-tidy`** — start with `modernize-`, `bugprone-`, `cert-`.
- [ ] **`docs/architecture.md`** — diagram of
      `launcher → inject → DllMain → MainThread → patches/hooks/D3D
      → ImGui overlay → CNetGame FSM → bridge → RakClient`.
- [ ] **`docs/offsets.md`** — for each address, where it came from
      (own reverse engineering / classic SAMP / plugin-sdk).
- [ ] **Document `parse_dmp.py` and `parse_stack.py`.** They aren't
      even tracked in git, and how to apply them to a `crash/*.dmp`
      is not documented anywhere.
- [ ] **Tests.** At least unit tests for `util/encoding.h` and the
      chat color-tag parser (`ImGuiChat::ParseHexColorTag`). The RPC
      bitstream parsers are great regression-test candidates.
- [ ] **Replace `using PLAYERID = unsigned short;`** globally with a
      strong typedef / enum class — prevents accidental confusion with
      `VEHICLEID`.
- [ ] **`ExitProcess(1)`** at
      [native/hooks.cpp:119](native/hooks.cpp:119) on init failure is
      brutal. Replace with a graceful shutdown + user-visible message.
- [ ] **MinHook via `#pragma comment(lib, ...)`** in
      [native/dllmain.cpp](native/dllmain.cpp) — implicit dependencies.
      Move to project linker settings.
- [ ] **Singletons via `static T instance;` in `Get()`** — fine for a
      DLL, but verify destruction order at `DLL_PROCESS_DETACH`.

## Explicit non-goals for this list

- Implementing the full SA-MP feature surface in one go (objects,
  gangzones, textdraws, menus, custom skins/objects). These are
  separate epics that come **after** basic player and vehicle sync
  are stable.
- Supporting other `gta_sa.exe` builds (EU 1.0, US/EU 1.01, Steam,
  re3/reVC). Separate epic, after the first public alpha.
