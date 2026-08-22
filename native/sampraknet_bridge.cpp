#include "sampraknet_bridge.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>
#include <RakNetworkFactory.h>
#include <random>
#include <SAMPAuth.h>
#include <StringCompressor.h>

#include "RakClientInterface.h"
#include "PacketEnumerations.h" // ID_* (RakNet)
#include "BitStream.h"
#include "gui/chat.hpp"
#include "gui/dialog.hpp"
#include "game/game.h"
#include "util/encoding.h"

using PLAYERID = unsigned short;
using VEHICLEID = unsigned short;

constexpr int MAX_PLAYERS = 1000;
constexpr int MAX_VEHICLES = 2000;
int g_myPlayerID = -1;

// Network id of the vehicle the local player is currently seated in, plus the
// seat index, as told to us by the server via ScrPutPlayerInVehicle (RPC 70).
// 0xFFFF == "on foot / unknown". Milestone-1 limitation: this is only set for
// server-seated entry; player-driven entry needs a CVehicle*<->VEHICLEID
// registry fed by WorldVehicleAdd (deferred). Seat 0 == driver.
constexpr VEHICLEID INVALID_VEHICLE_ID = 0xFFFF;
VEHICLEID g_localVehicleId = INVALID_VEHICLE_ID;
BYTE      g_localSeatId    = 0;
constexpr int MAX_PLAYER_NAME = 24;
char g_szNickName[MAX_PLAYER_NAME] = {0};

#define NETGAME_VERSION 4057

static int RPC_ServerJoin = 137;
static int RPC_ServerQuit = 138;
static int RPC_InitGame = 139;
static int RPC_ClientJoin = 25;
static int RPC_NPCJoin = 54;
static int RPC_Death = 53;
static int RPC_RequestClass = 128;
static int RPC_RequestSpawn = 129;
static int RPC_SetInteriorId = 118;
static int RPC_Spawn = 52;
static int RPC_Chat = 101;
static int RPC_EnterVehicle = 26;
static int RPC_ExitVehicle = 154;
static int RPC_DamageVehicle = 106;
static int RPC_MenuSelect = 132;
static int RPC_MenuQuit = 140;
static int RPC_ScmEvent = 96;
static int RPC_AdminMapTeleport = 255;
static int RPC_WorldPlayerAdd = 32;
static int RPC_WorldPlayerDeath = 166;
static int RPC_WorldPlayerRemove = 163;
static int RPC_WorldVehicleAdd = 164;
static int RPC_WorldVehicleRemove = 165;
static int RPC_SetCheckpoint = 107;
static int RPC_DisableCheckpoint = 37;
static int RPC_SetRaceCheckpoint = 38;
static int RPC_DisableRaceCheckpoint = 39;
static int RPC_UpdateScoresPingsIPs = 155;
static int RPC_SvrStats = 102;
static int RPC_GameModeRestart = 40;
static int RPC_ConnectionRejected = 130;
static int RPC_ClientMessage = 93;
static int RPC_WorldTime = 94;
static int RPC_Pickup = 95;
static int RPC_DestroyPickup = 63;
static int RPC_DestroyWeaponPickup = 97;
static int RPC_Weather = 152;
static int RPC_SetTimeEx = 255;
static int RPC_ToggleClock = 30;
static int RPC_ServerCommand = 50;
static int RPC_PickedUpPickup = 131;
static int RPC_PickedUpWeapon = 255;
static int RPC_VehicleDestroyed = 136;
static int RPC_DialogResponse = 62;
static int RPC_PlayAudioStream = 41;
static int RPC_StopAudioStream = 42;
static int RPC_ClickPlayer = 23;
static int RPC_PlayerUpdate = 60;
static int RPC_ClickTextDraw = 83;
static int RPC_MapMarker = 119;
static int RPC_PlayerGiveTakeDamage = 115;
static int RPC_EnterEditObject = 27;
static int RPC_EditObject = 117;

static int RPC_ScrSetSpawnInfo = 68;
static int RPC_ScrSetPlayerTeam = 69;
static int RPC_ScrSetPlayerSkin = 153;
static int RPC_ScrSetPlayerName = 11;
static int RPC_ScrSetPlayerPos = 12;
static int RPC_ScrSetPlayerPosFindZ = 13;
static int RPC_ScrSetPlayerHealth = 14;
static int RPC_ScrPutPlayerInVehicle = 70;
static int RPC_ScrRemovePlayerFromVehicle = 71;
static int RPC_ScrSetPlayerColor = 72;
static int RPC_ScrDisplayGameText = 73;
static int RPC_ScrSetInterior = 156;
static int RPC_ScrSetCameraPos = 157;
static int RPC_ScrSetCameraLookAt = 158;
static int RPC_ScrSetVehiclePos = 159;
static int RPC_ScrSetVehicleZAngle = 160;
static int RPC_ScrVehicleParams = 161;
static int RPC_ScrSetCameraBehindPlayer = 162;
static int RPC_ScrTogglePlayerControllable = 15;
static int RPC_ScrPlaySound = 16;
static int RPC_ScrSetWorldBounds = 17;
static int RPC_ScrHaveSomeMoney = 18;
static int RPC_ScrSetPlayerFacingAngle = 19;
static int RPC_ScrResetMoney = 20;
static int RPC_ScrResetPlayerWeapons = 21;
static int RPC_ScrGivePlayerWeapon = 22;
static int RPC_ScrRespawnVehicle = 255;
static int RPC_ScrLinkVehicle = 65;
static int RPC_ScrSetPlayerArmour = 66;
static int RPC_ScrDeathMessage = 55;
static int RPC_ScrSetMapIcon = 56;
static int RPC_ScrDisableMapIcon = 144;
static int RPC_ScrSetWeaponAmmo = 145;
static int RPC_ScrSetGravity = 146;
static int RPC_ScrSetVehicleHealth = 147;
static int RPC_ScrAttachTrailerToVehicle = 148;
static int RPC_ScrDetachTrailerFromVehicle = 149;
static int RPC_ScrCreateObject = 44;
static int RPC_ScrSetObjectPos = 45;
static int RPC_ScrSetObjectRotation = 46;
static int RPC_ScrDestroyObject = 47;
static int RPC_ScrCreateExplosion = 79;
static int RPC_ScrShowNameTag = 80;
static int RPC_ScrMoveObject = 99;
static int RPC_ScrStopObject = 122;
static int RPC_ScrNumberPlate = 123;
static int RPC_ScrTogglePlayerSpectating = 124;
static int RPC_ScrSetPlayerSpectating = 255;
static int RPC_ScrPlayerSpectatePlayer = 126;
static int RPC_ScrPlayerSpectateVehicle = 127;
static int RPC_ScrRemoveComponent = 57;
static int RPC_ScrForceSpawnSelection = 74;
static int RPC_ScrAttachObjectToPlayer = 75;
static int RPC_ScrInitMenu = 76;
static int RPC_ScrShowMenu = 77;
static int RPC_ScrHideMenu = 78;
static int RPC_ScrSetPlayerWantedLevel = 133;
static int RPC_ScrShowTextDraw = 134;
static int RPC_ScrHideTextDraw = 135;
static int RPC_ScrEditTextDraw = 105;
static int RPC_ScrAddGangZone = 108;
static int RPC_ScrRemoveGangZone = 120;
static int RPC_ScrFlashGangZone = 121;
static int RPC_ScrStopFlashGangZone = 85;
static int RPC_ScrApplyAnimation = 86;
static int RPC_ScrClearAnimations = 87;
static int RPC_ScrSetSpecialAction = 88;
static int RPC_ScrEnableStuntBonus = 104;
static int RPC_ScrSetFightingStyle = 89;
static int RPC_ScrSetPlayerVelocity = 90;
static int RPC_ScrSetVehicleVelocity = 91;
static int RPC_ScrToggleWidescreen = 255;
static int RPC_ScrSetVehicleTireStatus = 255;
static int RPC_ScrSetPlayerDrunkLevel = 35;
static int RPC_ScrDialogBox = 61;
static int RPC_ScrCreate3DTextLabel = 36;

RakClientInterface* pRakClient = nullptr;

DWORD dwTimeReconnect = 10000;

int iAreWeConnected = 0;

int iPassengerNotificationSent = 0, iDriverNotificationSent = 0;

#pragma pack(1)
using ONFOOT_SYNC_DATA = struct _ONFOOT_SYNC_DATA
{
    WORD lrAnalog;
    WORD udAnalog;
    WORD wKeys;
    float vecPos[3];
    float fQuaternion[4];
    BYTE byteHealth;
    BYTE byteArmour;
    BYTE byteCurrentWeapon;
    BYTE byteSpecialAction;
    float vecMoveSpeed[3];
    float vecSurfOffsets[3];
    WORD wSurfInfo;
    int iCurrentAnimationID;
};

#pragma pack(1)
using INCAR_SYNC_DATA = struct _INCAR_SYNC_DATA
{
    VEHICLEID VehicleID;
    WORD lrAnalog;
    WORD udAnalog;
    WORD wKeys;
    float fQuaternion[4];
    float vecPos[3];
    float vecMoveSpeed[3];
    float fCarHealth;
    BYTE bytePlayerHealth;
    BYTE bytePlayerArmour;
    BYTE byteCurrentWeapon;
    BYTE byteSirenOn;
    BYTE byteLandingGearState;
    WORD TrailerID_or_ThrustAngle;
    FLOAT fTrainSpeed;
};

#pragma pack(1)
using PASSENGER_SYNC_DATA = struct _PASSENGER_SYNC_DATA
{
    VEHICLEID VehicleID;
    BYTE byteSeatFlags : 7;
    BYTE byteDriveBy : 1;
    BYTE byteCurrentWeapon;
    BYTE bytePlayerHealth;
    BYTE bytePlayerArmour;
    WORD lrAnalog;
    WORD udAnalog;
    WORD wKeys;
    float vecPos[3];
};

enum eWeaponState
{
    WS_NO_BULLETS = 0,
    WS_LAST_BULLET = 1,
    WS_MORE_BULLETS = 2,
    WS_RELOADING = 3,
};

#pragma pack(1)
using AIM_SYNC_DATA = struct _AIM_SYNC_DATA
{
    BYTE byteCamMode;
    float vecAimf1[3];
    float vecAimPos[3];
    float fAimZ;
    BYTE byteCamExtZoom : 6; // 0-63 normalized
    BYTE byteWeaponState : 2; // see eWeaponState
    BYTE bUnk;
};

#pragma pack(1)
using UNOCCUPIED_SYNC_DATA = struct _UNOCCUPIED_SYNC_DATA // 67
{
    VEHICLEID VehicleID;
    short cvecRoll[3];
    short cvecDirection[3];
    BYTE unk[13];
    float vecPos[3];
    float vecMoveSpeed[3];
    float vecTurnSpeed[3];
    float fHealth;
};

#pragma pack(1)
using SPECTATOR_SYNC_DATA = struct _SPECTATOR_SYNC_DATA
{
    WORD lrAnalog;
    WORD udAnalog;
    WORD wKeys;
    float vecPos[3];
};

#pragma pack(1)
using BULLET_SYNC_DATA = struct _BULLET_SYNC_DATA
{
    BYTE bHitType;
    unsigned short iHitID;
    float fHitOrigin[3];
    float fHitTarget[3];
    float fCenterOfHit[3];
    BYTE bWeaponID;
};

#pragma pack(1)
using PLAYER_SPAWN_INFO = struct _PLAYER_SPAWN_INFO
{
    BYTE byteTeam;
    int iSkin;
    BYTE unk;
    float vecPos[3];
    float fRotation;
    int iSpawnWeapons[3];
    int iSpawnWeaponsAmmo[3];
};

#pragma pack(1)
using PICKUP = struct _PICKUP
{
    int iModel;
    int iType;
    float fX;
    float fY;
    float fZ;
};

using stPlayerInfo = struct stPlayerInfo
{
    char szPlayerName[MAX_PLAYER_NAME];
    int iIsConnected;

    ONFOOT_SYNC_DATA onfootData;
    INCAR_SYNC_DATA incarData;
    UNOCCUPIED_SYNC_DATA unocData;
    AIM_SYNC_DATA aimData;
    PASSENGER_SYNC_DATA passengerData;
    BULLET_SYNC_DATA bulletData;

    int iGotMarkersPos;
    int iIsStreamedIn;

    int iScore;
    DWORD dwPing;
    int iAreWeInAVehicle;
    BYTE byteInteriorId;
    BYTE byteIsNPC;
};
stPlayerInfo playerInfo[MAX_PLAYERS];

// Persistent log handle. Opened lazily on first Log() call. Writes are
// flushed after every line so a crash can be diagnosed post-mortem from
// OpenSamp.log.
static HANDLE g_persistentLog = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_persistentLogCs;
static bool g_persistentLogInit = false;

static void EnsurePersistentLogOpen()
{
    if (g_persistentLogInit) return;
    InitializeCriticalSection(&g_persistentLogCs);
    g_persistentLog = CreateFileA(
        "OpenSamp.log",
        FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_persistentLog != INVALID_HANDLE_VALUE)
    {
        const char* hdr = "---- OpenSamp session ----\r\n";
        DWORD w;
        WriteFile(g_persistentLog, hdr, (DWORD)strlen(hdr), &w, nullptr);
    }
    g_persistentLogInit = true;
}

void Log(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int offset = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (offset < 0) return;

    g_chat.PushLineAsync(buffer);

    // Mirror to OpenSamp.log. Writes are cached (no per-line flush) — a sync
    // on every call used to stall the game for seconds under high RPC rates.
    // Crash dumps give us the smoking gun; the log just needs to be mostly
    // intact, not byte-perfect up to the failure.
    EnsurePersistentLogOpen();
    if (g_persistentLog != INVALID_HANDLE_VALUE)
    {
        EnterCriticalSection(&g_persistentLogCs);
        DWORD w;
        WriteFile(g_persistentLog, buffer, (DWORD)offset, &w, nullptr);
        WriteFile(g_persistentLog, "\r\n", 2, &w, nullptr);
        LeaveCriticalSection(&g_persistentLogCs);
    }
}

void Packet_AUTH_KEY(Packet* p, RakClientInterface* pRakClient)
{
    char* auth_key = nullptr;
    bool found_key = false;

    for (int x = 0; x < 512; x++)
    {
        if (!strcmp(((char*)p->data + 2), AuthKeyTable[x][0]))
        {
            auth_key = AuthKeyTable[x][1];
            found_key = true;
        }
    }

    if (found_key)
    {
        RakNet::BitStream bsKey;
        BYTE byteAuthKeyLen;

        byteAuthKeyLen = static_cast<BYTE>(strlen(auth_key));

        bsKey.Write(static_cast<BYTE>(ID_AUTH_KEY));
        bsKey.Write(byteAuthKeyLen);
        bsKey.Write(auth_key, byteAuthKeyLen);

        pRakClient->Send(&bsKey, SYSTEM_PRIORITY, RELIABLE, NULL);

        //Log("[AUTH] %s -> %s", ((char*)p->data + 2), auth_key);
    }
    else
    {
        Log("Unknown AUTH_IN! (%s)", ((char*)p->data + 2));
    }
}

static inline uint32_t bswap32(uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0x0000FF00u) | ((v << 8) & 0x00FF0000u) | (v << 24);
}

