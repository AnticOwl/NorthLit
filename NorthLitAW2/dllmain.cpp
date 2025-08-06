#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

import NorthLit;

static HMODULE s_dllHandle;
static bool s_processDetachSignaled = false;

static void MainThread()
{
    if (NorthLit::Initialize())
    {
        NorthLit::Run();
    }

    if (!s_processDetachSignaled)
    {
        FreeLibraryAndExitThread(s_dllHandle, 0);
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        s_dllHandle = hModule;
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0);
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        NorthLit::Shutdown();
        break;
    }

    return TRUE;
}

