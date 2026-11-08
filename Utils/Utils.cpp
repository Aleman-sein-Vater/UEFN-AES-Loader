#include "pch.h"
#pragma comment(lib, "crypt32.lib")
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include "Utils/Utils.h"
#include "Settings/Settings.h"

uintptr_t Utils::GetModuleBase(const char* ModuleName)
{
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(ModuleName));
}

void Utils::ClearLogFile()
{                                                                   // CREATE_ALWAYS = clear
    HANDLE hFile = CreateFileA(logFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
}

void Utils::Log(const std::string& msg)
{
    HANDLE hFile = CreateFileA(logFileName, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(hFile, msg.c_str(), static_cast<DWORD>(msg.size()), &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

// from the goat: https://github.com/nathan-baggs/ufps/blob/main/src/utils/text_utils.cpp#L13
auto Utils::text_widen(const std::string& str) -> std::wstring
{
    if (str.empty())
        return {};

    const int len = static_cast<int>(str.size());
    const int num_wide_chars = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), len, nullptr, 0);
    if (num_wide_chars == 0)
    {
        Log("text_widen: failed to get wstring size");
        return {};
    }

    std::wstring wide_str(num_wide_chars, L'\0');
    if (::MultiByteToWideChar(CP_UTF8, 0, str.data(), len, wide_str.data(), num_wide_chars) != num_wide_chars)
    {
        Log("text_widen: failed to widen string");
        return {};
    }

    return wide_str;
}

std::string Utils::TryHexToBase64(const std::string& hexInput)
{
    if (!(hexInput.length() == 66 || hexInput.length() == 64)) // Hexadecimal Format check
        return hexInput; // if not HEX then just return orig

    size_t offset = (hexInput.compare(0, 2, "0x") == 0) ? 2 : 0; // trim 0x
    const char* hexData = hexInput.c_str() + offset;
    DWORD hexLen = static_cast<DWORD>(hexInput.length() - offset);

    if (hexLen == 0)
        return hexInput;

    do
    {
        DWORD binaryLen = 0; // pszString, cchString,   dwFlags,            *pbBinary, *pcbBinary, *pdwSkip, *pdwFlags
        if (!CryptStringToBinaryA(hexData, hexLen, CRYPT_STRING_HEXRAW, nullptr, &binaryLen, nullptr, nullptr)) {
            break;
        }

        std::vector<BYTE> binary(binaryLen);
        if (!CryptStringToBinaryA(hexData, hexLen, CRYPT_STRING_HEXRAW, binary.data(), &binaryLen, nullptr, nullptr)) {
            break;
        }

        DWORD base64Len = 0;
        if (!CryptBinaryToStringA(binary.data(), binaryLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Len)) {
            break;
        }

        std::string base64(base64Len, '\0');
        if (!CryptBinaryToStringA(binary.data(), binaryLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64.data(), &base64Len)) {
            break;
        }

        if (!base64.empty() && base64.back() == '\0') {
            base64.pop_back();
        }

        return base64;

    } while (false);

    Log(("Base64Converter: AES Key \"" + hexInput + "\" is invalid").c_str());
    return hexInput;
}

const void* Utils::GetAddressOfExportedFunction(const char* SearchModuleName, const char* SearchFunctionName)
{
    /* Get the module the function was exported from */
    const uintptr_t ModuleBase = GetModuleBase(SearchModuleName);
    const IMAGE_DOS_HEADER* DosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ModuleBase);

    if (ModuleBase == 0x0 || DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;

    const IMAGE_NT_HEADERS* NtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(ModuleBase + DosHeader->e_lfanew);

    if (!NtHeader)
        return nullptr;

    const DWORD ExportRVA = NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!ExportRVA)
        return nullptr;

    /* Get the table of functions exported by the module */
    const IMAGE_EXPORT_DIRECTORY* ExportTable = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(ModuleBase + ExportRVA);

    const DWORD* NameOffsets = reinterpret_cast<const DWORD*>(ModuleBase + ExportTable->AddressOfNames);
    const DWORD* FunctionOffsets = reinterpret_cast<const DWORD*>(ModuleBase + ExportTable->AddressOfFunctions);

    const WORD* Ordinals = reinterpret_cast<const WORD*>(ModuleBase + ExportTable->AddressOfNameOrdinals);

    /* Iterate all names and return the function if the name matches what we're looking for */
    for (DWORD i = 0; i < ExportTable->NumberOfNames; i++)
    {
        const WORD NameIndex = Ordinals[i];
        const char* Name = reinterpret_cast<const char*>(ModuleBase + NameOffsets[i]);

        if (strcmp(SearchFunctionName, Name) == 0)
            return reinterpret_cast<void*>(ModuleBase + FunctionOffsets[NameIndex]);
    }

    return nullptr;
}

static bool ParseEncryptionKeyParam(const FString& Param, FGuid& OutKeyGuid, FAES::FAESKey& OutKey)
{
    bool bSuccess = false;
    TArray<FString> Tokens;
    Param.ParseIntoArray(Tokens, TEXT(":"), true);
    if (Tokens.Num() == 2)
    {
        TArray<uint8> KeyBytes;
        if (!FGuid::Parse(Tokens[0], OutKeyGuid) || !OutKeyGuid.IsValid())
        {
            Utils::Log("Parser: Invalid GUID \"{}\"", Tokens[0].ToString());
        }
        else if (!FBase64::Decode(Tokens[1], KeyBytes) || KeyBytes.Num() != FAES::FAESKey::KeySize)
        {
            Utils::Log("Parser: Invalid AES key (decoded {} bytes, expected {})", KeyBytes.Num(), FAES::FAESKey::KeySize);
        }
        else
        {
            std::memcpy(OutKey.Key, KeyBytes.GetData(), FAES::FAESKey::KeySize);
            bSuccess = true;
        }
        KeyBytes.FreeArray();
    }
    else
    {
        Utils::Log("Parser: Invalid format (expected GUID:AES, got {} token(s))", Tokens.Num());
    }

    for (int32 i = 0; i < Tokens.Num(); ++i)
    {
        Tokens[i].FreeArray();
    }
    Tokens.FreeArray();
    return bSuccess;
}

static bool InvokeAddKeySEH(GetEncryptionKeyManagerType* GetMgr, AddKeyType* AddKey, const FGuid& KeyGuid, const FAES::FAESKey& Key, FEncryptionKeyManager** OutMgr, DWORD* OutExceptionCode)
{
    __try // Windows SEH if the addresses are wrong, so we dont crash
    {
        if (GetMgr && AddKey)
        {
            FEncryptionKeyManager* Mgr = GetMgr();
            if (OutMgr) *OutMgr = Mgr;
            if (!Mgr) return false;

            AddKey(Mgr, KeyGuid, Key);
            return true;
        }
    }
    __except (*OutExceptionCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

bool Utils::SafeCall(const Settings& Set, FString* param)
{
    FGuid KeyGuid;
    FAES::FAESKey Key;
    if (!ParseEncryptionKeyParam(*param, KeyGuid, Key))
    {
        return false;
    }

    FString GuidStr = KeyGuid.ToString(EGuidFormats::DigitsWithHyphens);
    Utils::Log("SafeCall: Parsed GUID = {}", GuidStr.ToString());
    GuidStr.FreeArray();

    auto* GetMgr = reinterpret_cast<GetEncryptionKeyManagerType*>(Set.GetGetEncKeyAddy());
    auto* AddKey = reinterpret_cast<AddKeyType*>(Set.GetAddKeyAddy());

    FEncryptionKeyManager* Mgr = nullptr;
    DWORD ExceptionCode = 0;
    if (!InvokeAddKeySEH(GetMgr, AddKey, KeyGuid, Key, &Mgr, &ExceptionCode))
    {
        if (ExceptionCode != 0)
            Utils::Log("SafeCall: SEH exception 0x{:08X} (Mgr @ {:p})", ExceptionCode, (void*)Mgr);
        else
            Utils::Log("SafeCall: FEncryptionKeyManager::Get() returned nullptr");
        return false;
    }

    Utils::Log("SafeCall: Added key to FEncryptionKeyManager @ {:p}", (void*)Mgr);
    return true;
}
