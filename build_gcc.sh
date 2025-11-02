#! /bin/bash

echo Building Executable bricks
g++ -D"DEVELOPER" -D"BOUNDS_CHECKING" -I"source" -I"dependencies/mountain/source" -g -o build/debug/bricks "source/bricks.cpp" "source/blueprint.cpp" "source/brickyard.cpp" "source/core_compilers/gcc.cpp" "dependencies/mountain/source/io.cpp" "dependencies/mountain/source/utf.cpp" "dependencies/mountain/source/ui.cpp" "dependencies/mountain/source/font.cpp" "dependencies/mountain/source/config.cpp" "dependencies/mountain/source/linux/platform.cpp"

echo build_gcc.sh finished.

