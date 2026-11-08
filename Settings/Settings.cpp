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
    if (!IniPath.empty()) 
        return;

    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
    {
        char* last = strrchr(exePath, '\\');
        if (last) *last = '\0';
        IniPath = exePath;
        IniPath += "\\DecrypterSettings.ini";
    }
    else
    {
        IniPath = "DecrypterSettings.ini";
    }
}

bool Settings::ValidateIni()
{
    EnsureIniPath();

    DWORD attr = GetFileAttributesA(IniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        Utils::Log("ValidateIni: \"{}\" not found", IniPath);
        return false;
    }

    HANDLE hFile = CreateFileA(IniPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart == 0)
    {
        CloseHandle(hFile);
        Utils::Log("ValidateIni: INI is empty");
        return false;
    }
    CloseHandle(hFile);

    char buf[512] = {};

    GetPrivateProfileStringA("Settings", "ModuleName", nullptr, buf, _countof(buf), IniPath.c_str());
    if (!buf[0]) { Utils::Log("ValidateIni: Missing ModuleName"); return false; }

    GetPrivateProfileStringA("Settings", "FunctionRVA", nullptr, buf, _countof(buf), IniPath.c_str());
    if (!buf[0]) { Utils::Log("ValidateIni: Missing FunctionRVA"); return false; }

    GetPrivateProfileStringA("Settings", "Signature", nullptr, buf, _countof(buf), IniPath.c_str());
    if (!buf[0]) { Utils::Log("ValidateIni: Missing Signature"); return false; }

    GetPrivateProfileStringA("Settings", "Timeout", nullptr, buf, _countof(buf), IniPath.c_str());
    if (!buf[0]) { Utils::Log("ValidateIni: Missing Timeout"); return false; }

    // At least one ContentKey
    std::vector<char> sectionBuf(BUF_CHARS);
    DWORD read = GetPrivateProfileSectionA("ContentKeys", sectionBuf.data(), (DWORD)sectionBuf.size(), IniPath.c_str());
    if (read == 0) { Utils::Log("ValidateIni: Missing [ContentKeys]"); return false; }

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
    if (!hasKey) { Utils::Log("ValidateIni: No keys in [ContentKeys]"); return false; }

    return true;
}

void Settings::CreateDefaultIni()
{
    EnsureIniPath(); // make sure the Ini Path is correct

    Utils::Log("\nINI is corrupted! Applying defaults..\n");

    std::string defaultData =
        "[Settings]\r\n"
        "ModuleName=unrealeditorfortnite-engine-win64-shipping.dll\r\n"
        "FunctionRVA=0x0\r\n"
        "Signature=??\r\n"          // Working Signature (1st Nov 2026 - ...?): 41 57 41 56 41 54 56 57 55 53 48 83 EC ?? 4C 89 C7 48 89 D3 48 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? B1 ?? 31 C0 F0 0F B0 0E 0F 85 ?? ?? ?? ?? 4C 8D 76 ?? 48 89 D9 BA ?? ?? ?? ??
        "Timeout=10000\r\n"
        "\r\n"
        "[ContentKeys]\r\n"
        "ExampleKey0=GUID:AES\r\n"
        "ExampleKey1=09CA2179883D4F55B8A296A6C149BF7A:A0HGfiKr7aVb5nv5c6lKtKPev3uZmcHqZzCNEkfyYr0=\r\n"
        "ExampleKey2=33353a66-bf69-44ff-a4c4-b1779bd15727:0xAECB03D5A775FEAEBB2F75479D3610408DA7975DA2CCD1A4917136757C39E2FB";

    HANDLE h = CreateFileA(IniPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        WriteFile(h, defaultData.c_str(), static_cast<DWORD>(defaultData.size()), nullptr, nullptr);
        CloseHandle(h);
    }
}

