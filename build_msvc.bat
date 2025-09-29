@echo off

echo Building Executable bricks
cl /nologo /permissive- /W2 /Zi /D"DEVELOPER" /D"BOUNDS_CHECKING" /I"source" /I"dependencies/mountain/source" /Fe"build/debug/bricks.exe" /Fo".bricks/bricks.exe/debug/" /Fd"build/debug/" "source/bricks.cpp" "source/blueprint.cpp" "source/brickyard.cpp" "source/core_compilers\msvc.cpp" "dependencies/mountain/source/io.cpp" "dependencies/mountain/source/utf.cpp" "dependencies/mountain/source/ui.cpp" "dependencies/mountain/source/font.cpp" "dependencies/mountain/source/config.cpp" "dependencies/mountain/source/win32/platform.cpp" /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO "User32.lib" "Shell32.lib" "Gdi32.lib" "Ole32.lib"
