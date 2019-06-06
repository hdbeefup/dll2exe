GTA ASI Injector, build date June 06th 2019, by The_GTA
pefrmdllembed - http://pefrm-units.osdn.jp/pefrmdllembed.html
===========================================================

This tool is made to install ASI files directly into GTA executables. It works just like any ASI loader but 
puts the ASI into the EXE directly, avoiding having to ship multiple ASI files.

You can test things with two sample ASIs. They are from DK22Pac's plugin-sdk.
Find plugin-sdk here: https://github.com/DK22Pac/plugin-sdk
- SA_CreateCar (compatible with SA 1.0 US): press tab to spawn funny Infernus
- VC_PedPainting (compatible with VC 1.0 US): Tommy is painted red ingame

Virus scan report:
https://www.virustotal.com/gui/file/aefeb77be65d05a70da27f2916c6b895bbee54f460d928fc6c175795b3130f12/detection

==========================
HOW TO USE
==========================
1) unpack pefrmdllembed.exe into a new folder on Desktop
2) copy your gta exe into the folder
3) copy your asi file into the folder
4) run cmd.exe in the new folder on Desktop
5) type into commandline:   pefrmdllembed *gta exe filename* *asi filename* output.exe
   where you replace things with the proper filenames
6) copy output.exe into your GTA folder
7) remove the .asi from your GTA folder if present; not required anymore
8) run output.exe

You can inject multiple ASI files by giving more ASI filenames (but you must give output exe filename).

==========================
COMMANDLINE OPTIONS
==========================

-efix: restores the original executable entry point after embedding so that version detection of .ASI files
 can use it. fixes support for OLA and modloader (possibly many more).
-impinj: removes DLL import dependencies by injecting the exports of the ASI directly into the import table; the ASI/DLL has
 to have the same name as the DLL import module
-noexp: skips embedding DLL exports into the output executable
-help: displays usage description

===========================
KNOWN WORKING
===========================
SilentPatch, outfit.asi, SkyGFX, CLEO, OLA, modloader and many more! Make many custom EXE files!


Hope this tool is useful to you.

- Martin