bool Settings::TryResolveAddKeyAddress()
{
    uintptr_t ModuleBase = Utils::GetModuleBase(ModuleName.c_str());
    if (!ModuleBase)
    {
        Utils::Log("\nResolver: Module \"{}\" doesn't exist", ModuleName);
        return false;
    }

    Utils::Log("\nResolver: Module \"{}\" loaded @ {:p}", ModuleName, (void*)ModuleBase);

    if (uintptr_t PotentialAddy = reinterpret_cast<uintptr_t>(Utils::GetAddressOfExportedFunction(ModuleName.c_str(), GetEncKeyExport.c_str())); PotentialAddy /* != nullptr */)
    {
        GetEncKeyVA = PotentialAddy;
        Utils::Log("Resolver: Export \"{}\" @ {:p} (RVA: 0x{:X})", GetEncKeyExport, (void*)GetEncKeyVA, GetEncKeyVA - ModuleBase);
    }
    else
    {
        Utils::Log("Resolver: Export \"{}\" in Module \"{}\" doesn't exist", GetEncKeyExport, ModuleName);
        return false;
    }

    bool bHasRVA = (AddKeyRVA != 0);
    bool bHasSig = (!AddKeyPattern.empty() && AddKeyPattern != "??"); // default / null values

    if (bHasRVA)
    {
        uintptr_t Canidate = ModuleBase + AddKeyRVA;

        Utils::Log("Resolver: Trying RVA @ {:p}", (void*)Canidate);

        if (Canidate > ModuleBase)
        {
            AddKeyVA = Canidate;
            Utils::Log("Resolver: RVA valid -> using RVA result");
            return true;
        }

        Utils::Log("Resolver: Invalid RVA");
        return false;
    }

    if (bHasSig)
    {
        AddKeyVA = FindPattern(ModuleName, AddKeyPattern);
        if (AddKeyVA) {
            Utils::Log("Resolver: Pattern found @ {:p} (RVA: 0x{:X})", (void*)AddKeyVA, AddKeyVA - ModuleBase);
            return true;
        }

        Utils::Log("Resolver: AddKeyPattern (Signature) scan failed");
        return false;
    }

    Utils::Log("Resolver: Failed to resolve VA, provide RVA or Signature");
    return false;
}

bool Settings::Load()
{
    EnsureIniPath();

    // if ini missing, create default
    DWORD attr = GetFileAttributesA(IniPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !ValidateIni())
    {
        CreateDefaultIni();
        return false;
    }

    char modBuf[512] = {};
    GetPrivateProfileStringA("Settings", "ModuleName", ModuleName.c_str(), modBuf, (UINT)_countof(modBuf), IniPath.c_str());
    ModuleName = modBuf[0] ? std::string(modBuf) : ModuleName;

    char rvaBuf[64] = {};
    GetPrivateProfileStringA("Settings", "FunctionRVA", nullptr, rvaBuf, (UINT)_countof(rvaBuf), IniPath.c_str());
    if (rvaBuf[0])
    {
        std::string s = rvaBuf;
        try
        {
            size_t idx = 0;
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) // strip "0x"
                AddKeyRVA = static_cast<uintptr_t>(std::stoull(s.substr(2), &idx, 16));
            else
                AddKeyRVA = static_cast<uintptr_t>(std::stoull(s, &idx, 0));
        }
        catch (const std::exception& e)
        {
            Utils::Log("Error while Loading FunctionRVA: {}", e.what());
        }
    }

    char sigBuf[1024] = {};
    GetPrivateProfileStringA("Settings", "Signature", "", sigBuf, sizeof(sigBuf) / sizeof(char), IniPath.c_str());

    AddKeyPattern = sigBuf;

    char timeoutBuf[32] = {};
    GetPrivateProfileStringA("Settings", "Timeout", nullptr, timeoutBuf, (UINT)_countof(timeoutBuf), IniPath.c_str());
    if (timeoutBuf[0])
    {
        try
        {
            Timeout = std::stoul(timeoutBuf);
        }
        catch (const std::exception& e)
        {
            Utils::Log("Error while Loading Timeout: {}", e.what());
        }
    }

    Utils::Log("Settings: Loaded \"{}\" (Module: \"{}\", RVA: 0x{:X})", IniPath, ModuleName, AddKeyRVA);

    std::vector<char> sectionBuf(BUF_CHARS);
    DWORD read = GetPrivateProfileSectionA("ContentKeys", sectionBuf.data(), (DWORD)sectionBuf.size(), IniPath.c_str());
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
                auto pos = GuidKeyPair.find(':');
                if (pos == std::string::npos) {
                    Utils::Log("ContentKey: Wrong format on \"{}\"", GuidKeyPair);
                    p += strlen(p) + 1;
                    continue;
                }
                std::string Guid = GuidKeyPair.substr(0, pos);  // Before ':'
                std::string Key  = GuidKeyPair.substr(pos + 1); // after ':'
                if (Guid.empty()) {
                    Utils::Log("ContentKey: Guid in \"{}\" is empty", GuidKeyPair);
                    p += strlen(p) + 1;
                    continue;
                }

                Key = Utils::TryHexToBase64(Key); // attempts to convert HEX AES to Base64, if already Base64 it will be untouched
                GuidKeyPair = Guid + ":" + Key;
                ContentKeys.push_back(GuidKeyPair);
            }
            p += strlen(p) + 1; // next line
        }
    }
    return true;
}
