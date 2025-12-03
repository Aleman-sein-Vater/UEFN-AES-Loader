#pragma once
#include "pch.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "Classes/Classes.h"

static constexpr const wchar_t* logFileName = L"Decrypter.log";

class Settings
{
public:
    Settings() = default;

    bool Load();

    const std::vector<std::wstring>& GetContentKeys() const { return ContentKeys; }
    uintptr_t ResolveFunctionAddress();
    unsigned long GetTimeout() const { return timeout; }

private:
    std::wstring iniPath;
    std::vector<std::wstring> ContentKeys;
    std::wstring moduleName = L"unrealeditorfortnite-engine-win64-shipping.dll";
    uintptr_t functionRVA = 0x0;
    std::string signature = "";
    uintptr_t functionVA = 0x0;
    unsigned long timeout = 10000; // sleep time

    void EnsureIniPath();
    void CreateDefaultIni();
    bool ValidateIni();
};
