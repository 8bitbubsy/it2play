/*
** ---- SB16 IT2 driver ----
*/

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../it_structs.h"
#include "../it_music.h" // Update()
#include "sb16_m.h"
#include "zerovol.h"

static uint16_t MixVolume;
static int32_t *MixBuffer, MixTransferRemaining, MixTransferOffset;

void SB16_MixSamples(void)
{
	MixTransferOffset = 0;

	memset(MixBuffer, 0, Driver.BytesToMix * 2 * sizeof (int32_t));

	slaveChn_t *sc = sChn;
	for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
			continue;

		if (sc->Flags & SF_NOTE_STOP)
		{
			sc->Flags &= ~SF_CHAN_ON;
			continue;
		}

		if (sc->Flags & SF_FREQ_CHANGE)
		{
			if ((uint32_t)sc->Frequency>>MIX_FRAC_BITS >= Driver.MixSpeed)
			{
				sc->Flags = SF_NOTE_STOP;
				if (!(sc->HCN & CHN_DISOWNED))
					((hostChn_t *)sc->HCOffst)->Flags &= ~HF_CHAN_ON; // Turn off channel

				continue;
			}

			// 8bb: calculate mixer delta
			uint32_t Quotient = (uint32_t)sc->Frequency / Driver.MixSpeed;
			uint32_t Remainder = (uint32_t)sc->Frequency % Driver.MixSpeed;
			sc->Delta = (Quotient << MIX_FRAC_BITS) | (uint16_t)((Remainder << MIX_FRAC_BITS) / Driver.MixSpeed);
		}

		if (sc->Flags & SF_NEW_NOTE) // 8bb: This is not really needed. We have no volume ramp in the SB16 driver!
			sc->OldLeftVolume = sc->OldRightVolume = 0; // Current Volume = 0 for volume sliding.

		if (sc->Flags & (SF_RECALC_FINALVOL | SF_LOOP_CHANGED | SF_PAN_CHANGED))
		{
			if (!(sc->Flags & SF_CHN_MUTED))
			{
				if (Driver.MixMode < 2) // 8bb: 16-bit mixer (aka. "12 Bit")
				{
					if (!(Song.Header.Flags & ITF_STEREO)) // 8bb: mono?
					{
						sc->LeftVolume = sc->RightVolume = sc->FV >> 1; // 8bb: 0..64
					}
					else if (sc->FPP == PAN_SURROUND)
					{
						sc->LeftVolume = sc->RightVolume = sc->FV >> 2; // 8bb: 0..32
					}
					else // 8bb: normal (panned)
					{
						sc->LeftVolume  = (((64-sc->FPP) * sc->FV) + 64) >> 7; // 8bb: 0..64
						sc->RightVolume = ((    sc->FPP  * sc->FV) + 64) >> 7;
					}
				}
				else // 8bb: 32-bit mixer
				{
					if (!(Song.Header.Flags & ITF_STEREO)) // 8bb: mono?
					{
						sc->LeftVolume = sc->RightVolume = (sc->vol16Bit * MixVolume) >> 8; // 8bb: 0..16384
					}
					else if (sc->FPP == PAN_SURROUND)
					{
						sc->LeftVolume = sc->RightVolume = (sc->vol16Bit * MixVolume) >> 9; // 8bb: 0..8192
					}
					else // 8bb: normal (panned)
					{
						sc->LeftVolume  = ((64-sc->FPP) * MixVolume * sc->vol16Bit) >> 14; // 8bb: 0..16384
						sc->RightVolume = (    sc->FPP  * MixVolume * sc->vol16Bit) >> 14;
					}
				}
			}
		}

		sc->OldSampleOffset = sc->SampleOffset;

		if (sc->Delta == 0) // 8bb: added this protection just in case (shouldn't happen)
			continue;

		uint32_t MixBlockSize = Driver.BytesToMix;
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBeg; // 8bb: also length for non-loopers

		if ((sc->Flags & SF_CHN_MUTED) || (sc->LeftVolume == 0 && sc->RightVolume == 0))
		{
			if ((int32_t)LoopLength > 0)
			{
				if (sc->LpM == LOOP_PINGPONG)
					UpdatePingPongLoop(sc, MixBlockSize);
				else if (sc->LpM == LOOP_FORWARDS)
					UpdateForwardsLoop(sc, MixBlockSize);
				else
					UpdateNoLoop(sc, MixBlockSize);
			}

			sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
			               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
			               SF_LOOP_CHANGED    | SF_PAN_CHANGED);

			continue;
		}

		const bool Surround = (sc->FPP == PAN_SURROUND);
		const bool Sample16it = !!(sc->Bit & SMPF_16BIT);
		const mixFunc Mix = SB16_MixFunctionTables[(Driver.MixMode << 2) + (Surround << 1) + Sample16it];
		int32_t *MixBufferPtr = MixBuffer;

		if (Driver.MixMode < 2) // 8bb: 16-bit mixers used?
		{
			LeftVolume16 = (uint16_t)sc->LeftVolume << 8;
			RightVolume16 = (uint16_t)sc->RightVolume << 8;
		}

		if ((int32_t)LoopLength > 0)
		{
			if (sc->LpM == LOOP_PINGPONG)
			{
				while (MixBlockSize > 0)
				{
					uint32_t NewLoopPos;
					if (sc->LpD == DIR_BACKWARDS)
					{
						if (sc->SampleOffset <= sc->LoopBeg)
						{
							NewLoopPos = (uint32_t)(sc->LoopBeg - sc->SampleOffset) % (LoopLength << 1);
							if (NewLoopPos >= LoopLength)
							{
								sc->SampleOffset = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);
							}
							else
							{
								sc->LpD = DIR_FORWARDS;
								sc->SampleOffset = sc->LoopBeg + NewLoopPos;
								sc->SmpError = (uint16_t)(0 - sc->SmpError);
							}
						}
					}
					else // 8bb: forwards
					{
						if ((uint32_t)sc->SampleOffset >= (uint32_t)sc->LoopEnd)
						{
							NewLoopPos = (uint32_t)(sc->SampleOffset - sc->LoopEnd) % (LoopLength << 1);
							if (NewLoopPos >= LoopLength)
							{
								sc->SampleOffset = sc->LoopBeg + (NewLoopPos - LoopLength);
							}
							else
							{
								sc->LpD = DIR_BACKWARDS;
								sc->SampleOffset = (sc->LoopEnd - 1) - NewLoopPos;
								sc->SmpError = (uint16_t)(0 - sc->SmpError);
							}
						}
					}

					uint32_t SamplesToMix;
					if (sc->LpD == DIR_BACKWARDS)
					{
						SamplesToMix = sc->SampleOffset - (sc->LoopBeg + 1);
						if (SamplesToMix > UINT16_MAX) // 8bb: added this to turn 64-bit div into 32-bit div (faster)
							SamplesToMix = UINT16_MAX;

						SamplesToMix = (((SamplesToMix << MIX_FRAC_BITS) | (uint16_t)sc->SmpError) / sc->Delta) + 1;
						Driver.Delta = 0 - sc->Delta;
					}
					else // 8bb: forwards
					{
						SamplesToMix = (sc->LoopEnd - 1) - sc->SampleOffset;
						if (SamplesToMix > UINT16_MAX)
							SamplesToMix = UINT16_MAX;

						SamplesToMix = (((SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->SmpError ^ MIX_FRAC_MASK)) / sc->Delta) + 1;
						Driver.Delta = sc->Delta;
					}

					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Mix(sc, MixBufferPtr, SamplesToMix);
					MixBufferPtr += SamplesToMix << 1;

					MixBlockSize -= SamplesToMix;
				}
			}
			else if (sc->LpM == LOOP_FORWARDS)
			{
				while (MixBlockSize > 0)
				{
					if ((uint32_t)sc->SampleOffset >= (uint32_t)sc->LoopEnd)
						sc->SampleOffset = sc->LoopBeg + ((uint32_t)(sc->SampleOffset - sc->LoopEnd) % LoopLength);

					uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SampleOffset;
					if (SamplesToMix > UINT16_MAX)
						SamplesToMix = UINT16_MAX;

					SamplesToMix = (((SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->SmpError ^ MIX_FRAC_MASK)) / sc->Delta) + 1;
					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Driver.Delta = sc->Delta;
					Mix(sc, MixBufferPtr, SamplesToMix);
					MixBufferPtr += SamplesToMix << 1;

					MixBlockSize -= SamplesToMix;
				}
			}
			else // 8bb: no loop
			{
				while (MixBlockSize > 0)
				{
					if ((uint32_t)sc->SampleOffset >= (uint32_t)sc->LoopEnd) // 8bb: LoopEnd = sample end, even for non-loopers
					{
						sc->Flags = SF_NOTE_STOP;
						if (!(sc->HCN & CHN_DISOWNED))
							((hostChn_t *)sc->HCOffst)->Flags &= ~HF_CHAN_ON; // Signify channel off

						break;
					}

					uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SampleOffset;
					if (SamplesToMix > UINT16_MAX)
						SamplesToMix = UINT16_MAX;

					SamplesToMix = (((SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->SmpError ^ MIX_FRAC_MASK)) / sc->Delta) + 1;
					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Driver.Delta = sc->Delta;
					Mix(sc, MixBufferPtr, SamplesToMix);
					MixBufferPtr += SamplesToMix << 1;

					MixBlockSize -= SamplesToMix;
				}
			}
		}

		sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
		               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
		               SF_LOOP_CHANGED    | SF_PAN_CHANGED);
	}
}

