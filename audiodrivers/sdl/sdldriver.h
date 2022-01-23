#pragma once

#include <stdint.h>
#include <stdbool.h>

void lockMixer(void);
void unlockMixer(void);
bool openMixer(int32_t mixingFrequency, int32_t mixingBufferSize);
void closeMixer(void);
