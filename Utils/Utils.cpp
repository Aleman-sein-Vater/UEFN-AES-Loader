#include "pch.h"
#include "Utils/Utils.h"
#include "Settings/Settings.h"

uintptr_t GetModuleBase(const wchar_t* ModuleName)
{
    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(0, ModuleName, &hMod))
    {
        return reinterpret_cast<uintptr_t>(hMod);
    }
    return NULL; // module doesnt exist..?
}

void ClearLogFile()
{                                                                   // CREATE_ALWAYS = clear
    HANDLE hFile = CreateFileW(logFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
}

void Log(const wchar_t* msg)
{
    HANDLE hFile = CreateFileW(logFileName, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, msg, static_cast<DWORD>(wcslen(msg) * sizeof(wchar_t)), &written, nullptr);
    WriteFile(hFile, L"\r\n", static_cast<DWORD>(wcslen(L"\r\n") * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(hFile);
}

bool SafeCall(FuncType* func, FString* param, bool* outResult)
{
    bool FuncResult = false;

    __try // structured exception handler
    {
        FuncResult = func(*param);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *outResult = false;
        return false;
    }

    *outResult = FuncResult; // no crashy, return result
    return true;
}