// SDL audio driver for it2play

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../../it_music.h"

static SDL_AudioDeviceID dev;

static void SDLCALL audioCallback(void *userdata, Uint8 *stream, int len)
{
	Music_FillAudioBuffer((int16_t *)stream, len / 4);
	(void)userdata;
}

void lockMixer(void)
{
	if (dev != 0)
		SDL_LockAudioDevice(dev);
}

void unlockMixer(void)
{
	if (dev != 0)
		SDL_UnlockAudioDevice(dev);
}

bool openMixer(int32_t mixingFrequency, int32_t mixingBufferSize)
{
	SDL_AudioSpec want, have;

	if (dev != 0)
		return true;

	if (SDL_Init(SDL_INIT_AUDIO) != 0)
		return false;

	memset(&want, 0, sizeof (want));
	want.freq = mixingFrequency;
	want.format = AUDIO_S16;
	want.channels = 2;
	want.samples = (uint16_t)mixingBufferSize;
	want.callback = audioCallback;

	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (dev == 0)
		return false;

	SDL_PauseAudioDevice(dev, false);
	return true;
}

void closeMixer(void)
{
	if (dev != 0)
	{
		SDL_PauseAudioDevice(dev, true);
		SDL_CloseAudioDevice(dev);
		dev = 0;
	}

	SDL_Quit();
}
