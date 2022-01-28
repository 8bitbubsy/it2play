#pragma once

#include <stdint.h>
#include "../it_structs.h"

// 8bb: assumed max sample data output from cubic spline + filter routines, after heavy testing
#define MAX_SAMPLE_VALUE 230242 

#define RAMPSPEED 8
#define RAMPCOMPENSATE 255

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

extern int32_t LastLeftValue, LastRightValue;

extern const mixFunc WAVWriter_MixFunctionTables[4];

void Mix32Stereo8Bit(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void Mix32Stereo16Bit(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void Mix32Surround8Bit(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
void Mix32Surround16Bit(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
