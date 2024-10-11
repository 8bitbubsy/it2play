#include <stdint.h>
#include <stdbool.h>
#include "../cpu.h"
#include "../it_structs.h"
#include "../it_music.h"
#include "hq.h"
#include "hq_m.h"

static void Mix8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void Mix16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixSurround8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixSurround16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void Mix8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void Mix16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixSurround8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixSurround16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFiltered8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFiltered16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFilteredSurround8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFilteredSurround16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFiltered8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFiltered16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFilteredSurround8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);
static void MixFilteredSurround16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t numSamples);

#define CubicSplineInterpolation(out, s, t, scale) \
	out = ((s[-1] * t[0]) + (s[0] * t[1]) + (s[1] * t[2]) + (s[2] * t[3])) * (1.0f / scale);

#define FilterSample \
	fSample = (fSample * sc->fFiltera) + (sc->fOldSamples[0] * sc->fFilterb) + (sc->fOldSamples[1] * sc->fFilterc); \
	\
	/* same filter clipping result as the SB16 MMX driver */ \
	     if (fSample < -2.0f) fSample = -2.0f; \
	else if (fSample >  2.0f) fSample =  2.0f; \
	\
	sc->fOldSamples[1] = sc->fOldSamples[0]; \
	sc->fOldSamples[0] = fSample;

#define FilterStereoSample \
	fSample  = (fSample  * sc->fFiltera) + (sc->fOldSamples[0] * sc->fFilterb) + (sc->fOldSamples[1] * sc->fFilterc); \
	fSampleR = (fSampleR * sc->fFiltera) + (sc->fOldSamples[2] * sc->fFilterb) + (sc->fOldSamples[3] * sc->fFilterc); \
	\
	     if (fSample  < -2.0f) fSample  = -2.0f; \
	else if (fSample  >  2.0f) fSample  =  2.0f; \
	     if (fSampleR < -2.0f) fSampleR = -2.0f; \
	else if (fSampleR >  2.0f) fSampleR =  2.0f; \
	\
	sc->fOldSamples[1] = sc->fOldSamples[0]; \
	sc->fOldSamples[0] = fSample; \
	sc->fOldSamples[3] = sc->fOldSamples[2]; \
	sc->fOldSamples[2] = fSampleR;

#if CPU_32BIT

#define Get8BitWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint16_t)sc->Frac32 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample, smp, t, 128.0f); \

#define Get16BitWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint16_t)sc->Frac32 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample, smp, t, 32768.0f); \

#define Get8BitStereoWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint16_t)sc->Frac32 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample,  smp,  t, 128.0f); \
	CubicSplineInterpolation(fSampleR, smpR, t, 128.0f);

#define Get16BitStereoWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint16_t)sc->Frac32 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample,  smp,  t, 32768.0f); \
	CubicSplineInterpolation(fSampleR, smpR, t, 32768.0f);

#else

#define Get8BitWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint32_t)sc->Frac64 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample, smp, t, 128.0f); \

#define Get16BitWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint32_t)sc->Frac64 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample, smp, t, 32768.0f); \

#define Get8BitStereoWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint32_t)sc->Frac64 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample,  smp,  t, 128.0f); \
	CubicSplineInterpolation(fSampleR, smpR, t, 128.0f);

