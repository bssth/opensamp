#pragma once
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

static constexpr DWORD wait_object_0 = 0x00000000;
static constexpr DWORD wait_timeout = 0x00000102;

static std::wstring get_exe_dir()
{
    wchar_t buf[MAX_PATH]{};
    if (const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH); n == 0 || n >= MAX_PATH) throw std::runtime_error(
        "GetModuleFileNameW failed");
    const fs::path p(buf);
    return p.parent_path().wstring();
}

static void try_kill_process(const HANDLE h_process)
{
    if (!h_process) return;
    DWORD code = 0;
    if (GetExitCodeProcess(h_process, &code) && code == STILL_ACTIVE)
    {
        TerminateProcess(h_process, 1);
        WaitForSingleObject(h_process, 3000);
    }
}

static void wait_for_process_ready(const DWORD pid, const DWORD timeout_ms)
{
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < timeout_ms)
    {
        const HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) throw std::runtime_error("Process exited prematurely (OpenProcess failed)");
        const DWORD wait = WaitForSingleObject(h, 0);
        CloseHandle(h);
        if (wait == wait_object_0) throw std::runtime_error("Process exited prematurely");

        if (const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid); snap !=
            INVALID_HANDLE_VALUE)
        {
            MODULEENTRY32W me{};
            me.dwSize = sizeof(me);
            const BOOL ok = Module32FirstW(snap, &me);
            CloseHandle(snap);

            if (ok) return;
        }

        Sleep(100);
    }
}

static int try_get_remote_last_error(const HANDLE h_process)
{
    const HMODULE h_kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!h_kernel32) return 0;

    const FARPROC p_get_last_error = GetProcAddress(h_kernel32, "GetLastError");
    if (!p_get_last_error) return 0;

    DWORD tid = 0;
    const HANDLE h_thread = CreateRemoteThread(h_process, nullptr, 0,
                                               reinterpret_cast<LPTHREAD_START_ROUTINE>(p_get_last_error), nullptr, 0,
                                               &tid);
    if (!h_thread) return 0;

    int result = 0;
    if (const DWORD wait = WaitForSingleObject(h_thread, 5000); wait == wait_object_0)
    {
        DWORD exit_code = 0;
        if (GetExitCodeThread(h_thread, &exit_code))
            result = static_cast<int>(exit_code);
    }
    CloseHandle(h_thread);
    return result;
}

static void inject_dll_load_library_w(const DWORD pid, const std::wstring& dll_full_path)
{
    const std::wstring full = fs::absolute(fs::path(dll_full_path)).wstring();
    if (full.empty()) throw std::runtime_error("DLL path is empty");

    const HANDLE h_process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (!h_process)
        throw std::runtime_error("OpenProcess failed");

    const size_t bytes_len = (full.size() + 1) * sizeof(wchar_t);

    const LPVOID remote_mem = VirtualAllocEx(h_process, nullptr, bytes_len,
                                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!remote_mem)
    {
        CloseHandle(h_process);
        throw std::runtime_error("VirtualAllocEx failed");
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(h_process, remote_mem, full.c_str(), bytes_len, &written) || written != bytes_len)
    {
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("WriteProcessMemory failed");
    }

    const HMODULE h_kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!h_kernel32)
    {
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("GetModuleHandle(kernel32) failed");
    }

    const FARPROC p_load_library_w = GetProcAddress(h_kernel32, "LoadLibraryW");
    if (!p_load_library_w)
    {
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("GetProcAddress(LoadLibraryW) failed");
    }

    DWORD tid = 0;
    const HANDLE h_thread = CreateRemoteThread(h_process, nullptr, 0,
                                               reinterpret_cast<LPTHREAD_START_ROUTINE>(p_load_library_w), remote_mem,
                                               0,
                                               &tid);

    if (!h_thread)
    {
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("CreateRemoteThread failed");
    }

    const DWORD wait = WaitForSingleObject(h_thread, 10000);
    if (wait == wait_timeout)
    {
        CloseHandle(h_thread);
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("Remote LoadLibraryW timed out");
    }
    if (wait != wait_object_0)
    {
        CloseHandle(h_thread);
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("WaitForSingleObject returned unexpected code");
    }

    DWORD exit_code = 0;
    if (!GetExitCodeThread(h_thread, &exit_code))
    {
        CloseHandle(h_thread);
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        throw std::runtime_error("GetExitCodeThread failed");
    }

    CloseHandle(h_thread);
    VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);

    if (exit_code == 0)
    {
        const int remote_err = try_get_remote_last_error(h_process);
        try_kill_process(h_process);

        const auto hint =
            L"DLL failed to load (LoadLibraryW returned NULL).\n\n"
            L"Common causes:\n"
            L" \u2022 Client.Native.dll is not x86\n"
            L" \u2022 Missing dependencies (VC++ runtime / other DLLs)\n"
            L" \u2022 Antivirus/permissions\n";

        std::wstring msg(hint);
        if (remote_err != 0)
        {
            msg += L"\nRemote GetLastError=" + std::to_wstring(remote_err);
        }

        CloseHandle(h_process);
        throw std::runtime_error(std::string("DLL failed to load"));
    }

    CloseHandle(h_process);
}

inline int run_game()
{
    HANDLE h_process = nullptr;
    HANDLE h_thread = nullptr;

    try
    {
        const std::wstring game_dir = get_exe_dir();
        const fs::path exe_path = fs::path(game_dir) / L"gta_sa.exe";

        const std::vector dlls = {
            fs::path(game_dir) / L"Client.Native.dll"
        };

        if (!fs::exists(exe_path))
            throw std::runtime_error("gta_sa.exe not found");

        for (const auto& dll : dlls)
            if (!fs::exists(dll))
                throw std::runtime_error("OpenSamp DLLs not found");

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::wstring cmd = L"\"" + exe_path.wstring() + L"\"";

        if (!CreateProcessW(
            exe_path.c_str(),
            cmd.data(),
            nullptr, nullptr,
            FALSE,
            0,
            nullptr,
            game_dir.c_str(),
            &si, &pi))
        {
            throw std::runtime_error("Failed to start gta_sa.exe");
        }

        h_process = pi.hProcess;
        h_thread = pi.hThread;
        const DWORD pid = pi.dwProcessId;

        CloseHandle(h_thread);
        h_thread = nullptr;

        wait_for_process_ready(pid, 15000);

        for (const auto& dllPath : dlls)
            inject_dll_load_library_w(pid, dllPath.wstring());

        CloseHandle(h_process);
        return 0;
    }
    catch (const std::exception& /*ex*/)
    {
        try_kill_process(h_process);
        if (h_thread) CloseHandle(h_thread);
        if (h_process) CloseHandle(h_process);

        MessageBoxW(
            nullptr,
            L"Launcher error.\n\n(See logs / debug build for details.)",
            L"Launcher error",
            MB_OK | MB_ICONERROR);

        return 1;
    }
}