static void BIG_NUM_MUL_SAFE(const uint8_t inBytes[24], uint8_t outBytes[24], uint32_t factor)
{
    // inBytes содержит 24 байта (6 dword). Оригинал читает только 5 dword (20 байт).
    uint32_t inWords[5]{};
    // читаем 5 dword как little-endian из первых 20 байт
    for (int i = 0; i < 5; ++i)
    {
        int o = i * 4;
        inWords[i] = static_cast<uint32_t>(inBytes[o])
            | (static_cast<uint32_t>(inBytes[o + 1]) << 8)
            | (static_cast<uint32_t>(inBytes[o + 2]) << 16)
            | (static_cast<uint32_t>(inBytes[o + 3]) << 24);
    }

    uint32_t src[5]{};
    for (int i = 0; i < 5; i++)
        src[i] = bswap32(inWords[4 - i]);

    uint32_t outWords[6]{};
    uint64_t tmp = 0;

    tmp = static_cast<uint64_t>(src[0]) * static_cast<uint64_t>(factor);
    outWords[0] = static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
    outWords[1] = static_cast<uint32_t>(tmp >> 32);

    tmp = static_cast<uint64_t>(src[1]) * static_cast<uint64_t>(factor) + static_cast<uint64_t>(outWords[1]);
    outWords[1] = static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
    outWords[2] = static_cast<uint32_t>(tmp >> 32);

    tmp = static_cast<uint64_t>(src[2]) * static_cast<uint64_t>(factor) + static_cast<uint64_t>(outWords[2]);
    outWords[2] = static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
    outWords[3] = static_cast<uint32_t>(tmp >> 32);

    tmp = static_cast<uint64_t>(src[3]) * static_cast<uint64_t>(factor) + static_cast<uint64_t>(outWords[3]);
    outWords[3] = static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
    outWords[4] = static_cast<uint32_t>(tmp >> 32);

    tmp = static_cast<uint64_t>(src[4]) * static_cast<uint64_t>(factor) + static_cast<uint64_t>(outWords[4]);
    outWords[4] = static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
    outWords[5] = static_cast<uint32_t>(tmp >> 32);

    // outWords -> outBytes little-endian (24 bytes)
    for (int i = 0; i < 6; ++i)
    {
        int o = i * 4;
        uint32_t w = outWords[i];
        outBytes[o] = static_cast<uint8_t>(w & 0xFF);
        outBytes[o + 1] = static_cast<uint8_t>((w >> 8) & 0xFF);
        outBytes[o + 2] = static_cast<uint8_t>((w >> 16) & 0xFF);
        outBytes[o + 3] = static_cast<uint8_t>((w >> 24) & 0xFF);
    }

    // байтовый реверс как в оригинале
    for (int i = 0; i < 12; i++)
        std::swap(outBytes[i], outBytes[23 - i]);
}

int gen_gpci(char buf[64], uint32_t factor)
{
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (!buf) return 0;
    buf[0] = '0';
    buf[1] = 0;
    if (factor == 0) return 1;

    uint8_t raw[24]{};

    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(alphanum)) - 2);

    // ВАЖНО: кладём ASCII как оригинал
    for (int i = 0; i < 24; ++i)
        raw[i] = static_cast<uint8_t>(alphanum[dist(rng)]);

    uint8_t mulOut[24]{};
    BIG_NUM_MUL_SAFE(raw, mulOut, factor);

    int pos = 0;
    bool notzero = false;

    for (int i = 0; i < 24; i++)
    {
        uint8_t tmp = mulOut[i] >> 4;
        uint8_t tmp2 = mulOut[i] & 0x0F;

        if (notzero || tmp)
        {
            buf[pos++] = static_cast<char>((tmp > 9) ? (tmp + 55) : (tmp + 48));
            notzero = true;
        }
        if (notzero || tmp2)
        {
            buf[pos++] = static_cast<char>((tmp2 > 9) ? (tmp2 + 55) : (tmp2 + 48));
            notzero = true;
        }
        if (pos >= 63) break;
    }

    if (!notzero)
    {
        buf[0] = '0';
        buf[1] = 0;
        return 1;
    }

    buf[pos] = 0;
    return pos;
}

#define szClientVersion "0.3.7"

void Packet_ConnectionSucceeded(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsSuccAuth(p->data, p->length, false);
    PLAYERID myPlayerID;
    unsigned int uiChallenge;

    bsSuccAuth.IgnoreBits(8); // ID_CONNECTION_REQUEST_ACCEPTED
    bsSuccAuth.IgnoreBits(32); // binaryAddress
    bsSuccAuth.IgnoreBits(16); // port

    bsSuccAuth.Read(myPlayerID);

    g_myPlayerID = myPlayerID;
    playerInfo[myPlayerID].iIsConnected = 1;
    strcpy(playerInfo[myPlayerID].szPlayerName, g_szNickName);

    bsSuccAuth.Read(uiChallenge);

    Log("Connected. Joining the game...");

    int iVersion = NETGAME_VERSION;
    unsigned int uiClientChallengeResponse = uiChallenge ^ iVersion;
    BYTE byteMod = 1;

    char auth_bs[4 * 16] = {0};
    gen_gpci(auth_bs, 0x3e9);

    BYTE byteAuthBSLen;
    byteAuthBSLen = static_cast<BYTE>(strlen(auth_bs));
    BYTE byteNameLen = static_cast<BYTE>(strlen(g_szNickName));
    BYTE iClientVerLen = static_cast<BYTE>(strlen(szClientVersion));

    RakNet::BitStream bsSend;

    Log("JOIN: ver=%d mod=%u nickLen=%u nick='%.*s' resp=%u authLen=%u auth='%s' clVerLen=%u clVer='%.*s'",
        NETGAME_VERSION, static_cast<unsigned>(byteMod), static_cast<unsigned>(byteNameLen),
        static_cast<int>(byteNameLen), g_szNickName,
        uiClientChallengeResponse,
        static_cast<unsigned>(byteAuthBSLen), auth_bs,
        static_cast<unsigned>(iClientVerLen), static_cast<int>(iClientVerLen), szClientVersion);

    bsSend.Write(iVersion);
    bsSend.Write(byteMod);
    bsSend.Write(byteNameLen);
    bsSend.Write(g_szNickName, byteNameLen);
    bsSend.Write(uiClientChallengeResponse);
    bsSend.Write(byteAuthBSLen);
    bsSend.Write(auth_bs, byteAuthBSLen);
    bsSend.Write(iClientVerLen);
    bsSend.Write(szClientVersion, iClientVerLen);

    auto dump_hex = [](const unsigned char* d, int n)
    {
        char line[2048];
        int pos = 0;
        for (int i = 0; i < n && pos < static_cast<int>(sizeof(line)) - 4; ++i)
            pos += sprintf_s(line + pos, sizeof(line) - pos, "%02X ", d[i]);
        Log("JOIN_BS bytesUsed=%d data=%s", n, line);
    };

    const int used = bsSend.GetNumberOfBytesUsed();
    dump_hex(bsSend.GetData(), used);

    pRakClient->RPC(&RPC_ClientJoin, &bsSend, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, nullptr);

    iAreWeConnected = 1;
}

void Packet_PlayerSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsPlayerSync(p->data, p->length, false);
    PLAYERID playerId;

    //Log("Packet_PlayerSync: %d \n%s\n", p->length, DumpMem((unsigned char *)p->data, p->length));

    bool bHasLR, bHasUD;
    bool bHasSurfInfo, bAnimation;

    bsPlayerSync.IgnoreBits(8);
    bsPlayerSync.Read(playerId);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    //playerInfo[playerId].incarData.VehicleID = -1;

    // clear last data
    memset(&playerInfo[playerId].onfootData, 0, sizeof(ONFOOT_SYNC_DATA));

    // LEFT/RIGHT KEYS
    bsPlayerSync.Read(bHasLR);
    if (bHasLR) bsPlayerSync.Read(playerInfo[playerId].onfootData.lrAnalog);

    // UP/DOWN KEYS
    bsPlayerSync.Read(bHasUD);
    if (bHasUD) bsPlayerSync.Read(playerInfo[playerId].onfootData.udAnalog);

    // GENERAL KEYS
    bsPlayerSync.Read(playerInfo[playerId].onfootData.wKeys);

    // VECTOR POS
    bsPlayerSync.Read(playerInfo[playerId].onfootData.vecPos[0]);
    bsPlayerSync.Read(playerInfo[playerId].onfootData.vecPos[1]);
    bsPlayerSync.Read(playerInfo[playerId].onfootData.vecPos[2]);

    // ROTATION
    bsPlayerSync.ReadNormQuat(
        playerInfo[playerId].onfootData.fQuaternion[0],
        playerInfo[playerId].onfootData.fQuaternion[1],
        playerInfo[playerId].onfootData.fQuaternion[2],
        playerInfo[playerId].onfootData.fQuaternion[3]);


    // HEALTH/ARMOUR (COMPRESSED INTO 1 BYTE)
    BYTE byteHealthArmour;
    BYTE byteHealth, byteArmour;
    BYTE byteArmTemp = 0, byteHlTemp = 0;

    bsPlayerSync.Read(byteHealthArmour);
    byteArmTemp = (byteHealthArmour & 0x0F);
    byteHlTemp = (byteHealthArmour >> 4);

    if (byteArmTemp == 0xF) byteArmour = 100;
    else if (byteArmTemp == 0) byteArmour = 0;
    else byteArmour = byteArmTemp * 7;

    if (byteHlTemp == 0xF) byteHealth = 100;
    else if (byteHlTemp == 0) byteHealth = 0;
    else byteHealth = byteHlTemp * 7;

    playerInfo[playerId].onfootData.byteHealth = byteHealth;
    playerInfo[playerId].onfootData.byteArmour = byteArmour;

    // CURRENT WEAPON
    bsPlayerSync.Read(playerInfo[playerId].onfootData.byteCurrentWeapon);

    // Special Action
    bsPlayerSync.Read(playerInfo[playerId].onfootData.byteSpecialAction);

    // READ MOVESPEED VECTORS
    bsPlayerSync.ReadVector(
        playerInfo[playerId].onfootData.vecMoveSpeed[0],
        playerInfo[playerId].onfootData.vecMoveSpeed[1],
        playerInfo[playerId].onfootData.vecMoveSpeed[2]);

    bsPlayerSync.Read(bHasSurfInfo);
    if (bHasSurfInfo)
    {
        bsPlayerSync.Read(playerInfo[playerId].onfootData.wSurfInfo);
        bsPlayerSync.Read(playerInfo[playerId].onfootData.vecSurfOffsets[0]);
        bsPlayerSync.Read(playerInfo[playerId].onfootData.vecSurfOffsets[1]);
        bsPlayerSync.Read(playerInfo[playerId].onfootData.vecSurfOffsets[2]);
    }
    else
        playerInfo[playerId].onfootData.wSurfInfo = -1;

    bsPlayerSync.Read(bAnimation);
    if (bAnimation)
        bsPlayerSync.Read(playerInfo[playerId].onfootData.iCurrentAnimationID);
}

//----------------------------------------------------

void Packet_UnoccupiedSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsUnocSync(p->data, p->length, false);
    PLAYERID playerId;

    //Log("\n%s\n", DumpMem((unsigned char *)p->data + bsUnocSync.GetReadOffset() / 8, p->length));

    bsUnocSync.IgnoreBits(8);
    bsUnocSync.Read(playerId);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    memset(&playerInfo[playerId].unocData, 0, sizeof(UNOCCUPIED_SYNC_DATA));

    bsUnocSync.Read((char*)&playerInfo[playerId].unocData, sizeof(UNOCCUPIED_SYNC_DATA));
}

void Packet_AimSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsAimSync(p->data, p->length, false);
    PLAYERID playerId;

    //Log("Packet_AimSync:\n%s\n", DumpMem((unsigned char *)p->data, p->length));

    bsAimSync.IgnoreBits(8);
    bsAimSync.Read(playerId);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    memset(&playerInfo[playerId].aimData, 0, sizeof(AIM_SYNC_DATA));

    bsAimSync.Read((PCHAR)&playerInfo[playerId].aimData, sizeof(AIM_SYNC_DATA));
}

void Packet_VehicleSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsSync(p->data, p->length, false);
    PLAYERID playerId;

    VEHICLEID VehicleID;
    bool bLandingGear;
    bool bHydra, bTrain, bTrailer;
    bool bSiren;

    //Log("Packet_VehicleSync: %d \n%s\n", p->length, DumpMem((unsigned char *)p->data, p->length));

    bsSync.IgnoreBits(8);
    bsSync.Read(playerId);
    bsSync.Read(VehicleID);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;
    if (VehicleID < 0 || VehicleID >= MAX_VEHICLES) return;

    // clear last data
    memset(&playerInfo[playerId].incarData, 0, sizeof(INCAR_SYNC_DATA));

    // LEFT/RIGHT KEYS
    bsSync.Read(playerInfo[playerId].incarData.lrAnalog);

    // UP/DOWN KEYS
    bsSync.Read(playerInfo[playerId].incarData.udAnalog);

    // GENERAL KEYS
    bsSync.Read(playerInfo[playerId].incarData.wKeys);

    // ROLL / DIRECTION
    // ROTATION
    bsSync.ReadNormQuat(
        playerInfo[playerId].incarData.fQuaternion[0],
        playerInfo[playerId].incarData.fQuaternion[1],
        playerInfo[playerId].incarData.fQuaternion[2],
        playerInfo[playerId].incarData.fQuaternion[3]);

    // POSITION
    bsSync.Read(playerInfo[playerId].incarData.vecPos[0]);
    bsSync.Read(playerInfo[playerId].incarData.vecPos[1]);
    bsSync.Read(playerInfo[playerId].incarData.vecPos[2]);

    // SPEED
    bsSync.ReadVector(
        playerInfo[playerId].incarData.vecMoveSpeed[0],
        playerInfo[playerId].incarData.vecMoveSpeed[1],
        playerInfo[playerId].incarData.vecMoveSpeed[2]);

    // VEHICLE HEALTH
    WORD wTempVehicleHealth;
    bsSync.Read(wTempVehicleHealth);
    playerInfo[playerId].incarData.fCarHealth = static_cast<float>(wTempVehicleHealth);

    // HEALTH/ARMOUR (COMPRESSED INTO 1 BYTE)
    BYTE byteHealthArmour;
    BYTE bytePlayerHealth, bytePlayerArmour;
    BYTE byteArmTemp = 0, byteHlTemp = 0;

    bsSync.Read(byteHealthArmour);
    byteArmTemp = (byteHealthArmour & 0x0F);
    byteHlTemp = (byteHealthArmour >> 4);

    if (byteArmTemp == 0xF) bytePlayerArmour = 100;
    else if (byteArmTemp == 0) bytePlayerArmour = 0;
    else bytePlayerArmour = byteArmTemp * 7;

    if (byteHlTemp == 0xF) bytePlayerHealth = 100;
    else if (byteHlTemp == 0) bytePlayerHealth = 0;
    else bytePlayerHealth = byteHlTemp * 7;

    playerInfo[playerId].incarData.bytePlayerHealth = bytePlayerHealth;
    playerInfo[playerId].incarData.bytePlayerArmour = bytePlayerArmour;

    // CURRENT WEAPON
    bsSync.Read(playerInfo[playerId].incarData.byteCurrentWeapon);

    // SIREN
    bsSync.ReadCompressed(bSiren);
    if (bSiren)
        playerInfo[playerId].incarData.byteSirenOn = 1;

    // LANDING GEAR
    bsSync.ReadCompressed(bLandingGear);
    if (bLandingGear)
        playerInfo[playerId].incarData.byteLandingGearState = 1;

    // HYDRA THRUST ANGLE AND TRAILER ID
    bsSync.ReadCompressed(bHydra);
    bsSync.ReadCompressed(bTrailer);

    DWORD dwTrailerID_or_ThrustAngle;
    bsSync.Read(dwTrailerID_or_ThrustAngle);
    playerInfo[playerId].incarData.TrailerID_or_ThrustAngle = static_cast<WORD>(dwTrailerID_or_ThrustAngle);

    // TRAIN SPECIAL
    WORD wSpeed;
    bsSync.ReadCompressed(bTrain);
    if (bTrain)
    {
        bsSync.Read(wSpeed);
        playerInfo[playerId].incarData.fTrainSpeed = static_cast<float>(wSpeed);
    }
}


