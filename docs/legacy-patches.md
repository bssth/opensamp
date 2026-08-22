# The legacy byte-patch block

`native/patches.cpp` contains `ApplyLegacyBasePatches()` — 261 byte patches
inherited from earlier work, which arrived with no comments at all.

This document is the result of a cross-reference pass that identified 254 of
them against open, named sources: primarily
[MTA:SA](https://github.com/multitheftauto/mtasa-blue) (GPL-3.0, so
licence-compatible with OpenSAMP and quotable rather than merely consultable),
plus plugin-sdk, GTA-SA-ScriptHook and SA-MP's own patch list, with
disassembly of the shipped `gta_sa.exe` used to settle anything the sources
disagreed on. 251 of the 261 entries match a documented upstream patch line
one-for-one: the block is essentially MTA's `InitHooks_Others` plus SA-MP's own
patch set, in MTA's order.

Citations of the form `MTA:1308` are line numbers in
`Client/multiplayer_sa/CMultiplayerSA.cpp`; `[dis]` marks an identification
made by disassembling the retail executable rather than by matching a source.

Entries that could not be attested are deliberately absent from the table
rather than guessed at. **Anything below is a claim with a citation; anything
missing is still unknown.**

## 3. Enumeration table

Paste above the corresponding write in `native/patches.cpp`. Every line below is backed by a named comment in MTA's `Client/multiplayer_sa/CMultiplayerSA.cpp` at the cited line, by a documented SA-MP patch of the same address, or by disassembly of the retail executable where marked `[dis]`. Entries with no attestation are in §4, not here.

