#pragma once

#include <stdint.h>
#include "../cpu.h"
#include "../it_structs.h"

#define RAMPSPEED 8 /* slightly faster than SB16 MMX driver */

#define SINC_WIDTH 8
#define SINC_WIDTH_BITS 3 /* log2(SINC_WIDTH) */
#define SINC_PHASES 4096
#define SINC_PHASES_BITS 12 /* log2(SINC_PHASES) */

#if CPU_32BIT /* note: mixer has 16-bit fractional precision if 32-bit */
#define SINC_FSHIFT (16-(SINC_PHASES_BITS+SINC_WIDTH_BITS))
#else
#define SINC_FSHIFT (32-(SINC_PHASES_BITS+SINC_WIDTH_BITS))
#endif

#define SINC_FMASK ((SINC_WIDTH*SINC_PHASES)-SINC_WIDTH)

typedef void (*MixFunc_t)(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);

extern const MixFunc_t HQ_MixFunctionTables[16];
