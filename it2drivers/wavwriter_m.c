/* 8bb:
** ---- "WAV writer" IT2.14 driver (not the same as the one found in 2.15 code repo) ----
*/

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "../it_structs.h"
#include "../it_music.h"
#include "wavwriter.h"
#include "wavwriter_m.h"

/* 8bb: Weird value, I know. From IT2.14 free WAV writer code.
** It uses a fast algorithm to clamp, and these are the exact 
** min/max ranges you get in the end.
*/

#define FILTER_CLAMP 65535.9961f

#define Get8BitWaveForm \
	fSample = IT2_Cubic8(base, smp, sc->SmpError); \
	fSample *= sc->fFiltera; \
	fSample += sc->fOldSamples[0] * sc->fFilterb; \
	fSample += sc->fOldSamples[1] * sc->fFilterc; \
	\
	sample = (int32_t)fSample; \
	\
	     if (fSample < -FILTER_CLAMP) fSample = -FILTER_CLAMP; \
	else if (fSample >  FILTER_CLAMP) fSample =  FILTER_CLAMP; \
	\
	sc->fOldSamples[1] = sc->fOldSamples[0]; \
	sc->fOldSamples[0] = fSample; \
	\
	

#define Get16BitWaveForm \
	fSample = IT2_Cubic16(base, smp, sc->SmpError); \
	fSample *= sc->fFiltera; \
	fSample += sc->fOldSamples[0] * sc->fFilterb; \
	fSample += sc->fOldSamples[1] * sc->fFilterc; \
	\
	sample = (int32_t)fSample; \
	\
	     if (fSample < -FILTER_CLAMP) fSample = -FILTER_CLAMP; \
	else if (fSample >  FILTER_CLAMP) fSample =  FILTER_CLAMP; \
	\
	sc->fOldSamples[1] = sc->fOldSamples[0]; \
	sc->fOldSamples[0] = fSample; \
	\

#define MixSample \
	LastLeftValue  = sample * sc->CurrVolL; \
	LastRightValue = sample * sc->CurrVolR; \
	MixBufPtr[0] -= LastLeftValue; \
	MixBufPtr[1] -= LastRightValue; \
	MixBufPtr += 2; \

#define MixSampleSurround \
	LastLeftValue = sample * sc->CurrVolL; \
	MixBufPtr[0] -= LastLeftValue; \
	MixBufPtr[1] += LastLeftValue; \
	MixBufPtr += 2; \

#define RampCurrVolumeL \
	sc->CurrVolL += (sc->DestVolL - sc->CurrVolL) >> RAMPSPEED;

#define RampCurrVolumeR \
	sc->CurrVolR += (sc->DestVolR - sc->CurrVolR) >> RAMPSPEED;

#define UpdatePos \
	sc->SmpError += Driver.Delta; \
	smp += (int32_t)sc->SmpError >> MIX_FRAC_BITS; \
	sc->SmpError &= MIX_FRAC_MASK;

#define Mix32Stereo8Bit_M \
	Get8BitWaveForm \
	MixSample \
	UpdatePos \
	RampCurrVolumeL \
	RampCurrVolumeR

#define Mix32Stereo16Bit_M \
	Get16BitWaveForm \
	MixSample \
	UpdatePos \
	RampCurrVolumeL \
	RampCurrVolumeR

#define Mix32Surround8Bit_M \
	Get8BitWaveForm \
	MixSampleSurround \
	UpdatePos \
	RampCurrVolumeL

#define Mix32Surround16Bit_M \
	Get16BitWaveForm \
	MixSampleSurround \
	UpdatePos \
	RampCurrVolumeL

const mixFunc WAVWriter_MixFunctionTables[4] =
{
	(mixFunc)Mix32Stereo8Bit,
	(mixFunc)Mix32Stereo16Bit,
	(mixFunc)Mix32Surround8Bit,
	(mixFunc)Mix32Surround16Bit
};

int32_t LastLeftValue, LastRightValue; // 8bb: globalized

// 8bb: these two routines seem to be Cubic Lagrange (?)

static inline float IT2_Cubic8(int8_t *base, int8_t *smp, int32_t frac)
{
	int32_t tap0Smp = -1;
	if (smp == base)
		tap0Smp = 0; // 8bb: if (SamplingPos == 0) tap0Smp = 0 (turns into branchless code)

	float s1 = smp[tap0Smp];
	float s2 = smp[0] * 3.0f;
	float s3 = smp[1] * 3.0f;
	float s4 = smp[2];
	float fFrac = (float)frac * (1.0f / 65536.0f);

	const float F = s4 + s2;
	const float d = s2 + s2;
	const float a = F - s3 - s1;
	const float b = ((s1 * 3.0f) - d) + s3;
	const float c = ((s3 - s1) * 2.0f) - F;

	return ((((((a * fFrac) + b) * fFrac) + c) * fFrac) + d) * (256.0f / 6.0f);
}

static inline float IT2_Cubic16(int16_t *base, int16_t *smp, int32_t frac)
{
	int32_t tap0Smp = -1;
	if (smp == base)
		tap0Smp = 0; // 8bb: if (SamplingPos == 0) tap0Smp = 0 (turns into branchless code)

	float s1 = smp[tap0Smp];
	float s2 = smp[0] * 3.0f;
	float s3 = smp[1] * 3.0f;
	float s4 = smp[2];
	float fFrac = (float)frac * (1.0f / 65536.0f);

	const float F = s4 + s2;
	const float d = s2 + s2;
	const float a = F - s3 - s1;
	const float b = ((s1 * 3.0f) - d) + s3;
	const float c = ((s3 - s1) * 2.0f) - F;

	return ((((((a * fFrac) + b) * fFrac) + c) * fFrac) + d) * (1.0f / 6.0f);
}

void Mix32Stereo8Bit(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample;
	float fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix32Stereo8Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix32Stereo8Bit_M
		Mix32Stereo8Bit_M
		Mix32Stereo8Bit_M
		Mix32Stereo8Bit_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void Mix32Stereo16Bit(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SampleOffset;
	int32_t sample;
	float fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix32Stereo16Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix32Stereo16Bit_M
		Mix32Stereo16Bit_M
		Mix32Stereo16Bit_M
		Mix32Stereo16Bit_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void Mix32Surround8Bit(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample;
	float fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix32Surround8Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix32Surround8Bit_M
		Mix32Surround8Bit_M
		Mix32Surround8Bit_M
		Mix32Surround8Bit_M
	}

	LastRightValue = -LastLeftValue;

	sc->SampleOffset = (int32_t)(smp - base);
}

void Mix32Surround16Bit(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SampleOffset;
	int32_t sample;
	float fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix32Surround16Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix32Surround16Bit_M
		Mix32Surround16Bit_M
		Mix32Surround16Bit_M
		Mix32Surround16Bit_M
	}

	LastRightValue = -LastLeftValue;

	sc->SampleOffset = (int32_t)(smp - base);
}
