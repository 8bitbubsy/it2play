/*
** ---- "WAV writer" (IT2.15 registered) driver ----
*/

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cpu.h"
#include "../it_structs.h"
#include "../it_music.h" // Update()
#include "wavwriter_m.h"
#include "zerovol.h"

static uint16_t MixVolume;
static int32_t BytesToMix, RealBytesToMix, *MixBuffer, MixTransferRemaining, MixTransferOffset;
static int32_t LastClickRemovalLeft, LastClickRemovalRight, LeftDitherValue, RightDitherValue;
static uint64_t BytesToMixFractional, CurrentFractional;

static void WAVWriter_MixSamples(void)
{
	MixTransferOffset = 0;

	RealBytesToMix = BytesToMix;

	/* 8bb:
	** IT2 attempts to handle the fractional part of BytesToMix here,
	** but it actually doesn't work correctly since BytesToMixFractional
	** is not in the correct range. Please read the comment in WAVWriter_SetTempo().
	*/
	CurrentFractional += BytesToMixFractional;
	if (CurrentFractional > UINT32_MAX)
	{
		CurrentFractional &= UINT32_MAX;
		RealBytesToMix++;
	}

	// 8bb: click removal (also clears buffer)

	int32_t *MixBufPtr = MixBuffer;
	for (int32_t i = 0; i < RealBytesToMix; i++)
	{
		*MixBufPtr++ = LastClickRemovalLeft;
		*MixBufPtr++ = LastClickRemovalRight;

		int32_t LSub = LastClickRemovalLeft  >> 12;
		int32_t RSub = LastClickRemovalRight >> 12;
		if (LSub == 0) LSub = 1;
		if (RSub == 0) RSub = 1;
		LastClickRemovalLeft  -= LSub;
		LastClickRemovalRight -= RSub;
	}

	// Check each channel... Prepare mixing stuff

	slaveChn_t *sc = &sChn[Driver.NumChannels - 1]; // Work backwards (8bb: why..?)
	for (uint32_t i = 0; i < Driver.NumChannels; i++, sc--)
	{
		if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
			continue;

		if (sc->Flags & SF_NOTE_STOP) // Note cut.
		{
			sc->Flags &= ~SF_CHAN_ON; // Turn off channel

			sc->FinalVol32768 = 0;
			sc->Flags |= SF_RECALC_FINALVOL;
		}

		if (sc->Flags & SF_FREQ_CHANGE)
		{
			if ((uint32_t)sc->Frequency>>MIX_FRAC_BITS >= Driver.MixSpeed)
			{
				sc->Flags = SF_NOTE_STOP;
				if (!(sc->HostChnNum & CHN_DISOWNED))
					((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Turn off channel

				continue;
			}

			// 8bb: calculate mixer delta
			uint32_t Quotient = (uint32_t)sc->Frequency / Driver.MixSpeed;
			uint32_t Remainder = (uint32_t)sc->Frequency % Driver.MixSpeed;
			sc->Delta32 = (Quotient << MIX_FRAC_BITS) | (uint16_t)((Remainder << MIX_FRAC_BITS) / Driver.MixSpeed);
		}

		if (sc->Flags & SF_NEW_NOTE)
		{
			sc->OldLeftVolume = sc->OldRightVolume = 0;

			// 8bb: reset filter state
			sc->fOldSamples[0] = sc->fOldSamples[1] = 0.0f;
			sc->fFiltera = 1.0f;
			sc->fFilterb = 0.0f;
			sc->fFilterc = 0.0f;
			// -----------------------
		}

		if (sc->Flags & (SF_RECALC_FINALVOL | SF_LOOP_CHANGED | SF_PAN_CHANGED))
		{
			uint8_t FilterQ;

			if (sc->HostChnNum & CHN_DISOWNED)
			{
				FilterQ = sc->MIDIBank >> 8; // Disowned? Then use channel filters.
			}
			else
			{
				uint8_t filterCutOff = Driver.FilterParameters[sc->HostChnNum];
				FilterQ = Driver.FilterParameters[64+sc->HostChnNum];

				sc->VolEnvState.CurNode = (filterCutOff << 8) | (sc->VolEnvState.CurNode & 0x00FF);
				sc->MIDIBank = (FilterQ << 8) | (sc->MIDIBank & 0x00FF);
			}

			// 8bb: FilterEnvVal (0..255) * CutOff (0..127)
			const uint16_t FilterFreqValue = (sc->MIDIBank & 0x00FF) * (uint8_t)((uint16_t)sc->VolEnvState.CurNode >> 8);
			if (FilterFreqValue != 127*255 || FilterQ != 0)
			{
				assert(FilterFreqValue <= 127*255 && FilterQ <= 127);
				const float r = powf(2.0f, (float)FilterFreqValue * Driver.FreqParameterMultiplier) * Driver.FreqMultiplier;
				const float p = Driver.QualityFactorTable[FilterQ];

				/* 8bb:
				** The code is done like this to be 100% accurate on x86/x86_64 with Visual Studio 2019, even if the FPU
				** is used instead of SIMD. The order (and amount) of operations really matter. The FPU precision is set
				** to 24-bit (32-bit) in IT2, so we want to lose a bit of precision per calculation. This has been tested
				** with careful bruteforcing to be 100% accurate to IT2 coeffs. This is not very readable, but the filters
				** are documented elsewhere anyway.
				*/
				const float psub1 = p - 1.0f;
				float d = p * r;
				d += psub1;
				const float e = r * r;
				float a = 1.0f + d;
				a += e;
				a = 1.0f / a;
				const float fa = a;
				float dea = d + e;
				dea += e;
				dea *= a;
				const float fb = dea;
				float fc = 1.0f - fa;
				fc -= fb;

				sc->fFiltera = fa;
				sc->fFilterb = fb;
				sc->fFilterc = fc;
			}

			if (sc->Flags & SF_CHN_MUTED)
			{
				sc->LeftVolume = sc->RightVolume = 0;
			}
			else if (!(Song.Header.Flags & ITF_STEREO)) // 8bb: mono?
			{
				sc->LeftVolume = sc->RightVolume = (sc->FinalVol32768 * MixVolume) >> 8; // 8bb: 0..16384
			}
			else if (sc->FinalPan == PAN_SURROUND)
			{
				sc->LeftVolume = sc->RightVolume = (sc->FinalVol32768 * MixVolume) >> 9; // 8bb: 0..8192
			}
			else // 8bb: normal (panned)
			{
				sc->LeftVolume  = ((64-sc->FinalPan) * MixVolume * sc->FinalVol32768) >> 14; // 8bb: 0..16384
				sc->RightVolume = (    sc->FinalPan  * MixVolume * sc->FinalVol32768) >> 14;
			}
		}

		if (sc->Delta32 == 0) // 8bb: added this protection just in case (shouldn't happen)
			continue;

		bool UseZeroVolMix = false;
		if (sc->LeftVolume == 0 && sc->RightVolume == 0 && sc->OldLeftVolume == 0 && sc->OldRightVolume == 0)
			UseZeroVolMix = true;

		uint32_t MixBlockSize = RealBytesToMix;

		// 8bb: don't ramp volume at start of sample (if enabled)
		if (Driver.StartNoRamp && (sc->Flags & SF_NEW_NOTE) && sc->SamplingPosition == 0)
		{
			sc->OldLeftVolume = sc->LeftVolume;
			sc->OldRightVolume = sc->RightVolume;
		}

		if (UseZeroVolMix)
		{
			// 8bb: use position update routine (zero volume)

			const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin; // 8bb: also length for non-loopers
			if ((int32_t)LoopLength > 0)
			{
				if (sc->LoopMode == LOOP_PINGPONG)
					UpdatePingPongLoop(sc, MixBlockSize);
				else if (sc->LoopMode == LOOP_FORWARDS)
					UpdateForwardsLoop(sc, MixBlockSize);
				else
					UpdateNoLoop(sc, MixBlockSize);
			}
		}
		else // 8bb: regular mixing
		{
			const bool Surround = (sc->FinalPan == PAN_SURROUND);
			const bool Sample16Bit = !!(sc->SmpBitDepth & SMPF_16BIT);
			mixFunc Mix = WAVWriter_MixFunctionTables[(Surround << 1) + Sample16Bit];
			assert(Mix != NULL);

			// 8bb: prepare volume ramp

			if (Surround)
			{
				int32_t DestVol = sc->LeftVolume;

				sc->CurrVolL = sc->OldLeftVolume;

				// 8bb: compensate for upwards ramp
				if (DestVol >= sc->CurrVolL)
					DestVol += RAMPCOMPENSATE;

				sc->DestVolL = DestVol;
			}
			else // 8bb: regular mixing
			{
				int32_t DestVolL = sc->LeftVolume; // New left volume
				int32_t DestVolR = sc->RightVolume; // New right volume

				sc->CurrVolL = sc->OldLeftVolume; // Old left volume
				sc->CurrVolR = sc->OldRightVolume; // Old right volume

				// Compensate for upwards ramp.
				if ((uint32_t)DestVolL >= (uint32_t)sc->CurrVolL) DestVolL += RAMPCOMPENSATE;
				if ((uint32_t)DestVolR >= (uint32_t)sc->CurrVolR) DestVolR += RAMPCOMPENSATE;

				sc->DestVolL = DestVolL;
				sc->DestVolR = DestVolR;
			}

			/* 8bb: IT2 attempts to calculate the final running ramp volume for OldVolumeX here,
			** but that's quite slow. All you need to do is to set OldVolumeX to CurrVolX after
			** this mixing cycle. I have no idea why he did it like this, but we don't because
			** it's completely unneeded.
			*/

			const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin; // 8bb: also length for non-loopers
			if ((int32_t)LoopLength > 0)
			{
				int32_t *MixBufferPtr = MixBuffer;
				if (sc->LoopMode == LOOP_PINGPONG)
				{
					while (MixBlockSize > 0)
					{
						uint32_t NewLoopPos;

						uint32_t SamplesToMix;
						if (sc->LoopDirection == DIR_BACKWARDS)
						{
							SamplesToMix = sc->SamplingPosition - (sc->LoopBegin + 1);
#if CPU_32BIT
							if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
								SamplesToMix = UINT16_MAX;
#endif
							SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | (uint16_t)sc->Frac32) / sc->Delta32) + 1;
							Driver.Delta32 = 0 - sc->Delta32;
						}
						else // 8bb: forwards
						{
							SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
							if (SamplesToMix > UINT16_MAX)
								SamplesToMix = UINT16_MAX;
#endif
							SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | (uint16_t)(sc->Frac32 ^ MIX_FRAC_MASK)) / sc->Delta32) + 1;
							Driver.Delta32 = sc->Delta32;
						}

						if (SamplesToMix > MixBlockSize)
							SamplesToMix = MixBlockSize;

						Mix(sc, MixBufferPtr, SamplesToMix);

						MixBlockSize -= SamplesToMix;
						MixBufferPtr += SamplesToMix << 1;

						if (sc->LoopDirection == DIR_BACKWARDS)
						{
							if (sc->SamplingPosition <= sc->LoopBegin)
							{
								NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
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
							if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
							{
								NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
								if (NewLoopPos >= LoopLength)
								{
									sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
								}
								else
								{
									sc->LoopDirection = DIR_BACKWARDS;
									sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
									sc->Frac32 = (uint16_t)(0 - sc->Frac32);
								}
							}
						}
					}
				}
				else if (sc->LoopMode == LOOP_FORWARDS)
				{
					while (MixBlockSize > 0)
					{
						uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
						if (SamplesToMix > UINT16_MAX)
							SamplesToMix = UINT16_MAX;
#endif
						SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | (uint16_t)(sc->Frac32 ^ MIX_FRAC_MASK)) / sc->Delta32) + 1;
						if (SamplesToMix > MixBlockSize)
							SamplesToMix = MixBlockSize;

						Driver.Delta32 = sc->Delta32;
						Mix(sc, MixBufferPtr, SamplesToMix);

						MixBlockSize -= SamplesToMix;
						MixBufferPtr += SamplesToMix << 1;

						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
							sc->SamplingPosition = sc->LoopBegin + ((uint32_t)(sc->SamplingPosition - sc->LoopEnd) % LoopLength);
					}
				}
				else // 8bb: no loop
				{
					while (MixBlockSize > 0)
					{
						uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
						if (SamplesToMix > UINT16_MAX)
							SamplesToMix = UINT16_MAX;
#endif
						SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | (uint16_t)(sc->Frac32 ^ MIX_FRAC_MASK)) / sc->Delta32) + 1;
						if (SamplesToMix > MixBlockSize)
							SamplesToMix = MixBlockSize;

						Driver.Delta32 = sc->Delta32;
						Mix(sc, MixBufferPtr, SamplesToMix);

						MixBlockSize -= SamplesToMix;
						MixBufferPtr += SamplesToMix << 1;

						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						{
							sc->Flags = SF_NOTE_STOP;
							if (!(sc->HostChnNum & CHN_DISOWNED))
								((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel of

							// 8bb: sample ended, ramp out very last sample point for the remaining samples

							int32_t L = -Driver.LastLeftValue;
							int32_t R = -Driver.LastRightValue;

							for (; MixBlockSize > 0; MixBlockSize--)
							{
								*MixBufferPtr++ += L;
								*MixBufferPtr++ += R;

								int32_t LSub = L >> 12;
								int32_t RSub = R >> 12;
								if (LSub == 0) LSub = 1;
								if (RSub == 0) RSub = 1;
								L -= LSub;
								R -= RSub;
							}

							// 8bb: update anti-click value for next mixing session
							LastClickRemovalLeft  += L;
							LastClickRemovalRight += R;

							break;
						}
					}
				}
			}

			sc->OldLeftVolume = sc->CurrVolL;
			if (!Surround)
				sc->OldRightVolume = sc->CurrVolR;
		}

		sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
		               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
		               SF_LOOP_CHANGED    | SF_PAN_CHANGED);
	}
}

