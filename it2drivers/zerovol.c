/*
** 8bb:
** ---- Code for "zero-vol" update routines ----
**
** These are used when the final volume is zero, and they'll only update
** the sampling position instead of doing actual mixing. They are the same
** for SB16/"SB16 MMX"/"WAV writer".
*/

#include <assert.h>
#include <stdint.h>
#include "../cpu.h"
#include "../it_structs.h"
#include "../it_music.h"

void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples)
{
	assert(numSamples <= UINT16_MAX);
	uint32_t IntSamples = (sc->Delta32 >> MIX_FRAC_BITS) * numSamples;
	uint32_t FracSamples = (sc->Delta32 & MIX_FRAC_MASK) * numSamples;

	uint32_t SampleOffset = sc->SamplingPosition + IntSamples;
	sc->Frac32 += FracSamples;
	SampleOffset += sc->Frac32 >> MIX_FRAC_BITS;
	sc->Frac32 &= MIX_FRAC_MASK;

	if (SampleOffset >= (uint32_t)sc->LoopEnd)
	{
		sc->Flags = SF_NOTE_STOP;
		if (!(sc->HostChnNum & CHN_DISOWNED))
		{
			((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off
			return;
		}
	}

	sc->SamplingPosition = SampleOffset;
}

void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples)
{
	assert(numSamples <= UINT16_MAX);
	uint32_t IntSamples = (sc->Delta32 >> MIX_FRAC_BITS) * numSamples;
	uint32_t FracSamples = (sc->Delta32 & MIX_FRAC_MASK) * numSamples;

	sc->Frac32 += FracSamples;
	sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
	sc->SamplingPosition += IntSamples;
	sc->Frac32 &= MIX_FRAC_MASK;

	if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd) // Reset position...
	{
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
		if (LoopLength == 0)
			sc->SamplingPosition = 0;
		else
			sc->SamplingPosition = sc->LoopBegin + ((sc->SamplingPosition - sc->LoopEnd) % LoopLength);
	}
}

void UpdatePingPongLoop(slaveChn_t *sc, uint32_t numSamples)
{
	assert(numSamples <= UINT16_MAX);
	uint32_t IntSamples = (sc->Delta32 >> MIX_FRAC_BITS) * numSamples;
	uint32_t FracSamples = (sc->Delta32 & MIX_FRAC_MASK) * numSamples;

	const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

	if (sc->LoopDirection == DIR_BACKWARDS)
	{
		sc->Frac32 -= FracSamples;
		sc->SamplingPosition -= sc->Frac32 >> MIX_FRAC_BITS;
		sc->SamplingPosition -= IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;

		if (sc->SamplingPosition <= sc->LoopBegin)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) + (LoopLength - NewLoopPos);

				if (sc->SamplingPosition <= sc->LoopBegin) // 8bb: non-IT2 edge-case safety for extremely high pitches
					sc->SamplingPosition = sc->LoopBegin + 1;
			}
			else
			{
				sc->LoopDirection = DIR_FORWARDS;
				sc->SamplingPosition = sc->LoopBegin + NewLoopPos;
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);
			}
		}
	}
	else // 8bb: forwards
	{
		sc->Frac32 += FracSamples;
		sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
		sc->SamplingPosition += IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;

		if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
			}
			else
			{
				sc->LoopDirection = DIR_BACKWARDS;
				sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);

				if (sc->SamplingPosition <= sc->LoopBegin) // 8bb: non-IT2 edge-case safety for extremely high pitches
					sc->SamplingPosition = sc->LoopBegin + 1;
			}
		}
	}
}

// zero-vol routines for HQ driver

