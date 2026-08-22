#include "netgame.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>

#include "../sampraknet_bridge.h"
#include "../gui/chat.hpp"

namespace opensamp::net
{
    namespace
    {
        std::uint32_t NowMs() { return GetTickCount(); }

        // Phase-2 timeouts. Temporary numbers; adjust once we have real UX.
        constexpr std::uint32_t kAwaitJoinTimeoutMs = 20'000;

        void Log(const char* fmt, ...)
        {
            char buf[512];
            va_list ap;
            va_start(ap, fmt);
            _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
            va_end(ap);
            g_chat.PushLineAsync(buf);
        }
    }

    const char* ToString(NetState s)
    {
        switch (s)
        {
        case NetState::None: return "None";
        case NetState::Connecting: return "Connecting";
        case NetState::AwaitJoin: return "AwaitJoin";
        case NetState::Connected: return "Connected";
        case NetState::Disconnected: return "Disconnected";
        case NetState::Restarting: return "Restarting";
        }
        return "?";
    }

    CNetGame& CNetGame::Get()
    {
        static CNetGame instance;
        return instance;
    }

    void CNetGame::Initialize()
    {
        if (m_initialized) return;

        if (!bridge::Initialize())
        {
            Log("[NetGame] ERROR: RakClient init failed");
            return;
        }

        m_initialized = true;
        m_state = NetState::None;
        m_stateEnterMs = NowMs();
    }

    void CNetGame::Shutdown()
    {
        if (!m_initialized) return;
        if (m_state != NetState::None && m_state != NetState::Disconnected)
            Disconnect("shutdown");
        bridge::Shutdown();
        m_initialized = false;
    }

    void CNetGame::Tick()
    {
        if (!m_initialized) return;

        // Bridge drives the RakNet pump and post-connect maintenance.
        // We only read its observable state to move the FSM.
        bridge::Tick();

        switch (m_state)
        {
        case NetState::Connecting: TickConnecting();
            break;
        case NetState::AwaitJoin: TickAwaitJoin();
            break;
        case NetState::Connected: TickConnected();
            break;
        case NetState::Restarting: TickRestarting();
            break;
        case NetState::None:
        case NetState::Disconnected:
        default:
            break;
        }
    }

    void CNetGame::Connect(ServerAddress addr, LocalInfo info)
    {
        if (!m_initialized) Initialize();
        if (!m_initialized) return;

        m_server = std::move(addr);
        m_local = std::move(info);

        // Tear down any in-progress session before starting a new one.
        if (m_state == NetState::Connecting ||
            m_state == NetState::AwaitJoin ||
            m_state == NetState::Connected ||
            m_state == NetState::Restarting)
        {
            bridge::Disconnect();
        }

        Log("[NetGame] Connecting to %s:%u as '%s'",
            m_server.host.c_str(),
            static_cast<unsigned>(m_server.port),
            m_local.nickname.c_str());

        if (!bridge::Connect(m_server.host.c_str(),
                             m_server.port,
                             m_local.nickname.c_str(),
                             m_server.password.c_str()))
        {
            Log("[NetGame] Connect call rejected by bridge");
            TransitionTo(NetState::Disconnected);
            return;
        }

        TransitionTo(NetState::Connecting);
    }

    void CNetGame::Disconnect(const char* reason)
    {
        if (m_state == NetState::None || m_state == NetState::Disconnected)
            return;

        Log("[NetGame] Disconnect (%s)", reason ? reason : "");
        bridge::Disconnect();
        TransitionTo(NetState::Disconnected);
    }

    // ---------------- state machine plumbing ----------------

    void CNetGame::TransitionTo(NetState s)
    {
        if (s == m_state) return;
        OnExit(m_state);
        Log("[NetGame] %s -> %s", ToString(m_state), ToString(s));
        m_state = s;
        m_stateEnterMs = NowMs();
        OnEnter(s);
    }

    void CNetGame::OnEnter(NetState /*s*/)
    {
    }

    void CNetGame::OnExit(NetState /*s*/)
    {
    }

    std::uint32_t CNetGame::MsInState() const
    {
        return NowMs() - m_stateEnterMs;
    }

    // ---------------- per-state ticks ----------------

    void CNetGame::TickConnecting()
    {
        // Bridge flips iAreWeConnected=1 inside Packet_ConnectionSucceeded,
        // which also sends the ClientJoin RPC. From that moment we're waiting
        // for the server's InitGame response.
        if (bridge::IsConnected())
        {
            TransitionTo(NetState::AwaitJoin);
        }
        // @todo explicit CONNECTION_FAILED / timeout handling — needs bridge
        //       to surface those events. For now, stay in Connecting.
    }

    void CNetGame::TickAwaitJoin()
    {
        if (bridge::IsGameInited())
        {
            Log("[NetGame] Joined as playerId=%d", bridge::MyPlayerId());
            TransitionTo(NetState::Connected);
            return;
        }

        if (MsInState() > kAwaitJoinTimeoutMs)
        {
            Disconnect("join timeout");
        }
    }

    void CNetGame::TickConnected()
    {
        // Bridge keeps the pump running. Nothing to do here yet — will grow
        // with outgoing sync, scoreboard updates, etc.
        if (!bridge::IsConnected())
        {
            TransitionTo(NetState::Disconnected);
        }
    }

    void CNetGame::TickRestarting()
    {
        // @todo exponential backoff, then re-issue Connect(m_server, m_local).
    }
} // namespace opensamp::net
