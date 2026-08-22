#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

#include "memory.h"
#include "vendor/minhook.h"

#pragma comment(lib, "MinHook.x86.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* ep)
{
    CreateDirectoryA("crash", nullptr);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char path[MAX_PATH];
    sprintf_s(path, "crash\\crash_%04d%02d%02d_%02d%02d%02d.dmp",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        // MiniDumpWithFullMemory captures every committed page — for a 32-bit
        // GTA process that's bounded and gives parse_stack.py a real call chain.
        // Indirectly-referenced + thread info kept so that helpers like CPed*
        // pointed at by registers are also followed.
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hFile,
                          static_cast<MINIDUMP_TYPE>(
                              MiniDumpWithFullMemory |
                              MiniDumpWithIndirectlyReferencedMemory |
                              MiniDumpWithThreadInfo |
                              MiniDumpWithProcessThreadData),
                          &mei, nullptr, nullptr);

        CloseHandle(hFile);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler()
{
    SetUnhandledExceptionFilter(TopLevelExceptionFilter);
}

DWORD WINAPI MainThread(LPVOID)
{
    InstallCrashHandler();
    ApplyBaseMemoryPatches();
    MH_Initialize();
    GameReady_Install();
    D3D_Start();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD reason,
                      LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH)
    {
        return true;
    }

    DisableThreadLibraryCalls(hModule);
    CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    return true;
}
