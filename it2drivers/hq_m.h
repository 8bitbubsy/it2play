#pragma once

#include <stdint.h>
#include "../cpu.h"
#include "../it_structs.h"

#define RAMPSPEED 8 /* slightly faster than SB16 MMX driver */

#if CPU_32BIT

#define CUBIC_PHASES 4096
#define CUBIC_PHASES_BITS 12

#else

#define CUBIC_PHASES 8192
#define CUBIC_PHASES_BITS 13

#endif

// don't change these!

#define CUBIC_WIDTH 4
#define CUBIC_WIDTH_BITS 2
#define CUBIC_LUT_LEN (CUBIC_WIDTH * CUBIC_PHASES)

#if CPU_32BIT
#define CUBIC_FSHIFT (16-(CUBIC_PHASES_BITS+CUBIC_WIDTH_BITS))
#else
#define CUBIC_FSHIFT (32-(CUBIC_PHASES_BITS+CUBIC_WIDTH_BITS))
#endif

#define CUBIC_FMASK ((CUBIC_WIDTH*CUBIC_PHASES)-CUBIC_WIDTH)

typedef void (*MixFunc_t)(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);

extern const MixFunc_t HQ_MixFunctionTables[16];