void Packet_PassengerSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsPassengerSync(p->data, p->length, false);
    PLAYERID playerId;
    PASSENGER_SYNC_DATA psSync{};

    bsPassengerSync.IgnoreBits(8);
    bsPassengerSync.Read(playerId);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    // Read returns false when the packet is short, and leaves psSync as it
    // found it. Value-initialised above so a partial packet cannot hand stale
    // stack bytes to the consumer this is about to grow.
    if (!bsPassengerSync.Read((PCHAR)&psSync, sizeof(PASSENGER_SYNC_DATA))) return;

    // Followed wants to drive the vehicle
    // playerInfo[playerId].passengerData.VehicleID = psSync.VehicleID;
}

void Packet_TrailerSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsSpectatorSync(p->data, p->length, false);

    PLAYERID playerId;
    //TRAILER_SYNC_DATA trSync;

    bsSpectatorSync.IgnoreBits(8);
    bsSpectatorSync.Read(playerId);
    //bsSpectatorSync.Read((PCHAR)&trSync, sizeof(TRAILER_SYNC_DATA));
}

void Packet_MarkersSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsMarkersSync(p->data, p->length, false);

    int i, iNumberOfPlayers;
    PLAYERID playerID;
    short sPosX, sPosY, sPosZ;
    bool bIsPlayerActive;

    bsMarkersSync.IgnoreBits(8);
    bsMarkersSync.Read(iNumberOfPlayers);

    if (iNumberOfPlayers < 0 || iNumberOfPlayers > MAX_PLAYERS) return;

    for (i = 0; i < iNumberOfPlayers; i++)
    {
        bsMarkersSync.Read(playerID);

        if (playerID < 0 || playerID >= MAX_PLAYERS) return;

        bsMarkersSync.ReadCompressed(bIsPlayerActive);
        if (bIsPlayerActive == 0)
        {
            playerInfo[playerID].iGotMarkersPos = 0;
            continue;
        }

        bsMarkersSync.Read(sPosX);
        bsMarkersSync.Read(sPosY);
        bsMarkersSync.Read(sPosZ);

        playerInfo[playerID].iGotMarkersPos = 1;
        playerInfo[playerID].onfootData.vecPos[0] = static_cast<float>(sPosX);
        playerInfo[playerID].onfootData.vecPos[1] = static_cast<float>(sPosY);
        playerInfo[playerID].onfootData.vecPos[2] = static_cast<float>(sPosZ);

        //Log("Packet_MarkersSync: %d %d %0.2f, %0.2f, %0.2f", playerID, bIsPlayerActive, (float)sPosX, (float)sPosY, (float)sPosZ);
    }
}

bool m_bLagCompensation = true;

void Packet_BulletSync(Packet* p, RakClientInterface* pRakClient)
{
    RakNet::BitStream bsBulletSync(p->data, p->length, false);

    if (m_bLagCompensation)
    {
        PLAYERID PlayerID;

        bsBulletSync.IgnoreBits(8);
        bsBulletSync.Read(PlayerID);

        if (PlayerID < 0 || PlayerID >= MAX_PLAYERS) return;

        memset(&playerInfo[PlayerID].bulletData, 0, sizeof(BULLET_SYNC_DATA));

        bsBulletSync.Read((PCHAR)&playerInfo[PlayerID].bulletData, sizeof(BULLET_SYNC_DATA));
    }
}

void resetPools(int iRestart, DWORD dwTimeReconnect)
{
    memset(playerInfo, 0, sizeof(stPlayerInfo));
}

void UpdatePlayerScoresAndPings(int iWait, int iMS, RakClientInterface* pRakClient)
{
    static DWORD dwLastUpdateTick = 0;

    if (iWait)
    {
        if ((GetTickCount() - dwLastUpdateTick) > static_cast<DWORD>(iMS))
        {
            dwLastUpdateTick = GetTickCount();
            RakNet::BitStream bsParams;
            pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY, RELIABLE, 0, FALSE,
                            UNASSIGNED_NETWORK_ID, nullptr);
        }
    }
    else
    {
        RakNet::BitStream bsParams;
        pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID,
                        nullptr);
    }
}

void UpdateNetwork(RakClientInterface* pRakClient)
{
    unsigned char packetIdentifier;
    Packet* pkt;

    while (pkt = pRakClient->Receive())
    {
        if (pkt->data[0] == ID_TIMESTAMP)
        {
            if (pkt->length > sizeof(unsigned char) + sizeof(unsigned int))
                packetIdentifier = pkt->data[sizeof(unsigned char) + sizeof(unsigned int)];
            else
            {
                pRakClient->DeallocatePacket(pkt);
                return;
            }
        }
        else
            packetIdentifier = pkt->data[0];

        __try
        {
        switch (packetIdentifier)
        {
        case ID_DISCONNECTION_NOTIFICATION:
            break;
        case ID_CONNECTION_BANNED:
            break;
        case ID_CONNECTION_ATTEMPT_FAILED:
            break;
        case ID_NO_FREE_INCOMING_CONNECTIONS:
            break;
        case ID_INVALID_PASSWORD:
            Log("[RAKSAMP] Invalid password");
            break;
        case ID_CONNECTION_LOST:
            Log("[RAKSAMP] The connection was lost.");
            break;
        case ID_CONNECTION_REQUEST_ACCEPTED:
            Packet_ConnectionSucceeded(pkt, pRakClient);
            break;
        case ID_AUTH_KEY:
            Packet_AUTH_KEY(pkt, pRakClient);
            break;
        case ID_PLAYER_SYNC:
            Packet_PlayerSync(pkt, pRakClient);
            break;
        case ID_VEHICLE_SYNC:
            Packet_VehicleSync(pkt, pRakClient);
            break;
        case ID_PASSENGER_SYNC:
            Packet_PassengerSync(pkt, pRakClient);
            break;
        case ID_AIM_SYNC:
            Packet_AimSync(pkt, pRakClient);
            break;
        case ID_TRAILER_SYNC:
            Packet_TrailerSync(pkt, pRakClient);
            break;
        case ID_UNOCCUPIED_SYNC:
            Packet_UnoccupiedSync(pkt, pRakClient);
            break;
        case ID_MARKERS_SYNC:
            Packet_MarkersSync(pkt, pRakClient);
            break;
        case ID_BULLET_SYNC:
            Packet_BulletSync(pkt, pRakClient);
            break;
        }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Log("[RX] !! exception while handling pid=%u — handler bug, dropping packet",
                static_cast<unsigned>(packetIdentifier));
        }

        pRakClient->DeallocatePacket(pkt);
    }

    UpdatePlayerScoresAndPings(1, 3000, pRakClient);
}

bool iGameInited = false;

void ServerJoin(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    CHAR szPlayerName[256];
    PLAYERID playerId;
    BYTE byteNameLen = 0;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(playerId);
    int iUnk = 0;
    bsData.Read(iUnk);
    BYTE bIsNPC = 0;
    bsData.Read(bIsNPC);
    bsData.Read(byteNameLen);
    if (byteNameLen > 20) return;
    bsData.Read(szPlayerName, byteNameLen);
    szPlayerName[byteNameLen] = '\0';

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    playerInfo[playerId].iIsConnected = 1;
    // playerInfo[playerId].byteIsNPC = bIsNPC;
    strcpy(playerInfo[playerId].szPlayerName, szPlayerName);

    //Log("***[JOIN] (%d) %s", playerId, szPlayerName);
}

void ServerQuit(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    PLAYERID playerId;
    BYTE byteReason;

    bsData.Read(playerId);
    bsData.Read(byteReason);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    playerInfo[playerId].iIsConnected = 0;
    // playerInfo[playerId].byteIsNPC = 0;
    //Log("***[QUIT:%d] (%d) %s", byteReason, playerId, playerInfo[playerId].szPlayerName);
    memset(playerInfo[playerId].szPlayerName, 0, 20);
}

int iConnectionRequested = 0, iSpawned = 0, iSpawnsAvailable = 0;
int iReconnectTime = 2 * 1000, iNotificationDisplayedBeforeSpawn = 0;
char g_szHostName[256];

void InitGame(RPCParameters* rpcParams)
{
    int bits = rpcParams->numberOfBitsOfData;
    int bytes = (bits + 7) / 8; // округление вверх, без +1!

    RakNet::BitStream bsInitGame(rpcParams->input, bytes, false);

    PLAYERID MyPlayerID;
    bool bLanMode, bStuntBonus;
    BYTE byteVehicleModels[212];

    bool m_bZoneNames, m_bUseCJWalk, m_bAllowWeapons, m_bLimitGlobalChatRadius;
    float m_fGlobalChatRadius, m_fNameTagDrawDistance;
    bool m_bDisableEnterExits, m_bNameTagLOS, m_bManualVehicleEngineAndLight;
    bool m_bShowPlayerTags;
    int m_iShowPlayerMarkers;
    BYTE m_byteWorldTime, m_byteWeather;
    float m_fGravity;
    int m_iDeathDropMoney;
    bool m_bInstagib;

    bsInitGame.ReadCompressed(m_bZoneNames);
    bsInitGame.ReadCompressed(m_bUseCJWalk);
    bsInitGame.ReadCompressed(m_bAllowWeapons);
    bsInitGame.ReadCompressed(m_bLimitGlobalChatRadius);
    bsInitGame.Read(m_fGlobalChatRadius);
    bsInitGame.ReadCompressed(bStuntBonus);
    bsInitGame.Read(m_fNameTagDrawDistance);
    bsInitGame.ReadCompressed(m_bDisableEnterExits);
    bsInitGame.ReadCompressed(m_bNameTagLOS);
    bsInitGame.ReadCompressed(m_bManualVehicleEngineAndLight); // 
    bsInitGame.Read(iSpawnsAvailable);
    bsInitGame.Read(MyPlayerID);
    bsInitGame.ReadCompressed(m_bShowPlayerTags);
    bsInitGame.Read(m_iShowPlayerMarkers);
    bsInitGame.Read(m_byteWorldTime);
    bsInitGame.Read(m_byteWeather);
    bsInitGame.Read(m_fGravity);
    bsInitGame.ReadCompressed(bLanMode);
    bsInitGame.Read(m_iDeathDropMoney);
    bsInitGame.ReadCompressed(m_bInstagib);

    constexpr unsigned int skipBits = 4 * 32;
    if (bsInitGame.GetNumberOfUnreadBits() < skipBits)
    {
        Log("InitGame: not enough bits to skip 128, left=%u", bsInitGame.GetNumberOfUnreadBits());
        return;
    }
    bsInitGame.SetReadOffset(bsInitGame.GetReadOffset() + skipBits);

    bool tmpLagComp = false;
    if (!bsInitGame.Read(tmpLagComp))
    {
        Log("InitGame: failed read lagcomp after skip");
        return;
    }
    m_bLagCompensation = tmpLagComp;

    BYTE unk;
    bsInitGame.Read(unk);
    bsInitGame.Read(unk);
    bsInitGame.Read(unk);

    BYTE byteStrLen;
    bsInitGame.Read(byteStrLen);
    if (byteStrLen)
    {
        memset(g_szHostName, 0, sizeof(g_szHostName));
        bsInitGame.Read(g_szHostName, byteStrLen);
    }
    g_szHostName[byteStrLen] = '\0';

    bsInitGame.Read((char*)&byteVehicleModels[0], 212);

    g_myPlayerID = MyPlayerID;

    char szTitle[64];
    sprintf(szTitle, "%s (%d)", g_szNickName, g_myPlayerID);
    Log("Connected to %.64s", g_szHostName);

    iGameInited = true;
}

void WorldPlayerAdd(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    PLAYERID playerId;
    BYTE byteFightingStyle = 4;
    BYTE byteTeam = 0;
    int iSkin = 0;
    float vecPos[3];
    float fRotation = 0;
    DWORD dwColor = 0;

    bsData.Read(playerId);
    bsData.Read(byteTeam);
    bsData.Read(iSkin);
    bsData.Read(vecPos[0]);
    bsData.Read(vecPos[1]);
    bsData.Read(vecPos[2]);
    bsData.Read(fRotation);
    bsData.Read(dwColor);
    bsData.Read(byteFightingStyle);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    playerInfo[playerId].iIsStreamedIn = 1;
    playerInfo[playerId].onfootData.vecPos[0] =
        playerInfo[playerId].incarData.vecPos[0] = vecPos[0];
    playerInfo[playerId].onfootData.vecPos[1] =
        playerInfo[playerId].incarData.vecPos[1] = vecPos[1];
    playerInfo[playerId].onfootData.vecPos[2] =
        playerInfo[playerId].incarData.vecPos[2] = vecPos[2];

    //Log("[WORLD ADD] Player [%d]", playerId);
}

void WorldPlayerDeath(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    PLAYERID playerId;
    bsData.Read(playerId);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    //Log("[PLAYER_DEATH] %d", playerId);
}

void WorldPlayerRemove(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    PLAYERID playerId = 0;
    bsData.Read(playerId);

    if (playerId < 0 || playerId >= MAX_PLAYERS) return;

    playerInfo[playerId].iIsStreamedIn = 0;
    playerInfo[playerId].incarData.vecPos[0] = 0.0f;
    playerInfo[playerId].incarData.vecPos[1] = 0.0f;
    playerInfo[playerId].incarData.vecPos[2] = 0.0f;

    //Log("[PLAYER_REMOVE] %d", playerId);
}

void WorldVehicleAdd(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    //auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    //int iBitLength = rpcParams->numberOfBitsOfData;
    //
    //RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    //
    //NEW_VEHICLE NewVehicle;
    //
    //bsData.Read(static_cast<char*>(&NewVehicle), sizeof(NEW_VEHICLE));
    //
    //if (NewVehicle.VehicleId < 0 || NewVehicle.VehicleId >= MAX_VEHICLES) return;
    //
    //vehiclePool[NewVehicle.VehicleId].iDoesExist = 1;
    //vehiclePool[NewVehicle.VehicleId].fPos[0] = NewVehicle.vecPos[0];
    //vehiclePool[NewVehicle.VehicleId].fPos[1] = NewVehicle.vecPos[1];
    //vehiclePool[NewVehicle.VehicleId].fPos[2] = NewVehicle.vecPos[2];
    //vehiclePool[NewVehicle.VehicleId].iModelID = NewVehicle.iVehicleType;

    //Log("[VEHICLE_ADD:%d] ModelID: %d, Position: %0.2f, %0.2f, %0.2f",
    //	NewVehicle.VehicleId, NewVehicle.iVehicleType, NewVehicle.vecPos[0], NewVehicle.vecPos[1], NewVehicle.vecPos[2]);
}

void WorldVehicleRemove(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    VEHICLEID VehicleID;

    bsData.Read(VehicleID);

    if (VehicleID < 0 || VehicleID >= MAX_VEHICLES) return;

    //vehiclePool[VehicleID].iDoesExist = 0;
    //vehiclePool[VehicleID].fPos[0] = 0.0f;
    //vehiclePool[VehicleID].fPos[1] = 0.0f;
    //vehiclePool[VehicleID].fPos[2] = 0.0f;

    //Log("[VEHICLE_REMOVE] %d", VehicleID);
}

#define REJECT_REASON_BAD_VERSION 1
#define REJECT_REASON_BAD_NICKNAME 2
#define REJECT_REASON_BAD_MOD 3
#define REJECT_REASON_BAD_PLAYERID 4

bool iGettingNewName = false;

void gen_random(char* s, const int len)
{
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i)
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

    s[len] = 0;
}

