/* winmm audio driver for it2play
**
** Warning: This might not be 100% thread-safe or lock-safe!
*/

#define WIN32_LEAN_AND_MEAN

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include "../../it_music.h"

#define MIX_BUF_NUM 4

static volatile BOOL mixerOpened, mixerBusy, mixerLocked;
static uint8_t currBuffer;
static int16_t *audioBuffer[MIX_BUF_NUM];
static int32_t bufferSize;
static HANDLE hThread, hAudioSem;
static WAVEHDR waveBlocks[MIX_BUF_NUM];
static HWAVEOUT hWave;

static DWORD WINAPI mixThread(LPVOID lpParam)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	while (mixerOpened)
	{
		WAVEHDR *waveBlock = &waveBlocks[currBuffer];

		if (!mixerLocked)
		{
			mixerBusy = true;
			Music_FillAudioBuffer((int16_t *)waveBlock->lpData, bufferSize);
			mixerBusy = false;
		}

		waveOutWrite(hWave, waveBlock, sizeof (WAVEHDR));
		currBuffer = (currBuffer + 1) % MIX_BUF_NUM;
		WaitForSingleObject(hAudioSem, INFINITE); // wait for buffer fill request
	}

	return 0;

	(void)lpParam;
}

static void CALLBACK waveProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WOM_DONE)
		ReleaseSemaphore(hAudioSem, 1, NULL);

	(void)hWaveOut;
	(void)uMsg;
	(void)dwInstance;
	(void)dwParam1;
	(void)dwParam2;
}

void lockMixer(void)
{
	mixerLocked = true;
	while (mixerBusy);
}

void unlockMixer(void)
{
	mixerBusy = false;
	mixerLocked = false;
}

void closeMixer(void)
{
	mixerOpened = false;
	mixerBusy = false;

	if (hAudioSem != NULL)
		ReleaseSemaphore(hAudioSem, 1, NULL);

	if (hThread != NULL)
	{
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		hThread = NULL;
	}

	if (hAudioSem != NULL)
	{
		CloseHandle(hAudioSem);
		hAudioSem = NULL;
	}

	if (hWave != NULL)
	{
		waveOutReset(hWave);

		for (int32_t i = 0; i < MIX_BUF_NUM; i++)
		{
			if (waveBlocks[i].dwUser != 0xFFFF)
				waveOutUnprepareHeader(hWave, &waveBlocks[i], sizeof (WAVEHDR));
		}

		waveOutClose(hWave);
		hWave = NULL;
	}

	for (int32_t i = 0; i < MIX_BUF_NUM; i++)
	{
		if (audioBuffer[i] != NULL)
		{
			free(audioBuffer[i]);
			audioBuffer[i] = NULL;
		}
	}
}

bool openMixer(int32_t mixingFrequency, int32_t mixingBufferSize)
{
	DWORD threadID;
	WAVEFORMATEX wfx;

	// don't unprepare headers on error
	for (int32_t i = 0; i < MIX_BUF_NUM; i++)
		waveBlocks[i].dwUser = 0xFFFF;

	closeMixer();
	bufferSize = mixingBufferSize;

	ZeroMemory(&wfx, sizeof (wfx));
	wfx.nSamplesPerSec = mixingFrequency;
	wfx.wBitsPerSample = 16;
	wfx.nChannels = 2;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if (waveOutOpen(&hWave, WAVE_MAPPER, &wfx, (DWORD_PTR)&waveProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		goto omError;

	// create semaphore for buffer fill requests
	hAudioSem = CreateSemaphore(NULL, MIX_BUF_NUM - 1, MIX_BUF_NUM, NULL);
	if (hAudioSem == NULL)
		goto omError;

	// allocate WinMM mix buffers
	for (int32_t i = 0; i < MIX_BUF_NUM; i++)
	{
		audioBuffer[i] = (int16_t *)calloc(mixingBufferSize, wfx.nBlockAlign);
		if (audioBuffer[i] == NULL)
			goto omError;
	}

	// initialize WinMM mix headers
	memset(waveBlocks, 0, sizeof (waveBlocks));
	for (int32_t i = 0; i < MIX_BUF_NUM; i++)
	{
		waveBlocks[i].lpData = (LPSTR)audioBuffer[i];
		waveBlocks[i].dwBufferLength = mixingBufferSize * wfx.nBlockAlign;
		waveBlocks[i].dwFlags = WHDR_DONE;

		if (waveOutPrepareHeader(hWave, &waveBlocks[i], sizeof (WAVEHDR)) != MMSYSERR_NOERROR)
			goto omError;
	}

	currBuffer = 0;
	mixerOpened = true;

	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mixThread, NULL, 0, &threadID);
	if (hThread == NULL)
		goto omError;

	return TRUE;

omError:
	closeMixer();
	return FALSE;
}