#define Get16BitStereoWaveForm \
	t = (float *)Driver.fCubicLUT + (((uint32_t)sc->Frac64 >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	CubicSplineInterpolation(fSample,  smp,  t, 32768.0f); \
	CubicSplineInterpolation(fSampleR, smpR, t, 32768.0f);

#endif

#define GetFiltered8BitWaveForm \
	Get8BitWaveForm \
	FilterSample

#define GetFiltered16BitWaveForm \
	Get16BitWaveForm \
	FilterSample

#define GetFiltered8BitStereoWaveForm \
	Get8BitStereoWaveForm \
	FilterStereoSample

#define GetFiltered16BitStereoWaveForm \
	Get16BitStereoWaveForm \
	FilterStereoSample

#define MixSample \
	Driver.fLastLeftValue  = fSample * sc->fCurrVolL; \
	Driver.fLastRightValue = fSample * sc->fCurrVolR; \
	fMixBufPtr[0] += Driver.fLastLeftValue; \
	fMixBufPtr[1] += Driver.fLastRightValue; \
	fMixBufPtr += 2; \

#define MixSampleSurround \
	Driver.fLastLeftValue = fSample * sc->fCurrVolL; \
	fMixBufPtr[0] += Driver.fLastLeftValue; \
	fMixBufPtr[1] -= Driver.fLastLeftValue; \
	fMixBufPtr += 2; \

#define MixStereoSample \
	Driver.fLastLeftValue  = fSample  * sc->fCurrVolL; \
	Driver.fLastRightValue = fSampleR * sc->fCurrVolR; \
	fMixBufPtr[0] += Driver.fLastLeftValue; \
	fMixBufPtr[1] += Driver.fLastRightValue; \
	fMixBufPtr += 2; \

#define MixStereoSampleSurround \
	Driver.fLastLeftValue  = fSample  * sc->fCurrVolL; \
	Driver.fLastRightValue = fSampleR * sc->fCurrVolL; \
	fMixBufPtr[0] += Driver.fLastLeftValue; \
	fMixBufPtr[1] -= Driver.fLastRightValue; \
	fMixBufPtr += 2; \

#define RampCurrVolumeL \
	sc->fCurrVolL += (sc->fLeftVolume - sc->fCurrVolL) * ((1 << RAMPSPEED) / 16384.0f);

#define RampCurrVolumeR \
	sc->fCurrVolR += (sc->fRightVolume - sc->fCurrVolR) * ((1 << RAMPSPEED) / 16384.0f);

#if CPU_32BIT

#define UpdatePos \
	sc->Frac32 += Driver.Delta32; \
	smp += (int32_t)sc->Frac32 >> 16; \
	sc->Frac32 &= UINT16_MAX;

#define UpdatePosStereo \
	sc->Frac32 += Driver.Delta32; \
	WholeSamples = (int32_t)sc->Frac32 >> 16; \
	smp += WholeSamples; \
	smpR += WholeSamples; \
	sc->Frac32 &= UINT16_MAX;

#else

#define UpdatePos \
	sc->Frac64 += Driver.Delta64; \
	smp += (int64_t)sc->Frac64 >> 32; \
	sc->Frac64 &= UINT32_MAX;

#define UpdatePosStereo \
	sc->Frac64 += Driver.Delta64; \
	WholeSamples = (int64_t)sc->Frac64 >> 32; \
	smp += WholeSamples; \
	smpR += WholeSamples; \
	sc->Frac64 &= UINT32_MAX;

#endif

#define Mix8Bit_M \
	Get8BitWaveForm \
	MixSample \
	UpdatePos \
	RampCurrVolumeL \
	RampCurrVolumeR

#define Mix16Bit_M \
	Get16BitWaveForm \
	MixSample \
	UpdatePos \
	RampCurrVolumeL \
	RampCurrVolumeR

#define MixSurround8Bit_M \
	Get8BitWaveForm \
	MixSampleSurround \
	UpdatePos \
	RampCurrVolumeL

#define MixSurround16Bit_M \
	Get16BitWaveForm \
	MixSampleSurround \
	UpdatePos \
	RampCurrVolumeL

#define Mix8BitStereo_M \
	Get8BitStereoWaveForm \
	MixStereoSample \
	UpdatePosStereo \
	RampCurrVolumeL \
	RampCurrVolumeR

#define Mix16BitStereo_M \
	Get16BitStereoWaveForm \
	MixStereoSample \
	UpdatePosStereo \
	RampCurrVolumeL \
	RampCurrVolumeR

#define MixSurround8BitStereo_M \
	Get8BitStereoWaveForm \
	MixStereoSampleSurround \
	UpdatePosStereo \
	RampCurrVolumeL

#define MixSurround16BitStereo_M \
	Get16BitStereoWaveForm \
	MixStereoSampleSurround \
	UpdatePosStereo \
	RampCurrVolumeL

#define MixFiltered8Bit_M \
	GetFiltered8BitWaveForm \
	MixSample \
	UpdatePos \
	RampCurrVolumeL \
	RampCurrVolumeR

#define MixFiltered16Bit_M \
	GetFiltered16BitWaveForm \
	MixSample \
	UpdatePos \
	RampCurrVolumeL \
	RampCurrVolumeR

#define MixFilteredSurround8Bit_M \
	GetFiltered8BitWaveForm \
	MixSampleSurround \
	UpdatePos \
	RampCurrVolumeL

#define MixFilteredSurround16Bit_M \
	GetFiltered16BitWaveForm \
	MixSampleSurround \
	UpdatePos \
	RampCurrVolumeL

#define MixFiltered8BitStereo_M \
	GetFiltered8BitStereoWaveForm \
	MixStereoSample \
	UpdatePosStereo \
	RampCurrVolumeL \
	RampCurrVolumeR

#define MixFiltered16BitStereo_M \
	GetFiltered16BitStereoWaveForm \
	MixStereoSample \
	UpdatePosStereo \
	RampCurrVolumeL \
	RampCurrVolumeR

#define MixFilteredSurround8BitStereo_M \
	GetFiltered8BitStereoWaveForm \
	MixStereoSampleSurround \
	UpdatePosStereo \
	RampCurrVolumeL

#define MixFilteredSurround16BitStereo_M \
	GetFiltered16BitStereoWaveForm \
	MixStereoSampleSurround \
	UpdatePosStereo \
	RampCurrVolumeL

const MixFunc_t HQ_MixFunctionTables[16] =
{
	(MixFunc_t)Mix8Bit,
	(MixFunc_t)Mix16Bit,
	(MixFunc_t)MixSurround8Bit,
	(MixFunc_t)MixSurround16Bit,
	(MixFunc_t)Mix8BitStereo,
	(MixFunc_t)Mix16BitStereo,
	(MixFunc_t)MixSurround8BitStereo,
	(MixFunc_t)MixSurround16BitStereo,
	(MixFunc_t)MixFiltered8Bit,
	(MixFunc_t)MixFiltered16Bit,
	(MixFunc_t)MixFilteredSurround8Bit,
	(MixFunc_t)MixFilteredSurround16Bit,
	(MixFunc_t)MixFiltered8BitStereo,
	(MixFunc_t)MixFiltered16BitStereo,
	(MixFunc_t)MixFilteredSurround8BitStereo,
	(MixFunc_t)MixFilteredSurround16BitStereo
};

static void Mix8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix8Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix8Bit_M
		Mix8Bit_M
		Mix8Bit_M
		Mix8Bit_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void Mix16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix16Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix16Bit_M
		Mix16Bit_M
		Mix16Bit_M
		Mix16Bit_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixSurround8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixSurround8Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixSurround8Bit_M
		MixSurround8Bit_M
		MixSurround8Bit_M
		MixSurround8Bit_M
	}

	Driver.fLastRightValue = -Driver.fLastLeftValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixSurround16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixSurround16Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixSurround16Bit_M
		MixSurround16Bit_M
		MixSurround16Bit_M
		MixSurround16Bit_M
	}

	Driver.fLastRightValue = -Driver.fLastLeftValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void Mix8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *baseR = (int8_t *)s->DataR;
	int8_t *smp = base + sc->SamplingPosition;
	int8_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix8BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix8BitStereo_M
		Mix8BitStereo_M
		Mix8BitStereo_M
		Mix8BitStereo_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void Mix16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *baseR = (int16_t *)s->DataR;
	int16_t *smp = base + sc->SamplingPosition;
	int16_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		Mix16BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		Mix16BitStereo_M
		Mix16BitStereo_M
		Mix16BitStereo_M
		Mix16BitStereo_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixSurround8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *baseR = (int8_t *)s->DataR;
	int8_t *smp = base + sc->SamplingPosition;
	int8_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixSurround8BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixSurround8BitStereo_M
		MixSurround8BitStereo_M
		MixSurround8BitStereo_M
		MixSurround8BitStereo_M
	}

	Driver.fLastRightValue = -Driver.fLastRightValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixSurround16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *baseR = (int16_t *)s->DataR;
	int16_t *smp = base + sc->SamplingPosition;
	int16_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixSurround16BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixSurround16BitStereo_M
		MixSurround16BitStereo_M
		MixSurround16BitStereo_M
		MixSurround16BitStereo_M
	}

	Driver.fLastRightValue = -Driver.fLastRightValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFiltered8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFiltered8Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFiltered8Bit_M
		MixFiltered8Bit_M
		MixFiltered8Bit_M
		MixFiltered8Bit_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFiltered16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFiltered16Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFiltered16Bit_M
		MixFiltered16Bit_M
		MixFiltered16Bit_M
		MixFiltered16Bit_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFilteredSurround8Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFilteredSurround8Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFilteredSurround8Bit_M
		MixFilteredSurround8Bit_M
		MixFilteredSurround8Bit_M
		MixFilteredSurround8Bit_M
	}

	Driver.fLastRightValue = -Driver.fLastLeftValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFilteredSurround16Bit(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *smp = base + sc->SamplingPosition;
	float *t, fSample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFilteredSurround16Bit_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFilteredSurround16Bit_M
		MixFilteredSurround16Bit_M
		MixFilteredSurround16Bit_M
		MixFilteredSurround16Bit_M
	}

	Driver.fLastRightValue = -Driver.fLastLeftValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFiltered8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *baseR = (int8_t *)s->DataR;
	int8_t *smp = base + sc->SamplingPosition;
	int8_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFiltered8BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFiltered8BitStereo_M
		MixFiltered8BitStereo_M
		MixFiltered8BitStereo_M
		MixFiltered8BitStereo_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFiltered16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *baseR = (int16_t *)s->DataR;
	int16_t *smp = base + sc->SamplingPosition;
	int16_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFiltered16BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFiltered16BitStereo_M
		MixFiltered16BitStereo_M
		MixFiltered16BitStereo_M
		MixFiltered16BitStereo_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFilteredSurround8BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int8_t *base = (int8_t *)s->Data;
	int8_t *baseR = (int8_t *)s->DataR;
	int8_t *smp = base + sc->SamplingPosition;
	int8_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFilteredSurround8BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFilteredSurround8BitStereo_M
		MixFilteredSurround8BitStereo_M
		MixFilteredSurround8BitStereo_M
		MixFilteredSurround8BitStereo_M
	}

	Driver.fLastRightValue = -Driver.fLastRightValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}

static void MixFilteredSurround16BitStereo(slaveChn_t *sc, float *fMixBufPtr, int32_t NumSamples)
{
	sample_t *s = sc->SmpPtr;
	int16_t *base = (int16_t *)s->Data;
	int16_t *baseR = (int16_t *)s->DataR;
	int16_t *smp = base + sc->SamplingPosition;
	int16_t *smpR = baseR + sc->SamplingPosition;
	int32_t WholeSamples;
	float *t, fSample, fSampleR;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		MixFilteredSurround16BitStereo_M
	}

	NumSamples >>= 2;
	for (int32_t i = 0; i < NumSamples; i++)
	{
		MixFilteredSurround16BitStereo_M
		MixFilteredSurround16BitStereo_M
		MixFilteredSurround16BitStereo_M
		MixFilteredSurround16BitStereo_M
	}

	Driver.fLastRightValue = -Driver.fLastRightValue;

	sc->SamplingPosition = (int32_t)(smp - base);
}
