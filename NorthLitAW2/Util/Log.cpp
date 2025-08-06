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
HANDLE hstdin = NULL;
HANDLE hstdout = NULL;

std::mutex logMutex;


static void PrintTimeStamp()
{
    auto now = std::chrono::system_clock::now();
    auto local_time = std::chrono::zoned_time(std::chrono::current_zone(), now);

    std::string sTimeStamp = std::format("[{}] ", local_time);
    SetConsoleTextAttribute(hstdout, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    printf(sTimeStamp.c_str());
    //fprintf(pfileout, sTimeStamp.c_str());
}

static void PrintMessage(WORD color, const char* type, const char* format, va_list args)
{
    // Block other threads from writing at the same time
    std::lock_guard<std::mutex> lock(logMutex);

    PrintTimeStamp();
    SetConsoleTextAttribute(hstdout, color);
    std::string finalFormat = std::string(type) + format + "\n";
    vfprintf(stdout, finalFormat.c_str(), args);
    //vfprintf(pfileout, finalFormat.c_str(), args);
    //fflush(pfileout);
}

void Log::Init()
{
#if LOG_ENABLE
    AllocConsole();
    freopen_s(&pfstdout, "CONOUT$", "w", stdout);
    freopen_s(&pfstdin, "CONIN$", "r", stdin);
    hstdin = GetStdHandle(STD_INPUT_HANDLE);
    hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

void Log::Shutdown()
{
#if LOG_ENABLE
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