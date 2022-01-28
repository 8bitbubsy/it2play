#pragma once

#include <stdint.h>
#include <stdbool.h>

bool SB16_InitSound(int32_t mixingFrequency);
void SB16_SetTempo(uint8_t Tempo);
void SB16_SetMixVolume(uint8_t vol);
void SB16_FixSamples(void);
void SB16_ResetMixer(void); // 8bb: added this
void SB16_MixSamples(void); // 8bb: added this
void SB16_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput); // 8bb: added this
void SB16_Mix(int32_t numSamples, int16_t *audioOut); // 8bb: added this (original SB16 MMX driver uses IRQ callback)
void SB16_UninitSound(void);
