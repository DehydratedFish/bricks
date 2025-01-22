@echo off

pushd dependencies\mountain
call build_msvc_lib.bat
popd

IF NOT EXIST "build" mkdir build

set mountain_lib="dependencies\mountain\build\mountain.lib"
set sources=source\bricks.cpp source\blueprint.cpp source\brickyard.cpp source\core_compilers\msvc.cpp
set libs=User32.lib Gdi32.lib Ole32.lib Shell32.lib Shlwapi.lib Dbghelp.lib %mountain_lib%

cl /D"DEVELOPER" /D"BOUNDS_CHECKING" /D"_HAS_EXCEPTIONS=0" /GR- /Isource /Idependencies\mountain\source /FC /Zi /nologo /W2 /permissive- /Fo"build/" /Fd"build/" /Fe"build/bricks.exe" %sources% /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO %libs%

