@echo off


IF NOT EXIST "build" mkdir build

cl /D"DEVELOPER" /D"BOUNDS_CHECKING" /Isource /FC /Zi /nologo /W2 /permissive- /Fo"build/" /Fe"build/bricks.exe" "source/bricks.cpp" "source/rpmalloc/rpmalloc.c" /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO User32.lib Ole32.lib Shell32.lib Shlwapi.lib Dbghelp.lib Advapi32.lib

