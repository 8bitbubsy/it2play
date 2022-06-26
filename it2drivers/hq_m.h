#pragma once

#include <stdint.h>
#include "../it_structs.h"

#define RAMPSPEED 7 /* matching SB16 MMX driver */

// if you change this, also change CUBIC_PHASES_BITS
#define CUBIC_PHASES 8192
#define CUBIC_PHASES_BITS 13 /* log2(CUBIC_PHASES) */

// don't change these!

#define CUBIC_WIDTH 4
#define CUBIC_WIDTH_BITS 2
#define CUBIC_LUT_LEN (CUBIC_WIDTH * CUBIC_PHASES)
#define CUBIC_FSHIFT (32-(CUBIC_PHASES_BITS+CUBIC_WIDTH_BITS))
#define CUBIC_FMASK ((CUBIC_WIDTH*CUBIC_PHASES)-CUBIC_WIDTH)

typedef void (*MixFunc_t)(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);

extern const MixFunc_t HQ_MixFunctionTables[16];