static void WAVWriter_SetTempo(uint8_t Tempo)
{
	assert(Tempo >= LOWEST_BPM_POSSIBLE);
	BytesToMix = ((Driver.MixSpeed << 1) + (Driver.MixSpeed >> 1)) / Tempo;

	/* 8bb:
	** IT2 calculates the fractional part of "bytes to mix" here,
	** but it does it very wrongly, so the range of BytesToMixFractional
	** is 0 .. BPM-1 instead of 0 .. UINT32_MAX-1.
	** It would take 16909320..inf replayer ticks for the fraction to
	** overflow!
	*/
	BytesToMixFractional = ((Driver.MixSpeed << 1) + (Driver.MixSpeed >> 1)) % Tempo;
}

static void WAVWriter_SetMixVolume(uint8_t vol)
{
	MixVolume = vol;
	RecalculateAllVolumes();
}

static void WAVWriter_ResetMixer(void) // 8bb: added this
{
	MixTransferRemaining = 0;
	MixTransferOffset = 0;
	CurrentFractional = 0;

	LastClickRemovalLeft = LastClickRemovalRight = 0;
	LeftDitherValue = RightDitherValue = 0;
}

static int32_t WAVWriter_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput) // 8bb: added this
{
	int32_t SamplesTodo = (SamplesToOutput == 0) ? RealBytesToMix : SamplesToOutput;
	for (int32_t i = 0; i < SamplesTodo; i++)
	{
		LeftDitherValue  += MixBuffer[MixTransferOffset++];
		RightDitherValue += MixBuffer[MixTransferOffset++];

		int32_t L =  LeftDitherValue >> 14;
		int32_t R = RightDitherValue >> 14;

		LeftDitherValue  &= 0x3FFF;
		RightDitherValue &= 0x3FFF;

		if (L < INT16_MIN)
			L = INT16_MIN;
		else if (L > INT16_MAX)
			L = INT16_MAX;

		if (R < INT16_MIN)
			R = INT16_MIN;
		else if (R > INT16_MAX)
			R = INT16_MAX;

		*AudioOut16++ = (int16_t)L;
		*AudioOut16++ = (int16_t)R;
	}

	return SamplesTodo;
}

