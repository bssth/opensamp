#pragma once

// Thin public API over sampraknet_bridge.cpp. The .cpp file owns the global
// RakClient, the RPC handlers, and the SA-MP 0.3.7 handshake. Callers drive
// it via this namespace — CNetGame is the only expected caller.

#include <cstdint>

namespace opensamp::bridge
{
    // One-time setup. Creates the RakClient and registers all RPC handlers.
    // Idempotent. Returns false if RakNetworkFactory failed.
    bool Initialize();

    // Tears down the RakClient. Safe to call without Initialize.
    void Shutdown();

    // Kicks off a connection. Non-blocking. Returns false immediately on
    // obvious errors (not initialized, bad args). Connection progress is
    // observed via IsConnected() / IsGameInited() after repeated Tick() calls.
    bool Connect(const char* host, std::uint16_t port,
                 const char* nickname, const char* password = "");

    // Ends the current session. Safe at any time.
    void Disconnect();

    // Pumps RakNet: receive + RPC dispatch + post-connect maintenance.
    // Must be called every game frame from the main thread.
    void Tick();

    // --- state observation (CNetGame polls these to drive its FSM) ---
    bool IsInitialized();
    bool IsConnected();     // RakNet layer up + ClientJoin sent
    bool IsGameInited();    // InitGame RPC received: we're fully in the game
    int  MyPlayerId();      // -1 if not assigned yet

    // --- actions ---
    void SendChat(const char* utf8_message); // send a chat line to the server

    // Send a `/foo bar baz` server command (RPC_ServerCommand, id 50).
    // The leading slash MUST be present in the string. UTF-8 in.
    void SendCommand(const char* utf8_command);

    // Reply to a server-pushed dialog (RPC_DialogResponse, id 62).
    //  button   : 1 = left/OK, 0 = right/cancel
    //  listItem : for DIALOG_STYLE_LIST, selected index; 0xFFFF otherwise
    //  input    : NUL-terminated reply text, may be empty
    void SendDialogResponse(std::uint16_t dialogId, std::uint8_t button,
                            std::uint16_t listItem, const char* input);
} // namespace opensamp::bridge

// Legacy entry kept for now — do not call. Will be removed once the old
// monolithic loop in sampraknet_bridge.cpp is fully retired.
void TestRakNet();
