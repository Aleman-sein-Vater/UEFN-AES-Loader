#pragma once
#include <Windows.h>
#include <cstdint>
#include <format>
#include "Classes/Classes.h"
#include "Settings/Settings.h"

// From: https://github.com/EpicGames/UnrealEngine/blob/ue6-main/Engine/Source/Runtime/Core/Internal/Misc/EncryptionKeyManager.h
class FEncryptionKeyManager;

typedef FEncryptionKeyManager* GetEncryptionKeyManagerType(); // static FEncryptionKeyManager& FEncryptionKeyManager::Get()

typedef void AddKeyType(FEncryptionKeyManager* This, const FGuid& Id, const FAES::FAESKey& Key); // void FEncryptionKeyManager::AddKey(const FGuid& Id, const FAES::FAESKey& Key)

namespace Utils
{
	bool SafeCall(const Settings& Set, FString* param);
	uintptr_t GetModuleBase(const char* ModuleName);

	void ClearLogFile();
	void Log(const std::string& msg);

	template<typename... Args>
	void Log(std::format_string<Args...> format, Args&&... args)
	{
		Log(std::format(format, std::forward<Args>(args)...));
	}

	auto text_widen(const std::string& str) -> std::wstring; // Thank you Nathan Baggs my goat <3
	std::string TryHexToBase64(const std::string& hexInput);

	const void* GetAddressOfExportedFunction(const char* SearchModuleName, const char* SearchFunctionName); // Thank you Dumper-7
}