// NorthLitInjector.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <filesystem>
#include <iostream>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Winternl.h"
#include <tlhelp32.h>

static const wchar_t* s_wProcessName = L"AlanWake2.exe";
static const char* s_ProcessName = "AlanWake2.exe";

static const char* s_DllName = "WinPixGpuCapturer.dll";

HANDLE GetProcessHandle(const wchar_t* name)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (wcscmp(entry.szExeFile, name) == 0)
			{
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
				return hProcess;
			}
		}
	}

	return NULL;
}

std::pair<HANDLE, DWORD> GetProcessHandleAndID(const wchar_t* name)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (wcscmp(entry.szExeFile, name) == 0)
			{
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
				return { hProcess, entry.th32ProcessID };
			}
		}
	}

	return { NULL, 0 };
}

void SuspendThreads(DWORD processId, std::vector<HANDLE>& suspendedThreads)
{
	THREADENTRY32  entry;
	entry.dwSize = sizeof(THREADENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);

	if (Thread32First(snapshot, &entry) == TRUE)
	{
		do
		{
			if (entry.th32OwnerProcessID == processId)
			{
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
				SuspendThread(hThread);
				suspendedThreads.emplace_back(hThread);
			}
		} while (Thread32Next(snapshot, &entry) == TRUE);
	}

	CloseHandle(snapshot);
}

void ResumeThreads(std::vector<HANDLE>& suspendedThreads)
{
	for (HANDLE& threadHandle : suspendedThreads)
	{
		ResumeThread(threadHandle);
		CloseHandle(threadHandle);
	}

	suspendedThreads.clear();
}

int PauseAndReturn()
{
	system("pause");
	return 0;
}

int main(int argc, char* argv[])
{
	std::cout << "Waiting for game process to start..." << std::endl;

	std::string DllPath = std::string(argv[0]);
	DllPath = DllPath.substr(0, DllPath.find_last_of('\\') + 1) + s_DllName;

//DllPath = "C:\\Program Files\\Microsoft PIX\\2310.30\\WinPixGpuCapturer.dll";

	if (std::filesystem::exists(std::filesystem::path(DllPath)) == false)
	{
		std::cout << "Could not find " << s_DllName << std::endl;
		std::cout << "Make sure it's in the same folder" << std::endl;
		return PauseAndReturn();
	}

	HANDLE processHandle = NULL;
	DWORD processId = 0;

	while (processHandle == NULL)
	{
		auto handleAndId = GetProcessHandleAndID(s_wProcessName);
		processHandle = handleAndId.first;
		processId = handleAndId.second;
		Sleep(100);
	}

	std::cout << "Suspending threads" << std::endl;
	std::vector<HANDLE> suspendedThreads;

	SuspendThreads(processId, suspendedThreads);

	PauseAndReturn();
	ResumeThreads(suspendedThreads);
	return 0;

	std::cout << "Injecting " << s_DllName << std::endl;

	LPVOID pRemoteMemory = VirtualAllocEx(processHandle, NULL, strlen(DllPath.c_str()) + 1, MEM_COMMIT, PAGE_READWRITE);
	if (!pRemoteMemory) 
	{
		std::cout << "VirtualAllocEx failed" << std::endl;
		ResumeThreads(suspendedThreads);
		return PauseAndReturn();
	}

	if (!WriteProcessMemory(processHandle, pRemoteMemory, (LPVOID)DllPath.c_str(), strlen(DllPath.c_str()) + 1, NULL)) 
	{
		VirtualFreeEx(processHandle, pRemoteMemory, 0, MEM_RELEASE);
		CloseHandle(processHandle);
		std::cout << "Could not write to process memory" << std::endl;
		ResumeThreads(suspendedThreads);
		return PauseAndReturn();
	}

	// Create a remote thread that calls LoadLibrary
	HANDLE hThread = CreateRemoteThread(
		processHandle, 
		NULL, 
		0,
		(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "LoadLibraryA"),
		pRemoteMemory, 
		0, 
		NULL);

	if (!hThread) 
	{
		VirtualFreeEx(processHandle, pRemoteMemory, 0, MEM_RELEASE);
		CloseHandle(processHandle);
		std::cout << "Could not create remote thread" << std::endl;
		ResumeThreads(suspendedThreads);
		return PauseAndReturn();
	}

	std::cout << "Injected DLL, resuming threads" << std::endl;
	//Sleep(1000);

	ResumeThreads(suspendedThreads);

	// Wait for the remote thread to terminate
	WaitForSingleObject(hThread, INFINITE);

	// Clean up
	VirtualFreeEx(processHandle, pRemoteMemory, 0, MEM_RELEASE);
	CloseHandle(hThread);
	CloseHandle(processHandle);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
