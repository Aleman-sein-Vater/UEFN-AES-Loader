#include "pch.h"
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <cstdint>
#include <wchar.h>

#include "Classes/Classes.h"
#include "Utils/Utils.h"
#include "Settings/Settings.h"

Settings set;
static HANDLE hWorkerThread = nullptr; // thread handle

DWORD WINAPI WorkerThread(LPVOID)
{
    Utils::Log("Worker: started");

    unsigned long Time = set.GetTimeout();
    double Seconds = static_cast<double>(Time) / 1000.0;
    std::stringstream SS;
    SS.precision(2);
    SS << std::fixed << "Worker: sleeping " << Seconds << "s.";
    Utils::Log(SS.str());

    Sleep(Time);
    Utils::Log("Worker: wakey wakey, cooking...");

    auto &Keys = set.GetContentKeys();
    Utils::Log("Fetched {} Key(s)", Keys.size());

    for (size_t i = 0; i < Keys.size(); ++i)
    {
        Utils::Log("Loaded Key {}: {}", i + 1, Keys[i]);
    }

    set.TryResolveAddKeyAddress(); // attempt to find the function via sig or RVA

    if (!set.GetGetEncKeyAddy() || !set.GetAddKeyAddy())
    {
        Utils::Log("\nAddresses are wrong, ABORTING!\n");
        return NULL;
    }

    for (size_t i = 0; i < Keys.size(); i++)
    {
        std::wstring WideKey = Utils::text_widen(Keys[i]);

        std::stringstream KeySS;
        KeySS << "\nRaw Buffer Bytes " << i + 1 << ": ";
        for (size_t j = 0; j < WideKey.size(); j++)
        {   // log the bytes of the buffer
            KeySS << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(WideKey[j]);
            if (j + 1 < WideKey.size())
                KeySS << ' ';
        }
        KeySS << std::endl;
        Utils::Log(KeySS.str());

        FString Param(WideKey.c_str()); // construct FString

        if (!Utils::SafeCall(set, &Param))
        {
            Utils::Log("Worker: Key {} failed", i + 1);
        }
        else
        {
            Utils::Log("Worker: Key {} registered", i + 1);
        }
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
