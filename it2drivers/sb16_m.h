#pragma once

#include <stdint.h>
#include "../it_structs.h"

#define MIXTABLESIZE (256*65)

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

extern int16_t MixSegment[MIXTABLESIZE];
extern uint32_t LeftVolume16, RightVolume16;

extern const mixFunc SB16_MixFunctionTables[16];

void M12Mix8(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix16(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix8S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix16S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix8I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix16I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix8IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M12Mix16IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix8IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void M32Mix16IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

