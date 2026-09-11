#pragma once
#include <Windows.h>
#include <cstdint>
#include <format>
#include "Classes/Classes.h"

typedef bool FuncType(const FString& GuidKeyPair);

namespace Utils
{
	bool SafeCall(FuncType* func, FString* param, bool* outResult);
	uintptr_t GetModuleBase(const char* ModuleName);

	void ClearLogFile();
	void Log(const std::string& msg);

	template<typename... Args>
	void Log(std::format_string<Args...> format, Args&&... args)
	{
		Log(std::format(format, std::forward<Args>(args)...));
	}

	auto text_widen(const std::string& str) -> std::wstring;
	std::string TryHexToBase64(const std::string& hexInput);
}