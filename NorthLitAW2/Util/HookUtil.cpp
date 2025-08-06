#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "MinHook.h"
#pragma comment(lib, "libMinHook.x64.lib")

import HookUtil;
import Log;

bool WriteMemory(void* dwAddress, const void* cpvPatch, unsigned int dwSize)
{
	DWORD dwProtect;
	if (VirtualProtect((void*)dwAddress, dwSize, PAGE_READWRITE, &dwProtect)) //Unprotect the memory
		memcpy((void*)dwAddress, cpvPatch, dwSize); //Write our patch
	else

		return false; //Failed to unprotect, so return false..
	return VirtualProtect((void*)dwAddress, dwSize, dwProtect, new DWORD); //Reprotect the memory
}

void OverrideVTableFunction(void* ppVTable, unsigned int index, void* pHook, void* pOriginal)
{
	DWORD dwOld = 0;
	VirtualProtect((void*)((*(PDWORD64*)ppVTable) + index), sizeof(PDWORD64), PAGE_EXECUTE_READWRITE, &dwOld);

	PBYTE pOrig = ((PBYTE)(*(PDWORD64*)ppVTable)[index]);
	(*(PDWORD64*)ppVTable)[index] = (DWORD64)pHook;

	VirtualProtect((void*)((*(PDWORD64*)ppVTable) + index), sizeof(PDWORD64), dwOld, &dwOld);

	if (pOriginal)
	{
		*(PBYTE*)pOriginal = pOrig;
	}
}

void InitializeMinHook()
{
	MH_Initialize();
}

void UninitializeMinHook()
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}

void CreateHook(void* pFunction, void* pHook, void* ppOriginal)
{
	MH_STATUS result = MH_CreateHook(pFunction, pHook, (LPVOID*)ppOriginal);
	if (result != MH_OK)
	{
		Log::Error("Error creating hook at 0x%I64X - %s", pFunction, MH_StatusToString(result));
		return;
	}

	result = MH_EnableHook(pFunction);
	if (result != MH_OK)
	{
		Log::Error("Error enabling hook at 0x%I64X - %s", pFunction, MH_StatusToString(result));
	}
}

void RemoveHook(void* pFunction)
{
	MH_DisableHook(pFunction);
	MH_RemoveHook(pFunction);
}