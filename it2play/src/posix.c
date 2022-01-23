/* Hackish attempt at making certain routines portable.
**
** Warning: Do not use these for other projects! They are not safe for general use.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32 

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE hThread;

bool createSingleThread(void (*threadFunc)(void *arg))
{
	DWORD dwThreadId;

	if (hThread != NULL)
		return false;

	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadFunc, NULL, 0, &dwThreadId);
	if (hThread == NULL)
		return false;

	return true;
}

void closeSingleThread(void)
{
	if (hThread != NULL)
	{
		WaitForSingleObject(hThread, INFINITE);

		CloseHandle(hThread);
		hThread = NULL;

	}
}

#else

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pthread.h>

static bool threadOpen;
static pthread_t threadId;
static int32_t lastChar = -1;
static struct termios tOld, tNew;

void Sleep(uint32_t ms)
{
	usleep(ms * 1000);
}

bool createSingleThread(void *(*threadFunc)(void *arg))
{
	if (threadOpen)
		return false;

	const int32_t err = pthread_create(&threadId, NULL, threadFunc, NULL);
	if (err)
	{
		threadOpen = false;
		return false;
	}

	threadOpen = true;
	return true;
}

void closeSingleThread(void)
{
	if (threadOpen)
	{
		pthread_join(threadId, NULL);
		threadOpen = false;
	}
}

// the following routines were found on google, and were modified

void modifyTerminal(void)
{
	tcgetattr(0, &tOld);

	tNew = tOld;
	tNew.c_lflag &= ~ICANON;
	tNew.c_lflag &= ~ECHO;
	tNew.c_lflag &= ~ISIG;
	tNew.c_cc[VMIN] = 1;
	tNew.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &tNew);

	lastChar = -1;
}

void revertTerminal(void)
{
	tcsetattr(0, TCSANOW, &tOld);
}

bool _kbhit(void)
{
	uint8_t ch;

	if (lastChar != -1)
		return true;

	tNew.c_cc[VMIN] = 0;
	tcsetattr(0, TCSANOW, &tNew);

	const int32_t nread = read(0, &ch, 1);
	tNew.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &tNew);

	if (nread == 1)
	{
		lastChar = ch;
		return true;
	}

	return false;
}

int32_t _getch(void)
{
	char ch;

	if (lastChar != -1)
	{
		ch = lastChar;
		lastChar = -1;
	}
	else
	{
		read(0, &ch, 1);
	}

	return ch;
}

#endif
