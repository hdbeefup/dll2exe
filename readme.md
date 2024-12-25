# What is this?
This tool will embed dll files or asi files into exe.
In some cases, asi/dll version info might be overriden, overwrited, and Resource Hacker might be unable to edit, or even load the final exe

This project is based on http://pefrm-units.osdn.jp/pefrmdllembed.html

# Requiements (no need, already included)
eirrepo https://svn.osdn.net/svnroot/eirrepo/common
peframework https://svn.osdn.net/svnroot/peframework/library
asmjit https://svn.osdn.net/svnroot/eirasmjit/eirasmjit / https://github.com/asmjit/asmjit
asmjitshared https://svn.osdn.net/svnroot/eirasmjit/shared  

https://osdn.net/users/quiret/  

# Compile
Open build/pefrmdllembed.sln with Visual Studio 2017+

# HOW TO USE
1) unpack pefrmdllembed.exe into a new folder on Desktop
2) copy your exe into the folder
3) copy your asi file into the folder
4) run cmd.exe in the new folder on Desktop
5) type into commandline:   pefrmdllembed *exe filename* *asi filename* output.exe
   where you replace things with the proper filenames
6) Use output.exe, asi and original exe might not be needed now.

You can inject multiple ASI files by giving more ASI filenames (but you must give output exe filename).

`dll2exe base202012-swine.exe SWINE.nutmaster.asi releasepigz.exe`

# COMMANDLINE OPTIONS

```
-efix: restores the original executable entry point after embedding so that version detection of .ASI files
 can use it. fixes support for OLA and modloader (possibly many more).
-impinj: removes DLL import dependencies by injecting the exports of the ASI directly into the import table; the ASI/DLL has
 to have the same name as the DLL import module
-noexp: skips embedding DLL exports into the output executable
-help: displays usage description
```