```cpp
// ============ WinMain / bootstrap / loading screen ============
// [#1-3]    0x748EF8,0x748EFC,0x748B0E  No intro movies: repoint two WinMain jump-table slots to
//                                       0x748AE7/0x748B08 and force the state index to 5.   MTA:1308-1311
// [#4-5]    0x748C2B, 0x748C9A          Skip copyright screen: NOP call CLoadingScreen::DoPCTitleFadeIn
//                                       (0x590990) / ...FadeOut (0x590860).                 MTA:1313-1315
// [#6]      0x748CF6                    Disable the loading screen tune.                    MTA:1390
// [#7-9]    0x590D7C,0x590DB3,0x590D9F  Do not render the loading screen (CLoadingScreen::NewChunkLoaded
//                                       @0x590D00; +0x9F becomes an early RET).             MTA:1393-1396
// [#10]     0x7459E1                    Allow all screen aspect ratios in multi-monitor dialog. MTA:1364
// [#157]    0x745BC9                    Allow all screen aspect ratios.                     MTA:1362
// [#11]     0x748054                    Don't clear g_bGotFocus when minimising (ADDR_GotFocus). MTA:57,788
// [#94]     0x748A8D                    ALLOW ALT+TABBING WITHOUT PAUSING.                  MTA:1077
// [#107]    0x53BC78                    Disable the pause menu appearing after alt+tab
//                                       (imm of `mov byte [0xBA677B],1` -> 0).              MTA:1123
// [#193]    0x53E94C                    Remove the 14ms inter-frame wait in Idle.           MTA:1385
// [#190-192] 0x576CCC,0x576EBA,0x576F8A Disable the game re-initialising the DirectInput mouse. MTA:1377
// [#261]    0xBAB318                    CLoadingScreen::g_bActive = 0.                      SA-MP:745
// [#224]    0x406946                    [dis] `push 0x858AD4` ("CdStream") -> `push 0`: make the CdStream
//                                       kernel object UNNAMED so a second instance can't collide.
// [#225]    0x74872D                    [dis] NOP 9 over `call 0x7468E0; test eax,eax; jne 0x748745`.
//                                       0x7468E0 = CreateEventA + GetLastError()==0xB7 (ERROR_ALREADY_EXISTS)
//                                       -> removes the single-instance guard. Multi-instance patch.

// ============ Pools / streaming limits ============
// [#12]     0x550F82                    Increase "PtrNode Double" pool 3200 -> 8000.        SA-MP:339
// [#13]     0x8A5A84                    CStreaming::desiredNumVehiclesLoaded = 127.         MTA:629
// [#50-55]  0x5B8342 (6 bytes)          Increase VehicleStruct pool: force capacity to 255
//                                       (`xor eax,eax; mov al,0FFh; mov edi,eax`).          MTA:980
// [#118-119] 0x550FBA, 0x550FBB         Increase "EntryInfoNode" pool (default 500) -> 0x1000. SA-MP:370
// [#211]    0x40E7DF                    Skip CStreaming::StreamVehiclesAndPeds / StreamZoneModels. MTA:1434
// [#221]    0x40B650                    Disable CStreaming::StreamVehiclesAndPeds_Always.   MTA:1456

// ============ Audio ============
// [#16-18]  0x4E9820                    `ret 8` over the global func CAudio::StopRadio jumps to. MTA:791
// [#19-21]  0x4DBEC0                    `ret 0` over CAudio::StartRadio.                    MTA:798
// [#22-24]  0x4EB3C0                    `ret 0x10` over CAERadioTrackManager::StartRadio.    MTA:803
// [#31]     0x4E8410                    Disable CAERadioTrackManager::CheckForMissionStatsChanges
//                                       (special DJ banter).                                MTA:840
// [#59]     0x4DCF87                    NOP call FxSystem_c::GetCompositeMatrix in
//                                       CAEFireAudioEntity::UpdateParameters.   MTA:896 -- SEE SECTION 4
// [#189]    0x4F9CCE                    Force the MrWhoopee music to load even if we are not the
//                                       driver ([dis] retargets `call 0x4D88C0` -> 0x4D88A0). MTA:1374
// [#233-235] 0x5023B2,0x5023E1,0x502341 Disable vehicle audio driver logic (MTA #9681):
//                                       m_bPlayerDriver update, JustGotIn/OutOfVehicleAsDriver. MTA:1505

// ============ Single-player world logic (disabled for MP) ============
// [#14]     0x442AD0                    DISABLE CGameLogic::Update.                         MTA:632
// [#26]     0x72DF0D                    DISABLE wanted levels for military zones.           MTA:814
// [#30]     0x4629E0                    DISABLE CRoadBlocks::GenerateRoadblocks.            MTA:824
// [#32-34]  0x614720                    DISABLE CPopulation::AddToPopulation (`xor al,al; ret`). MTA:843
// [#43]     0x441770                    DISABLE CGameLogic::SetPlayerWantedLevelForForbiddenTerritories. MTA:869
// [#44]     0x532010                    DISABLE CCrime::ReportCrime.                        MTA:872
// [#56-58]  0x56E870                    Disable CPlayerInfo::MakePlayerSafe (`ret 8`).      MTA:891
// [#67-69]  0x460390,0x4600F0,0x45F050  DISABLE PLAYING REPLAYS.                            MTA:996
// [#215]    0x460500                    (also replays)                        MTA:949 -- SEE SECTION 4
// [#70-71]  0x439AF0, 0x438370          DISABLE CHEATS.                                     MTA:1003
// [#72-77]  0x44AA89 (6 bytes)          DISABLE GARAGES: `jmp +0x128` (-> 0x44ABB6) + NOP.  MTA:1008
// [#78]     0x44C7E0 .. 0x44C8B0        DISABLE GARAGES: 20-slot dispatch table -> 0x44C7C4 stub. MTA:1016
// [#79-84]  0x44C39A (6 bytes)          DISABLE GARAGES: `jz +0x424` -> the 0x44C7C4 stub.  MTA:1037
// [#210]    0x44C6FA                    Allow Player Garages to shut with players inside.   MTA:1431
// [#86]     0x55C180                    Disable CStats::IncrementStat.                      MTA:1047
// [#87-88]  0x559FD5, 0x559FEB          DISABLE STATS DECREMENTING.                         MTA:1054
// [#89-90]  0x55B980, 0x559760          DISABLE STATS MESSAGES.                             MTA:1058
// [#100]    0x5A07D0                    DISABLE SAM SITES.                                  MTA:1098
// [#101-102] 0x6F7900, 0x6F7865         DISABLE auto-generated TRAINS / trains with PEDs.   MTA:1101,1104
// [#103]    0x6CD2F0                    DISABLE PLANES.                                     MTA:1110
// [#104]    0x42B7D0                    DISABLE EMERGENCY VEHICLES.                         MTA:1113
// [#105]    0x6F3F40                    DISABLE CAR GENERATORS.                             MTA:1116
// [#106]    0x440D10                    DISABLE CEntryExitManager::Update.                  MTA:1119
// [#108]    0x56E740                    DISABLE HUNGER MESSAGES.                            MTA:1126
// [#109]    0x6B0BC2                    DISABLE RANDOM VEHICLE UPGRADES.                    MTA:1129
// [#114]    0x5B47B0                    DISABLE weapon pickups.                             MTA:1174
// [#115]    0x42CD10                    Prevent the game deleting any far-away vehicles.    MTA:1171
// [#120-121] 0x561FA4, 0x561FA5         DISABLE CWanted helis (CWanted::NumOfHelisRequired -> 0). MTA:1200
// [#122]    0x53BFF6                    DISABLE CWanted::UpdateEachFrame.                   MTA:1204
// [#123]    0x60EBCC                    DISABLE CWanted::Update.                            MTA:1207
// [#125]    0x591F90                    Remove the last weapon pickups from interiors too.  MTA:1213
// [#126]    0x4418E0                    Trains may in fact go further than Los Santos.      MTA:1216
// [#141]    0x59FAA3                    Create pickup objects in interior 0 instead of 13.  MTA:1255
// [#194]    0x53C127                    Disable ped-to-player conversations.                MTA:1398
// [#212]    0x611FC0                    Disable CPopulation::ManagePed.                     MTA:1437
// [#213-214] 0x616698, 0x616699         Stop CPopulation::Update after the ManagePopulation call. MTA:1439
// [#216]    0x605A30                    Disable CInterestingEvents::ScanForNearbyEntities.  MTA:1445
// [#217]    0x446610                    Disable CGangWars::Update.                          MTA:1447
// [#218]    0x43C590                    Disable CConversations::Update.                     MTA:1449
// [#219]    0x43B0F0                    Disable CPedToPlayerConversations::Update.          MTA:1451
// [#220]    0x4322B0                    Disable CCarCtrl::RemoveCarsIfThePoolGetsFull.      MTA:1453
// [#222-223] 0x468EB5, 0x468EB6         STOP IT TRYING TO LOAD THE SCM.                     MTA:636
// [#226]    0x56A404 (0x42 bytes)       Disable CFireManager::ExtinguishPoint /
//                                       CWorld::ExtinguishAllCarFiresInArea calls from
//                                       CWorld::ClearExcitingStuffFromArea.                 MTA:1471
// [#110-111] 0x53C017, 0x53C018         Peds always walk around, even in free-camera mode.  MTA:1137

// ============ Peds / tasks / weapons ============
// [#27-28]  0x742685, 0x742686          THROWN projectiles throw more accurately (0F8x -> 90 E9). MTA:817
// [#29]     0x7399B0                    DISABLE CProjectileInfo::RemoveAllProjectiles.      MTA:821
// [#35-41]  0x620607..0x62061C          CTaskSimpleChoking: force 0 time remaining (two
//                                       `xor eax,eax` + 3 NOPs over `call 0x407180`).       MTA:854
// [#42]     0x61EFFE                    Non-local players always update aim on akimbo weapons
//                                       (CTaskSimpleUseGun::AimGun, je -> jmp).             MTA:865
// [#60-61]  0x6A436C, 0x6A436D          ALLOW picking up of all vehicles.                   MTA:910
// [#62]     0x621983                    Players always lean out whatever the camera mode.   MTA:961
// [#63-64]  0x627E01, 0x62840D          Players can fire drivebys whatever the camera mode. MTA:965
// [#65]     0x738F3A (83 bytes)         Satchel crash fix in CProjectileInfo::Update
//                                       (0xC891A8+4 nulled on player death).                MTA:971
// [#66]     0x6B5B17                    Prevent GTA stopping driveby players from falling off. MTA:977
// [#91]     0x5FBA26                    PREVENT CJ smoking and drinking like an addict.     MTA:1069
// [#124]    0x6D189B                    Disable armour increase upon entering an Enforcer.  MTA:1210
// [#127-129] 0x632140                   CTaskComplexSunbathe::CanSunbathe always returns true. MTA:1222
// [#130-131] 0x644C18, 0x644C19         Stop CTaskSimpleCarDrive::ProcessPed exiting passengers
//                                       with CTaskComplexSequence.                          MTA:1227
// [#134-135] 0x741FD0                   Disable CVehicle::DoDriveByShootings.               MTA:1237
// [#136-138] 0x6872C0                   Disable CTaskSimplePlayerOnFoot::PlayIdleAnimations (`ret 4`). MTA:1241
// [#139-140] 0x55E870, 0x55E874         Let us sprint everywhere (CSurfaceData::isSprint -> 0). MTA:1251
// [#147]    0x73FDF9                    Instant-hit bullets: stop ignoring the first few bullets
//                                       from remote players after reloading.                MTA:1272
// [#150-153] 0x5E1E72..0x5E1E77         Fix sliding over objects and vehicles (ice floor).  MTA:1283
// [#159-160] 0x633695, 0x633720         Force the damage event to trigger for players on fire. MTA:1317
// [#161-165] 0x53A459,0x53A568,0x53A4A9,0x53A55F,0x73EC06
//                                       CCreepingFire::TryToStartFireAtCoors returns the fire
//                                       pointer rather than a bool.                         MTA:1321
// [#168-179] 0x685DFB,0x685C3E,0x685DC4,0x685DE6 (4 x [NOP5 + `xor eax,eax`])
//                                       Disable stealth-kill aiming (holding the knife up). MTA:1335
// [#180-188] 0x62E63F,0x62E659,0x62E692 (3 x [NOP6 + `fstp st(0)`])
//                                       Disable stealth-kill rotation in
//                                       CTaskSimpleStealthKill::ProcessPed (MTA #4937).     MTA:1350
// [#198-201] 0x5FFAEE,0x5FFB4B,0x5FFBA2,0x5FFC00
//                                       Fix melee not working outside the world bounds.     MTA:1412
// [#202-205] 0x7361BF,0x7361D4,0x7361E9,0x7361FE
//                                       Fix sniper not firing outside the world bounds.     MTA:1418
// [#227]    0x53A651                    Don't set the occupied vehicle's health to 75.0f when a
//                                       burning ped enters it (CFire::ProcessFire).         MTA:1475
// [#232]    0x63F576                    Fix killing a ped during carjacking (MTA #4319).    MTA:1501
// [#236]    0x60D861                    Allow switching weapons while glued.                MTA:1512
// [#241]    0x61ECD2                    Show muzzle flash for the last bullet in the magazine. MTA:1535
// [#250]    0x60D86F                    Allow switching weapon during the jetpack task (MTA #3569). MTA:1569
// [#259-260] 0x53A23F, 0x53A00A         Disable spreading fires.                            MTA:1583

// ============ Vehicles ============
// [#112-113] 0x6F2089, 0x6F208A         HACK to allow boats to be rotated.                  MTA:1167
// [#142-144] 0x6D19CD,0x6D1A1A,0x6D1762 No shotguns from police cars / golf clubs from caddies /
//                                       20 health from ambulances.                          MTA:1258
// [#145-146] 0x6F701D, 0x6F7069         Prevent CVehicle::RecalcTrainRailPosition changing train speed. MTA:1267
// [#148-149] 0x6E1DBC, 0x6E1D4F         Allow vehicle lights with the engine off; fix both back
//                                       lights using light state 3.                         MTA:1277,1280
// [#154]    0x6D65C5                    Don't reset vehicle colour 1 to white after a paintjob. MTA:1289
// [#206-209] 0x6E2FBC,0x6E301C,0x6E3075,0x6E30D6
//                                       Fix heli blades lacking collision outside world bounds. MTA:1424
// [#228]    0x6D1741                    Prevent the +$12 when entering a taxi/cabbie (MTA #8332). MTA:1478
// [#229]    0x6E1A22                    Increase intensity of the vehicle tail-light corona. MTA:1481
// [#230-231] 0x6D6517, 0x6D0E43         Skip the vehicle-type check in CVehicle::SetupRender /
//                                       ResetAfterRender (MTA #8158).                       MTA:1496
// [#251]    0x6E1425                    Fix invisible vehicle windows when lights are on (MTA #2936). MTA:1572
// [#252-253] 0x6C444B, 0x6C4453         Allow alpha change for the helicopter rotor (MTA #523). MTA:1575

// ============ Camera ============
// [#25]     0x52A535                    Disable cinematic camera for trains.                SA-MP:788
// [#92-93]  0x522423, 0x522424          Prevent the camera messing up for driver drivebys.  MTA:1073
// [#155]    0x522C80                    Disable the idle cam.                               MTA:1299
// [#166-167] 0x52A2BB, 0x52A4F8         Don't fixate the camera behind a spectated player when
//                                       the local player is dead.                           MTA:1328
// [#195-197] 0x41AD12,0x41ADA7,0x41ADF3 Clip the camera outside the world bounds too.       MTA:1406
// [#246-247] 0x524084, 0x524089         Allow vertical camera movement during a fade (MTA #411). MTA:1561
// [#156]    0x53E9C6                    Ignore the camera fade state in the rendering routine.
//                                       [dis] NOPs `je 0x53EB19` after TheCamera(0xB6F028) fade
//                                       check. Makes MORE render, not less.                 MTA:1302

// ============ HUD / 2D ============
// [#95-99]  0x58B0AE,0x58AD56,0x85953C,0x58B149,0x58AE52
//                                       CENTER the vehicle-name and zone-name messages: push 0
//                                       instead of 2 (orientation), store 320.0f at 0x85953C,
//                                       and redirect both FMULs from [0x85950C] to [0x85953C]. MTA:1080
// [#158]    0x58FC3E                    Don't hide the radar map when pressing TAB on foot. MTA:1305
// [#240]    0x58FBC4                    Skip the check for a disabled HUD.                  MTA:1532

// ============ Render (none of these touch sky / fog / timecycle) ============
// [#237-238] 0x72925D, 0x729263         Let the water cannon hit objects and players visually. MTA:1515
// [#239]    0x6FB9A0                    Fix corona rain reflections (zBufferFar instead of
//                                       zBufferNear) (MTA #2345).                           MTA:1528
// [#242-243] 0x7069F5, 0x7069FE         CRealTimeShadowManager::GetRealTimeShadow: process every
//                                       ped like a non-player ped; iterate the 16-slot shadow
//                                       array from 0 instead of 1.                          MTA:1541,1546
// [#244-245] 0x70A83B, 0x70A4CB         Skip an entity-flag check in CShadows::CastRealTimeShadow-
//                                       SectorList / CastPlayerShadowSectorList.            MTA:1553,1557
// [#248-249] 0x7225F5, 0x725DDE         Allow alpha change for arrow & checkpoint markers
//                                       (C3dMarker::Render 0x7223D0) (MTA #1860).           MTA:1565
// [#254-258] 0x725844,0x725619,0x72565A,0x7259B0,0x7258B8
//                                       [dis] All five are inside C3dMarkers::PlaceMarker (0x725120,
//                                       named in MTA C3DMarkersSA.h:17). They suppress the marker
//                                       Z-coordinate stores (`fstp [ebx+8]` / `fstp [esi+0x38]` ->
//                                       `fstp st(0); nop`, and `mov [esi+0x38],edx` -> NOP6), so a
//                                       marker keeps the exact Z the caller asked for instead of
//                                       the engine's ground-snap. NOT in MTA or SA-MP.
// [#132-133] 0x5E8E84, 0x6D29CB         Stop CPlayerPed::ProcessControl / CVehicle::UpdateClumpAlpha
//                                       calling CVisibilityPlugins::SetClumpAlpha.          MTA:1231,1234

// ============ Crash fixes / misc ============
// [#15]     0x731AB5                    [dis] CTxdStore::GetNumRefs (0x731AA0): the free-slot branch
//                                       is `xor eax,eax` followed by `movsx eax, word [eax+4]` -- a
//                                       near-NULL read. NOP 4 leaves `xor eax,eax; ret`, i.e. an
//                                       invalid TXD slot now reports 0 refs. Not in MTA/SA-MP.
// [#85]     0x4486F7                    [dis] Inside the garage state machine at 0x4486C0 (dispatch
//                                       on [ecx+0x4C] via the table at 0x4486FC). NOP 4 removes
//                                       `mov byte [ecx+0x4D], 2`, leaving a bare RET. Garage module
//                                       by containment; not in MTA/SA-MP.
// [#116]    0x5E68A0                    [dis] `je 0x5E6908` -> `jmp`: unconditionally skip
//                                       `mov ecx,0xC40350; call 0x706BA0` (CRealTimeShadowManager).
//                                       Not in MTA/SA-MP.
// [#117]    0x542483                    [dis] `je 0x542490` -> `jmp`: skip `push eax; mov ecx,
//                                       0xC40350; call 0x705B30` (CRealTimeShadowManager).
//                                       Not in MTA/SA-MP. SEE SECTION 4 -- duplicates the NOP-11
//                                       at 0x542485 in ApplyCrashFixes().
// [#45-49]  0x4C01F0 (5 bytes)          `call 0x4ADBF0` -> `mov al,0` + 3 NOPs, forcing the
//                                       callee's bool result to false. MTA applies the identical
//                                       bytes with no comment; subsystem not identified.     MTA:885
```

---

## 4. Alarming

**1. The header comment at `patches.cpp:276-296` is false and is actively misdirecting the investigation.** "262 undocumented byte patches… a third of the addresses fall inside CTimeCycle / CRenderer / CClouds / CVisibilityPlugins / CWaterLevel." It's 261 patches, 251 of them are individually documented upstream, and the count inside CTimeCycle/CClouds/CWeather/CColourSet is **zero**. Delete it or replace it with §3.

**2. `#59` (`0x4DCF87`) is a patch MTA deliberately reverted, and OpenSAMP still applies it.** `CMultiplayerSA.cpp:899`: `// MemSet ((void*)0x4DCF87,0x90,6);` with `// The above MemSet was commented out because of mantis#8590, gh#124, see c20d2adc5`. The NOP kills `push eax; call FxSystem_c::GetCompositeMatrix` but leaves the preceding `lea eax,[esp+8]`, so the matrix buffer at `[esp+8]` is consumed uninitialised from `0x4DCF8D` onward. Remove it.

