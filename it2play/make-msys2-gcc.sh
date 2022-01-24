#!/bin/bash

rm release/win64/it2play &> /dev/null
echo Compiling, please wait...

gcc -DNDEBUG -DAUDIODRIVER_WINMM ../audiodrivers/winmm/*.c ../it2drivers/*.c ../mmcmp/*.c ../*.c src/*.c -g0 -lwinmm -lm -lpthread -Wshadow -Winit-self -Wall -Wno-uninitialized -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -m64 -mmmx -mfpmath=sse -msse2 -O3 -s -o release/win64/it2play

rm ../*.o src/*.o &> /dev/null

echo Done. The executable can be found in \'release/win64\' if everything went well.