void ConnectionRejected(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    BYTE byteRejectReason;

    bsData.Read(byteRejectReason);

    if (byteRejectReason == REJECT_REASON_BAD_VERSION)
    {
        Log("[RAKSAMP] Bad SA-MP version.");
    }
    else if (byteRejectReason == REJECT_REASON_BAD_NICKNAME)
    {
        char szNewNick[32], randgen[4];

        iGettingNewName = true;

        gen_random(randgen, 4);
        sprintf(szNewNick, "%s_%s", g_szNickName, randgen);

        Log("[RAKSAMP] Bad nickname. Changing name to %s", szNewNick);

        strcpy(g_szNickName, szNewNick);
        resetPools(1, 0);
    }
    else if (byteRejectReason == REJECT_REASON_BAD_MOD)
    {
        Log("[RAKSAMP] Bad mod version.");
    }
    else if (byteRejectReason == REJECT_REASON_BAD_PLAYERID)
    {
        Log("[RAKSAMP] Bad player ID.");
    }
    else
        Log("ConnectionRejected: unknown");
}


void sendServerCommand(char* szCommand)
{
    //if (!strnicmp(szCommand + 1, "rcon", 4))
    //{
    //    RakNet::BitStream bsSend;
    //    bsSend.Write(static_cast<BYTE>(ID_RCON_COMMAND));
    //    DWORD len = strlen(szCommand + 4);
    //    if (len > 0)
    //    {
    //        bsSend.Write(len);
    //        bsSend.Write(szCommand + 6, len);
    //    }
    //    else
    //    {
    //        bsSend.Write(len);
    //        bsSend.Write(szCommand + 5, len);
    //    }
    //    pRakClient->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);
    //}
    //else
    //{
    RakNet::BitStream bsParams;
    int iStrlen = strlen(szCommand);
    bsParams.Write(iStrlen);
    bsParams.Write(szCommand, iStrlen);
    pRakClient->RPC(&RPC_ServerCommand, &bsParams, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID,
                    nullptr);
    //}
}

void sendChat(char* szMessage)
{
    // Incoming text from our UI is UTF-8. SA-MP servers expect Windows-1251.
    const auto payload = opensamp::util::utf8_to_cp1251(szMessage ? szMessage : "");

    RakNet::BitStream bsSend;
    BYTE byteTextLen = static_cast<BYTE>(std::min<std::size_t>(payload.size(), 255));
    bsSend.Write(byteTextLen);
    if (byteTextLen > 0)
        bsSend.Write(payload.data(), byteTextLen);
    pRakClient->RPC(&RPC_Chat, &bsSend, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, nullptr);
}

void ClientMessage(RPCParameters* rpcParams)
{
    //if(!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    DWORD dwStrLen, dwColor;
    char szMsg[257];
    memset(szMsg, 0, 257);

    bsData.Read(dwColor);
    bsData.Read(dwStrLen);
    if (dwStrLen > 256) return;

    bsData.Read(szMsg, dwStrLen);
    szMsg[dwStrLen] = 0;

    // Server bytes are CP1251. Preserve {RRGGBB} color tags verbatim (ASCII);
    // convert the rest to UTF-8 for our chat.
    const auto utf8 = opensamp::util::cp1251_to_utf8({szMsg, dwStrLen});
    Log("%s", utf8.c_str());
}

void Chat(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;
    PlayerID sender = rpcParams->sender;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    PLAYERID playerId;
    BYTE byteTextLen;

    unsigned char szText[256];
    memset(szText, 0, 256);

    bsData.Read(playerId);
    bsData.Read(byteTextLen);
    bsData.Read((char*)szText, byteTextLen);
    szText[byteTextLen] = 0;

    if (playerId < 0 || playerId >= MAX_PLAYERS)
        return;

    const auto utf8 = opensamp::util::cp1251_to_utf8(
        {reinterpret_cast<const char*>(szText), byteTextLen});
    Log("[CHAT] %s: %s", playerInfo[playerId].szPlayerName, utf8.c_str());
}

void UpdateScoresPingsIPs(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    PLAYERID playerId;
    int iPlayerScore;
    DWORD dwPlayerPing;

    for (PLAYERID i = 0; i < (iBitLength / 8) / 9; i++)
    {
        bsData.Read(playerId);
        bsData.Read(iPlayerScore);
        bsData.Read(dwPlayerPing);

        if (playerId < 0 || playerId >= MAX_PLAYERS)
            continue;

        playerInfo[playerId].iScore = iPlayerScore;
        playerInfo[playerId].dwPing = dwPlayerPing;
    }
}

struct stCheckpointData
{
    bool bActive;
    float fPosition[3];
    float fSize;
};

stCheckpointData CurrentCheckpoint;

void SetCheckpoint(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(CurrentCheckpoint.fPosition[0]);
    bsData.Read(CurrentCheckpoint.fPosition[1]);
    bsData.Read(CurrentCheckpoint.fPosition[2]);
    bsData.Read(CurrentCheckpoint.fSize);

    CurrentCheckpoint.bActive = true;

    char SetCheckpointAlert[256];
    sprintf_s(SetCheckpointAlert, 256, "[CP] Checkpoint set to %.2f %.2f %.2f position. (size: %.2f)",
              CurrentCheckpoint.fPosition[0], CurrentCheckpoint.fPosition[1],
              CurrentCheckpoint.fPosition[2], CurrentCheckpoint.fSize);
    Log(SetCheckpointAlert);
}

void DisableCheckpoint(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    CurrentCheckpoint.bActive = false;

    Log("[CP] Current checkpoint disabled.");
}

void Pickup(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    int PickupID;
    PICKUP Pickup{};

    bsData.Read(PickupID);
    // Every field below is formatted into the log line, so a short packet
    // would print uninitialised stack bytes.
    if (!bsData.Read((PCHAR)&Pickup, sizeof(PICKUP))) return;

    char szCreatePickupAlert[256];
    sprintf_s(szCreatePickupAlert, sizeof(szCreatePickupAlert),
              "[CREATEPICKUP] ID: %d | Model: %d | Type: %d | X: %.2f | Y: %.2f | Z: %.2f", PickupID, Pickup.iModel,
              Pickup.iType, Pickup.fX, Pickup.fY, Pickup.fZ);
    Log(szCreatePickupAlert);
}

void DestroyPickup(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    int PickupID;

    bsData.Read(PickupID);

    Log("[DESTROYPICKUP] %d", PickupID);
}

PLAYER_SPAWN_INFO SpawnInfo;
int iLocalPlayerSkin = 0;
bool g_spawnInfoReceived = false; // set when server responds to RPC_RequestClass

void RequestClass(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    BYTE byteRequestOutcome = 0;

    bsData.Read(byteRequestOutcome);

    Log("[RequestClass] outcome=%u bits=%d", static_cast<unsigned>(byteRequestOutcome), iBitLength);

    if (byteRequestOutcome)
    {
        bsData.Read((PCHAR)&SpawnInfo, sizeof(PLAYER_SPAWN_INFO));
        iLocalPlayerSkin      = SpawnInfo.iSkin;
        g_spawnInfoReceived   = true;
        Log("[RequestClass] skin=%d pos=(%.1f, %.1f, %.1f) rot=%.1f",
            SpawnInfo.iSkin,
            SpawnInfo.vecPos[0], SpawnInfo.vecPos[1], SpawnInfo.vecPos[2],
            SpawnInfo.fRotation);
    }
}

void ScrInitMenu(RPCParameters* rpcParams)
{
    if (!iGameInited) return;

    //auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    //int iBitLength = rpcParams->numberOfBitsOfData;
    //RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    //
    //memset(&GTAMenu, 0, sizeof(struct stGTAMenu));
    //
    //BYTE byteMenuID;
    //BOOL bColumns; // 0 = 1, 1 = 2
    //CHAR cText[MAX_MENU_LINE];
    //float fX;
    //float fY;
    //float fCol1;
    //float fCol2 = 0.0;
    //MENU_INT MenuInteraction;
    //
    //bsData.Read(byteMenuID);
    //bsData.Read(bColumns);
    //bsData.Read(cText, MAX_MENU_LINE);
    //bsData.Read(fX);
    //bsData.Read(fY);
    //bsData.Read(fCol1);
    //if (bColumns) bsData.Read(fCol2);
    //bsData.Read(MenuInteraction.bMenu);
    //for (BYTE i = 0; i < MAX_MENU_ITEMS; i++)
    //    bsData.Read(MenuInteraction.bRow[i]);
    //
    //Log("[MENU] %s", cText);
    //strcpy(GTAMenu.szTitle, cText);
    //
    //BYTE byteColCount;
    //bsData.Read(cText, MAX_MENU_LINE);
    //Log("[MENU] %s", cText);
    //strcpy(GTAMenu.szSeparator, cText);
    //
    //bsData.Read(byteColCount);
    //GTAMenu.byteColCount = byteColCount;
    //for (BYTE i = 0; i < byteColCount; i++)
    //{
    //    bsData.Read(cText, MAX_MENU_LINE);
    //    Log("[MENU:%d] %s", i, cText);
    //    strcpy(GTAMenu.szColumnContent[i], cText);
    //}
    //
    //if (bColumns)
    //{
    //    bsData.Read(cText, MAX_MENU_LINE);
    //    //Log("4: %s", cText);
    //
    //    bsData.Read(byteColCount);
    //    for (BYTE i = 0; i < byteColCount; i++)
    //    {
    //        bsData.Read(cText, MAX_MENU_LINE);
    //        //Log("5: %d %s", i, cText);
    //    }
    //}
}

#define IDB_BUTTON1			10
#define IDB_BUTTON2			11
#define IDE_INPUTEDIT			12
#define IDL_LISTBOX			13

#define DIALOG_STYLE_MSGBOX		0
#define DIALOG_STYLE_INPUT		1
#define DIALOG_STYLE_LIST		2
#define DIALOG_STYLE_PASSWORD		3

// `szInfo` is the dialog body — server-side SA-MP allows up to 4096 chars
// (Pawn `string`-style limit), and modern open.mp / RP servers happily push
// long auth/help dialogs that exceed the original 256-byte cap. Title and
// button captions stay byte-prefixed (max 255), so 257 is correct for them.
struct stSAMPDialog
{
    int  iIsActive;
    BYTE bDialogStyle;
    WORD wDialogID;
    BYTE bTitleLength;
    char szTitle[257];
    BYTE bButton1Len;
    char szButton1[257];
    BYTE bButton2Len;
    char szButton2[257];
    char szInfo[4097];
};

stSAMPDialog sampDialog;

LRESULT CALLBACK SAMPDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    //HWND hwndEditBox = GetDlgItem(hwnd, IDE_INPUTEDIT);
    //HWND hwndListBox = GetDlgItem(hwnd, IDL_LISTBOX);
    //WORD wSelection;
    //char szResponse[257];

    Log("Dialog proc: %d", msg);
    Log("Dialog style: %d", sampDialog.bDialogStyle);
    Log("Button1: %s", sampDialog.szButton1);
    Log("Button2: %s", sampDialog.szButton2);
    Log("Info: %s", sampDialog.szInfo);

    /*
    switch (msg)
    {
    case WM_CREATE:
        {
            HINSTANCE hInst = GetModuleHandle(nullptr);
            switch (sampDialog.bDialogStyle)
            {
            case DIALOG_STYLE_MSGBOX:
                if (sampDialog.bButton1Len == 0 && sampDialog.bButton2Len == 0)
                {
                    // no butans, no badi cana cross it
                }
                if (sampDialog.bButton1Len != 0 && sampDialog.bButton2Len == 0) // a butan
                {
                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   150, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON1), hInst, NULL);
                }
                else if (sampDialog.bButton1Len != 0 && sampDialog.bButton2Len != 0) // tu butans
                {
                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   100, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON1), hInst, NULL);

                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   210, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON2), hInst, NULL);
                }

                break;

            case DIALOG_STYLE_INPUT:
            case DIALOG_STYLE_PASSWORD:
                {
                    CreateWindowEx(NULL, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                   50, 200, 300, 24, hwnd, static_cast<HMENU>(IDE_INPUTEDIT), hInst, NULL);

                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   100, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON1), hInst, NULL);

                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   210, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON2), hInst, NULL);
                }

                break;

            case DIALOG_STYLE_LIST:
                {
                    hwndListBox = CreateWindowEx(NULL, "LISTBOX", "",
                                                 WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER |
                                                 LBS_HASSTRINGS,
                                                 10, 10, 375, 225, hwnd, static_cast<HMENU>(IDL_LISTBOX), hInst, NULL);

                    char* szInfoTemp = strtok(sampDialog.szInfo, "\n");
                    while (szInfoTemp != nullptr)
                    {
                        int id = SendMessage(hwndListBox, LB_ADDSTRING, 0, static_cast<LPARAM>(szInfoTemp));
                        SendMessage(hwndListBox, LB_SETITEMDATA, id, static_cast<LPARAM>(id));

                        szInfoTemp = strtok(nullptr, "\n");
                    }

                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   100, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON1), hInst, NULL);

                    CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2,
                                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                   210, 230, 100, 24, hwnd, static_cast<HMENU>(IDB_BUTTON2), hInst, NULL);
                }

                break;
            }
        }
        break;

    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDB_BUTTON1:
                if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST)
                {
                    wSelection = static_cast<WORD>(SendMessage(hwndListBox, LB_GETCURSEL, 0, 0));
                    if (wSelection != static_cast<WORD>(-1))
                    {
                        SendMessage(hwndListBox, LB_GETTEXT, wSelection, (LPARAM)szResponse);
                        sendDialogResponse(sampDialog.wDialogID, 1, 0, szResponse);
                        PostQuitMessage(0);
                    }
                    break;
                }

                GetWindowText(hwndEditBox, szResponse, 257);
                sendDialogResponse(sampDialog.wDialogID, 1, 0, szResponse);
                PostQuitMessage(0);
                break;

            case IDB_BUTTON2:
                GetWindowText(hwndEditBox, szResponse, 257);
                sendDialogResponse(sampDialog.wDialogID, 0, 0, szResponse);
                PostQuitMessage(0);
                break;
            }
        }

        break;

    case WM_PAINT:
        {
            if (sampDialog.bDialogStyle != DIALOG_STYLE_LIST)
            {
                RECT rect;
                GetClientRect(hwnd, &rect);
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdc, hSAMPDlgFont);
                DrawText(hdc, sampDialog.szInfo, strlen(sampDialog.szInfo), &rect, DT_WORDBREAK | DT_EXPANDTABS);
                DeleteDC(hdcMem);
                EndPaint(hwnd, &ps);
            }
            else
            {
                RECT rect;
                GetClientRect(hwnd, &rect);
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                HDC hdcMem = CreateCompatibleDC(hdc);
                DeleteDC(hdcMem);
                EndPaint(hwnd, &ps);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    */

    return 0;
}

void DialogBoxThread()
{
    Log("DialogBoxThread started.");
    /*WNDCLASSEX wc;
    MSG Msg;
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    RECT conRect;
    if (iConsole)
        GetWindowRect(GetConsoleWindow(), &conRect);
    else
        GetWindowRect(hwnd, &conRect);

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = SAMPDlgProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = "dlgWndClass";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
        return 0;

    hSAMPDlgFont = CreateFont(18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");

    hwndSAMPDlg = CreateWindowEx(NULL, "dlgWndClass", sampDialog.szTitle, NULL,
                                 conRect.right, conRect.top, 400, 300, NULL, NULL, hInstance, NULL);

    if (hwndSAMPDlg == NULL)
        return 0;

    ShowWindow(hwndSAMPDlg, 1);
    UpdateWindow(hwndSAMPDlg);
    SetForegroundWindow(hwndSAMPDlg);

    while (GetMessage(&Msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    sampDialog.iIsActive = 0;
    SendMessage(hwndSAMPDlg, WM_DESTROY, 0, 0);
    DestroyWindow(hwndSAMPDlg);
    UnregisterClass("dlgWndClass", GetModuleHandle(nullptr));
    hSAMPDlgFont = NULL;
    TerminateThread(hDlgThread, 0);*/
}

