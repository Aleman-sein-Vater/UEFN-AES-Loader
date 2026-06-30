#pragma comment(lib, "crypt32.lib")
#include "pch.h"
#include <comdef.h>
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include "Utils/Utils.h"
#include "Settings/Settings.h"

uintptr_t GetModuleBase(const char* ModuleName)
{
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(0, ModuleName, &hMod))
    {
        return reinterpret_cast<uintptr_t>(hMod);
    }
    return NULL; // module doesnt exist..?
}

void ClearLogFile()
{                                                                   // CREATE_ALWAYS = clear
    HANDLE hFile = CreateFileA(logFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
}

void Log(const char* msg)
{
    HANDLE hFile = CreateFileA(logFileName, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

// from the goat: https://github.com/nathan-baggs/ufps/blob/main/src/utils/text_utils.cpp#L13
auto text_widen(const std::string& str) -> std::wstring
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

std::string TryHexToBase64(const std::string& hexInput)
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

        if (!base64.empty() && base64.back() == L'\0') {
            base64.pop_back();
        }

        return base64;

    } while (false);

    Log(("Base64Converter: AES Key \"" + hexInput + "\" is invalid").c_str());
    return hexInput;
}

bool SafeCall(FuncType* func, FString* param, bool* outResult)
{
    bool FuncResult = false;

    __try // structured exception handler
    {
        FuncResult = func(*param);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *outResult = false;
        return false;
    }

    *outResult = FuncResult; // no crashy, return result
    return true;
}