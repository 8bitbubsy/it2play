#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"

bool WAVWriter_InitSound(int32_t mixingFrequency);
void WAVWriter_SetTempo(uint8_t Tempo);
void WAVWriter_SetMixVolume(uint8_t vol);
void WAVWriter_FixSamples(void);
void WAVWriter_ResetMixer(void); // 8bb: added this
void WAVWriter_MixSamples(void); // 8bb: added this
void WAVWriter_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput); // 8bb: added this
void WAVWriter_Mix(int32_t numSamples, int16_t *audioOut); // 8bb: added this (original SB16 MMX driver uses IRQ callback)
void WAVWriter_UninitSound(void);
