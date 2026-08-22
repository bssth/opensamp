#pragma once

// Minimal SCM (CRunningScript) opcode dispatcher.
//
// GTA's `CRunningScript::ProcessOneCommand` (0x469EB0) executes a single
// opcode read from a `GAME_SCRIPT_THREAD`'s instruction pointer. SCM opcodes
// often touch internal subsystems (interior selection, streaming refresh,
// model requests, etc.) that have no clean C ABI exposed — calling the
// underlying functions directly skips bookkeeping the dispatcher does.
//
// We allocate a single reusable `ThreadFrame` and an opcode buffer, encode
// `[opcode | params...]` into the buffer, point `script_ip` at it, then
// invoke `ProcessOneCommand` (it's __thiscall, takes the thread in ECX).
//
// Layout of `ThreadFrame` (0xE0 bytes) is dictated by the binary; field
// offsets verified against US 1.0.

#include <cstdint>
#include <cstring>

#include "../gta/common.h"

namespace gta::sa::scm
{
    // GTA SA US 1.0: CRunningScript::ProcessOneCommand.
    inline constexpr std::uintptr_t FUNC_ProcessOneCommand = 0x469EB0;

    // Pack(1) is required: `if_flag` lives at 0xC5 — a non-DWORD-aligned
    // offset — and the binary reads/writes it there by absolute layout, not
    // via a C++ accessor. Default 4-byte alignment would silently shift it.
#pragma pack(push, 1)
    struct ThreadFrame
    {
        std::uint8_t  pad0[0x14];
        std::uint32_t script_ip;          // +0x14 — instruction pointer
        std::uint8_t  pad1[0x24];
        std::uint32_t local_vars[18];     // +0x3C — local var slots
        std::uint8_t  pad2[0x41];
        std::uint32_t if_flag;            // +0xC5 — opcode return / IF flag
        std::uint8_t  pad3[0x0F];
        std::uint32_t index_something;    // +0xD8
        std::uint8_t  has_local_copy;     // +0xDC
        std::uint8_t  pad4[0x03];
    };
#pragma pack(pop)
    static_assert(offsetof(ThreadFrame, script_ip)  == 0x14, "script_ip offset");
    static_assert(offsetof(ThreadFrame, local_vars) == 0x3C, "local_vars offset");
    static_assert(offsetof(ThreadFrame, if_flag)    == 0xC5, "if_flag offset");
    static_assert(sizeof(ThreadFrame) == 0xE0,               "ThreadFrame size");

    // Singletons. Not thread-safe — opcode dispatch happens from the main
    // thread / RPC handlers, sequentially.
    inline ThreadFrame  g_thread{};
    inline std::uint8_t g_buf[256]{};
    inline std::size_t  g_buf_pos = 0;

    // Param tag bytes the dispatcher recognises.
    enum ParamTag : std::uint8_t
    {
        TAG_END    = 0x00,
        TAG_INT32  = 0x01,
        TAG_VAR    = 0x03,
        TAG_INT8   = 0x04,
        TAG_INT16  = 0x05,
        TAG_FLOAT  = 0x06,
        TAG_STRING = 0x0E,
    };

    inline void reset(std::uint16_t opcode)
    {
        std::memset(g_thread.local_vars, 0, sizeof(g_thread.local_vars));
        g_thread.if_flag = 0;
        g_buf[0] = static_cast<std::uint8_t>(opcode        & 0xFF);
        g_buf[1] = static_cast<std::uint8_t>((opcode >> 8) & 0xFF);
        g_buf_pos = 2;
    }

    inline void push_int(std::int32_t v)
    {
        g_buf[g_buf_pos++] = TAG_INT32;
        std::memcpy(&g_buf[g_buf_pos], &v, sizeof(v));
        g_buf_pos += sizeof(v);
    }

    inline void push_float(float v)
    {
        g_buf[g_buf_pos++] = TAG_FLOAT;
        std::memcpy(&g_buf[g_buf_pos], &v, sizeof(v));
        g_buf_pos += sizeof(v);
    }

    // Run the encoded opcode through GTA's dispatcher. Returns if_flag.
    inline std::uint32_t exec()
    {
        g_thread.script_ip = reinterpret_cast<std::uint32_t>(&g_buf[0]);

        using fn_t = void(__thiscall*)(ThreadFrame*);
        // SEH-wrap the dispatch — SAMP's reference impl does the same; some
        // opcodes touch subsystems that AV in our partially-bootstrapped
        // state, and we'd rather log+continue than tear down the RPC handler.
        __try
        {
            reinterpret_cast<fn_t>(FUNC_ProcessOneCommand)(&g_thread);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }

        return g_thread.if_flag;
    }

    // ---------------- common opcode wrappers ----------------

    // 0x04BB select_interior — switches the global "current area" used by the
    // streamer / renderer to decide which interior cells & buildings are
    // visible. This is the entry that triggers all of GTA's interior
    // bookkeeping (CCullZones, building visibility, ambient cull, etc.).
    inline void select_interior(std::int32_t id)
    {
        reset(0x04BB);
        push_int(id);
        exec();
    }

    // 0x0860 link_actor_to_interior — tags the actor's m_areaCode + flags so
    // the streamer treats the actor as "inside" that interior cell.
    inline void link_actor_to_interior(std::int32_t actor_id, std::int32_t id)
    {
        reset(0x0860);
        push_int(actor_id);
        push_int(id);
        exec();
    }

    // 0x04E4 refresh_streaming_at — synchronously requests/loads collision +
    // IPLs around (X, Y) for the *current* interior. SAMP calls this right
    // after a SetInterior to drag in the floor under the player so they
    // don't fall through.
    inline void refresh_streaming_at(float x, float y)
    {
        reset(0x04E4);
        push_float(x);
        push_float(y);
        exec();
    }
} // namespace gta::sa::scm
