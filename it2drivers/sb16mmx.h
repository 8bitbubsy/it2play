#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"

bool SB16MMX_InitSound(int32_t mixingFrequency);
void SB16MMX_SetTempo(uint8_t Tempo);
void SB16MMX_SetMixVolume(uint8_t vol);
void SB16MMX_FixSamples(void);
void SB16MMX_ResetMixer(void); // 8bb: added this
void SB16MMX_MixSamples(void); // 8bb: added this
int32_t SB16MMX_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput); // 8bb: added this
void SB16MMX_Mix(int32_t numSamples, int16_t *audioOut); // 8bb: added this (original SB16 MMX driver uses IRQ callback)
void SB16MMX_UninitSound(void);
