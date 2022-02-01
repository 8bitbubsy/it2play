/*
** ---- "WAV writer" (IT2.15 registered) driver ----
*/

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "../it_structs.h"
#include "../it_music.h"
#include "wavwriter.h"
#include "wavwriter_m.h"

#define Get8BitWaveForm \
	fSample = IT2_Cubic8(base, smp, sc->SmpError); \
	fSample *= sc->fFiltera; \
	f1 = sc->fOldSamples[0]; \
	f1 *= sc->fFilterb; \
	f2 = sc->fOldSamples[1]; \
	f2 *= sc->fFilterc; \
	f1 += f2; \
	fSample += f1; \
	\
	sc->fOldSamples[1] = sc->fOldSamples[0]; \
	sc->fOldSamples[0] = fSample; \
	\
	sample = ApplyCompressorAndQuantize(fSample);

#define Get16BitWaveForm \
	fSample = IT2_Cubic16(base, smp, sc->SmpError); \
	fSample *= sc->fFiltera; \
	f1 = sc->fOldSamples[0]; \
	f1 *= sc->fFilterb; \
	f2 = sc->fOldSamples[1]; \
	f2 *= sc->fFilterc; \
	f1 += f2; \
	fSample += f1; \
	\
	sc->fOldSamples[1] = sc->fOldSamples[0]; \
	sc->fOldSamples[0] = fSample; \
	\
	sample = ApplyCompressorAndQuantize(fSample);

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

// 8bb: these two have very small rounding errors, so I keep them as IT2 constants
static const float Const256On6 = 42.6666641f;
static const float Const1On6 = 0.166666657f;

static inline int32_t ApplyCompressorAndQuantize(float fSample)
{
	int32_t SampleValue;

	if (fSample < -32768.0f || fSample > 32768.0f)
	{
		uint32_t XORMask = 0;
		if (fSample < 0.0f)
		{
			fSample = -fSample;
			XORMask = 0xFFFFFFFF;
		}

#define LN_32768 22713.0 /* 8bb: 22713 = round[ln(2) * 32768] */

		SampleValue = (int32_t)((log2(fSample * (1.0 / 32768.0)) * LN_32768) + (32768.0 + 0.5));
		SampleValue ^= XORMask;
		SampleValue -= XORMask;
	}
	else
	{
		if (fSample < 0.0)
			fSample -= 0.5f;
		else
			fSample += 0.5f;

		SampleValue = (int32_t)fSample;
	}

	return SampleValue;
}

// 8bb: these two routines seem to be Cubic Lagrange (?)

static inline float IT2_Cubic8(int8_t *base, int8_t *smp, int32_t frac)
{
	float F, a, b, c, d;

	int32_t tap0Smp = -1;
	if (smp == base)
		tap0Smp = 0; // 8bb: if (SamplingPos == 0) tap0Smp = 0 (turns into branchless code)

	// 8bb: done like this to match precision loss in IT2 code (FPU is in 24-bit (32) mode)

	float s1 = smp[tap0Smp];
	float s2 = smp[0];
	float s3 = smp[1];
	float s4 = smp[2];
	float fFrac = (float)frac;

	fFrac *= 1.0f / 65536.0f;
	s2 *= 3.0f;
	s3 *= 3.0f;

	F = s4;
	F += s2;
	c = s3;
	c -= s1;
	c += c;
	c -= F;
	a = F;
	a -= s3;
	a -= s1;
	a *= fFrac;
	d = s2;
	d += d;
	b = s1;
	b *= 3.0f;
	b -= d;
	b += s3;
	b += a;
	b *= fFrac;
	b += c;
	b *= fFrac;
	b += d;
	b *= Const256On6; // 8bb: b = (b * 256.0f) / 6.0f

	return b;
}

static inline float IT2_Cubic16(int16_t *base, int16_t *smp, int32_t frac)
{
	float F, a, b, c, d;

	int32_t tap0Smp = -1;
	if (smp == base)
		tap0Smp = 0; // 8bb: if (SamplingPos == 0) tap0Smp = 0 (turns into branchless code)

	// 8bb: done like this to match precision loss in IT2 code (FPU is in 24-bit (32) mode)

	float s1 = smp[tap0Smp];
	float s2 = smp[0];
	float s3 = smp[1];
	float s4 = smp[2];
	float fFrac = (float)frac;

	fFrac *= 1.0f / 65536.0f;
	s2 *= 3.0f;
	s3 *= 3.0f;

	F = s4;
	F += s2;
	c = s3;
	c -= s1;
	c += c;
	c -= F;
	a = F;
	a -= s3;
	a -= s1;
	a *= fFrac;
	d = s2;
	d += d;
	b = s1;
	b *= 3.0f;
	b -= d;
	b += s3;
	b += a;
	b *= fFrac;
	b += c;
	b *= fFrac;
	b += d;
	b *= Const1On6; // 8bb: b /= 6.0f

	return b;
}

void Mix32Stereo8Bit(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample;
	float f1, f2, fSample;

	if (NumSamples & 1)
	{
		Mix32Stereo8Bit_M
	}

	NumSamples >>= 1;
	for (int32_t i = 0; i < NumSamples; i++)
	{
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
	float f1, f2, fSample;

	if (NumSamples & 1)
	{
		Mix32Stereo16Bit_M
	}

	NumSamples >>= 1;
	for (int32_t i = 0; i < NumSamples; i++)
	{
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
	float f1, f2, fSample;

	if (NumSamples & 1)
	{
		Mix32Surround8Bit_M
	}

	NumSamples >>= 1;
	for (int32_t i = 0; i < NumSamples; i++)
	{
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
	float f1, f2, fSample;

	if (NumSamples & 1)
	{
		Mix32Surround16Bit_M
	}

	NumSamples >>= 1;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix32Surround16Bit_M
		Mix32Surround16Bit_M
	}

	LastRightValue = -LastLeftValue;

	sc->SampleOffset = (int32_t)(smp - base);
}
