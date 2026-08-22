#pragma once

#include <cstdlib>
#include <cstring>
#include <Windows.h>

#include "gta/common.h"

template <class U>
bool mem_set(U addr, const int value, const std::size_t amount)
{
    if (amount == 0)
        return true;

    const auto a = reinterpret_cast<std::uintptr_t>(to_ptr<void>(addr));

    if (amount <= 4096)
    {
        alignas(16) unsigned char buffer[4096];
        std::memset(buffer, value, amount);
        return gta::write_memory(a, amount, buffer) == static_cast<int>(amount);
    }

    auto* buffer = static_cast<unsigned char*>(std::malloc(amount));
    if (!buffer)
        return false;

    std::memset(buffer, value, amount);
    const bool ok = gta::write_memory(a, amount, buffer) == static_cast<int>(amount);
    std::free(buffer);
    return ok;
}

template <class T, class U>
bool mem_get(U addr, T& out)
{
    const auto a = reinterpret_cast<std::uintptr_t>(to_ptr<void>(addr));
    return gta::read_memory(a, sizeof(T), &out) == static_cast<int>(sizeof(T));
}

template <class T, class U>
bool mem_put(U addr, const T& value)
{
    const auto a = reinterpret_cast<std::uintptr_t>(to_ptr<void>(addr));

    T current{};
    if (!mem_get<T>(addr, current))
        return false;

    if (current == value)
        return true;

    return gta::write_memory(a, sizeof(T), &value) == static_cast<int>(sizeof(T));
}

void ApplyBaseMemoryPatches();
void D3D_Start();
void TickGameReady();
bool GameReady_Install();