// Send RPC_DialogResponse (id=62). BitStream body for SA-MP 0.3.7:
//   WORD dialogId
//   BYTE buttonResponse   (1 = left/OK, 0 = right/cancel)
//   WORD listItem         (0 / 0xFFFF if not a list)
//   BYTE inputLen
//   char[inputLen] inputText  (CP1251 on the wire)
void sendDialogResponse(WORD wDialogId, BYTE bButton, WORD wListItem, const char* szInput)
{
    if (!pRakClient) return;

    // UI gives us UTF-8. SAMP servers expect CP1251.
    const auto payload = opensamp::util::utf8_to_cp1251(szInput ? szInput : "");
    BYTE inputLen = static_cast<BYTE>(std::min<std::size_t>(payload.size(), 128));

    RakNet::BitStream bs;
    bs.Write(wDialogId);
    bs.Write(bButton);
    bs.Write(wListItem);
    bs.Write(inputLen);
    if (inputLen > 0)
        bs.Write(payload.data(), inputLen);

    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE, 0, FALSE,
                    UNASSIGNED_NETWORK_ID, nullptr);
}

void ScrDialogBox(RPCParameters* rpcParams)
{
    // NOTE: previously gated on `iGameInited`, but SampSharp/open.mp servers
    // routinely push the OnPlayerConnect dialog BEFORE sending InitGame (see
    // e.g. VSRP Auth.cs — ShowAuth runs the moment the connect callback
    // fires). Dropping that RPC means the login dialog never surfaces.
    // Dialog rendering is decoupled from game readiness — ImGui will display
    // it as soon as allow_chat flips.
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;
    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(sampDialog.wDialogID);
    bsData.Read(sampDialog.bDialogStyle);

    bsData.Read(sampDialog.bTitleLength);
    bsData.Read(sampDialog.szTitle, sampDialog.bTitleLength);
    sampDialog.szTitle[sampDialog.bTitleLength] = 0;

    bsData.Read(sampDialog.bButton1Len);
    bsData.Read(sampDialog.szButton1, sampDialog.bButton1Len);
    sampDialog.szButton1[sampDialog.bButton1Len] = 0;

    bsData.Read(sampDialog.bButton2Len);
    bsData.Read(sampDialog.szButton2, sampDialog.bButton2Len);
    sampDialog.szButton2[sampDialog.bButton2Len] = 0;

    stringCompressor->DecodeString(sampDialog.szInfo, sizeof(sampDialog.szInfo) - 1, &bsData);
    sampDialog.szInfo[sizeof(sampDialog.szInfo) - 1] = 0; // belt-and-braces NUL

    // Style 255 from the server = "close any active dialog" (per SA-MP).
    if (sampDialog.bDialogStyle == 255)
    {
        opensamp::gui::Dialog::Get().Hide();
        return;
    }

    // Server strings are Windows-1251; translate to UTF-8 for the UI.
    const auto title = opensamp::util::cp1251_to_utf8(
        {sampDialog.szTitle, sampDialog.bTitleLength});
    const auto btn1  = opensamp::util::cp1251_to_utf8(
        {sampDialog.szButton1, sampDialog.bButton1Len});
    const auto btn2  = opensamp::util::cp1251_to_utf8(
        {sampDialog.szButton2, sampDialog.bButton2Len});
    const auto info  = opensamp::util::cp1251_to_utf8(sampDialog.szInfo);

    opensamp::gui::Dialog::Get().Show(
        sampDialog.wDialogID,
        static_cast<opensamp::gui::DialogStyle>(sampDialog.bDialogStyle),
        title.c_str(), btn1.c_str(), btn2.c_str(), info.c_str());
}

void ScrGameText(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    char szMessage[400];
    int iType, iTime, iLength;

    bsData.Read(iType);
    bsData.Read(iTime);
    bsData.Read(iLength);

    // iLength is server-controlled and signed. `> 400` let 400 through, and the
    // terminator below then wrote one byte past the end of the buffer; a
    // negative length skipped the check entirely and wrote below it.
    if (iLength < 0 || iLength >= static_cast<int>(sizeof(szMessage))) return;

    bsData.Read(szMessage, iLength);
    szMessage[iLength] = '\0';

    Log("[GAMETEXT] %s", szMessage);
}

void ScrPlayAudioStream(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
    unsigned char bURLLen;
    char szURL[256];

    bsData.Read(bURLLen);
    bsData.Read(szURL, bURLLen);
    szURL[bURLLen] = 0;

    Log("[AUDIO_STREAM] %s", szURL);
}

int iDrunkLevel = 0;

void ScrSetDrunkLevel(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(iDrunkLevel);
}

int iMoney = 0;

void ScrHaveSomeMoney(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    int iGivenMoney;
    bsData.Read(iGivenMoney);

    iMoney += iGivenMoney;
}

void ScrResetMoney(RPCParameters* rpcParams)
{
    iMoney = 0;
}

float fNormalModePos[3];
float fNormalModeRot;