void SB16_SetTempo(uint8_t Tempo)
{
	assert(Tempo >= 31);
	Driver.BytesToMix = ((Driver.MixSpeed << 1) + (Driver.MixSpeed >> 1)) / Tempo;
}

void SB16_SetMixVolume(uint8_t vol)
{
	MixVolume = vol;

	if (Driver.MixMode <= 2) // 8bb: 16-bit mixers used? Calculate mixing LUT.
	{
		for (uint16_t i = 0; i < MIXTABLESIZE; i++)
		{
			int16_t Value = i;
			int8_t WaveValue = (Value & 0xFF);
			int8_t Volume = Value >> 8;

			MixSegment[i] = ((Volume * WaveValue * (int16_t)MixVolume) + 64) >> 7;
		}
	}

	RecalculateAllVolumes();
}

void SB16_ResetMixer(void) // 8bb: added this
{
	MixTransferRemaining = 0;
	MixTransferOffset = 0;
}

void SB16_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput) // 8bb: added this
{
	// 8bb: we add +1 here to match WAV-writer gain (and OpenMPT)
	const uint8_t SampleShiftValue = (Song.Header.Flags & ITF_STEREO) ? (13+1) : (14+1);

	for (int32_t i = 0; i < SamplesToOutput * 2; i++)
	{
		int32_t Sample = MixBuffer[MixTransferOffset++] >> SampleShiftValue;

		if (Sample < INT16_MIN)
			Sample = INT16_MIN;
		else if (Sample > INT16_MAX)
			Sample = INT16_MAX;

		*AudioOut16++ = (int16_t)Sample;
	}
}

