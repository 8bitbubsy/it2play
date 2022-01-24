// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../../it_d_rm.h"
#include "../../it_music.h"
#include "../../it_structs.h"
#include "posix.h"

// set to true if you want it2play to always render to WAV
#define DEFAULT_WAVRENDER_MODE_FLAG false

// defaults when not overriden by argument switches
#define DEFAULT_MIX_FREQ_SB 45454
#define DEFAULT_MIX_FREQ_WAVWRITER 44100

#define DEFAULT_MIX_BUFSIZE 1024
#define DEFAULT_DRIVER DRIVER_SB16MMX

static bool customMixFreqSet = false;
static bool renderToWavFlag = DEFAULT_WAVRENDER_MODE_FLAG;
static int32_t mixingFrequency = DEFAULT_MIX_FREQ_SB;
static int32_t mixingBufferSize = DEFAULT_MIX_BUFSIZE;
static int32_t IT2SoundDriver = DEFAULT_DRIVER;

static volatile bool programRunning;
static char *filename, *WAVRenderFilename;

static void showUsage(void);
static void handleArguments(int argc, char *argv[]);
static void readKeyboard(void);
static int32_t renderToWav(void);

double timingFunction()
{
#ifdef _WIN32
	LARGE_INTEGER performanceFrequency, performanceCounter;
	QueryPerformanceFrequency(&performanceFrequency);
	QueryPerformanceCounter(&performanceCounter);
	return ((double)performanceCounter.QuadPart) / ((double)performanceFrequency.QuadPart);
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + ((double)t.tv_nsec) / 1000000000.0;
#endif
}

// yuck!
#ifdef _WIN32
void wavRecordingThread(void *arg)
#else
void *wavRecordingThread(void *arg)
#endif
{
	(void)arg;
	Music_RenderToWAV(WAVRenderFilename);
#ifndef _WIN32
	return NULL;
#endif
}

#ifndef _WIN32
static void sigtermFunc(int32_t signum)
{
	programRunning = false; // unstuck main loop
	WAVRender_Flag = false; // unstuck WAV render loop
	(void)signum;
}
#endif

int main(int argc, char *argv[])
{
	// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#ifdef _DEBUG
	filename = "debug.it";
	(void)argc;
	(void)argv;
#else
	if (argc < 2 || (argc == 2 && (!strcmp(argv[1], "/?") || !strcmp(argv[1], "-h"))))
	{
		showUsage();
#ifdef _WIN32
		system("PAUSE");
#endif
		return 1;
	}

	handleArguments(argc, argv);
#endif

	if (!customMixFreqSet)
	{
		if (IT2SoundDriver == DRIVER_WAVWRITER)
			mixingFrequency = DEFAULT_MIX_FREQ_WAVWRITER;
		else
			mixingFrequency = DEFAULT_MIX_FREQ_SB;
	}

	if (!Music_Init(mixingFrequency, mixingBufferSize, IT2SoundDriver))
	{
		Music_Close();
		printf("ERROR: Couldn't initiaize it2play!\n");
		return 1;
	}

	if (!Music_LoadFromFile(filename))
	{
		Music_FreeSong();
		Music_Close();
		printf("ERROR: Couldn't open \"%s\" for reading!\n", filename);
		return 1;
	}

	// trap sigterm on Linux/macOS (since we need to properly revert the terminal)
#ifndef _WIN32
	struct sigaction action;
	memset(&action, 0, sizeof (struct sigaction));
	action.sa_handler = sigtermFunc;
	sigaction(SIGTERM, &action, NULL);
#endif

	if (renderToWavFlag)
		return renderToWav();

	Music_PlaySong(0);

	printf("Playing, press ESC to stop...\n");
	printf("\n");
	printf("Controls:\n");
	printf(" Esc=Quit   Plus = inc. song pos   Minus = dec. song pos\n");
	printf("\n");
	printf("Name: %s\n", Song.Header.SongName);
	printf("Stereo mode: %s\n", (Song.Header.Flags & ITF_STEREO) ? "Yes" : "No");
	printf("Mixing volume: %d/128\n", Song.Header.MixVolume);
	printf("Mixing frequency: %dHz\n", mixingFrequency);
	printf("IT2 sound driver: %s\n",
		(IT2SoundDriver == DRIVER_WAVWRITER) ? "WAV writer (filter clip: limit, no compression)" :
		(IT2SoundDriver == DRIVER_SB16MMX)   ? "SB16 MMX"   :
		(IT2SoundDriver == DRIVER_SB16)      ? "SB16"       : "Unknown");
	printf("\n");
	printf("Status:\n");

#ifndef _WIN32
	modifyTerminal();
#endif

	Music_RegisterTimingFunction(timingFunction);

	programRunning = true;
	while (programRunning)
	{
		readKeyboard();

		static int32_t highestVoiceCount = 0;
		int32_t activeVoices = Music_GetActiveVoices();
		if (activeVoices > highestVoiceCount)
			highestVoiceCount = activeVoices;

		int16_t order = Song.CurrentOrder;
		if (order < 0) // this can happen for a split second when you decrease the position :)
			order = 0;

		double timeSpent, timeIdle;
		Music_GetTiming(&timeSpent, &timeIdle);

		printf("Row: %03d/%03d - Pos: %03d - BPM: %3d - Speed: %3d - Channels: %3d (%d) - CPU: %.2f%%      \r",
			Song.CurrentRow, Song.NumberOfRows,
			order,
			Song.Tempo, Song.CurrentSpeed,
			activeVoices, highestVoiceCount,
			((timeSpent + timeIdle) > 0) ? timeSpent * 100.0 / (timeSpent + timeIdle) : 0.0);
		fflush(stdout);

		Sleep(50);
	}

#ifndef _WIN32
	revertTerminal();
#endif

	printf("\n");

	Music_FreeSong();
	Music_Close();

	printf("Playback stopped.\n");
	return 0;
}

