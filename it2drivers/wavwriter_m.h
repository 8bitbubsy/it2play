#pragma once

#include <stdint.h>
#include "../it_structs.h"

#define RAMPSPEED 8
#define RAMPCOMPENSATE 255

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

extern const mixFunc WAVWriter_MixFunctionTables[4];