void SB16_Mix(int32_t numSamples, int16_t *audioOut) // 8bb: added this (original SB16 driver uses IRQ callback)
{
	int32_t SamplesLeft = numSamples;
	while (SamplesLeft > 0)
	{
		if (MixTransferRemaining == 0)
		{
			Update();
			SB16_MixSamples();
			MixTransferRemaining = Driver.BytesToMix;
		}

		int32_t SamplesToTransfer = SamplesLeft;
		if (SamplesToTransfer > MixTransferRemaining)
			SamplesToTransfer = MixTransferRemaining;

		SB16_PostMix(audioOut, SamplesToTransfer);
		audioOut += SamplesToTransfer * 2;

		MixTransferRemaining -= SamplesToTransfer;
		SamplesLeft -= SamplesToTransfer;
	}
}

/* 8bb:
** Fixes sample end bytes for interpolation (yes, we have room after the data).
** Sustain loops are always handled as non-looping during fix in IT2.
*/
void SB16_FixSamples(void)
{
	sample_t *s = Song.Smp;
	for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		if (s->Data == NULL || s->Length == 0)
			continue;

		int8_t *data8 = (int8_t *)s->Data;
		const bool Sample16Bit = !!(s->Flags & SMPF_16BIT);
		const bool HasLoop = !!(s->Flags & SMPF_USE_LOOP);

		int8_t *smp8Ptr = &data8[s->Length << Sample16Bit];

		// 8bb: added this protection for looped samples
		if (HasLoop && s->LoopEnd-s->LoopBeg < 2)
		{
			*smp8Ptr++ = 0;
			*smp8Ptr++ = 0;
			return;
		}

		int8_t byte1 = 0;
		int8_t byte2 = 0;

		if (HasLoop)
		{
			int32_t src;
			if (s->Flags & SMPF_LOOP_PINGPONG)
			{
				src = s->LoopEnd - 2;
				if (src < 0)
					src = 0;
			}
			else // 8bb: forward loop
			{
				src = s->LoopBeg;
			}

			if (Sample16Bit)
				src <<= 1;

			byte1 = data8[src+0];
			byte2 = data8[src+1];
		}

		*smp8Ptr++ = byte1;
		*smp8Ptr++ = byte2;
	}
}

bool SB16_InitSound(int32_t mixingFrequency)
{
	if (mixingFrequency < 16000)
		mixingFrequency = 16000;
	else if (mixingFrequency > 64000)
		mixingFrequency = 64000;

	const int32_t LowestTempo = 31; // 8bb: 31 is possible through initial tempo (but 32 is general minimum)
	const int32_t MaxSamplesToMix = (((mixingFrequency << 1) + (mixingFrequency >> 1)) / LowestTempo) + 1;

	MixBuffer = (int32_t *)malloc(MaxSamplesToMix * 2 * sizeof (int32_t));
	if (MixBuffer == NULL)
		return false;

	Driver.MixSpeed = mixingFrequency;
	Driver.Type = DRIVER_SB16;

	/*
	** MixMode 0 = "16 Bit Non-interpolated"
	** MixMode 1 = "16 Bit Interpolated"
	** MixMode 2 = "32 Bit Non-interpolated"
	** MixMode 3 = "32 Bit Interpolated"
	*/
	Driver.MixMode = 3; // 8bb: "32 Bit Interpolated"

	return true;
}

void SB16_UninitSound(void)
{
	if (MixBuffer != NULL)
	{
		free(MixBuffer);
		MixBuffer = NULL;
	}
}
