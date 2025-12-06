#pragma once
#include "pch.h"
#include <sstream>
#include "Settings.h"
#include "Utils/Utils.h"
#include "Pattern.h"
using namespace PatternScanner;

static constexpr size_t BUF_CHARS = 32768;

void Settings::EnsureIniPath()
{
    if (!iniPath.empty()) 
        return;

    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH))
    {
        wchar_t* last = wcsrchr(exePath, L'\\');
        if (last) *last = L'\0';
        iniPath = exePath;
        iniPath += L"\\DecrypterSettings.ini";
    }
    else
    {
        iniPath = L"DecrypterSettings.ini";
    }
}

bool Settings::ValidateIni()
{
    EnsureIniPath();

    DWORD attr = GetFileAttributesW(iniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    HANDLE hFile = CreateFileW(iniPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart == 0)
    {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    wchar_t buf[512] = {};

    GetPrivateProfileStringW(L"Settings", L"ModuleName", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    GetPrivateProfileStringW(L"Settings", L"FunctionRVA", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    GetPrivateProfileStringW(L"Settings", L"Signature", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    GetPrivateProfileStringW(L"Settings", L"Timeout", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    // At least one ContentKey
    std::vector<wchar_t> sectionBuf(BUF_CHARS);
    DWORD read = GetPrivateProfileSectionW(L"ContentKeys", sectionBuf.data(), (DWORD)sectionBuf.size(), iniPath.c_str());
    if (read == 0) return false;

    wchar_t* p = sectionBuf.data();
    bool hasKey = false;
    while (*p)
    {
        wchar_t* eq = wcschr(p, L'=');
        if (eq && *(eq + 1)) // key has a value
        {
            hasKey = true;
            break;
        }
        p += wcslen(p) + 1;
    }
    if (!hasKey) return false;

    return true;
}

void Settings::CreateDefaultIni()
{
    EnsureIniPath();

    std::wstring defaultData =
        L"[General]\r\n"
        L"ModuleName=unrealeditorfortnite-engine-win64-shipping.dll\r\n"
        L"FunctionRVA=0x0\r\n"
        L"Signature=??\r\n"
        L"Timeout=10000\r\n"
        L"\r\n"
        L"[ContentKeys]\r\n"
        L"Key1=AAAAAAAABBBBBBBBCCCCCCCCDDDDDDDD:A0HGfiKr7aVb5nv5c6lKtKPev3uZmcHqZzCNEkfyYr0=\r\n"
        L"Key2=AAAAAAAABBBBBBBBCCCCCCCCDDDDDDDD:BsnaYzjKolC4Ynb1zhYTZZTNLMIQ97SsbAoakGi0mmE=\r\n";

    HANDLE h = CreateFileW(iniPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WORD bom = 0xFEFF; // Write Byte-Order-Mark
        WriteFile(h, &bom, sizeof(bom), &written, nullptr);
        WriteFile(h, defaultData.c_str(), static_cast<DWORD>(defaultData.size() * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(h);
    }

    Log(L"\nINI is corrupted! resetting..\n");
}

uintptr_t Settings::ResolveFunctionAddress()
{

    uintptr_t ModuleBase = GetModuleBase(moduleName.c_str());
    if (!ModuleBase)
    {
        Log(L"\nResolver: module doesn't exist!");
        return 0;
    }

    wchar_t temp[256];
    swprintf_s(temp, L"\nResolver: module '%s' loaded @ 0x%p", moduleName.c_str(), (void*)ModuleBase);
    Log(temp);

    bool hasRVA = (functionRVA != 0);
    bool hasSig = (!signature.empty() && signature != "??");

    if (hasRVA)
    {
        uintptr_t candidate = ModuleBase + functionRVA;

        swprintf_s(temp, L"Resolver: trying RVA @ 0x%p", (void*)candidate);
        Log(temp);

        if (candidate > ModuleBase)
        {
            functionVA = candidate;
            Log(L"Resolver: RVA valid → using RVA result");
            return functionVA;
        }

        Log(L"Resolver: invalid RVA! attempting signature...");
        return 0;
    }

    if (hasSig)
    {
        functionVA = FindPattern(moduleName, signature);
        if (functionVA) {
            swprintf_s(temp, L"Resolver: pattern found @ 0x%p", (void*)functionVA);
            Log(temp);
            return functionVA;
        }

        Log(L"Resolver ERROR: signature scan failed");
        return 0;
    }

    Log(L"Resolver: failed to resolve VA!");
    return 0;
}

bool Settings::Load()
{
    EnsureIniPath();

    // if ini missing, create default
    DWORD attr = GetFileAttributesW(iniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !ValidateIni())
    {
        CreateDefaultIni();
        return false;
    }

    wchar_t buff[512] = {};
    GetPrivateProfileStringW(L"Settings", L"ModuleName", moduleName.c_str(), buff, (UINT)_countof(buff), iniPath.c_str());
    moduleName = buff[0] ? std::wstring(buff) : moduleName;

    wchar_t rvaBuf[64] = {};
    GetPrivateProfileStringW(L"Settings", L"FunctionRVA", nullptr, rvaBuf, (UINT)_countof(rvaBuf), iniPath.c_str());
    if (rvaBuf[0])
    {
        std::wstring s = rvaBuf;
        try
        {
            size_t idx = 0;
            // strip "0x"
            if (s.rfind(L"0x", 0) == 0 || s.rfind(L"0X", 0) == 0)
                functionRVA = static_cast<uintptr_t>(std::stoull(s.substr(2), &idx, 16));
            else
                functionRVA = static_cast<uintptr_t>(std::stoull(s, &idx, 0));
        }
        catch (...)
        {
            // keep default on parse failure
        }
    }

    wchar_t sigBufW[1024] = {};
    GetPrivateProfileStringW(L"Settings", L"Signature", L"", sigBufW, sizeof(sigBufW) / sizeof(wchar_t), iniPath.c_str());

    std::wstring ws(sigBufW);
    std::string sig(ws.begin(), ws.end());
    signature = sig;

    wchar_t timeoutBuf[32] = {};
    GetPrivateProfileStringW(L"Settings", L"Timeout", nullptr, timeoutBuf, (UINT)_countof(timeoutBuf), iniPath.c_str());
    if (timeoutBuf[0])
    {
        try
        {
            timeout = std::stoul(timeoutBuf);
        }
        catch (...)
        {
            // keep default on parse failure
        }
    }

    std::vector<wchar_t> sectionBuf(BUF_CHARS);
    DWORD read = GetPrivateProfileSectionW(L"ContentKeys", sectionBuf.data(), (DWORD)sectionBuf.size(), iniPath.c_str());
    ContentKeys.clear();
    if (read > 0)
    {
        wchar_t* p = sectionBuf.data();
        while (*p)
        {
            std::wstring line = p; // "KeyN=value"
            wchar_t* eq = wcschr(p, L'=');
            if (eq)
            {
                std::wstring value = eq + 1;
                if (!value.empty())
                    ContentKeys.push_back(value);
            }
            p += wcslen(p) + 1; // move past this null-terminated entry

        }
    }
    return true;
}