static void WAVWriter_Mix(int32_t numSamples, int16_t *audioOut) // 8bb: added this (original SB16 MMX driver uses IRQ callback)
{
	int32_t SamplesLeft = numSamples;
	while (SamplesLeft > 0)
	{
		if (MixTransferRemaining == 0)
		{
			Update();
			WAVWriter_MixSamples();
			MixTransferRemaining = RealBytesToMix;
		}

		int32_t SamplesToTransfer = SamplesLeft;
		if (SamplesToTransfer > MixTransferRemaining)
			SamplesToTransfer = MixTransferRemaining;

		WAVWriter_PostMix(audioOut, SamplesToTransfer);
		audioOut += SamplesToTransfer * 2;

		MixTransferRemaining -= SamplesToTransfer;
		SamplesLeft -= SamplesToTransfer;
	}
}

/* 8bb:
** Fixes sample end bytes for interpolation (yes, we have room after the data).
** Sustain loops are always handled as non-looping during fix in IT2.
*/
static void WAVWriter_FixSamples(void)
{
	sample_t *s = Song.Smp;
	for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		if (s->Data == NULL || s->Length == 0)
			continue;
		
		const bool Sample16Bit = !!(s->Flags & SMPF_16BIT);
		const bool HasLoop = !!(s->Flags & SMPF_USE_LOOP);

		if (Sample16Bit)
		{
			// 16 bit

			int16_t *Data16 = (int16_t *)s->Data;

			if (HasLoop)
			{
				if (s->Flags & SMPF_LOOP_PINGPONG)
				{
					int32_t LastSample = s->LoopEnd - 1;
					if (LastSample < 0)
						LastSample = 0;

					Data16[s->LoopEnd+0] = Data16[LastSample];
					if (LastSample > 0)
						Data16[s->LoopEnd+1] = Data16[LastSample-1];
					else
						Data16[s->LoopEnd+1] = Data16[LastSample];
				}
				else
				{
					Data16[s->LoopEnd+0] = Data16[s->LoopBegin+0];
					Data16[s->LoopEnd+1] = Data16[s->LoopBegin+1];
				}
			}
			else
			{
				Data16[s->Length+0] = Data16[s->Length-1];
				Data16[s->Length+1] = Data16[s->Length-1];
			}
		}
		else
		{
			// 8 bit

			int8_t *Data8 = (int8_t *)s->Data;

			if (HasLoop)
			{
				if (s->Flags & SMPF_LOOP_PINGPONG)
				{
					int32_t LastSample = s->LoopEnd - 1;
					if (LastSample < 0)
						LastSample = 0;

					Data8[s->LoopEnd+0] = Data8[LastSample];
					if (LastSample > 0)
						Data8[s->LoopEnd+1] = Data8[LastSample-1];
					else
						Data8[s->LoopEnd+1] = Data8[LastSample];
				}
				else
				{
					Data8[s->LoopEnd+0] = Data8[s->LoopBegin+0];
					Data8[s->LoopEnd+1] = Data8[s->LoopBegin+1];
				}
			}
			else
			{
				Data8[s->Length+0] = Data8[s->Length-1];
				Data8[s->Length+1] = Data8[s->Length-1];
			}
		}
	}
}

