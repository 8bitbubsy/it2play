#pragma once

#include <stdint.h>
#include "../it_structs.h"

#define RAMPSPEED 7
#define RAMPCOMPENSATE 63
#define FILTER_BITS 14

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

extern int32_t Delta;

extern const mixFunc SB16MMX_MixFunctionTables[8];

void M32Bit8M(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit16M(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit8MI(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit16MI(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit8MV(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit16MV(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit8MF(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Bit16MF(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
