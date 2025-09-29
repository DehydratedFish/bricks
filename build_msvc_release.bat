@echo off

echo Building Executable bricks
IF NOT EXIST build/release mkdir "build/release"
IF NOT EXIST .bricks/bricks.exe/release mkdir ".bricks/bricks.exe/release"
cl /nologo /permissive- /W2 /D"DEVELOPER" /D"BOUNDS_CHECKING" /I"source" /I"dependencies/mountain/source" /Fe"build/release/bricks.exe" /Fo".bricks/bricks.exe/release/" "source/bricks.cpp" "source/blueprint.cpp" "source/brickyard.cpp" "source/core_compilers\msvc.cpp" "dependencies/mountain/source/io.cpp" "dependencies/mountain/source/utf.cpp" "dependencies/mountain/source/ui.cpp" "dependencies/mountain/source/font.cpp" "dependencies/mountain/source/config.cpp" "dependencies/mountain/source/win32/platform.cpp" /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO "User32.lib" "Shell32.lib" "Gdi32.lib" "Ole32.lib"