static void WAVWriter_CloseDriver(void)
{
	if (MixBuffer != NULL)
	{
		free(MixBuffer);
		MixBuffer = NULL;
	}

	DriverClose = NULL;
	DriverMix = NULL;
	DriverSetTempo = NULL;
	DriverSetMixVolume = NULL;
	DriverFixSamples = NULL;
	DriverResetMixer = NULL;
	DriverPostMix = NULL;
	DriverMixSamples = NULL;
}

bool WAVWriter_InitDriver(int32_t mixingFrequency)
{
	if (mixingFrequency < 8000)
		mixingFrequency = 8000;
	else if (mixingFrequency > 64000)
		mixingFrequency = 64000;

	// 8bb: +2, make room for "RealBytesToMix" overflow addition
	const int32_t MaxSamplesToMix = (((mixingFrequency << 1) + (mixingFrequency >> 1)) / LOWEST_BPM_POSSIBLE) + 2;

	MixBuffer = (int32_t *)malloc(MaxSamplesToMix * 2 * sizeof (int32_t));
	if (MixBuffer == NULL)
		return false;

	LastClickRemovalLeft = LastClickRemovalRight = 0;
	LeftDitherValue = RightDitherValue = 0;

	Driver.Flags = DF_SUPPORTS_MIDI | DF_USES_VOLRAMP | DF_HAS_RESONANCE_FILTER;
	Driver.NumChannels = 256;
	Driver.MixSpeed = mixingFrequency;
	Driver.Type = DRIVER_WAVWRITER;
	Driver.StartNoRamp = false;

	// 8bb: setup driver functions
	DriverClose = WAVWriter_CloseDriver;
	DriverMix = WAVWriter_Mix;
	DriverSetTempo = WAVWriter_SetTempo;
	DriverSetMixVolume = WAVWriter_SetMixVolume;
	DriverFixSamples = WAVWriter_FixSamples;
	DriverResetMixer = WAVWriter_ResetMixer;
	DriverPostMix = WAVWriter_PostMix;
	DriverMixSamples = WAVWriter_MixSamples;

	return true;
}
