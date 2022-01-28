#!/bin/bash

rm release/other/it2play &> /dev/null
echo Compiling, please wait...

gcc -DNDEBUG -DAUDIODRIVER_SDL ../audiodrivers/sdl/*.c ../it2drivers/*.c ../loaders/mmcmp/*.c ../loaders/*.c ../*.c src/*.c -g0 -lSDL2 -lm -lpthread -Wshadow -Winit-self -Wall -Wno-uninitialized -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -march=native -mtune=native -O3 -o release/other/it2play

rm ../*.o src/*.o &> /dev/null

echo Done. The executable can be found in \'release/other\' if everything went well.
