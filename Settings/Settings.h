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

    const std::vector<std::string>& GetContentKeys() const { return ContentKeys; }
    uintptr_t ResolveFunctionAddress();
    unsigned long GetTimeout() const { return timeout; }

private:
    std::string iniPath;
    std::vector<std::string> ContentKeys;
    std::string moduleName = "unrealeditorfortnite-engine-win64-shipping.dll";
    uintptr_t functionRVA = 0x0;
    std::string signature = "";
    uintptr_t functionVA = 0x0;
    unsigned long timeout = 10000; // sleep time

    void EnsureIniPath();
    void CreateDefaultIni();
    bool ValidateIni();
};