// SEH-wrap CStreaming::LoadScene + LoadAllRequestedModels in a frame with no
// C++ destructors. The streamer occasionally AVs when called from our
// bootstrap state (no CGame::Initialise) — losing the teleport because the
// outer dispatch SEH would skip the SetPosition that should follow. With this
// shim, a fault in the pre-load step is logged and swallowed, and the caller
// continues to apply the position.
static int try_stream_load_scene(float x, float y, float z)
{
    __try
    {
        gta::sa::StreamingLoadScene(x, y, z);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void ScrSetPlayerPos(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(fNormalModePos[0]);
    bsData.Read(fNormalModePos[1]);
    bsData.Read(fNormalModePos[2]);

    auto* ped = gta::sa::GetLocalPlayerPed();
    if (!ped) return;

    // Pre-load IPLs / collision around the destination so interiors at high Z
    // don't drop the player into the void while geometry streams in. If the
    // streamer faults (it sometimes does in our bootstrap state), swallow
    // and proceed — getting the ped to the right XY beats the rare void-fall.
    if (!try_stream_load_scene(fNormalModePos[0], fNormalModePos[1], fNormalModePos[2]))
        Log("[SetPlayerPos] streaming pre-load faulted, applying position anyway");

    ped->SetPosition(fNormalModePos[0], fNormalModePos[1], fNormalModePos[2]);

    // Drive the SCM-side `refresh_streaming_at(X, Y)` to mirror what SAMP's
    // CPlayerPed::SetInterior does after a teleport. Triggers GTA's interior
    // collision/IPL load through the proper opcode dispatcher — direct
    // CStreaming::LoadScene above isn't enough on its own when the
    // destination is an interior cell at high Z.
    gta::sa::RefreshStreamingAt(fNormalModePos[0], fNormalModePos[1]);
}

void ScrSetPlayerFacingAngle(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(fNormalModeRot);

    if (auto* ped = gta::sa::GetLocalPlayerPed())
        ped->SetHeading(fNormalModeRot);
}

void ScrSetSpawnInfo(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    // NOTE: intentionally writes the *global* SpawnInfo (declared near
    // RequestClass) so sampSpawn() sees an up-to-date record. Using a local
    // here used to be a legacy bug — silently shadowed the global.
    bsData.Read((PCHAR)&SpawnInfo, sizeof(PLAYER_SPAWN_INFO));

    fNormalModePos[0] = SpawnInfo.vecPos[0];
    fNormalModePos[1] = SpawnInfo.vecPos[1];
    fNormalModePos[2] = SpawnInfo.vecPos[2];
    fNormalModeRot    = SpawnInfo.fRotation;
    iLocalPlayerSkin  = SpawnInfo.iSkin;
}

float fPlayerHealth;

void ScrSetPlayerHealth(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(fPlayerHealth);

    if (auto* ped = gta::sa::GetLocalPlayerPed())
        ped->Health() = fPlayerHealth;
}

float fPlayerArmour;

void ScrSetPlayerArmour(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    bsData.Read(fPlayerArmour);

    if (auto* ped = gta::sa::GetLocalPlayerPed())
        ped->Armour() = fPlayerArmour;
}

void ScrSetPlayerSkin(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    int iPlayerID;
    unsigned int uiSkin;

    bsData.Read(iPlayerID);
    bsData.Read(uiSkin);

    if (iPlayerID < 0 || iPlayerID >= MAX_PLAYERS)
        return;

    if (iGameInited && g_myPlayerID == iPlayerID)
    {
        iLocalPlayerSkin = uiSkin;

        if (auto* ped = gta::sa::GetLocalPlayerPed())
        {
            // Mirror SAMP's `CEntity::SetModelIndex` flow: pre-load model and
            // busy-wait until ready, only then swap. The strengthened
            // EnsureModelLoaded (LoadAll + Sleep loop) avoids the "swap to a
            // not-yet-loaded model" path that left m_rwObject hanging and
            // crashed deep inside CVisibilityPlugins lookups in earlier
            // attempts. The 0x5A82C0 NOP + clump-table guards make the
            // remaining vtable[8]/vtable[5] dance survivable.
            if (gta::sa::EnsureModelLoaded(static_cast<int>(uiSkin)))
                gta::sa::SafeSetPlayerPedModel(ped, static_cast<int>(uiSkin));
            else
                Log("[SetPlayerSkin] model %u failed to load — keeping current ped", uiSkin);
        }
    }
}

void ScrSetInterior(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    BYTE byteInterior;
    bsData.Read(byteInterior);

    Log("[ScrSetInterior] %u", static_cast<unsigned>(byteInterior));

    if (auto* ped = gta::sa::GetLocalPlayerPed())
        gta::sa::SetCurrentInterior(ped, byteInterior);
}

// ---------------- batch of newly-wired SAMP RPCs ----------------
// These fill in the gaps from TODO.md "Most RPC handlers are declared but not
// implemented". Each is one-direction (server → client). Wire formats taken
// from RakSAMP; effects tied to vanilla GTA SA US 1.0 globals
// where possible. Logging-only handlers note this and will need a real impl
// once the relevant local subsystem is wired.

namespace { struct WorldRpcState {
    BYTE weather = 0;
    BYTE worldTimeHour = 12;
    BYTE worldTimeMinute = 0;
    BYTE clockEnabled = 1;
    float gravity = 0.008f;
    float worldBounds[4] = { 20000.f, -20000.f, 20000.f, -20000.f }; // maxX,minX,maxY,minY
    bool  serverTimeKnown = false; // flips on first Weather/WorldTime/SetTimeEx
}; }
static WorldRpcState g_world;

static void ScrTogglePlayerControllable(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE byteControllable = 0;
    bs.Read(byteControllable);
    Log("[Ctrl] controllable=%u (no-op for now — needs CPlayerInfo wiring)",
        static_cast<unsigned>(byteControllable));
    // @todo find CPlayerInfo::m_PlayerData.bCanBeDamaged + control flags;
    //       for now, server can't disable our input.
}

static void ScrSetGravity(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    float g = 0;
    bs.Read(g);
    g_world.gravity = g;
    gta::sa::SetGravity(g);
    Log("[Gravity] %.5f", g);
}

static void ScrSetWorldBounds(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    bs.Read(g_world.worldBounds[0]);
    bs.Read(g_world.worldBounds[1]);
    bs.Read(g_world.worldBounds[2]);
    bs.Read(g_world.worldBounds[3]);
    Log("[Bounds] x[%.1f..%.1f] y[%.1f..%.1f]",
        g_world.worldBounds[1], g_world.worldBounds[0],
        g_world.worldBounds[3], g_world.worldBounds[2]);
    // @todo enforce by clamping ped position; cosmetic for now.
}

static void Weather(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE w = 0;
    bs.Read(w);
    g_world.weather = w;
    gta::sa::SetWeather(w);
    Log("[Weather] %u", static_cast<unsigned>(w));
}

static void WorldTime(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE h = 0;
    bs.Read(h);
    g_world.worldTimeHour = h;
    g_world.serverTimeKnown = true;
    gta::sa::SetWorldTime(h, g_world.worldTimeMinute);
    Log("[WorldTime] hour=%u", static_cast<unsigned>(h));
}

static void SetTimeEx(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE h = 0, m = 0;
    bs.Read(h);
    bs.Read(m);
    g_world.worldTimeHour   = h;
    g_world.worldTimeMinute = m;
    g_world.serverTimeKnown = true;
    gta::sa::SetWorldTime(h, m);
    Log("[SetTimeEx] %02u:%02u", static_cast<unsigned>(h), static_cast<unsigned>(m));
}

static void ToggleClock(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE c = 0;
    bs.Read(c);
    g_world.clockEnabled = c;
    Log("[ToggleClock] %u (no-op — clock keeps running for now)",
        static_cast<unsigned>(c));
    // @todo route to CClock disable: 0xB7014C area "ClockSpeed" or similar.
}

static void ScrSetCameraPos(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    float x = 0, y = 0, z = 0;
    bs.Read(x); bs.Read(y); bs.Read(z);
    gta::sa::CameraSetPosition(x, y, z);
    Log("[Camera] pos (%.1f, %.1f, %.1f)", x, y, z);
}

static void ScrSetCameraLookAt(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    float x = 0, y = 0, z = 0;
    bs.Read(x); bs.Read(y); bs.Read(z);
    Log("[Camera] lookAt (%.1f, %.1f, %.1f) — @todo PointCamera invocation",
        x, y, z);
    // @todo CCamera::PointCamera (0x50AD60). The thiscall signature isn't
    //       fully reversed in mod_s0beit_sa; need a separate exploration pass.
}

static void ScrSetCameraBehindPlayer(RPCParameters* /*rpcParams*/)
{
    gta::sa::CameraRestore();
    Log("[Camera] behind-player (RestoreWithJumpCut)");
}

static void ScrPlaySound(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    int sound = 0;
    float x = 0, y = 0, z = 0;
    bs.Read(sound); bs.Read(x); bs.Read(y); bs.Read(z);
    Log("[PlaySound] id=%d at (%.1f, %.1f, %.1f)", sound, x, y, z);
    // @todo route to CAudioEngine.
}

static void GameModeRestart(RPCParameters* /*rpcParams*/)
{
    Log("[GameModeRestart] server requested gamemode restart");
    // @todo proper shutdown — for now we drop spawn state so the FSM
    //       re-runs RequestClass / Spawn next iteration.
    extern int iSpawned;
    extern bool g_spawnInfoReceived;
    iSpawned = 0;
    g_spawnInfoReceived = false;
}

static void ScrSetPlayerName(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    PLAYERID playerId = 0;
    BYTE nameLen = 0, success = 0;
    char name[64] = {};
    bs.Read(playerId);
    bs.Read(nameLen);
    if (nameLen >= sizeof(name)) return;
    bs.Read(name, nameLen);
    name[nameLen] = '\0';
    bs.Read(success);
    const auto utf8 = opensamp::util::cp1251_to_utf8(name);
    Log("[SetPlayerName] %u -> '%s' (success=%u)",
        static_cast<unsigned>(playerId), utf8.c_str(),
        static_cast<unsigned>(success));
    if (success && playerId < MAX_PLAYERS)
    {
        std::strncpy(playerInfo[playerId].szPlayerName, utf8.c_str(),
                     sizeof(playerInfo[playerId].szPlayerName) - 1);
    }
}

static void ScrSetPlayerColor(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    PLAYERID playerId = 0;
    DWORD color = 0;
    bs.Read(playerId);
    bs.Read(color);
    Log("[SetPlayerColor] %u = %08X",
        static_cast<unsigned>(playerId), color);
    // @todo apply to nametags / blip / chat color when those are wired.
}

static void ScrShowNameTag(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    PLAYERID playerId = 0;
    BYTE show = 0;
    bs.Read(playerId);
    bs.Read(show);
    Log("[ShowNameTag] %u show=%u",
        static_cast<unsigned>(playerId), static_cast<unsigned>(show));
}

static void ScrCreateExplosion(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    float x = 0, y = 0, z = 0, radius = 0;
    int type = 0;
    bs.Read(x); bs.Read(y); bs.Read(z);
    bs.Read(type); bs.Read(radius);
    Log("[Explosion] type=%d radius=%.1f at (%.1f, %.1f, %.1f)",
        type, radius, x, y, z);
    // @todo call CExplosion::AddExplosion (0x736A50) with proper signature.
}

static void ScrSetMapIcon(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE iconId = 0;
    float x = 0, y = 0, z = 0;
    BYTE iconType = 0;
    DWORD color = 0;
    BYTE style = 0;
    bs.Read(iconId);
    bs.Read(x); bs.Read(y); bs.Read(z);
    bs.Read(iconType); bs.Read(color); bs.Read(style);
    Log("[MapIcon] id=%u type=%u style=%u at (%.1f, %.1f, %.1f)",
        static_cast<unsigned>(iconId), static_cast<unsigned>(iconType),
        static_cast<unsigned>(style), x, y, z);
}

static void ScrDisableMapIcon(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE iconId = 0;
    bs.Read(iconId);
    Log("[MapIcon] disable id=%u", static_cast<unsigned>(iconId));
}

static void ScrDeathMessage(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    PLAYERID killer = 0, killed = 0;
    BYTE weapon = 0;
    bs.Read(killer);
    bs.Read(killed);
    bs.Read(weapon);

    const char* killerName = (killer < MAX_PLAYERS && killer != 0xFFFF)
        ? playerInfo[killer].szPlayerName : nullptr;
    const char* killedName = (killed < MAX_PLAYERS && killed != 0xFFFF)
        ? playerInfo[killed].szPlayerName : nullptr;

    if (killerName && killedName)
        Log("[Death] %s -> %s (weapon %u)",
            killerName, killedName, static_cast<unsigned>(weapon));
    else if (killedName)
        Log("[Death] %s died (reason %u)",
            killedName, static_cast<unsigned>(weapon));
    else
        Log("[Death] %u -> %u (weapon %u)",
            static_cast<unsigned>(killer), static_cast<unsigned>(killed),
            static_cast<unsigned>(weapon));
}

static void StopAudioStream(RPCParameters* /*rpcParams*/)
{
    Log("[Audio] stop stream");
    // @todo route to streaming audio module.
}

static void ScrGivePlayerWeapon(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    int weaponId = 0, ammo = 0;
    bs.Read(weaponId);
    bs.Read(ammo);
    if (auto* ped = gta::sa::GetLocalPlayerPed())
    {
        ped->GiveWeapon(weaponId, ammo);
        Log("[Weapon] give id=%d ammo=%d", weaponId, ammo);
    }
}

static void ScrResetPlayerWeapons(RPCParameters* /*rpcParams*/)
{
    if (auto* ped = gta::sa::GetLocalPlayerPed())
    {
        ped->ClearWeapons();
        Log("[Weapon] reset all");
    }
}

static void ScrSetWeaponAmmo(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    BYTE weapon = 0;
    WORD ammo = 0;
    bs.Read(weapon);
    bs.Read(ammo);
    Log("[Weapon] setAmmo slot=%u ammo=%u — @todo direct SetAmmo call",
        static_cast<unsigned>(weapon), static_cast<unsigned>(ammo));
    // @todo gta_sa.exe doesn't expose a single CPed::SetAmmo entry point;
    //       SAMP iterates m_pWeapons[] and writes ammo fields. We don't
    //       have CWeapon offsets mapped yet, so this is a stub.
}

static void ScrSetPlayerPosFindZ(RPCParameters* rpcParams)
{
    RakNet::BitStream bs(reinterpret_cast<unsigned char*>(rpcParams->input),
                         (rpcParams->numberOfBitsOfData / 8) + 1, false);
    float x = 0, y = 0, z = 0;
    bs.Read(x); bs.Read(y); bs.Read(z);

    const float groundZ = gta::sa::FindGroundZ(x, y);
    const float finalZ  = (groundZ > -100.0f ? groundZ : z) + 1.5f;

    if (auto* ped = gta::sa::GetLocalPlayerPed())
    {
        ped->SetPosition(x, y, finalZ);
        fNormalModePos[0] = x;
        fNormalModePos[1] = y;
        fNormalModePos[2] = finalZ;
    }
    Log("[SetPlayerPosFindZ] (%.1f, %.1f, %.1f -> groundZ=%.1f, final=%.1f)",
        x, y, z, groundZ, finalZ);
}

void ScrCreateObject(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    unsigned short ObjectID;
    bsData.Read(ObjectID);

    unsigned long ModelID;
    bsData.Read(ModelID);

    float vecPos[3];
    bsData.Read(vecPos[0]);
    bsData.Read(vecPos[1]);
    bsData.Read(vecPos[2]);

    float vecRot[3];
    bsData.Read(vecRot[0]);
    bsData.Read(vecRot[1]);
    bsData.Read(vecRot[2]);

    float fDrawDistance;
    bsData.Read(fDrawDistance);

    char szCreateObjectAlert[256];
    sprintf_s(szCreateObjectAlert, sizeof(szCreateObjectAlert),
              "[OBJECT] %d, %d, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.2f", ObjectID, ModelID, vecPos[0], vecPos[1],
              vecPos[2], vecRot[0], vecRot[1], vecRot[2], fDrawDistance);
    Log(szCreateObjectAlert);
}

void ScrCreate3DTextLabel(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    WORD ID;
    CHAR Text[256];
    DWORD dwColor;
    FLOAT vecPos[3];
    FLOAT DrawDistance;
    BYTE UseLOS;
    WORD PlayerID;
    WORD VehicleID;

    bsData.Read(ID);
    bsData.Read(dwColor);
    bsData.Read(vecPos[0]);
    bsData.Read(vecPos[1]);
    bsData.Read(vecPos[2]);
    bsData.Read(DrawDistance);
    bsData.Read(UseLOS);
    bsData.Read(PlayerID);
    bsData.Read(VehicleID);

    stringCompressor->DecodeString(Text, 256, &bsData);

    char szCreate3DTextLabelAlert[256];
    sprintf_s(szCreate3DTextLabelAlert, sizeof(szCreate3DTextLabelAlert),
              "[TEXTLABEL] %d - %s (%X, %.3f, %.3f, %.3f, %.2f, %i, %d, %d)", ID, Text, dwColor, vecPos[0], vecPos[1],
              vecPos[2], DrawDistance, UseLOS, PlayerID, VehicleID);
    Log(szCreate3DTextLabelAlert);
}

using TEXT_DRAW_TRANSMIT = struct _TEXT_DRAW_TRANSMIT
{
    union
    {
        BYTE byteFlags;

        struct
        {
            BYTE byteBox : 1;
            BYTE byteLeft : 1;
            BYTE byteRight : 1;
            BYTE byteCenter : 1;
            BYTE byteProportional : 1;
            BYTE bytePadding : 3;
        };
    };

    float fLetterWidth;
    float fLetterHeight;
    DWORD dwLetterColor;
    float fLineWidth;
    float fLineHeight;
    DWORD dwBoxColor;
    BYTE byteShadow;
    BYTE byteOutline;
    DWORD dwBackgroundColor;
    BYTE byteStyle;
    BYTE byteSelectable;
    float fX;
    float fY;
    WORD wModelID;
    float fRotX;
    float fRotY;
    float fRotZ;
    float fZoom;
    WORD wColor1;
    WORD wColor2;
};

FILE* flTextDrawsLog = nullptr;

void SaveTextDrawData(WORD wTextID, TEXT_DRAW_TRANSMIT* pData, CHAR* cText)
{
    Log("TextDraw ID: %d, Text: %s\n", wTextID, cText);
    Log("Flags: box(%i), left(%i), right(%i), center(%i), proportional(%i), padding(%i)\n", pData->byteBox,
        pData->byteLeft, pData->byteRight, pData->byteCenter, pData->byteProportional, pData->bytePadding);
    Log("LetterWidth: %.3f, LetterHeight: %.3f, LetterColor: %X, LineWidth: %.3f, LineHeight: %.3f\n",
        pData->fLetterWidth, pData->fLetterHeight, pData->dwLetterColor, pData->fLineWidth, pData->fLineHeight);
    Log("BoxColor: %X, Shadow: %i, Outline: %i, BackgroundColor: %X, Style: %i, Selectable: %i\n", pData->dwBoxColor,
        pData->byteShadow, pData->byteOutline, pData->dwBackgroundColor, pData->byteStyle, pData->byteSelectable);
    Log("X: %.3f, Y: %.3f, ModelID: %d, RotX: %.3f, RotY: %.3f, RotZ: %.3f, Zoom: %.3f, Colors: %d, %d", pData->fX,
        pData->fY, pData->wModelID, pData->fRotX, pData->fRotY, pData->fRotZ, pData->fZoom, pData->wColor1,
        pData->wColor2);
}

void ScrShowTextDraw(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    WORD wTextID;
    TEXT_DRAW_TRANSMIT TextDrawTransmit{};

    CHAR cText[1024];
    unsigned short cTextLen = 0;

    bsData.Read(wTextID);
    // Stored by SaveTextDrawData and branched on below, so a short packet
    // must not be allowed to leave it holding stack garbage.
    if (!bsData.Read((PCHAR)&TextDrawTransmit, sizeof(TEXT_DRAW_TRANSMIT))) return;
    bsData.Read(cTextLen);
    // cTextLen is server-controlled and reaches 65535; cText is 1 KiB.
    if (cTextLen >= sizeof(cText)) return;
    bsData.Read(cText, cTextLen);
    cText[cTextLen] = '\0';

    SaveTextDrawData(wTextID, &TextDrawTransmit, cText);

    if (TextDrawTransmit.byteSelectable)
        Log("[SELECTABLE-TEXTDRAW] ID: %d, Text: %s.", wTextID, cText);
}


void ScrHideTextDraw(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    WORD wTextID;
    bsData.Read(wTextID);

    Log("[TEXTDRAW:HIDE] ID: %d.", wTextID);
}

void ScrEditTextDraw(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    WORD wTextID;
    CHAR cText[1024];
    unsigned short cTextLen = 0;

    bsData.Read(wTextID);
    bsData.Read(cTextLen);
    // cTextLen is server-controlled and reaches 65535; cText is 1 KiB.
    if (cTextLen >= sizeof(cText)) return;
    bsData.Read(cText, cTextLen);
    cText[cTextLen] = '\0';

    Log("[TEXTDRAW:EDIT] ID: %d, Text: %s.", wTextID, cText);
}

bool bIsSpectating = false;

void sampSpawn()
{
    if (pRakClient == nullptr) return;

    if (iSpawned == 0)
    {
        iLocalPlayerSkin  = SpawnInfo.iSkin;
        fNormalModePos[0] = SpawnInfo.vecPos[0];
        fNormalModePos[1] = SpawnInfo.vecPos[1];
        fNormalModePos[2] = SpawnInfo.vecPos[2];
        fNormalModeRot    = SpawnInfo.fRotation;
    }

    RakNet::BitStream bsSendRequestSpawn;
    pRakClient->RPC(&RPC_RequestSpawn, &bsSendRequestSpawn, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID,
                    nullptr);

    RakNet::BitStream bsSendSpawn;
    pRakClient->RPC(&RPC_Spawn, &bsSendSpawn, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, nullptr);

    // Apply spawn point to the local ped directly — server-driven sync will
    // eventually take over but for now we must teleport here or the player
    // stays wherever BootstrapStart_NoScm parked them.
    //
    // Caveats:
    //  - SetModelIndex on an un-streamed model is a known crash. Block on
    //    EnsureModelLoaded first.
    //  - SetModelIndex can replace the underlying CPed instance, invalidating
    //    the pointer we just read. Re-read via *0xB6F5F0 after the swap.
    //  - Calling SetModelIndex with the model already in use is not a no-op
    //    for GTA: it still walks the ped's internal anim/task lists, and a
    //    stale/uninitialised entry there crashes us deep inside (observed
    //    ACCESS_VIOLATION at gta_sa.exe+0x349b7b). Skip when unchanged.
    if (auto* ped = gta::sa::GetLocalPlayerPed())
    {
        // Skin swap intentionally skipped — see ScrSetPlayerSkin and
        // OnGameReadyOnce for the rationale. We just teleport the
        // bootstrap-default ped to the spawn point.
        Log("[Spawn] skin requested=%u, keeping default 'player' model",
            (unsigned)iLocalPlayerSkin);
        ped->SetPosition(fNormalModePos[0], fNormalModePos[1], fNormalModePos[2]);
        ped->SetHeading(fNormalModeRot);
        ped->Health() = 100.0f;
        ped->Armour() = 0.0f;
    }

    bIsSpectating = false;

    Log("You have been spawned!");
}

void ScrTogglePlayerSpectating(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    BOOL bToggle;

    bsData.Read(bToggle);

    if (bIsSpectating && !bToggle && !iSpawned)
    {
        sampSpawn();
        iSpawned = 1;
    }

    bIsSpectating = bToggle;
}

// RPC 70 — server seats the local player into a vehicle. Wire format (0.3.7):
// VEHICLEID + BYTE seat (no playerId — it always targets the receiving client).
// We record the network vehicle id so in-car sync can stamp it into the packet.
void ScrPutPlayerInVehicle(RPCParameters* rpcParams)
{
    auto Data = reinterpret_cast<PCHAR>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;

    RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

    VEHICLEID vehicleId;
    BYTE      seatId;
    bsData.Read(vehicleId);
    bsData.Read(seatId);

    if (vehicleId >= MAX_VEHICLES)
        return;

    g_localVehicleId = vehicleId;
    g_localSeatId    = seatId;
}

// RPC 71 — server removes the local player from their vehicle.
void ScrRemovePlayerFromVehicle(RPCParameters* /*rpcParams*/)
{
    g_localVehicleId = INVALID_VEHICLE_ID;
    g_localSeatId    = 0;
}

void RegisterRPCs(RakClientInterface* pRakClient)
{
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerJoin, ServerJoin);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerQuit, ServerQuit);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_InitGame, InitGame);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd, WorldPlayerAdd);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath, WorldPlayerDeath);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove, WorldPlayerRemove);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd, WorldVehicleAdd);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove, WorldVehicleRemove);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPutPlayerInVehicle, ScrPutPlayerInVehicle);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemovePlayerFromVehicle, ScrRemovePlayerFromVehicle);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ConnectionRejected, ConnectionRejected);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, ClientMessage);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_Chat, Chat);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs, UpdateScoresPingsIPs);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetCheckpoint, SetCheckpoint);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_DisableCheckpoint, DisableCheckpoint);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_Pickup, Pickup);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_DestroyPickup, DestroyPickup);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_RequestClass, RequestClass);

    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrInitMenu, ScrInitMenu);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDialogBox, ScrDialogBox);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText, ScrGameText);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_PlayAudioStream, ScrPlayAudioStream);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel, ScrSetDrunkLevel);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney, ScrHaveSomeMoney);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetMoney, ScrResetMoney);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, ScrSetPlayerPos);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle, ScrSetPlayerFacingAngle);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, ScrSetSpawnInfo);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth, ScrSetPlayerHealth);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour, ScrSetPlayerArmour);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin, ScrSetPlayerSkin);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetInterior, ScrSetInterior);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateObject, ScrCreateObject);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel, ScrCreate3DTextLabel);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw, ScrShowTextDraw);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw, ScrHideTextDraw);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw, ScrEditTextDraw);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, ScrTogglePlayerSpectating);

    // ---- Newly-wired RPCs (world / time / weather / camera / misc) ----
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerControllable, ScrTogglePlayerControllable);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetGravity,            ScrSetGravity);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWorldBounds,        ScrSetWorldBounds);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_Weather,                  Weather);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldTime,                WorldTime);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetTimeEx,                SetTimeEx);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ToggleClock,              ToggleClock);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraPos,          ScrSetCameraPos);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraLookAt,       ScrSetCameraLookAt);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraBehindPlayer, ScrSetCameraBehindPlayer);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlaySound,             ScrPlaySound);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_GameModeRestart,          GameModeRestart);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerName,         ScrSetPlayerName);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerColor,        ScrSetPlayerColor);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowNameTag,           ScrShowNameTag);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateExplosion,       ScrCreateExplosion);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetMapIcon,            ScrSetMapIcon);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisableMapIcon,        ScrDisableMapIcon);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDeathMessage,          ScrDeathMessage);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_StopAudioStream,          StopAudioStream);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrGivePlayerWeapon,      ScrGivePlayerWeapon);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetPlayerWeapons,    ScrResetPlayerWeapons);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWeaponAmmo,         ScrSetWeaponAmmo);
    pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPosFindZ,     ScrSetPlayerPosFindZ);
}

