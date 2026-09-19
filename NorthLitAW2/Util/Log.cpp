#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <stdio.h>
#include <Windows.h>

import Log;

#define LOG_ENABLE 1

FILE* pfstdout = NULL;
FILE* pfstdin = NULL;
FILE* pfileout = NULL;
HANDLE hstdin = NULL;
HANDLE hstdout = NULL;

std::mutex logMutex;


static std::string MakeTimeStamp()
{
    auto now = std::chrono::system_clock::now();
    auto local_time = std::chrono::zoned_time(std::chrono::current_zone(), now);
    return std::format("[{}] ", local_time);
}

static void PrintMessage(WORD color, const char* type, const char* format, va_list args)
{
    // Block other threads from writing at the same time
    std::lock_guard<std::mutex> lock(logMutex);

    const std::string timeStamp = MakeTimeStamp();
    const std::string finalFormat = std::string(type) + format + "\n";

    SetConsoleTextAttribute(hstdout, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    fputs(timeStamp.c_str(), stdout);
    SetConsoleTextAttribute(hstdout, color);

    va_list consoleArgs;
    va_copy(consoleArgs, args);
    vfprintf(stdout, finalFormat.c_str(), consoleArgs);
    va_end(consoleArgs);
    fflush(stdout);

    if (pfileout)
    {
        fputs(timeStamp.c_str(), pfileout);
        va_list fileArgs;
        va_copy(fileArgs, args);
        vfprintf(pfileout, finalFormat.c_str(), fileArgs);
        va_end(fileArgs);
        // Flush every line so the last successful init step survives a hard game crash.
        fflush(pfileout);
    }
}

void Log::Init()
{
#if LOG_ENABLE
    AllocConsole();
    freopen_s(&pfstdout, "CONOUT$", "w", stdout);
    freopen_s(&pfstdin, "CONIN$", "r", stdin);
    hstdin = GetStdHandle(STD_INPUT_HANDLE);
    hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

    // Persist the current run beside AlanWake2.exe so startup crashes do not erase diagnostics.
    char exePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
    {
        std::string logPath(exePath);
        const size_t slash = logPath.find_last_of("\\/");
        if (slash != std::string::npos)
            logPath.resize(slash + 1);
        else
            logPath.clear();
        logPath += "NorthLit.log";
        fopen_s(&pfileout, logPath.c_str(), "w");
    }
#endif
}

void Log::Shutdown()
{
#if LOG_ENABLE
    if (pfileout)
    {
        fflush(pfileout);
        fclose(pfileout);
        pfileout = NULL;
    }
    fclose(pfstdin);
    fclose(pfstdout);
    CloseHandle(hstdin);
    CloseHandle(hstdout);
#endif
}

void Log::Write(const char* fmt, ...)
{
#if LOG_ENABLE
    va_list args;
    va_start(args, fmt);
    PrintMessage(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "", fmt, args);
    va_end(args);
#endif
}

void Log::Warning(const char* fmt, ...)
{
#if LOG_ENABLE
    va_list args;
    va_start(args, fmt);
    PrintMessage(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[WARNING] ", fmt, args);
    va_end(args);
#endif
}

void Log::Error(const char* fmt, ...)
{
#if LOG_ENABLE
    va_list args;
    va_start(args, fmt);
    PrintMessage(FOREGROUND_RED | FOREGROUND_INTENSITY, "[ERROR] ", fmt, args);
    va_end(args);
#endif
}

void Log::Success(const char* fmt, ...)
{
#if LOG_ENABLE
    va_list args;
    va_start(args, fmt);
    PrintMessage(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[OK] ", fmt, args);
    va_end(args);
#endif
}