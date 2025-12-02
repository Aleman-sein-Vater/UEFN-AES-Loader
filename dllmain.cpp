#include "pch.h"
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <cstdint>
#include <wchar.h>
#include <new> // for std::nothrow?

#include "Classes.h"
#include "Utils.h"
#include "Settings.h"

Settings set;
static HANDLE hWorkerThread = nullptr; // thread handle

DWORD WINAPI WorkerThread(LPVOID)
{
    Log(L"Worker: started");

    unsigned long time = set.GetTimeout();
    double seconds = static_cast<double>(time) / 1000.0;
    std::wstringstream wss;
    wss.precision(2);
    wss << std::fixed << L"Worker: sleeping " << seconds << L"s.";
    Log(wss.str().c_str());

    Sleep(time);
    Log(L"Worker: wakey wakey, cooking...");
    uintptr_t FunctionRVA = set.GetFunctionRVA();

    std::vector<std::wstring> keys = set.GetContentKeys(); // try applying all keys
    for (int i = 0; i < keys.size(); i++) {

        const wchar_t* ContentKey = keys[i].c_str();

        size_t len = wcslen(ContentKey) + 1; // +1 for null terminator
        wchar_t* heapBuf = nullptr;
        wchar_t bufLog[256];

        heapBuf = new (std::nothrow) wchar_t[len]; // allocate buffer for FString content
        if (!heapBuf)
        {
            Log(L"Worker: failed to allocate heap buffer!");
            continue;
        }

        wcscpy_s(heapBuf, len, ContentKey); // write ContentKey into buffer

        std::wstringstream wstream;
        wstream << "\nRaw Buffer Bytes " << i + 1 << ": ";
        for (int i = 0; i < len; i++)
        {   // log the bytes of the allocated buffer
            wstream << std::uppercase << std::setfill(L'0') << std::setw(2) << std::hex << static_cast<unsigned int>(heapBuf[i]);
            if (i + 1 < len)
                wstream << L' ';
        }
        wstream << std::endl;
        std::wstring wresult = wstream.str();
        Log(wresult.c_str());

        FString param(heapBuf, static_cast<int>(len)); // construct FString

        uintptr_t ModuleBase = GetModuleBase(set.GetModuleName().c_str());
        FuncType* func = (FuncType*) (ModuleBase + FunctionRVA); // convert to absolute addy --> ApplyEncryptionKeyFromString
        if (!func || !ModuleBase || !FunctionRVA)
        {
            std::wstring err = L"Worker: ";
            if (!func) err += L"Func pointer invalid; ";
            if (!ModuleBase) err += L"ModuleBase invalid; ";
            if (!FunctionRVA) err += L"FunctionRVA invalid; ";
            Log(err.c_str());
            delete[] heapBuf;
            continue;
        }

        swprintf_s(bufLog, L"Worker: calling decryption function at 0x%p", (void*)FunctionRVA);
        Log(bufLog);

        bool callResult = false;
        bool ok = SafeCall(func, &param, &callResult); // SEH - wrapped call

        if (!ok)
        {
            Log(L"Worker: exception during decryption function call! (check RVA & ModuleName)"); // Corrupted FString?
        }
        else
        {
            swprintf_s(bufLog, L"Worker: decryption function call returned = %d", callResult ? 1 : 0);
            Log(bufLog);
        }

        delete[] heapBuf;
    }

    Log(L"Worker: exiting...");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
    {
        ClearLogFile();
        Log(L"DLLMain: PROCESS_ATTACH");

        DisableThreadLibraryCalls(hModule);

        if (!set.Load())
        {
            Log(L"\nABORTING!\n");
            return FALSE;
        }

        const auto& keys = set.GetContentKeys();
        for (int i = 0; i < keys.size(); ++i)
        {
            std::wstring buf = L"Loaded Key " + std::to_wstring(i + 1) + L": " + keys[i];
            Log(buf.c_str());
        }

        hWorkerThread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        if (hWorkerThread)
        {
            Log(L"DLLMain: Worker thread created");
        }
        else
        {
            Log(L"DLLMain: Failed to create worker thread!");
        }
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        Log(L"DLLMain: PROCESS_DETACH");
        if (hWorkerThread)
        {
            WaitForSingleObject(hWorkerThread, 5000);
            CloseHandle(hWorkerThread);
            hWorkerThread = nullptr;
        }
        break;
    }
    }
    return TRUE;
}