void UnRegisterRPCs(RakClientInterface* pRakClient)
{
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerJoin);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerQuit);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_InitGame);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ConnectionRejected);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ClientMessage);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Chat);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetCheckpoint);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DisableCheckpoint);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Pickup);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DestroyPickup);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_RequestClass);

    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrInitMenu);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDialogBox);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_PlayAudioStream);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrResetMoney);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetInterior);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateObject);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating);

    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerControllable);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetGravity);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetWorldBounds);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Weather);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldTime);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetTimeEx);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ToggleClock);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraPos);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraLookAt);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraBehindPlayer);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrPlaySound);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_GameModeRestart);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerName);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerColor);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrShowNameTag);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateExplosion);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetMapIcon);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisableMapIcon);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDeathMessage);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_StopAudioStream);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrGivePlayerWeapon);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrResetPlayerWeapons);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetWeaponAmmo);
    pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPosFindZ);
}

int sampConnect(char* szHostname, int iPort, char* szNickname, char* szPassword, RakClientInterface* pRakClient)
{
    if (!iAreWeConnected) Log("Connecting to %s:%d..", szHostname, iPort);

    strcpy(g_szNickName, szNickname);
    if (pRakClient == nullptr) return 0;

    pRakClient->SetPassword(szPassword);
    return pRakClient->Connect(szHostname, iPort, 0, 0, 5);
}

void sampRequestClass(int iClass)
{
    if (pRakClient == nullptr) return;

    RakNet::BitStream bsSpawnRequest;
    bsSpawnRequest.Write(iClass);
    pRakClient->RPC(&RPC_RequestClass, &bsSpawnRequest, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID,
                    nullptr);
}

struct stServer
{
    char szAddr[256];
    int iPort;
    char szNickname[20];
    char szPassword[32];
};

unsigned short getPlayerCount()
{
    unsigned short count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!playerInfo[i].iIsConnected) continue;
        count++;
    }
    return count;
}

int iClassID = 0;
int iNetModeNormalOnfootSendRate = 50;
DWORD dwLastOnFootDataSentTick = GetTickCount();
byte bCurrentWeapon = 0;

void SendWastedNotification(BYTE byteDeathReason, PLAYERID WhoWasResponsible)
{
    RakNet::BitStream bsPlayerDeath;

    bsPlayerDeath.Write(byteDeathReason);
    bsPlayerDeath.Write(WhoWasResponsible);
    pRakClient->RPC(&RPC_Death, &bsPlayerDeath, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID,
                    nullptr);
}

void SendDamageVehicle(WORD vehicleID, DWORD panel, DWORD door, BYTE lights, BYTE tires)
{
    RakNet::BitStream bsDamageVehicle;

    bsDamageVehicle.Write(vehicleID);
    bsDamageVehicle.Write(panel);
    bsDamageVehicle.Write(door);
    bsDamageVehicle.Write(lights);
    bsDamageVehicle.Write(tires);
    pRakClient->RPC(&RPC_DamageVehicle, &bsDamageVehicle, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE,
                    UNASSIGNED_NETWORK_ID, nullptr);
}

void SendEnterVehicleNotification(VEHICLEID VehicleID, BOOL bPassenger)
{
    RakNet::BitStream bsSend;
    BYTE bytePassenger = 0;

    if (bPassenger)
        bytePassenger = 1;

    bsSend.Write(VehicleID);
    bsSend.Write(bytePassenger);
    pRakClient->RPC(&RPC_EnterVehicle, &bsSend, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID,
                    nullptr);
}

void SendExitVehicleNotification(VEHICLEID VehicleID)
{
    RakNet::BitStream bsSend;
    bsSend.Write(VehicleID);
    pRakClient->RPC(&RPC_ExitVehicle, &bsSend, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID,
                    nullptr);
}

void SendOnFootFullSyncData(ONFOOT_SYNC_DATA* pofSync, int sendDeathNoti)
{
    if (pofSync == nullptr)
        return;

    RakNet::BitStream bsPlayerSync;

    if (dwLastOnFootDataSentTick && dwLastOnFootDataSentTick < (GetTickCount() - iNetModeNormalOnfootSendRate))
    {
        if (true)
        {
            //if(!playerInfo[followPlayerID].iIsConnected)
            //	return;

            //pofSync->lrAnalog = playerInfo[followPlayerID].onfootData.lrAnalog;
            //pofSync->udAnalog = playerInfo[followPlayerID].onfootData.udAnalog;
            //pofSync->wKeys = playerInfo[followPlayerID].onfootData.wKeys;

            pofSync->vecPos[0] = playerInfo[g_myPlayerID].onfootData.vecPos[0] + static_cast<float>(0.5);
            pofSync->vecPos[1] = playerInfo[g_myPlayerID].onfootData.vecPos[1] + static_cast<float>(0.5);
            pofSync->vecPos[2] = playerInfo[g_myPlayerID].onfootData.vecPos[2];

            //pofSync->fQuaternion[0] = playerInfo[followPlayerID].onfootData.fQuaternion[0];
            //pofSync->fQuaternion[1] = playerInfo[followPlayerID].onfootData.fQuaternion[1];
            //pofSync->fQuaternion[2] = playerInfo[followPlayerID].onfootData.fQuaternion[2];
            //pofSync->fQuaternion[3] = playerInfo[followPlayerID].onfootData.fQuaternion[3];

            // pofSync->byteHealth = playerInfo[followPlayerID].onfootData.byteHealth;
            // pofSync->byteArmour = playerInfo[followPlayerID].onfootData.byteArmour;

            pofSync->byteCurrentWeapon = bCurrentWeapon;
            //pofSync->byteSpecialAction = playerInfo[g_myPlayerID].onfootData.byteSpecialAction;

            pofSync->vecMoveSpeed[0] = 0.5;
            pofSync->vecMoveSpeed[1] = 0.5;
            pofSync->vecMoveSpeed[2] = playerInfo[g_myPlayerID].onfootData.vecMoveSpeed[2];

            // pofSync->iCurrentAnimationID = playerInfo[g_myPlayerID].onfootData.iCurrentAnimationID;

            //fCurrentPosition[0] = pofSync->vecPos[0];
            //fCurrentPosition[1] = pofSync->vecPos[1];
            //fCurrentPosition[2] = pofSync->vecPos[2];

            bsPlayerSync.Write(static_cast<BYTE>(ID_PLAYER_SYNC));
            bsPlayerSync.Write((PCHAR)pofSync, sizeof(ONFOOT_SYNC_DATA));
            pRakClient->Send(&bsPlayerSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);

            if (sendDeathNoti && pofSync->byteHealth == 0)
                SendWastedNotification(0, -1);

            dwLastOnFootDataSentTick = GetTickCount();
        }
    }
}

DWORD dwLastInVehicleDataSentTick = GetTickCount();
int iNetModeNormalIncarSendRate = 40;

