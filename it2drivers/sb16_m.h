#pragma once

#include <stdint.h>
#include "../it_structs.h"

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

extern const mixFunc SB16_MixFunctionTables[8];

void M32Mix8(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
