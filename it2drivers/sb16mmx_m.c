/*
** ---- SB16 MMX IT2 driver (mixing code) ----
*/

#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"
#include "../it_music.h"
#include "sb16mmx.h"
#include "sb16mmx_m.h"

#define Get8BitWaveForm \
	frac = sc->SmpError >> 1; \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample = (sample << 8) | (uint8_t)sample; \
	sample2 = (sample2 << 8) | (uint8_t)sample2; \
	sample2 *= frac; \
	frac ^= MIX_FRAC_MASK>>1; \
	sample *= frac; \
	sample = (sample + sample2) >> (MIX_FRAC_BITS-1);

#define Get16BitWaveForm \
	frac = sc->SmpError >> 1; \
	sample2 = smp[1] * frac; \
	frac ^= MIX_FRAC_MASK>>1; \
	sample = smp[0] * frac; \
	sample = (sample + sample2) >> (MIX_FRAC_BITS-1);

#define Get8BitWaveFormFiltered /* 8bb: result is 15-bit for filter to work properly */ \
	frac = sc->SmpError >> 1; \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample = (sample << 8) | (uint8_t)sample; \
	sample2 = (sample2 << 8) | (uint8_t)sample2; \
	sample2 *= frac; \
	frac ^= MIX_FRAC_MASK>>1; \
	sample *= frac; \
	sample = (sample + sample2) >> MIX_FRAC_BITS;

#define Get16BitWaveFormFiltered /* 8bb: result is 15-bit for filter to work properly */ \
	frac = sc->SmpError >> 1; \
	sample2 = smp[1] * frac; \
	frac ^= MIX_FRAC_MASK>>1; \
	sample = smp[0] * frac; \
	sample = (sample + sample2) >> MIX_FRAC_BITS;

#define FilterSample \
	sample = (int32_t)(((int64_t)(sample            * sc->filtera) + \
	                    (int64_t)(sc->OldSamples[0] * sc->filterb) + \
	                    (int64_t)(sc->OldSamples[1] * sc->filterc)) >> FILTER_BITS); \
	\
	     if (sample < INT16_MIN) sample = INT16_MIN; \
	else if (sample > INT16_MAX) sample = INT16_MAX; \
	\
	sc->OldSamples[1] = sc->OldSamples[0]; \
	sc->OldSamples[0] = sample;

#define RampCurrVolume1 \
	sc->CurrVolL += (sc->DestVolL - sc->CurrVolL) >> RAMPSPEED; \
	sc->CurrVolR += (sc->DestVolR - sc->CurrVolR) >> RAMPSPEED;

#define RampCurrVolume2 \
	sc->CurrVolL += (sc->DestVolL - sc->CurrVolL) >> (RAMPSPEED-1); \
	sc->CurrVolR += (sc->DestVolR - sc->CurrVolR) >> (RAMPSPEED-1);

#define UpdatePos \
	sc->SmpError += Driver.Delta; \
	smp += (int32_t)sc->SmpError >> MIX_FRAC_BITS; \
	sc->SmpError &= MIX_FRAC_MASK;

#define M32Bit8M_M \
	sample = (*smp) << 8; \
	(*MixBufPtr++) += sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Bit16M_M \
	sample = *smp; \
	(*MixBufPtr++) += sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Bit8MI_M \
	Get8BitWaveForm \
	(*MixBufPtr++) += sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Bit16MI_M \
	Get16BitWaveForm \
	(*MixBufPtr++) += sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

// 8bb: ramped mixers need to be done slightly differently for the ramping to be sample-accurate

#define M32Bit8MV_M1 \
	Get8BitWaveForm \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	RampCurrVolume1 \
	UpdatePos

#define M32Bit8MV_M2 \
	Get8BitWaveForm \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	Get8BitWaveForm \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	RampCurrVolume2

#define M32Bit16MV_M1 \
	Get16BitWaveForm \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	RampCurrVolume1 \
	UpdatePos

#define M32Bit16MV_M2 \
	Get16BitWaveForm \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	Get16BitWaveForm \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	RampCurrVolume2

#define M32Bit8MF_M1 \
	Get8BitWaveFormFiltered \
	FilterSample \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	RampCurrVolume1 \
	UpdatePos

#define M32Bit8MF_M2 \
	Get8BitWaveFormFiltered \
	FilterSample \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	Get8BitWaveFormFiltered \
	FilterSample \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	RampCurrVolume2

#define M32Bit16MF_M1 \
	Get16BitWaveFormFiltered \
	FilterSample \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	RampCurrVolume1 \
	UpdatePos

#define M32Bit16MF_M2 \
	Get16BitWaveFormFiltered \
	FilterSample \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	Get16BitWaveFormFiltered \
	FilterSample \
	(*MixBufPtr++) += sample * sc->CurrVolL; \
	(*MixBufPtr++) += sample * sc->CurrVolR; \
	UpdatePos \
	RampCurrVolume2

const mixFunc SB16MMX_MixFunctionTables[8] =
{
	(mixFunc)M32Bit8M,
	(mixFunc)M32Bit16M,
	(mixFunc)M32Bit8MI,
	(mixFunc)M32Bit16MI,
	(mixFunc)M32Bit8MV,
	(mixFunc)M32Bit16MV,
	(mixFunc)M32Bit8MF,
	(mixFunc)M32Bit16MF
};

void M32Bit8M(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Bit8M_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit8M_M
		M32Bit8M_M
		M32Bit8M_M
		M32Bit8M_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Bit16M(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SampleOffset;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Bit16M_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit16M_M
		M32Bit16M_M
		M32Bit16M_M
		M32Bit16M_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Bit8MI(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2, frac;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Bit8MI_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit8MI_M
		M32Bit8MI_M
		M32Bit8MI_M
		M32Bit8MI_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Bit16MI(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2, frac;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Bit16MI_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit16MI_M
		M32Bit16MI_M
		M32Bit16MI_M
		M32Bit16MI_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

// 8bb: ramped mixers need to be done slightly differently for the ramping to be sample-accurate

void M32Bit8MV(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2, frac;

	if (NumSamples & 1)
	{
		M32Bit8MV_M1
	}

	if (NumSamples & 2)
	{
		M32Bit8MV_M2
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit8MV_M2
		M32Bit8MV_M2
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Bit16MV(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2, frac;

	if (NumSamples & 1)
	{
		M32Bit16MV_M1
	}

	if (NumSamples & 2)
	{
		M32Bit16MV_M2
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit16MV_M2
		M32Bit16MV_M2
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Bit8MF(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2, frac;

	if (NumSamples & 1)
	{
		M32Bit8MF_M1
	}

	if (NumSamples & 2)
	{
		M32Bit8MF_M2
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit8MF_M2
		M32Bit8MF_M2
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Bit16MF(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpOffs;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2, frac;

	if (NumSamples & 1)
	{
		M32Bit16MF_M1
	}

	if (NumSamples & 2)
	{
		M32Bit16MF_M2
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Bit16MF_M2
		M32Bit16MF_M2
	}

	sc->SampleOffset = (int32_t)(smp - base);
}
