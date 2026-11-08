# UEFN AES Loader
A UEFN DLL that emulates the [ApplyEncryptionKeyFromString](https://github.com/EpicGames/UnrealEngine/blob/684b4c133ed87e8050d1fdaa287242f0fe2c1153/Engine/Source/Runtime/Experimental/IoStore/OnDemand/Private/OnDemandConfig.cpp#L65) function to load provided AES Keys

------------------------
## How To Use
- This version is intended for `43.00-CL-59596594` and future UEFN version, please use version `1.1.3` of this DLL if you want to use it for earlier UEFN versions
    - Ensure you're looking at the `Finding the Function` of `1.1.3`!

- Either download the DLL or compile it from source

- Make sure the DLL is called `amfrt64.dll` and is placed inside your `..\Binaries\Win64` directory

- On the first launch, two files are generated:
    - `Decrypter.log`

        - The log file containing information about what the DLL has attempted
    - `DecrypterSettings.ini`

        - The settings file you need to configure
            - `ModuleName` is the name of the module where the function is located (either **unrealeditorfortnite-engine-win64-shipping.dll** for newer versions or the **.EXE** name for older versions)

            - `FunctionRVA` is the **R**elative **V**irtual **A**ddress of the function (read **Finding the function**)

            - `Signature` is the hex/byte pattern that will return the address of the function (has the potential to last for more than one update)

            - `Timeout` is the time in **milliseconds** to wait until attempting to load the AES Keys (loading too early may result in unexpected crashes.)

            - In the `[ContentKeys]` section, you provide the `FGuidAESPair` entries (ContentKeys) to load ( **MUST BE IN THE CORRECT FORMAT❗**)

- After configuring the settings correctly, each UEFN launch should automatically load the AES Keys

- **NOTES:**
    - Only provide either the **RVA** or the **Signature**. Do **NOT** provide both at once!
    - If something goes wrong, check `Decrypter.log` and investigate
        - Is the **ModuleName** correct?
        - Is the **RVA** valid?
        - Is the **Signature** correct?
        - Is the **Timeout** long enough?
        - Did the **Timeout** cause the decryption to be too late?
        - Did you rename the DLL to `amfrt64.dll`?

-----------------------
## Finding the Function
### I recommend using [x64dbg](https://x64dbg.com) for this

1. Open your **patched UEFN** and attach **x64dbg** to the running process
  
2. Download [AutoRTFM-Skipper.dp64](https://github.com/Aleman-sein-Vater/UEFN-AES-Loader/raw/master/plugins/AutoRTFM-Skipper.dp64) and place it inside your `x64dbg\release\x64\plugins` folder

3. **Search For** --> **All Modules** --> **String references**

4. After the search reaches 100%, search for `Ias.EncryptionKey=` and go to it

5. Scroll down a bit until you are roughly here:
<div align="center">
  <img width="1479" height="502" alt="Inside x64dbg showcasing the function" src="https://github.com/user-attachments/assets/598e9e33-3f91-4317-9173-4f282a40c7c3" />
</div>

6. Find the **exact same highlighted** CALL instruction and **double-click it**
    - The surrounding instructions may differ between versions, but the highlighted CALL should still appear

7. And you're done! Now just right-click on the function's first instruction (ignore the `dq A2737FF92A26A8C0`) and copy the RVA (or generate a signature)