// Build and send the local DRIVER's in-car sync (ID_VEHICLE_SYNC).
//
// The wire format is the EXACT inverse of Packet_VehicleSync: per-field
// (de)compression, NOT a raw struct dump. The server prepends our playerId
// when relaying to other clients, so we must NOT write it ourselves.
//
// Milestone-1 scope: driver only, and only when we know our network VehicleID
// (set by the server via ScrPutPlayerInVehicle). Input keys, weapon, siren,
// landing gear, trailer, hydra and train specials are left at neutral defaults
// — position/orientation/velocity/health are the live, correct values.
//
// @todo player-driven entry (no server seat) needs a CVehicle*<->VEHICLEID
//       registry fed by WorldVehicleAdd; passenger/unoccupied/trailer senders.
void inCarUpdate()
{
    if (g_localVehicleId >= MAX_VEHICLES)
        return; // unknown network id — cannot address the vehicle yet

    auto* veh = gta::sa::GetLocalVehicle();
    auto* ped = gta::sa::GetLocalPlayerPed();
    if (!veh || !ped)
        return;

    if (dwLastInVehicleDataSentTick &&
        dwLastInVehicleDataSentTick >= (GetTickCount() - iNetModeNormalIncarSendRate))
        return;

    // ---- gather live state ----
    float qw, qx, qy, qz;
    veh->GetQuaternion(qw, qx, qy, qz);

    const auto pos = veh->m_position;
    const auto vel = veh->MoveSpeed();
    const auto wVehicleHealth = static_cast<WORD>(veh->Health());

    // Health/armour packed into one byte (inverse of Packet_VehicleSync decode:
    // 100 -> 0xF, 0 -> 0, else value/7). High nibble = health, low = armour.
    const auto encode4 = [](int v) -> BYTE
    {
        if (v >= 100) return 0xF;
        if (v <= 0)   return 0;
        return static_cast<BYTE>(v / 7);
    };
    const auto byteHealthArmour = static_cast<BYTE>(
        (encode4(static_cast<int>(ped->Health())) << 4) |
         encode4(static_cast<int>(ped->Armour())));

    // ---- write packet (mirror of Packet_VehicleSync, minus the playerId) ----
    RakNet::BitStream bsVehicleSync;
    bsVehicleSync.Write(static_cast<BYTE>(ID_VEHICLE_SYNC));
    bsVehicleSync.Write(g_localVehicleId);

    bsVehicleSync.Write(static_cast<WORD>(0)); // lrAnalog
    bsVehicleSync.Write(static_cast<WORD>(0)); // udAnalog
    bsVehicleSync.Write(static_cast<WORD>(0)); // wKeys

    bsVehicleSync.WriteNormQuat(qw, qx, qy, qz);

    bsVehicleSync.Write(pos.x);
    bsVehicleSync.Write(pos.y);
    bsVehicleSync.Write(pos.z);

    bsVehicleSync.WriteVector(vel.x, vel.y, vel.z);

    bsVehicleSync.Write(wVehicleHealth);
    bsVehicleSync.Write(byteHealthArmour);
    bsVehicleSync.Write(static_cast<BYTE>(0)); // byteCurrentWeapon (drive-by only)

    bsVehicleSync.WriteCompressed(false); // siren on
    bsVehicleSync.WriteCompressed(false); // landing gear
    bsVehicleSync.WriteCompressed(false); // hydra thrust present
    bsVehicleSync.WriteCompressed(false); // trailer present
    bsVehicleSync.Write(static_cast<DWORD>(0)); // trailer id / hydra thrust angle
    bsVehicleSync.WriteCompressed(false); // train (no WORD train-speed follows)

    pRakClient->Send(&bsVehicleSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
    dwLastInVehicleDataSentTick = GetTickCount();
}

DWORD dwLastPassengerDataSentTick = GetTickCount();

void SendPassengerFullSyncData(VEHICLEID vehicleID)
{
    //if(!vehiclePool[vehicleID].iDoesExist)
    //	return;
    //
    //if(dwLastPassengerDataSentTick && dwLastPassengerDataSentTick < (GetTickCount() - iNetModeNormalIncarSendRate))
    //{
    //	RakNet::BitStream bsPassengerSync;
    //
    //	PASSENGER_SYNC_DATA psSync;
    //	memset(&psSync, 0, sizeof(PASSENGER_SYNC_DATA));
    //
    //	psSync.VehicleID = vehicleID;
    //
    //	psSync.vecPos[0] = vehiclePool[vehicleID].fPos[0];
    //	psSync.vecPos[1] = vehiclePool[vehicleID].fPos[1];
    //	psSync.vecPos[2] = vehiclePool[vehicleID].fPos[2];
    //
    //	psSync.bytePlayerHealth = (BYTE)fPlayerHealth;
    //	psSync.bytePlayerArmour = (BYTE)fPlayerArmour;
    //
    //	bsPassengerSync.Write((BYTE)ID_PASSENGER_SYNC);
    //	bsPassengerSync.Write((PCHAR)&psSync, sizeof(PASSENGER_SYNC_DATA));
    //	pRakClient->Send(&bsPassengerSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
    //
    //	dwLastPassengerDataSentTick = GetTickCount();
    //}
}

int iNetModeFiringSendRate = 40; // @todo rates from server

DWORD dwLastAimDataSentTick = GetTickCount();

void SendAimSyncData(DWORD dwAmmoInClip, int iReloading, PLAYERID copyFromPlayer)
{
    if (dwLastAimDataSentTick && dwLastAimDataSentTick < (GetTickCount() - iNetModeFiringSendRate))
    {
        RakNet::BitStream bsAimSync;
        AIM_SYNC_DATA aimSync;

        if (copyFromPlayer != static_cast<PLAYERID>(-1))
        {
            if (!playerInfo[copyFromPlayer].iIsConnected)
                return;

            memcpy(&aimSync, &playerInfo[copyFromPlayer].aimData, sizeof(AIM_SYNC_DATA));

            if (aimSync.vecAimPos[0] == 0.0f && aimSync.vecAimPos[1] == 0.0f && aimSync.vecAimPos[2] == 0.0f)
            {
                aimSync.vecAimPos[0] = 0.25f;
            }

            bsAimSync.Write(static_cast<BYTE>(ID_AIM_SYNC));
            bsAimSync.Write((PCHAR)&aimSync, sizeof(AIM_SYNC_DATA));

            pRakClient->Send(&bsAimSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);

            dwLastAimDataSentTick = GetTickCount();
        }
        else
        {
            if (iReloading)
                playerInfo[g_myPlayerID].aimData.byteWeaponState = WS_RELOADING;
            else
                playerInfo[g_myPlayerID].aimData.byteWeaponState = (dwAmmoInClip > 1) ? WS_MORE_BULLETS : dwAmmoInClip;

            playerInfo[g_myPlayerID].aimData.bUnk = 0x55;

            memcpy(&aimSync, &playerInfo[g_myPlayerID].aimData, sizeof(AIM_SYNC_DATA));

            bsAimSync.Write(static_cast<BYTE>(ID_AIM_SYNC));
            bsAimSync.Write((PCHAR)&aimSync, sizeof(AIM_SYNC_DATA));

            pRakClient->Send(&bsAimSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);

            dwLastAimDataSentTick = GetTickCount();
        }
    }
}

void onFootUpdateAtNormalPos()
{
    ONFOOT_SYNC_DATA ofSync;
    memset(&ofSync, 0, sizeof(ONFOOT_SYNC_DATA));

    ofSync.byteHealth = static_cast<BYTE>(fPlayerHealth);
    ofSync.byteArmour = static_cast<BYTE>(fPlayerArmour);
    ofSync.fQuaternion[3] = fNormalModeRot;
    ofSync.vecPos[0] = fNormalModePos[0];
    ofSync.vecPos[1] = fNormalModePos[1];
    ofSync.vecPos[2] = fNormalModePos[2];

    SendOnFootFullSyncData(&ofSync, 0);

    AIM_SYNC_DATA aimSync;
    memset(&aimSync, 0, sizeof(AIM_SYNC_DATA));

    playerInfo[g_myPlayerID].aimData.byteCamMode = 4;
    playerInfo[g_myPlayerID].aimData.vecAimf1[0] = 0.1f;
    playerInfo[g_myPlayerID].aimData.vecAimf1[1] = 0.1f;
    playerInfo[g_myPlayerID].aimData.vecAimf1[2] = 0.1f;
    playerInfo[g_myPlayerID].aimData.vecAimPos[0] = fNormalModePos[0];
    playerInfo[g_myPlayerID].aimData.vecAimPos[1] = fNormalModePos[1];
    playerInfo[g_myPlayerID].aimData.vecAimPos[2] = fNormalModePos[2];

    SendAimSyncData(0, 0, -1);
}

void SendSpectatorData(SPECTATOR_SYNC_DATA* pSpecData)
{
    RakNet::BitStream bsSpecSync;

    bsSpecSync.Write(static_cast<BYTE>(ID_SPECTATOR_SYNC));
    bsSpecSync.Write((PCHAR)pSpecData, sizeof(SPECTATOR_SYNC_DATA));

    pRakClient->Send(&bsSpecSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
}

void spectatorUpdate()
{
    SPECTATOR_SYNC_DATA spSync;
    memset(&spSync, 0, sizeof(SPECTATOR_SYNC_DATA));

    spSync.vecPos[0] = fNormalModePos[0];
    spSync.vecPos[1] = fNormalModePos[1];
    spSync.vecPos[2] = fNormalModePos[2];

    SendSpectatorData(&spSync);

    AIM_SYNC_DATA aimSync;
    memset(&aimSync, 0, sizeof(AIM_SYNC_DATA));

    playerInfo[g_myPlayerID].aimData.byteCamMode = 4;
    playerInfo[g_myPlayerID].aimData.vecAimf1[0] = 0.1f;
    playerInfo[g_myPlayerID].aimData.vecAimf1[1] = 0.1f;
    playerInfo[g_myPlayerID].aimData.vecAimf1[2] = 0.1f;
    playerInfo[g_myPlayerID].aimData.vecAimPos[0] = fNormalModePos[0];
    playerInfo[g_myPlayerID].aimData.vecAimPos[1] = fNormalModePos[1];
    playerInfo[g_myPlayerID].aimData.vecAimPos[2] = fNormalModePos[2];

    SendAimSyncData(0, 0, -1);
}

// Legacy entry — used to spawn a dedicated RakNet thread with a hardcoded
// server, its own Sleep(100) loop and auto-spawn. Superseded by the
// opensamp::bridge API driven from CNetGame. Kept as a no-op for now.
void TestRakNet() { }

namespace opensamp::bridge
{
    namespace
    {
        // NOTE: do NOT name anything `s_host` / `s_port` / `s_net` / `s_impno`
        // here — those are preprocessor macros inside winsock.h (`#define s_host
        // S_un.S_un_b.s_b2`) which will mangle our identifiers.
        bool           s_initialized         = false;
        bool           s_connectionRequested = false;
        char           s_serverHost[128]{};
        std::uint16_t  s_serverPort          = 0;
        char           s_serverPassword[64]{};

        // Spawn sub-FSM: can't send RequestClass+Spawn in the same tick
        // because SpawnInfo only lands in the RPC response from the server
        // (50-200ms later). Walk through stages instead.
        enum class SpawnStage : std::uint8_t { Idle, WaitingClass, Ready, Done };
        SpawnStage    s_spawnStage   = SpawnStage::Idle;
        std::uint32_t s_spawnStageMs = 0;
    }

    bool Initialize()
    {
        if (s_initialized) return true;
        pRakClient = RakNetworkFactory::GetRakClientInterface();
        if (!pRakClient) return false;

        pRakClient->SetMTUSize(576);
        resetPools(1, 0);
        RegisterRPCs(pRakClient);

        s_initialized = true;
        return true;
    }

    void Shutdown()
    {
        if (!s_initialized) return;

        if (iAreWeConnected)
        {
            pRakClient->Disconnect(0);
            iAreWeConnected = 0;
        }

        UnRegisterRPCs(pRakClient);
        RakNetworkFactory::DestroyRakClientInterface(pRakClient);
        pRakClient = nullptr;

        iGameInited = false;
        iConnectionRequested = 0;
        iSpawned = 0;
        g_myPlayerID = -1;
        g_localVehicleId = INVALID_VEHICLE_ID;
        g_localSeatId = 0;
        s_connectionRequested = false;
        s_initialized = false;
    }

    bool Connect(const char* host, std::uint16_t port,
                 const char* nickname, const char* password)
    {
        if (!s_initialized && !Initialize()) return false;
        if (!host || !nickname) return false;

        strncpy(s_serverHost, host, sizeof(s_serverHost) - 1);
        s_serverHost[sizeof(s_serverHost) - 1] = 0;
        s_serverPort = port;

        s_serverPassword[0] = 0;
        if (password)
        {
            strncpy(s_serverPassword, password, sizeof(s_serverPassword) - 1);
            s_serverPassword[sizeof(s_serverPassword) - 1] = 0;
        }

        strncpy(g_szNickName, nickname, MAX_PLAYER_NAME - 1);
        g_szNickName[MAX_PLAYER_NAME - 1] = 0;

        // Actual connect is issued on next Tick — keeps this call non-blocking
        // and ensures the pump is already running when the response arrives.
        s_connectionRequested = true;
        iConnectionRequested  = 0;
        iAreWeConnected       = 0;
        iGameInited           = false;
        iSpawned              = 0;
        return true;
    }

    void Disconnect()
    {
        if (!s_initialized) return;
        if (iAreWeConnected || iConnectionRequested)
        {
            pRakClient->Disconnect(0);
        }
        iAreWeConnected       = 0;
        iConnectionRequested  = 0;
        iGameInited           = false;
        iSpawned              = 0;
        s_connectionRequested = false;
        s_spawnStage          = SpawnStage::Idle;
        g_spawnInfoReceived   = false;
        g_myPlayerID          = -1;
        opensamp::gui::Dialog::Get().Hide();
    }

    void Tick()
    {
        if (!s_initialized) return;

        UpdateNetwork(pRakClient);

        // Issue RakNet connect once the Connect() caller has queued a request.
        if (s_connectionRequested && !iConnectionRequested)
        {
            sampConnect(s_serverHost, s_serverPort, g_szNickName, s_serverPassword, pRakClient);
            iConnectionRequested = 1;
        }

        // Auto-pick class/spawn after InitGame — temporary until class-select UI.
        // SAMP 0.3.7 requires RequestClass → wait for response → Spawn, so we
        // run a tiny FSM here to avoid firing Spawn with a zeroed SpawnInfo.
        //
        // The server may push a dialog from OnPlayerConnect *before* the
        // player picks a class (login/auth flow). Pause the FSM while any
        // dialog is up so our RequestClass doesn't race the dialog handshake.
        const bool dialog_blocking = opensamp::gui::Dialog::Get().IsActive();
        if (iAreWeConnected && iGameInited && !iSpawned && !dialog_blocking)
        {
            const std::uint32_t now = GetTickCount();
            switch (s_spawnStage)
            {
            case SpawnStage::Idle:
                g_spawnInfoReceived = false;
                sampRequestClass(iClassID);
                s_spawnStage   = SpawnStage::WaitingClass;
                s_spawnStageMs = now;
                break;

            case SpawnStage::WaitingClass:
                if (g_spawnInfoReceived)
                {
                    s_spawnStage = SpawnStage::Ready;
                }
                else if (now - s_spawnStageMs > 3000)
                {
                    // No response in 3s — resend. Servers sometimes ignore
                    // the first RequestClass during their own init.
                    s_spawnStage = SpawnStage::Idle;
                }
                break;

            case SpawnStage::Ready:
                sampSpawn();
                iSpawned     = 1;
                s_spawnStage = SpawnStage::Done;
                break;

            case SpawnStage::Done:
                break;
            }
        }

        // Server is authoritative for time — vanilla CClock keeps ticking
        // every frame, so once we've heard a server time we re-pin it each
        // tick. Otherwise the HUD clock drifts forward independent of what
        // the server thinks the world time is.
        if (iAreWeConnected && iGameInited && g_world.serverTimeKnown)
        {
            gta::sa::SetWorldTime(g_world.worldTimeHour, g_world.worldTimeMinute);
        }

        // Once spawned: refresh local-player globals from the live ped and
        // pump on-foot sync to the server. Without this the server sees us
        // as silent and starts an AFK timer (~30 min on most servers).
        // @todo distinguish in-vehicle (driver/passenger) and emit the right
        //       packet — for now we only cover on-foot to clear AFK and get
        //       basic position sync going.
        static std::uint32_t s_lastSyncTick = 0;
        if (iAreWeConnected && iGameInited && iSpawned && !bIsSpectating)
        {
            if (auto* ped = gta::sa::GetLocalPlayerPed())
            {
                fNormalModePos[0] = ped->m_position.x;
                fNormalModePos[1] = ped->m_position.y;
                fNormalModePos[2] = ped->m_position.z;
                fNormalModeRot    = ped->CurrentRotation();
                fPlayerHealth     = ped->Health();
                fPlayerArmour     = ped->Armour();
            }

            const std::uint32_t now = GetTickCount();
            if (now - s_lastSyncTick >= 50) // SAMP runs body sync at ~20 Hz
            {
                // Driver of a vehicle whose network id the server told us about
                // -> in-car sync; otherwise fall back to on-foot sync (also
                // covers walking into a car ourselves, until the VehicleID
                // registry lands — see inCarUpdate()/@todo).
                if (auto* ped = gta::sa::GetLocalPlayerPed();
                    ped && ped->IsDriver() && g_localVehicleId < MAX_VEHICLES)
                {
                    inCarUpdate();
                }
                else
                {
                    onFootUpdateAtNormalPos();
                }
                s_lastSyncTick = now;
            }
        }
    }

    bool IsInitialized() { return s_initialized; }
    bool IsConnected()   { return iAreWeConnected != 0; }
    bool IsGameInited()  { return iGameInited; }
    int  MyPlayerId()    { return g_myPlayerID; }

    void SendChat(const char* utf8_message)
    {
        if (!utf8_message || !iAreWeConnected) return;
        ::sendChat(const_cast<char*>(utf8_message));
    }

    void SendCommand(const char* utf8_command)
    {
        if (!utf8_command || !iAreWeConnected) return;
        // Server expects CP-1251 — same convention as chat (see sendChat).
        const auto payload = opensamp::util::utf8_to_cp1251(utf8_command);
        ::sendServerCommand(const_cast<char*>(payload.c_str()));
    }

    void SendDialogResponse(std::uint16_t dialogId, std::uint8_t button,
                            std::uint16_t listItem, const char* input)
    {
        if (!iAreWeConnected) return;
        ::sendDialogResponse(dialogId, button, listItem, input ? input : "");
    }
} // namespace opensamp::bridge

// Original TestRakNet body kept disabled below for reference / salvage.
static void TestRakNet_Legacy_Unused()
{
    try
    {
        pRakClient = RakNetworkFactory::GetRakClientInterface();
        if (pRakClient == nullptr)
            return;

        pRakClient->SetMTUSize(576);

        resetPools(1, 0);
        RegisterRPCs(pRakClient);

        auto server = stServer();
        strcpy(server.szAddr, "127.0.0.1");
        server.iPort = 7777;
        strcpy(server.szNickname, "OpenSamp");
        strcpy(server.szPassword, "");

        char szInfo[400];
        char szLastInfo[400]{};   // read by strcmp below before it is ever written

        int iLastMoney = iMoney;
        int iLastDrunkLevel = iDrunkLevel;

        int iLastStatsUpdate = GetTickCount();
        while (true)
        {
            Sleep(100);
            UpdateNetwork(pRakClient);

            if (!iConnectionRequested)
            {
                if (!iGettingNewName)
                    sampConnect(server.szAddr, server.iPort, server.szNickname,
                                server.szPassword, pRakClient);
                else
                    sampConnect(server.szAddr, server.iPort, g_szNickName, server.szPassword,
                                pRakClient);

                iConnectionRequested = 1;
            }

            if (iAreWeConnected && iGameInited)
            {
                static DWORD dwLastInfoUpdate;

                if (!dwLastInfoUpdate)
                    dwLastInfoUpdate = GetTickCount();

                if (dwLastInfoUpdate < (GetTickCount() - 1000))
                {
                    char szHealthText[16], szArmourText[16];

                    if (fPlayerHealth > 200.0f)
                        sprintf_s(szHealthText, sizeof(szHealthText), "N/A");
                    else
                        sprintf_s(szHealthText, sizeof(szHealthText), "%.2f", fPlayerHealth);

                    if (fPlayerArmour > 200.0f)
                        sprintf_s(szArmourText, sizeof(szArmourText), "N/A");
                    else
                        sprintf_s(szArmourText, sizeof(szArmourText), "%.2f", fPlayerArmour);

                    sprintf_s(szInfo, 400,
                              "Hostname: %s     Players: %d     Ping: %d     Authors: %s\nHealth: %s     Armour: %s     Skin: %d     X: %.4f     Y: %.4f     Z: %.4f     Rotation: %.4f",
                              g_szHostName, getPlayerCount(), playerInfo[g_myPlayerID].dwPing, ".", szHealthText,
                              szArmourText, iLocalPlayerSkin, fNormalModePos[0], fNormalModePos[1],
                              fNormalModePos[2], fNormalModeRot);

                    if (strcmp(szInfo, szLastInfo) != 0)
                    {
                        sprintf_s(szLastInfo, szInfo);
                    }
                }

                if (true)
                {
                    if ((GetTickCount() - iLastStatsUpdate >= 1000) || iMoney != iLastMoney || iDrunkLevel !=
                        iLastDrunkLevel)
                    {
                        RakNet::BitStream bsSend;

                        bsSend.Write(static_cast<BYTE>(ID_STATS_UPDATE));

                        iDrunkLevel -= (rand() % 60 + 1);

                        if (iDrunkLevel < 0)
                            iDrunkLevel = 0;

                        bsSend.Write(iMoney);
                        bsSend.Write(iDrunkLevel);

                        pRakClient->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);

                        iLastMoney = iMoney;
                        iLastDrunkLevel = iDrunkLevel;

                        iLastStatsUpdate = GetTickCount();
                    }
                }

                if (!iSpawned)
                {
                    if (false)
                    {
                        if (!iNotificationDisplayedBeforeSpawn)
                        {
                            sampRequestClass(iClassID);

                            Log("Please write !spawn into the console when you're ready to spawn.");

                            iNotificationDisplayedBeforeSpawn = 1;
                        }
                    }
                    sampRequestClass(iClassID);
                    sampSpawn();

                    iSpawned = 1;
                    iNotificationDisplayedBeforeSpawn = 1;
                }
                else
                {
                    if (true)
                    {
                        if (!bIsSpectating)
                        {
                            fNormalModePos[0] = CurrentCheckpoint.fPosition[0];
                            fNormalModePos[1] = CurrentCheckpoint.fPosition[1];
                            fNormalModePos[2] = CurrentCheckpoint.fPosition[2];
                            onFootUpdateAtNormalPos();
                        }
                        else
                            spectatorUpdate();
                    }
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        Log("Exception: %s", ex.what());
    }
    catch (...)
    {
        Log("Unknown exception occurred.");
    }
}