static void showUsage(void)
{
	printf("Usage:\n");
	printf("  it2play input_module [-f hz] [-b buffersize] [-d driver] [--render-to-wav]\n");
	printf("\n");
	printf("  Options:\n");
	printf("    input_module      Specifies the IT module file to load\n");
	printf("    -f hz             Specifies the mixing frequency (16000..64000)\n");
	printf("    -b buffersize     Specifies the mixing buffer size (256..8192)\n");
	printf("    -d driver         Specifies what IT2 driver to use. Available choices are:\n");
	printf("                         SB16      (Sound Blaster 16)\n");
	printf("                         SB16MMX   (Sound Blaster 16 MMX, has filters + volramp)\n");
	printf("                         WAVWRITER (WAV writer, has filters + volramp)\n");
	printf("                      If no driver is specificed, SB16MMX will be used.\n");
	printf("    --render-to-wav   Renders song to WAV instead of playing it. The output\n");
	printf("                      filename will be the input filename with .WAV added to the\n");
	printf("                      end.\n");
	printf("\n");
}

static void handleArguments(int argc, char *argv[])
{
	filename = argv[1];
	if (argc > 2) // parse arguments
	{
		for (int32_t i = 1; i < argc; i++)
		{
			if (!_stricmp(argv[i], "-f") && i+1 < argc)
			{
				const int32_t num = atoi(argv[i+1]);
				mixingFrequency = CLAMP(num, 16000, 64000);
				customMixFreqSet = true;
			}
			else if (!_stricmp(argv[i], "-b") && i+1 < argc)
			{
				const int32_t num = atoi(argv[i+1]);
				mixingBufferSize = CLAMP(num, 256, 8192);
			}
			else if (!_stricmp(argv[i], "-d") && i+1 < argc)
			{
				     if (!_stricmp(argv[i+1], "SB16"))      IT2SoundDriver = DRIVER_SB16;
				else if (!_stricmp(argv[i+1], "SB16MMX"))   IT2SoundDriver = DRIVER_SB16MMX;
				else if (!_stricmp(argv[i+1], "WAVWRITER")) IT2SoundDriver = DRIVER_WAVWRITER;
			}
			else if (!_stricmp(argv[i], "--render-to-wav"))
			{
				renderToWavFlag = true;
			}
		}
	}
}

static void readKeyboard(void)
{
	if (_kbhit())
	{
		const int32_t key = _getch();
		switch (key)
		{
			case 0x1B: // esc
				programRunning = false;
				break;

			case 0x20: // space
				break;

			case 0x2B: // numpad +
				Music_NextOrder();
				break;

			case 0x2D: // numpad -
				Music_PreviousOrder();
				break;

			default: break;
		}
	}
}

static int32_t renderToWav(void)
{
	const size_t filenameLen = strlen(filename);
	WAVRenderFilename = (char *)malloc(filenameLen+5);

	if (WAVRenderFilename == NULL)
	{
		printf("Error: Out of memory!\n");
		Music_FreeSong();
		Music_Close();
		return 1;
	}

	strcpy(WAVRenderFilename, filename);
	strcat(WAVRenderFilename, ".wav");

	/* The WAV render loop also sets/listens/clears "WAVRender_Flag", but let's set it now
	** since we're doing the render in a separate thread (to be able to force-abort it if
	** the user is pressing a key).
	**
	** If you don't want to create a thread for the render, you don't have to
	** set this flag, and you just call Music_RenderToWAV("output.wav") directly.
	** Though, some songs will render forever (if they Bxx-jump to a previous order),
	** thus having this in a thread is recommended so that you can force-abort it, if stuck.
	*/
	WAVRender_Flag = true;
	if (!createSingleThread(wavRecordingThread))
	{
		printf("Error: Couldn't create WAV rendering thread!\n");
		free(WAVRenderFilename);
		Music_FreeSong();
		Music_Close();
		return 1;
	}

	printf("Rendering to WAV. If stuck forever, press any key to stop rendering...\n");

#ifndef _WIN32
	modifyTerminal();
#endif
	while (WAVRender_Flag)
	{
		Sleep(100);
		if ( _kbhit())
			WAVRender_Flag = false;
	}
#ifndef _WIN32
	revertTerminal();
#endif

	closeSingleThread();

	free(WAVRenderFilename);

	Music_FreeSong();
	Music_Close();

	return 0;
}
