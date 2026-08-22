#pragma once

// CNetGame — main networked-game state machine. One instance per process.
// Ticked from the per-frame game-side hook (CGame::Process). Does not own
// the D3D/UI side — UI reads state through accessors.

#include <cstdint>
#include <string>

namespace opensamp::net
{
    enum class NetState : int
    {
        None = 0,          // idle, not connecting
        Connecting,        // RakNet socket opening / handshake
        AwaitJoin,         // ClientJoin sent, waiting for ServerJoin + InitGame
        Connected,         // fully joined, syncing
        Disconnected,      // terminal: last session ended
        Restarting,        // connection dropped, reconnect scheduled
    };

    const char* ToString(NetState s);

    struct ServerAddress
    {
        std::string   host;
        std::uint16_t port = 7777;
        std::string   password;
    };

    struct LocalInfo
    {
        std::string nickname; // ≤ 24 chars (SA-MP limit)
    };

    class CNetGame
    {
    public:
        static CNetGame& Get();

        // Lifecycle. Initialize is idempotent and safe to call before the
        // game is done loading. Shutdown disconnects if needed.
        void Initialize();
        void Shutdown();

        // Called every game-frame from the CGame::Process hook, after the
        // original function runs. Must be cheap; heavy work goes to workers.
        void Tick();

        // User-facing actions. Wire from /connect, /disconnect, UI buttons.
        void Connect(ServerAddress addr, LocalInfo info);
        void Disconnect(const char* reason = nullptr);

        NetState             State()  const { return m_state; }
        const ServerAddress& Server() const { return m_server; }
        const LocalInfo&     Local()  const { return m_local; }

    private:
        CNetGame() = default;
        ~CNetGame() = default;
        CNetGame(const CNetGame&) = delete;
        CNetGame& operator=(const CNetGame&) = delete;

        void TransitionTo(NetState s);
        void OnEnter(NetState s);
        void OnExit(NetState s);

        // Per-state tick handlers. Each is called at most once per frame.
        void TickConnecting();
        void TickAwaitJoin();
        void TickConnected();
        void TickRestarting();

        std::uint32_t MsInState() const;

        NetState       m_state         = NetState::None;
        ServerAddress  m_server{};
        LocalInfo      m_local{};
        bool           m_initialized   = false;
        std::uint32_t  m_stateEnterMs  = 0;
    };
} // namespace opensamp::net
