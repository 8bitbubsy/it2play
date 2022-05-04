# it2play
Aims to be an accurate C port of Impulse Tracker 2.15's IT replayer (with selectable IT2 sound drivers). \
This is a direct port of the original asm source codes. \
\
The project contains example code in the it2play folder on how to interface with the API.

# Notes
- The default driver (HQ) is my own, which has a floating-point mixer, better tempo (BPM) precision, 4-tap cubic spline interpolation and stereo sample support.
- it2play uses integer arithmetics for pitch slides, just like IT2 versions before 2.15 (2.15 uses FPU code). \
  This was done to match the IT2 versions most people were making music with. 2.15 was paid software, few people used it.
- To compile it2play (the test program) on macOS/Linux, you need SDL2. TODO: Implement ALSA/JACK drivers.
- When compiling, you need to pass the driver to use as a compiler pre-processor definition (f.ex. AUDIODRIVER_SDL, check "it_music.h")
- There may be porting mistakes in the replayer, but the accuracy has been tested against quite a few songs, and seems to be accurate so far
- The code may not be 100% thread-safe (or safe in general), and as such I don't really recommend using this replayer in serious projects.
  My primary goal was to create an accurate C port that people can use for reference, or for personal use.

