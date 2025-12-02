#pragma once
#include <Windows.h>
#include <cstdint>
#include "Classes.h"

uintptr_t GetModuleBase(const wchar_t* ModuleName);
void ClearLogFile();
void Log(const wchar_t* msg);

typedef bool FuncType(const FString& GuidKeyPair);
bool SafeCall(FuncType* func, FString* param, bool* outResult);