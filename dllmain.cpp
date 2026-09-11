#include "pch.h"
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <cstdint>
#include <wchar.h>
#include <new> // for std::nothrow?

#include "Classes/Classes.h"
#include "Utils/Utils.h"
#include "Settings/Settings.h"

Settings set;
static HANDLE hWorkerThread = nullptr; // thread handle

DWORD WINAPI WorkerThread(LPVOID)
{
    Utils::Log("Worker: started");

    unsigned long time = set.GetTimeout();
    double seconds = static_cast<double>(time) / 1000.0;
    std::stringstream wss;
    wss.precision(2);
    wss << std::fixed << "Worker: sleeping " << seconds << "s.";
    Utils::Log(wss.str());

    Sleep(time);
    Utils::Log("Worker: wakey wakey, cooking...");

    auto &keys = set.GetContentKeys();
    std::string msg = "Fetched " + std::to_string(keys.size()) + " Key(s)";
    Utils::Log(msg.c_str());
    for (int i = 0; i < keys.size(); ++i)
    {
        std::string buf = "Loaded Key " + std::to_string(i + 1) + ": " + keys[i];
        Utils::Log(buf);
    }

    uintptr_t FunctionVA = set.ResolveFunctionAddress(); // attempt to find the function via sig or RVA

    if (!FunctionVA) {
        Utils::Log("\nABORTING!\n");
        return NULL;
    }

    for (int i = 0; i < keys.size(); i++) {

        const char* ContentKey = keys[i].c_str();

        std::wstring WideKey = Utils::text_widen(keys[i]);

        size_t len = WideKey.size() + 1; // +1 for null terminator
        wchar_t* heapBuf = nullptr;
        char bufLog[256] = {};

        heapBuf = new (std::nothrow) wchar_t[len]; // allocate buffer for FString content
        if (!heapBuf)
        {
            Utils::Log("Worker: failed to allocate heap buffer");
            continue;
        }

        errno_t Errno = wcscpy_s(heapBuf, len, WideKey.c_str()); // write ContentKey into buffer
        if (Errno != 0)
        {
            Utils::Log("Worker: wcscpy_s failed with: {0}, skipping Key: \"{1}\"", Errno, keys[i]);
            continue;
        }

        std::stringstream sstream;
        sstream << "\nRaw Buffer Bytes " << i + 1 << ": ";
        for (size_t j = 0; j < WideKey.size(); j++)
        {   // log the bytes of the allocated buffer
            sstream << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(heapBuf[j]);
            if (j + 1 < len)
                sstream << ' ';
        }
        sstream << std::endl;
        Utils::Log(sstream.str());

        FString param(heapBuf, static_cast<int>(len - 1)); // construct FString

        FuncType* func = (FuncType*) (FunctionVA); // convert to absolute addy --> ApplyEncryptionKeyFromString's Signature

        sprintf_s(bufLog, "Worker: calling decryption function at 0x%p", (void*)FunctionVA);
        Utils::Log(bufLog);

        bool callResult = false;
        bool ok = Utils::SafeCall(func, &param, &callResult); // SEH - wrapped call

        if (!ok)
        {
            Utils::Log("Worker: exception during decryption function call! (check RVA & ModuleName)"); // Corrupted FString?
        }
        else
        {
            Utils::Log("Worker: decryption function call returned = {}", callResult ? "true" : "false");
        }

        delete[] heapBuf;
    }

    Utils::Log("Worker: exiting...");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            Utils::ClearLogFile();
            Utils::Log("// Only specify the RVA or a Signature. Do not provide both\n\nDLLMain: PROCESS_ATTACH\n");

            DisableThreadLibraryCalls(hModule); // TLS Callback

            hWorkerThread = CreateThread(nullptr, 0, [](LPVOID lpParam) -> DWORD
                {
                    Sleep(100); // gives DLLMAIN time to finsh

                    if (!set.Load())
                    {
                        Utils::Log("\nABORTING: Settings failed to load!\n");
                        return 0;
                    }

                    return WorkerThread(lpParam);
                }, nullptr, 0, nullptr);

            if (hWorkerThread)
            {
                Utils::Log("DLLMain: Worker thread created");
            }
            else
            {
                Utils::Log("DLLMain: Failed to create worker thread");
            }
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            Utils::Log("DLLMain: PROCESS_DETACH");
            if (hWorkerThread)
            {
                WaitForSingleObject(hWorkerThread, 2000);
                CloseHandle(hWorkerThread);
                hWorkerThread = nullptr;
            }
            break;
        }
    }
    return TRUE;
}