/*
** ---- SB16 IT2 driver (mixing code) ----
*/

#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"
#include "../it_music.h"
#include "sb16_m.h"

#define Get32BitI8Waveform /* 8bb: "12 Bit Interpolated" */ \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample2 -= sample; \
	sample2 *= (int32_t)sc->SmpError; \
	sample2 >>= MIX_FRAC_BITS; \
	sample += sample2;

#define Get32BitI16Waveform /* 8bb: "12 Bit Interpolated" */ \
	sample = smp[0] >> 8; \
	sample2 = smp[1] >> 8; \
	sample2 -= sample; \
	sample2 *= (int32_t)sc->SmpError; \
	sample2 >>= MIX_FRAC_BITS; \
	sample += sample2;

#define Get32Bit8Waveform /* 8bb: "32Bit Interpolated" */ \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample2 -= sample; \
	sample2 *= (int32_t)sc->SmpError; \
	sample2 >>= MIX_FRAC_BITS-8; \
	sample <<= 8; \
	sample += sample2;

#define Get32Bit16Waveform /* 8bb: "32Bit Interpolated" */ \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample2 -= sample; \
	sample2 >>= 1; \
	sample2 *= (int32_t)sc->SmpError; \
	sample2 >>= MIX_FRAC_BITS-1; \
	sample += sample2;

#define UpdatePos \
	sc->SmpError += Delta; \
	smp += (int32_t)sc->SmpError >> MIX_FRAC_BITS; \
	sc->SmpError &= MIX_FRAC_MASK;

#define M12Mix8_M \
	sample = *smp; \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] -= MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix16_M \
	sample = (*smp) >> 8; \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] -= MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix8S_M \
	sample = *smp; \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] += MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix16S_M \
	sample = (*smp) >> 8; \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] += MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix8I_M \
	Get32BitI8Waveform \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] -= MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix16I_M \
	Get32BitI16Waveform \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] -= MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix8IS_M \
	Get32BitI8Waveform \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] += MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M12Mix16IS_M \
	Get32BitI16Waveform \
	MixBufPtr16[0] -= MixSegment[LeftVolume16 + (uint8_t)sample]; \
	MixBufPtr16[2] += MixSegment[RightVolume16 + (uint8_t)sample]; \
	MixBufPtr16 += 4; \
	UpdatePos

#define M32Mix8_M \
	sample = *smp << 8; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16_M \
	sample = *smp; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix8S_M \
	sample = *smp << 8; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16S_M \
	sample = *smp; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Mix8I_M \
	Get32Bit8Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16I_M \
	Get32Bit16Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix8IS_M \
	Get32Bit8Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16IS_M \
	Get32Bit16Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

const mixFunc SB16_MixFunctionTables[16] =
{
	(mixFunc)M12Mix8,
	(mixFunc)M12Mix16,
	(mixFunc)M12Mix8S,
	(mixFunc)M12Mix16S,
	(mixFunc)M12Mix8I,
	(mixFunc)M12Mix16I,
	(mixFunc)M12Mix8IS,
	(mixFunc)M12Mix16IS,
	(mixFunc)M32Mix8,
	(mixFunc)M32Mix16,
	(mixFunc)M32Mix8S,
	(mixFunc)M32Mix16S,
	(mixFunc)M32Mix8I,
	(mixFunc)M32Mix16I,
	(mixFunc)M32Mix8IS,
	(mixFunc)M32Mix16IS
};

// 8bb: globalized
int16_t MixSegment[MIXTABLESIZE]; // 8bb: volume LUT for 16-bit mix mode
int32_t Delta;
uint32_t LeftVolume16, RightVolume16;
// ------------------

void M12Mix8(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix8_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix8_M
		M12Mix8_M
		M12Mix8_M
		M12Mix8_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix16(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix16_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix16_M
		M12Mix16_M
		M12Mix16_M
		M12Mix16_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix8S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix8S_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix8S_M
		M12Mix8S_M
		M12Mix8S_M
		M12Mix8S_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix16S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix16S_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix16S_M
		M12Mix16S_M
		M12Mix16S_M
		M12Mix16S_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix8I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix8I_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix8I_M
		M12Mix8I_M
		M12Mix8I_M
		M12Mix8I_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix16I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix16I_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix16I_M
		M12Mix16I_M
		M12Mix16I_M
		M12Mix16I_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix8IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix8IS_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix8IS_M
		M12Mix8IS_M
		M12Mix8IS_M
		M12Mix8IS_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M12Mix16IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;
	int16_t *MixBufPtr16 = (int16_t *)MixBufPtr + 1;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M12Mix16IS_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M12Mix16IS_M
		M12Mix16IS_M
		M12Mix16IS_M
		M12Mix16IS_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix8(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8_M
		M32Mix8_M
		M32Mix8_M
		M32Mix8_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix16(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16_M
		M32Mix16_M
		M32Mix16_M
		M32Mix16_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix8S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8S_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8S_M
		M32Mix8S_M
		M32Mix8S_M
		M32Mix8S_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix16S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16S_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16S_M
		M32Mix16S_M
		M32Mix16S_M
		M32Mix16S_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix8I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8I_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8I_M
		M32Mix8I_M
		M32Mix8I_M
		M32Mix8I_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix16I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16I_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16I_M
		M32Mix16I_M
		M32Mix16I_M
		M32Mix16I_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix8IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpOffs->Data;
	const int8_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8IS_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8IS_M
		M32Mix8IS_M
		M32Mix8IS_M
		M32Mix8IS_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}

void M32Mix16IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpOffs->Data;
	const int16_t *smp = base + sc->SampleOffset;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16IS_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16IS_M
		M32Mix16IS_M
		M32Mix16IS_M
		M32Mix16IS_M
	}

	sc->SampleOffset = (int32_t)(smp - base);
}
