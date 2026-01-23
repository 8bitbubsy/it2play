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
#include "../it_structs.h"
#include "../it_music.h"

void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples)
{
	uint32_t SamplingPosition = sc->SamplingPosition;

	const uint64_t Delta = (uint64_t)sc->Delta32 * numSamples;
	const uint32_t IntSamples = (uint32_t)(Delta >> MIX_FRAC_BITS);
	const uint16_t FracSamples = Delta & MIX_FRAC_MASK;

	sc->Frac32 += FracSamples;
	SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
	sc->Frac32 &= MIX_FRAC_MASK;
	SamplingPosition += IntSamples;

	if (SamplingPosition >= (uint32_t)sc->LoopEnd)
	{
		sc->Flags = SF_NOTE_STOP;
		if (!(sc->HostChnNum & CHN_DISOWNED))
		{
			((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off
			return;
		}
	}

	sc->SamplingPosition = SamplingPosition;
}

void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t Delta = (uint64_t)sc->Delta32 * numSamples;
	const uint32_t IntSamples = (uint32_t)(Delta >> MIX_FRAC_BITS);
	const uint16_t FracSamples = Delta & MIX_FRAC_MASK;

	sc->Frac32 += FracSamples;
	sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
	sc->Frac32 &= MIX_FRAC_MASK;
	sc->SamplingPosition += IntSamples;

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

	const uint64_t Delta = (uint64_t)sc->Delta32 * numSamples;
	const uint32_t IntSamples = (uint32_t)(Delta >> MIX_FRAC_BITS);
	const uint16_t FracSamples = Delta & MIX_FRAC_MASK;

	const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

	if (sc->LoopDirection == DIR_BACKWARDS)
	{
		sc->Frac32 -= FracSamples;
		sc->SamplingPosition += (int32_t)sc->Frac32 >> MIX_FRAC_BITS;
		sc->SamplingPosition -= IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;

		if (sc->SamplingPosition <= sc->LoopBegin)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);
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
				sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
				sc->LoopDirection = DIR_BACKWARDS;
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);
			}
		}
	}
}

// zero-vol routines for HQ driver

void UpdateNoLoopHQ(slaveChn_t *sc, uint32_t numSamples)
{
	uint32_t SamplingPosition = sc->SamplingPosition;

	const uint64_t Delta = (uint64_t)sc->Delta64 * numSamples;
	const uint32_t IntSamples = (uint32_t)(Delta >> 32);
	const uint32_t FracSamples = Delta & UINT32_MAX;

	sc->Frac64 += FracSamples;
	SamplingPosition += sc->Frac64 >> 32;
	SamplingPosition += IntSamples;
	sc->Frac64 &= UINT32_MAX;

	if (SamplingPosition >= (uint32_t)sc->LoopEnd)
	{
		sc->Flags = SF_NOTE_STOP;
		if (!(sc->HostChnNum & CHN_DISOWNED))
		{
			((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off
			return;
		}
	}

	sc->SamplingPosition = SamplingPosition;
}

void UpdateForwardsLoopHQ(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t Delta = (uint64_t)sc->Delta64 * numSamples;
	const uint32_t IntSamples = Delta >> 32;
	const uint32_t FracSamples = Delta & UINT32_MAX;

	sc->Frac64 += FracSamples;
	sc->SamplingPosition += sc->Frac64 >> 32;
	sc->SamplingPosition += IntSamples;
	sc->Frac64 &= UINT32_MAX;

	if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
	{
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
		if (LoopLength == 0)
			sc->SamplingPosition = 0;
		else
			sc->SamplingPosition = sc->LoopBegin + ((sc->SamplingPosition - sc->LoopEnd) % LoopLength);

		sc->HasLooped = true;
	}
}

void UpdatePingPongLoopHQ(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t Delta = (uint64_t)sc->Delta64 * numSamples;
	const uint32_t IntSamples = Delta >> 32;
	const uint32_t FracSamples = Delta & UINT32_MAX;

	const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

	if (sc->LoopDirection == DIR_BACKWARDS)
	{
		sc->Frac64 -= FracSamples;
		sc->SamplingPosition += (int32_t)(sc->Frac64 >> 32);
		sc->SamplingPosition -= IntSamples;
		sc->Frac64 &= UINT32_MAX;

		if (sc->SamplingPosition <= sc->LoopBegin)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);

				if (sc->SamplingPosition == sc->LoopBegin)
				{
					sc->LoopDirection = DIR_FORWARDS;
					sc->Frac64 = (uint32_t)(0 - sc->Frac64);
				}
			}
			else
			{
				sc->LoopDirection = DIR_FORWARDS;
				sc->SamplingPosition = sc->LoopBegin + NewLoopPos;
				sc->Frac64 = (uint32_t)(0 - sc->Frac64);
			}

			sc->HasLooped = true;
		}
	}
	else // 8bb: forwards
	{
		sc->Frac64 += FracSamples;
		sc->SamplingPosition += (int32_t)(sc->Frac64 >> 32);
		sc->SamplingPosition += IntSamples;
		sc->Frac64 &= UINT32_MAX;

		if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
			}
			else
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
				if (sc->SamplingPosition != sc->LoopBegin)
				{
					sc->LoopDirection = DIR_BACKWARDS;
					sc->Frac64 = (uint32_t)(0 - sc->Frac64);
				}
			}

			sc->HasLooped = true;
		}
	}
}
