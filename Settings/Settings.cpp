#pragma once
#include "pch.h"
#include <sstream>
#include <algorithm>
#include "Settings.h"
#include "Utils/Utils.h"
#include "Pattern.h"
using namespace PatternScanner;

static constexpr size_t BUF_CHARS = 32768;

void Settings::EnsureIniPath()
{
    if (!iniPath.empty()) 
        return;

    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
    {
        char* last = strrchr(exePath, '\\');
        if (last) *last = '\0';
        iniPath = exePath;
        iniPath += "\\DecrypterSettings.ini";
    }
    else
    {
        iniPath = "DecrypterSettings.ini";
    }
}

bool Settings::ValidateIni()
{
    EnsureIniPath();

    DWORD attr = GetFileAttributesA(iniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    HANDLE hFile = CreateFileA(iniPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart == 0)
    {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    char buf[512] = {};

    GetPrivateProfileStringA("Settings", "ModuleName", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    GetPrivateProfileStringA("Settings", "FunctionRVA", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    GetPrivateProfileStringA("Settings", "Signature", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    GetPrivateProfileStringA("Settings", "Timeout", nullptr, buf, _countof(buf), iniPath.c_str());
    if (!buf[0]) return false;

    // At least one ContentKey
    std::vector<char> sectionBuf(BUF_CHARS);
    DWORD read = GetPrivateProfileSectionA("ContentKeys", sectionBuf.data(), (DWORD)sectionBuf.size(), iniPath.c_str());
    if (read == 0) return false;

    char* p = sectionBuf.data();
    bool hasKey = false;
    while (*p)
    {
        char* eq = strchr(p, '=');
        if (eq && *(eq + 1)) // key has a value
        {
            hasKey = true;
            break;
        }
        p += strlen(p) + 1;
    }
    if (!hasKey) return false;

    return true;
}

void Settings::CreateDefaultIni()
{
    EnsureIniPath();

    std::string defaultData =
        "[Settings]\r\n"
        "ModuleName=unrealeditorfortnite-engine-win64-shipping.dll\r\n"
        "FunctionRVA=0x0\r\n"
        "Signature=??\r\n"          // Working Signature (30th June 2026 - 41.10): 48 89 ?? ?? ?? 48 89 ?? ?? ?? 55 57 41 56 48 8D ?? ?? ?? 48 81 ?? ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? 48 33 ?? 48 89 ?? ?? 45 33 ?? 4C 89 ?? ?? 4C 89 ?? ?? 0F 57 ?? 0F 11 ?? ?? 0F 11 ?? ?? ---- (Old Signature - Pre 41.00: 48 89 ?? ?? ?? 48 89 ?? ?? ?? 48 89 ?? ?? ?? 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 33 DB 48 89 5D ?? 48 89 5D ?? 0F ?? ??)
        "Timeout=10000\r\n"
        "\r\n"
        "[ContentKeys]\r\n"
        "ExampleKey0=GUID:AES\r\n"
        "ExampleKey1=09CA2179883D4F55B8A296A6C149BF7A:A0HGfiKr7aVb5nv5c6lKtKPev3uZmcHqZzCNEkfyYr0=\r\n"
        "ExampleKey2=31aa3066-bfb9-44ff-a4c4-c1779bd11727:0x5E3B03D5A775FE495B2F75479D3610408DA7975DA2CCD1A4917136757C39E2FB";

    HANDLE h = CreateFileA(iniPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        WriteFile(h, defaultData.c_str(), static_cast<DWORD>(defaultData.size()), nullptr, nullptr);
        CloseHandle(h);
    }

    Log("\nINI is corrupted! applying defaults..\n");
}

uintptr_t Settings::ResolveFunctionAddress()
{

    uintptr_t ModuleBase = GetModuleBase(moduleName.c_str());
    if (!ModuleBase)
    {
        Log(("\nResolver: module \"" + moduleName + "\"doesn't exist").c_str());
        return 0;
    }

    char temp[256];
    sprintf_s(temp, "\nResolver: module '%s' loaded @ 0x%p", moduleName.c_str(), (void*)ModuleBase);
    Log(temp);

    bool hasRVA = (functionRVA != 0);
    bool hasSig = (!signature.empty() && signature != "??"); // default / null values

    if (hasRVA)
    {
        uintptr_t candidate = ModuleBase + functionRVA;

        sprintf_s(temp, "Resolver: trying RVA @ 0x%p", (void*)candidate);
        Log(temp);

        if (candidate > ModuleBase)
        {
            functionVA = candidate;
            Log("Resolver: RVA valid -> using RVA result");
            return functionVA;
        }

        Log("Resolver: invalid RVA");
        return 0;
    }

    if (hasSig)
    {
        functionVA = FindPattern(moduleName, signature);
        if (functionVA) {
            sprintf_s(temp, "Resolver: pattern found @ 0x%p", (void*)functionVA);
            Log(temp);
            return functionVA;
        }

        Log("Resolver: signature scan failed");
        return 0;
    }

    Log("Resolver: failed to resolve VA, provide RVA or Signature");
    return 0;
}

bool Settings::Load()
{
    EnsureIniPath();

    // if ini missing, create default
    DWORD attr = GetFileAttributesA(iniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !ValidateIni())
    {
        CreateDefaultIni();
        return false;
    }

    char modBuf[512] = {};
    GetPrivateProfileStringA("Settings", "ModuleName", moduleName.c_str(), modBuf, (UINT)_countof(modBuf), iniPath.c_str());
    moduleName = modBuf[0] ? std::string(modBuf) : moduleName;

    char rvaBuf[64] = {};
    GetPrivateProfileStringA("Settings", "FunctionRVA", nullptr, rvaBuf, (UINT)_countof(rvaBuf), iniPath.c_str());
    if (rvaBuf[0])
    {
        std::string s = rvaBuf;
        try
        {
            size_t idx = 0;
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) // strip "0x"
                functionRVA = static_cast<uintptr_t>(std::stoull(s.substr(2), &idx, 16));
            else
                functionRVA = static_cast<uintptr_t>(std::stoull(s, &idx, 0));
        }
        catch (...)
        {
            // keep default on parse failure
        }
    }

    char sigBuf[1024] = {};
    GetPrivateProfileStringA("Settings", "Signature", "", sigBuf, sizeof(sigBuf) / sizeof(char), iniPath.c_str());

    signature = sigBuf;

    char timeoutBuf[32] = {};
    GetPrivateProfileStringA("Settings", "Timeout", nullptr, timeoutBuf, (UINT)_countof(timeoutBuf), iniPath.c_str());
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

    std::vector<char> sectionBuf(BUF_CHARS);
    DWORD read = GetPrivateProfileSectionA("ContentKeys", sectionBuf.data(), (DWORD)sectionBuf.size(), iniPath.c_str());
    ContentKeys.clear();
    if (read > 0)
    {
        char* p = sectionBuf.data();
        while (*p)
        {
            char* eq = strchr(p, '=');
            if (eq)
            {
                std::string GuidKeyPair = eq + 1; // move past the = and read until '\0'
                auto pos = GuidKeyPair.find(L':');
                if (pos == std::string::npos) {
                    Log(("ContentKey: Wrong format on \"" + GuidKeyPair + "\"").c_str());
                    p += strlen(p) + 1;
                    continue;
                }
                std::string Guid = GuidKeyPair.substr(0, pos);  // Before ':'
                std::string Key  = GuidKeyPair.substr(pos + 1); // after ':'
                if (!Guid.empty()) {
                    Guid.erase(                                  // removes '-' from the GUID
                        std::remove(
                            Guid.begin(),
                            Guid.end(),
                            L'-'
                        ),
                        Guid.end()
                    );
                    std::transform(Guid.begin(), Guid.end(), Guid.begin(), ::toupper); // uppercase the GUID: 4f322681-e6f3-4d13-bf74-bf3244b27787 --> 4F322681E6F34D13BF74BF3244B27787
                }
                else
                    Log(("ContentKey: Guid in \"" + GuidKeyPair + "\" is empty").c_str());

                Key = TryHexToBase64(Key); // attempts to convert HEX AES to Base64, if already Base64 it will be untouched
                GuidKeyPair = Guid + ":" + Key;
                ContentKeys.push_back(GuidKeyPair);
            }
            p += strlen(p) + 1; // move past this null-terminated line
        }
    }
    return true;
}