void UpdateNoLoopHQ(slaveChn_t *sc, uint32_t numSamples)
{
	uint32_t IntSamples, SampleOffset;
	uintCPUWord_t FracSamples;
	assert(numSamples <= UINT16_MAX);
#if CPU_32BIT
	IntSamples = (sc->Delta32 >> MIX_FRAC_BITS) * numSamples;
	FracSamples = (sc->Delta32 & MIX_FRAC_MASK) * numSamples;

	SampleOffset = sc->SamplingPosition + IntSamples;
	sc->Frac32 += FracSamples;
	SampleOffset += sc->Frac32 >> MIX_FRAC_BITS;
	sc->Frac32 &= MIX_FRAC_MASK;
#else
	IntSamples = (uint32_t)(sc->Delta64 >> 32) * numSamples;
	FracSamples = (uint64_t)(sc->Delta64 & UINT32_MAX) * numSamples;

	SampleOffset = sc->SamplingPosition + IntSamples;
	sc->Frac64 += FracSamples;
	SampleOffset += sc->Frac64 >> 32;
	sc->Frac64 &= UINT32_MAX;
#endif

	if (SampleOffset >= (uint32_t)sc->LoopEnd)
	{
		sc->Flags = SF_NOTE_STOP;
		if (!(sc->HostChnNum & CHN_DISOWNED))
		{
			((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off
			return;
		}
	}

	sc->SamplingPosition = SampleOffset;
}

void UpdateForwardsLoopHQ(slaveChn_t *sc, uint32_t numSamples)
{
	uint32_t IntSamples;
	uintCPUWord_t FracSamples;
	assert(numSamples <= UINT16_MAX);
#if CPU_32BIT
	IntSamples = (sc->Delta32 >> MIX_FRAC_BITS) * numSamples;
	FracSamples = (sc->Delta32 & MIX_FRAC_MASK) * numSamples;

	sc->Frac32 += FracSamples;
	sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
	sc->SamplingPosition += IntSamples;
	sc->Frac32 &= MIX_FRAC_MASK;
#else
	IntSamples = (uint32_t)(sc->Delta64 >> 32) * numSamples;
	FracSamples = (uint64_t)(sc->Delta64 & UINT32_MAX) * numSamples;

	sc->Frac64 += FracSamples;
	sc->SamplingPosition += sc->Frac64 >> 32;
	sc->SamplingPosition += IntSamples;
	sc->Frac64 &= UINT32_MAX;
#endif

	if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd) // Reset position...
	{
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
		if (LoopLength == 0)
			sc->SamplingPosition = 0;
		else
			sc->SamplingPosition = sc->LoopBegin + ((sc->SamplingPosition - sc->LoopEnd) % LoopLength);
	}
}

void UpdatePingPongLoopHQ(slaveChn_t *sc, uint32_t numSamples)
{
	uint32_t IntSamples;
	uintCPUWord_t FracSamples;
	assert(numSamples <= UINT16_MAX);
#if CPU_32BIT
	IntSamples = (sc->Delta32 >> MIX_FRAC_BITS) * numSamples;
	FracSamples = (sc->Delta32 & MIX_FRAC_MASK) * numSamples;
#else
	IntSamples = (uint32_t)(sc->Delta64 >> 32) * numSamples;
	FracSamples = (uint64_t)(sc->Delta64 & UINT32_MAX) * numSamples;
#endif

	const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

	if (sc->LoopDirection == DIR_BACKWARDS)
	{
#if CPU_32BIT
		sc->Frac32 -= FracSamples;
		sc->SamplingPosition -= sc->Frac32 >> MIX_FRAC_BITS;
		sc->SamplingPosition -= IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;
#else
		sc->Frac64 -= FracSamples;
		sc->SamplingPosition -= sc->Frac64 >> 32;
		sc->SamplingPosition -= IntSamples;
		sc->Frac64 &= UINT32_MAX;
#endif

		if (sc->SamplingPosition <= sc->LoopBegin)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) + (LoopLength - NewLoopPos);

				if (sc->SamplingPosition <= sc->LoopBegin) // 8bb: non-IT2 edge-case safety for extremely high pitches
					sc->SamplingPosition = sc->LoopBegin + 1;
			}
			else
			{
				sc->LoopDirection = DIR_FORWARDS;
				sc->SamplingPosition = sc->LoopBegin + NewLoopPos;

#if CPU_32BIT
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);
#else
				sc->Frac64 = (uint32_t)(0 - sc->Frac64);
#endif
			}
		}
	}
	else // 8bb: forwards
	{
#if CPU_32BIT
		sc->Frac32 += FracSamples;
		sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
		sc->SamplingPosition += IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;
#else
		sc->Frac64 += FracSamples;
		sc->SamplingPosition += sc->Frac64 >> 32;
		sc->SamplingPosition += IntSamples;
		sc->Frac64 &= UINT32_MAX;
#endif

		if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
			}
			else
			{
				sc->LoopDirection = DIR_BACKWARDS;
				sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;

#if CPU_32BIT
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);
#else
				sc->Frac64 = (uint32_t)(0 - sc->Frac64);
#endif
				if (sc->SamplingPosition <= sc->LoopBegin) // 8bb: non-IT2 edge-case safety for extremely high pitches
					sc->SamplingPosition = sc->LoopBegin + 1;
			}
		}
	}
}
