#pragma once

#include <stdint.h>
#include "../it_structs.h"

#define RAMPSPEED 7
#define RAMPCOMPENSATE 63
#define FILTER_BITS 14

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

extern const mixFunc SB16MMX_MixFunctionTables[8];
