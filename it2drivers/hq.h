#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"

bool HQ_InitSound(int32_t mixingFrequency);
void HQ_SetTempo(uint8_t Tempo);
void HQ_SetMixVolume(uint8_t Vol);
void HQ_FixSamples(void);
void HQ_ResetMixer(void);
void HQ_MixSamples(void);
int32_t HQ_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput);
void HQ_Mix(int32_t numSamples, int16_t *audioOut);
void HQ_UninitSound(void);
