# it2play
Aims to be an accurate C port of Impulse Tracker 2.15's IT replayer (with selectable IT2 sound drivers). \
This is a direct port of the original asm source codes. \
\
The project contains example code in the it2play folder on how to interface with the API.

# Notes
- There are some differences between it2play and the way IT2.15 plays modules:
  1) it2play uses integer arithmetics for pitch slides, just like IT2.14 (IT2.15 uses FPU code)
  2) it2play uses clamp-limiting for the filters in the "WAV writer" driver, instead of IT2.15's filter compressor
  These changes were done to better match IT2.14, which is the version most people used (IT2.15 was paid software)
- To compile it2play (the test program) on macOS/Linux, you need SDL2
- When compiling, you need to pass the driver to use as a compiler pre-processor definition (f.ex. AUDIODRIVER_WINMM, check "it_music.h")
- There may be porting mistakes in the replayer, but the accuracy has been tested against quite a few songs, and seems to be accurate so far
- The code may not be 100% thread-safe (or safe in general), and as such I don't really recommend using this replayer in other projects.
  My primary goal was to create an accurate C port that people can use for reference.

