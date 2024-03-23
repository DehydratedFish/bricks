@echo off


IF NOT EXIST "build" mkdir build

cl /D"DEVELOPER" /D"BOUNDS_CHECKING" /Isource /FC /Zi /nologo /W2 /permissive- /Fo"build/" /Fd"build/" /Fe"build/bricks.exe" "source/bricks.cpp" /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO User32.lib Ole32.lib Shell32.lib Shlwapi.lib Dbghelp.lib

