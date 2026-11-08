#pragma once
#include "pch.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "Classes/Classes.h"

static constexpr const char* logFileName = "Decrypter.log";

class Settings
{
public:
    Settings() = default;

    bool Load();

    bool TryResolveAddKeyAddress();
    const uintptr_t GetAddKeyAddy() const { return AddKeyVA; }
    const uintptr_t GetGetEncKeyAddy() const { return GetEncKeyVA; }
    unsigned long GetTimeout() const { return Timeout; }
    const std::vector<std::string>& GetContentKeys() const { return ContentKeys; }

private:
    std::string IniPath;
    std::string ModuleName = "unrealeditorfortnite-engine-win64-shipping.dll";
    uintptr_t AddKeyRVA = 0x0, AddKeyVA = 0x0, GetEncKeyVA = 0x0;
    std::string AddKeyPattern = "", GetEncKeyExport = "?Get@FEncryptionKeyManager@UE@@SAAEAV12@XZ";
    unsigned long Timeout = 10000; // sleep time in ms
    std::vector<std::string> ContentKeys;

    void EnsureIniPath();
    void CreateDefaultIni();
    bool ValidateIni();
};
