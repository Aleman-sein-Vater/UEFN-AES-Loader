#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

// skidded from ddl

namespace PatternScanner
{
    inline std::vector<int> PatternToBytes(const std::string& pattern)
    {
        std::vector<int> bytes;
        const char* p = pattern.c_str();

        while (*p)
        {
            if (*p == ' ')
            {
                p++;
                continue;
            }

            if (*p == '?')
            {
                bytes.push_back(-1);

                if (*(p + 1) == '?')
                    p += 2;
                else
                    p += 1;

                continue;
            }

            char* end = nullptr;
            int value = strtoul(p, &end, 16);

            if (end == p)
                break;

            bytes.push_back(value);
            p = end;
        }

        return bytes;
    }

    inline uintptr_t GetModuleBase(const std::wstring& moduleName)
    {
        return (uintptr_t)GetModuleHandleW(moduleName.c_str());
    }

    inline size_t GetModuleSize(const std::wstring& moduleName)
    {
        auto base = (PBYTE)GetModuleHandleW(moduleName.c_str());
        auto nt = (PIMAGE_NT_HEADERS)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
        return nt->OptionalHeader.SizeOfImage;
    }

    template <typename T = uintptr_t>
    T FindPattern(const std::wstring& moduleName, const std::string& pattern)
    {
        uintptr_t base = GetModuleBase(moduleName);
        size_t size = GetModuleSize(moduleName);

        auto patternBytes = PatternToBytes(pattern);
        size_t patternLength = patternBytes.size();

        for (size_t i = 0; i < size - patternLength; i++)
        {
            bool found = true;
            for (size_t j = 0; j < patternLength; j++)
            {
                if (patternBytes[j] != -1 && patternBytes[j] != *(uint8_t*)(base + i + j))
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                if constexpr (std::is_pointer<T>::value)
                    return reinterpret_cast<T>(base + i);
                else
                    return static_cast<T>(base + i);
            }
        }

        if constexpr (std::is_pointer<T>::value)
            return nullptr;
        else
            return 0;
    }
}
