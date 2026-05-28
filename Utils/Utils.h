#pragma once
#include <Windows.h>
#include <cstdint>
#include "Classes/Classes.h"

uintptr_t GetModuleBase(const char* ModuleName);
void ClearLogFile();
void Log(const char* msg);
auto text_widen(const std::string& str) -> std::wstring;
std::string TryHexToBase64(const std::string& hexInput);

typedef bool FuncType(const FString& GuidKeyPair);
bool SafeCall(FuncType* func, FString* param, bool* outResult);