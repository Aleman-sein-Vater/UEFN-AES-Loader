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

uintptr_t GetModuleBase(const wchar_t* ModuleName)
{
    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(0, ModuleName, &hMod))
    {
        return reinterpret_cast<uintptr_t>(hMod);
    }
    return NULL; // module doesnt exist..?
}

void ClearLogFile()
{                                                                   // CREATE_ALWAYS = clear
    HANDLE hFile = CreateFileW(logFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
}

void Log(const wchar_t* w_msg) // ..don't ask me why i decided to make this wide
{
    _bstr_t t(w_msg);
    const char* msg = t; // Conversion, so the Decrypter.log is not UTF-16
    HANDLE hFile = CreateFileW(logFileName, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg) * sizeof(char)), &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

std::wstring TryHexToBase64(const std::wstring& hexInput)
{
    if (!(hexInput.length() == 66 || hexInput.length() == 64)) // Hexadecimal Format check
        return hexInput; // if not HEX then just return orig

    size_t offset = (hexInput.compare(0, 2, L"0x") == 0) ? 2 : 0; // trim 0x
    const wchar_t* hexData = hexInput.c_str() + offset;
    DWORD hexLen = static_cast<DWORD>(hexInput.length() - offset);

    if (hexLen == 0)
        return hexInput;

    DWORD binaryLen = 0; // pszString, cchString,   dwFlags,            *pbBinary, *pcbBinary, *pdwSkip, *pdwFlags
    if (!CryptStringToBinaryW(hexData, hexLen,      CRYPT_STRING_HEXRAW, nullptr,  &binaryLen,  nullptr, nullptr)) {
        Log((L"Base64Converter: AES Key \"" + hexInput + L"\" is invalid").c_str());
        return hexInput;
    }

    std::vector<BYTE> binary(binaryLen);
    CryptStringToBinaryW(hexData, hexLen, CRYPT_STRING_HEXRAW, binary.data(), &binaryLen, nullptr, nullptr);

    DWORD base64Len = 0;
    CryptBinaryToStringW(binary.data(), binaryLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Len);

    std::wstring base64(base64Len, L'\0');
    CryptBinaryToStringW(binary.data(), binaryLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64.data(), &base64Len);

    if (!base64.empty() && base64.back() == L'\0') {
        base64.pop_back();
    }

    return base64;
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