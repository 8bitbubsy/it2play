#!/bin/bash

echo Compiling 64-bit Intel binary, please wait...

rm release/other/it2play &> /dev/null

clang -mmacosx-version-min=10.7 -arch x86_64 -mmmx -mfpmath=sse -msse2 -I/Library/Frameworks/SDL2.framework/Headers -F/Library/Frameworks -g0 -DNDEBUG -DAUDIODRIVER_SDL ../audiodrivers/sdl/*.c ../it2drivers/*.c ../loaders/mmcmp/*.c ../loaders/*.c ../*.c src/*.c -march=native -mtune=native -O3 -ffast-math -lm -Winit-self -Wno-deprecated -Wextra -Wunused -mno-ms-bitfields -Wno-missing-field-initializers -framework SDL2 -framework Cocoa -o release/other/it2play
strip release/other/it2play
install_name_tool -change @rpath/SDL2.framework/Versions/A/SDL2 /Library/Frameworks/SDL2.framework/Versions/A/SDL2 release/other/it2play

rm ../*.o src/*.o &> /dev/null
echo Done. The executable can be found in \'release/other\' if everything went well.
