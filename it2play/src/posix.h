#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

void closeSingleThread(void);

#ifdef _WIN32 

#define WIN32_LEAN_AND_MEAN
#include <windows.h> // Sleep()
#include <conio.h> // _kbhit(), _getch()

bool createSingleThread(void (*threadFunc)(void *arg));

#else

#define _stricmp strcasecmp
#define _strnicmp strncasecmp

bool createSingleThread(void *(*threadFunc)(void *arg));

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

void Sleep(uint32_t ms);
void modifyTerminal(void);
void revertTerminal(void);
bool _kbhit(void);
int32_t _getch(void);

#endif