**3. `#215` (`0x460500`) is also inside a `/* */` block in MTA** (`CMultiplayerSA.cpp:949`, "DISABLE REPLAYS"). OpenSAMP applies it live. Lower severity than #2 but the same class of problem: dead upstream code was copied as live code. These are the *only two* such cases — I checked all 261 against MTA's comment state programmatically.

**4. Real-time shadows are disabled four times over, while four MTA patches that *improve* real-time shadows are applied on top.** Disabled by: `#116` (`0x5E68A0` je→jmp), `#117` (`0x542483` je→jmp), `ApplyCrashFixes` `0x53EA08` NOP-10 (kills `call 0x706AB0`, the manager's per-frame update), `ApplyCrashFixes` `0x542485` NOP-11. Enhanced by: `#242`, `#243`, `#244`, `#245` (`CRealTimeShadowManager::GetRealTimeShadow` + `CShadows::Cast*SectorList`). The four fixes are dead code. Additionally **`#117` and `ApplyCrashFixes` line 139 are the same fix twice** — `#117` turns `je 0x542490` into `jmp 0x542490` and line 139 then NOPs the exact 11 bytes that jump skips.

**5. Two more redundant double-kills.** `#105` RETs `0x6F3F40` (`CTheCarGenerators`), and `ApplyBehaviorPatches` line 237 *also* NOPs the call site at `0x53C06A`. `#215` RETs `0x460500`, and line 238 *also* NOPs its call site at `0x53C090`. Both verified by disassembly of `CGame::Process`; both harmless but they mean toggling one of the pair proves nothing during bisection.

**6. `SetWeather()` (`native/game/game.h:373`) writes the wrong global at the wrong width.** See §1 S3. `0xC81318` is `ForcedWeatherType`, read by the engine as a *signed short* whose "inactive" value is `0xFFFF`; a byte write can never clear the sign bit. MTA writes `0xFF` there to *release* and never a weather id. All three should be `short`, and `0xC81318` should not be written at all by a `SetWeather`.

**7. Bisection with a limit inside a byte-group corrupts the instruction stream.** `LP()` gates each *byte*, not each *patch*. A limit of e.g. 52 leaves `0x5B8342` holding `33 C0 B0` followed by the original `8B` — a different instruction than either the original or the patched form. See the forbidden-cutoff list in §2. Ideally, change `LP()` to gate logical patches rather than writes.

**8. `native/patches.cpp:257` is mislabelled.** `mem_set(0x575B0E, 0x90, 5); // remove blue fog from map` is *not* world fog. SA-MP's own source: `// Remove the blue(-ish) fog in the map`, and `0x575B0E` sits between two named `CMenuManager` methods (`0x575130`, `0x576320`). It is the **pause-menu map screen**. Rename it before the next person chases it.

**9. Fragile page-protection coupling.** `ApplyCrashFixes`, `ApplyIntroSkipPatches`, `ApplyPoolLimits` and `ApplyFilesystemPatches` use raw `std::memcpy`/`std::memset` into `.text` and `.rdata` (`0x7271CB`, `0x866CD8`, `0x849AB4`, `0x551024`, …). Those have no protection of their own — they work only because `ApplyLegacyBasePatches` did `VirtualProtect(0x401000, 0x4A3000, …)` first, which happens to end at exactly `0x8A4000`, the end of `.rdata`. `opensamp_patchlimit.txt = 0` is safe (the `VirtualProtect` precedes the first `LP`), but **deleting the legacy block would AV every one of them**. Route them through `mem_put`/`mem_set`, which do their own `VirtualProtect` + SEH (`gta/common.h:94`).

**10. One thing that is *not* alarming, for the record.** I fully decoded OpenSAMP's SilentPatch "mirrors" patch at `0x7271CB` (`patches.cpp:171`) since it lands inside the mirror render pass at `0x727140` — the function called from `Idle` at `0x53EA12`. It rewrites `add esp,4; test eax,eax; je 0x727222` as `test eax,eax; je 0x727203; add esp,4`. On the `RwCameraBeginUpdate` failure path it now restores `[RwCamera+0x60]/[+0x64]` from `esi`/`edi` and calls `0x51A5A0` before returning — which the original skipped, leaving the mirror raster bound to the main camera. The skipped `add esp,4` is compensated by the one at `0x727212`. **Stack-balanced and correct.**
