#pragma once

#include <windows.h>
#include <d3d9.h>
#include <DirectXMath.h>
#include <type_traits>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstring>

#include "../addresses.h"

using namespace DirectX;

template <class T, class U>
T* to_ptr(U p)
{
    if constexpr (std::is_pointer_v<U>)
    {
        return reinterpret_cast<T*>(p);
    }
    else
    {
        static_assert(std::is_integral_v<U>, "addr must be an integer address or a pointer");
        return reinterpret_cast<T*>(static_cast<std::uintptr_t>(p));
    }
}

namespace gta
{
    inline IDirect3DDevice9* get_d3d_device()
    {
        return *reinterpret_cast<IDirect3DDevice9**>(opensamp::addr::kD3DDevicePtr);
    }

    inline void* GetPlayerPed()
    {
        return *reinterpret_cast<void**>(opensamp::addr::kLocalPlayerPedPtr);
    }

    inline bool is_readable(const void* ptr, const std::size_t size)
    {
        if (!ptr || size == 0) return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT)
            return false;

        if (mbi.Protect & PAGE_NOACCESS) return false;
        if (mbi.Protect & PAGE_GUARD) return false;

        const DWORD p = mbi.Protect & 0xFF;
        const bool readable =
            (p == PAGE_READONLY) ||
            (p == PAGE_READWRITE) ||
            (p == PAGE_WRITECOPY) ||
            (p == PAGE_EXECUTE_READ) ||
            (p == PAGE_EXECUTE_READWRITE) ||
            (p == PAGE_EXECUTE_WRITECOPY);

        if (!readable)
            return false;

        const auto region_start = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto region_end = region_start + mbi.RegionSize;
        const auto req_start = reinterpret_cast<std::uintptr_t>(ptr);
        const auto req_end = req_start + size;

        return req_end <= region_end;
    }

    inline int read_memory(const std::uintptr_t address, const std::size_t size, void* out)
    {
        if (!out || size == 0) return 0;

        const auto src = reinterpret_cast<const void*>(address); // NOLINT(performance-no-int-to-ptr)

        if (!is_readable(src, size))
            return 0;

        __try // NOLINT(clang-diagnostic-language-extension-token)
        {
            std::memcpy(out, src, size);
            return static_cast<int>(size);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }

    inline int write_memory(const std::uintptr_t address, const std::size_t size, const void* src)
    {
        if (!src || size == 0) return 0;

        const auto dst = reinterpret_cast<void*>(address); // NOLINT(performance-no-int-to-ptr)

        if (!is_readable(dst, size))
            return 0;

        DWORD old_prot = 0;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &old_prot))
            return 0;

        __try
        {
            std::memcpy(dst, src, size);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            DWORD tmp;
            VirtualProtect(dst, size, old_prot, &tmp);
            return 0;
        }

        DWORD tmp;
        VirtualProtect(dst, size, old_prot, &tmp);
        return static_cast<int>(size);
    }

    inline bool is_menu_open()
    {
        BYTE b_open;
        read_memory(opensamp::addr::kMenuOpenFlag, 1, &b_open);
        return b_open != 0;
    }

    struct vec2
    {
        float x, y;
    };

    struct vec3
    {
        float x, y, z;
    };

    inline bool world_to_screen(const vec3& world, vec3& out)
    {
        const auto dev = get_d3d_device();
        if (!dev) return false;

        D3DVIEWPORT9 vp{};
        if (FAILED(dev->GetViewport(&vp)))
            return false;

        D3DMATRIX view_m{}, proj_m{};
        if (FAILED(dev->GetTransform(D3DTS_VIEW, &view_m)))
            return false;

        if (FAILED(dev->GetTransform(D3DTS_PROJECTION, &proj_m)))
            return false;

        const XMMATRIX view = XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&view_m));
        const XMMATRIX proj = XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&proj_m));
        const XMMATRIX view_proj = XMMatrixMultiply(view, proj);
        const XMVECTOR pos = XMVectorSet(world.x, world.y, world.z, 1.0f);
        const XMVECTOR clip = XMVector3TransformCoord(pos, view_proj);

        const float x = XMVectorGetX(clip);
        const float y = XMVectorGetY(clip);
        const float z = XMVectorGetZ(clip);

        if (z < 0.0f)
            return false;

        out.x = (x + 1.0f) * 0.5f * static_cast<float>(vp.Width) + static_cast<float>(vp.X);
        out.y = (1.0f - y) * 0.5f * static_cast<float>(vp.Height) + static_cast<float>(vp.Y);
        out.z = z;

        return true;
    }

    inline bool screen_to_world(const vec2& screen, const float depth, vec3& out)
    {
        const auto dev = get_d3d_device();
        if (!dev) return false;

        D3DVIEWPORT9 vp{};
        if (FAILED(dev->GetViewport(&vp)))
            return false;

        D3DMATRIX view_m{}, proj_m{};
        if (FAILED(dev->GetTransform(D3DTS_VIEW, &view_m)))
            return false;

        if (FAILED(dev->GetTransform(D3DTS_PROJECTION, &proj_m)))
            return false;

        const XMMATRIX view = XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&view_m));
        const XMMATRIX proj = XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&proj_m));
        const XMMATRIX view_proj = XMMatrixMultiply(view, proj);
        const XMMATRIX inv_view_proj = XMMatrixInverse(nullptr, view_proj);

        const float nx = (screen.x - static_cast<float>(vp.X)) / static_cast<float>(vp.Width) * 2.0f - 1.0f;
        const float ny = 1.0f - (screen.y - static_cast<float>(vp.Y)) / static_cast<float>(vp.Height) * 2.0f;

        const XMVECTOR pos = XMVectorSet(nx, ny, depth, 1.0f);
        const XMVECTOR world = XMVector3TransformCoord(pos, inv_view_proj);

        out.x = XMVectorGetX(world);
        out.y = XMVectorGetY(world);
        out.z = XMVectorGetZ(world);

        return true;
    }
}
