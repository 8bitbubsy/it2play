/*
** ---- SB16 MMX IT2 driver ----
*/

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // nearbyintf()
#include <fenv.h> // fesetround()
#include "../cpu.h"
#include "../it_structs.h"
#include "../it_music.h" // Update()
#include "sb16mmx_m.h"
#include "zerovol.h"

static uint16_t MixVolume;
static int32_t BytesToMix, *MixBuffer, MixTransferRemaining, MixTransferOffset;

static void SB16MMX_MixSamples(void)
{
	MixTransferOffset = 0;

	memset(MixBuffer, 0, BytesToMix * 2 * sizeof (int32_t));

	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
			continue;

		bool UseOldMixOffset = false;
		if (sc->Flags & SF_NOTE_STOP)
		{
			sc->Flags &= ~SF_CHAN_ON;

			if (Driver.MixMode < 2)
				continue; // 8bb: if no ramp enabled in driver, don't ramp ending voices

			sc->LeftVolume = sc->RightVolume = 0;
		}
		else
		{
			int16_t OldSamplesBug = 0;

			if (sc->Flags & SF_FREQ_CHANGE)
			{
				if ((uint32_t)sc->Frequency>>MIX_FRAC_BITS >= Driver.MixSpeed ||
					(uint32_t)sc->Frequency >= INT32_MAX/2) // 8bb: non-IT2 limit, but required for safety
				{
					sc->Flags = SF_NOTE_STOP;
					if (!(sc->HostChnNum & CHN_DISOWNED))
						((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Turn off channel

					continue;
				}

				// 8bb: calculate mixer delta (could be faster, but slow method needed for OldSamplesBug)
				uint32_t Quotient = (uint32_t)sc->Frequency / Driver.MixSpeed;
				uint32_t Remainder = (uint32_t)sc->Frequency % Driver.MixSpeed;
				sc->Delta32 = Quotient << MIX_FRAC_BITS;
				Remainder <<= MIX_FRAC_BITS;
				Quotient = (uint32_t)Remainder / Driver.MixSpeed;
				Remainder = (uint32_t)Remainder % Driver.MixSpeed;
				sc->Delta32 |= (uint16_t)Quotient;

				OldSamplesBug = (uint16_t)Remainder; // 8bb: fun
			}

			if (sc->Flags & SF_NEW_NOTE)
			{
				// 8bb: reset filter state
				sc->filtera = sc->filterb = sc->filterc = 0;
				sc->OldSamples[0] = 0;

				/* 8bb: This one was supposed to be cleared, but Jeffrey Lim accidentally used
				** the DX register instead of the AX one.
				** That means that the content relies on what was in DX at the time. Thankfully,
				** whenever the SF_NEW_NOTE is set, SF_FREQ_CHANGE is also set, hence we only need
				** to simulate a DX value change from the mixer delta calculation (see above).
				**
				** This quirk is important, as it can actually change the shape of the waveform.
				*/
				sc->OldSamples[1] = OldSamplesBug;
				// -----------------------

				sc->OldLeftVolume = sc->OldRightVolume = 0; // Current Volume = 0 for volume sliding.
			}

			if (sc->Flags & (SF_RECALC_FINALVOL | SF_NEW_NOTE | SF_LOOP_CHANGED | SF_PAN_CHANGED))
			{
				if (sc->Flags & SF_CHN_MUTED)
				{
					sc->MixOffset = 4; // 8bb: use zero-vol mixer
					UseOldMixOffset = true;

					// 8bb: reset filter state
					sc->filtera = sc->filterb = sc->filterc = 0;
					sc->OldSamples[0] = sc->OldSamples[1] = 0;
					// -----------------------

					sc->LeftVolume = sc->RightVolume = sc->OldLeftVolume = sc->OldRightVolume = 0;

					if (!(sc->HostChnNum & CHN_DISOWNED))
					{
						const uint8_t filterCutOff = Driver.FilterParameters[sc->HostChnNum];
						const uint8_t filterQ = Driver.FilterParameters[64+sc->HostChnNum];

						sc->VolEnvState.CurNode = (filterCutOff << 8) | (sc->VolEnvState.CurNode & 0x00FF);
						sc->MIDIBank = (filterQ << 8) | (sc->MIDIBank & 0x00FF);
					}
				}
				else
				{
					if (Driver.MixMode == 3) // 8bb: filters enabled in driver?
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

							// If the values are different, then force recalculate volume. (and hence mixmode)
							if (filterCutOff != (uint16_t)sc->VolEnvState.CurNode>>8 && FilterQ != sc->MIDIBank>>8)
								sc->LeftVolume = sc->RightVolume = 0;

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
							** The code is done like this to be more accurate on x86/x86_64, even if the FPU is used instead of SIMD.
							** The order (and amount) of operations really matter. The FPU precision is set to 24-bit (32-bit) in IT2,
							** so we want to lose a bit of precision per calculation.
							*/
							const float psub1 = p - 1.0f;
							float d = p * r;
							d += psub1;
							const float e = r * r;
							float a = 1.0f + d;
							a += e;
							a = 16384.0f / a;
							const float fa = a;
							float dea = d + e;
							dea += e;
							dea *= a;
							const float fb = dea;
							float fc = e * a;
							fc = -fc;

							/*
							** 8bb: For all possible filter parameters ((127*255+1)*(127+1) = 4145408), there's about
							** 0.06% off-by-one errors on x86/x86_64 versus real IT2. This is still pretty accurate.
							**
							** Use nearbyintf() instead of roundf(), to get even less rounding errors vs. IT2 for
							** special numbers. The default rounding mode is FE_TONEAREST, which is what we want.
							*/
							sc->filtera = (int32_t)nearbyintf(fa);
							sc->filterb = (int32_t)nearbyintf(fb);
							sc->filterc = (int32_t)nearbyintf(fc);

							sc->LeftVolume = sc->RightVolume = 0;
						}
					}

					const int32_t OldLeftVolume = sc->LeftVolume;
					const int32_t OldRightVolume = sc->RightVolume;

					if (!(Song.Header.Flags & ITF_STEREO)) // 8bb: mono?
					{
						sc->LeftVolume = sc->RightVolume = (sc->FinalVol32768 * MixVolume) >> 9; // 8bb: 0..8192
					}
					else if (sc->FinalPan == PAN_SURROUND)
					{
						sc->LeftVolume = (sc->FinalVol32768 * MixVolume) >> 10; // 8bb: 0..4096
						sc->RightVolume = -sc->LeftVolume;
					}
					else // 8bb: normal (panned)
					{
						sc->LeftVolume  = ((64-sc->FinalPan) * MixVolume * sc->FinalVol32768) >> 15; // 8bb: 0..8192
						sc->RightVolume = (    sc->FinalPan  * MixVolume * sc->FinalVol32768) >> 15;
					}

					if (!(sc->Flags & (SF_NEW_NOTE|SF_NOTE_STOP|SF_LOOP_CHANGED)) && sc->LeftVolume == OldLeftVolume && sc->RightVolume == OldRightVolume)
						UseOldMixOffset = true;
				}
			}
			else
			{
				// 8bb: No vol/pan update needed, use old mix offset (IT2 BUG: fast zero-vol mixer is rarely used because of this)
				UseOldMixOffset = true;
			}
		}

		if (!UseOldMixOffset)
		{
			sc->MixOffset = Driver.MixMode;

			if (sc->LeftVolume == 0 && sc->RightVolume == 0)
			{
				if (Driver.MixMode < 2 || (sc->OldLeftVolume == 0 && sc->OldRightVolume == 0)) // 8bb: ramp disabled or ramp volumes zero?
					sc->MixOffset = 4; // 8bb: use position update routine (zero volume)
			}

			if (sc->MixOffset != 4 && Driver.MixMode == 3 && sc->filterb == 0 && sc->filterc == 0)
				sc->MixOffset--; // 8bb: Filter driver selected, but no filter active. Use non-filter mixer.
		}

		if (sc->Delta32 == 0) // 8bb: added this protection just in case (shouldn't happen)
			continue;

		uint32_t MixBlockSize = BytesToMix;
		const uint32_t OldMixOffset = sc->MixOffset;

		if (sc->MixOffset == 4) // 8bb: use position update routine (zero volume)
		{
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
			/*
			** MixOffset 0 = No interpolation, no volume ramp, no filtering
			** MixOffset 1 = Interpolation, no volume ramp, no filtering
			** MixOffset 2 = Interpolation, volume ramp, no filtering
			** MixOffset 3 = Interpolation, volume ramp, filtering
			** MixOffset 4 = Use position update routine (zero volume)
			*/
			mixFunc Mix = SB16MMX_MixFunctionTables[(sc->MixOffset << 1) + sc->SmpIs16Bit];
			assert(Mix != NULL);

			// 8bb: pre-mix routine
			if (sc->MixOffset >= 2) // 8bb: volramp used?
			{
				// 8bb: prepare volume ramp
				int32_t DestVolL = sc->LeftVolume;
				int32_t DestVolR = sc->RightVolume;

				if (sc->MixOffset == 3) // 8bb: filters (and volramp)
				{
					// 8bb: filters uses double volume range (because of 15-bit sample input)
					if (sc->Flags & (SF_RECALC_FINALVOL | SF_NEW_NOTE | SF_LOOP_CHANGED | SF_PAN_CHANGED))
					{
						DestVolL += DestVolL;
						DestVolR += DestVolR;
					}

					if (DestVolL == 0 && DestVolR == 0)
						sc->MixOffset++; // Zero-vol mixing (update positions only) (8bb: next round, under some circumstances)
				}
				else if (sc->MixOffset == 2) // 8bb: volramp + no filters
				{
					sc->MixOffset--; // 8bb: disable volume ramp next round (under some circumstances)
				}

				// 8bb: compensate for upwards ramp
				if (DestVolL >= sc->CurrVolL) DestVolL += RAMPCOMPENSATE;
				if (DestVolR >= sc->CurrVolR) DestVolR += RAMPCOMPENSATE;

				sc->DestVolL = DestVolL;
				sc->DestVolR = DestVolR;

				sc->CurrVolL = sc->OldLeftVolume;
				sc->CurrVolR = sc->OldRightVolume;
			}

			if (sc->MixOffset == 3 && sc->filtera == 0) // 8bb: filters?
				sc->filtera = 1;

			const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin; // 8bb: also length for non-loopers
			if ((int32_t)LoopLength > 0)
			{
				int32_t *MixBufferPtr = MixBuffer;
				if (sc->LoopMode == LOOP_PINGPONG)
				{
					while (MixBlockSize > 0)
					{
						uint32_t NewLoopPos;
						if (sc->LoopDirection == DIR_BACKWARDS)
						{
							if (sc->SamplingPosition <= sc->LoopBegin)
							{
								NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
								if (NewLoopPos >= LoopLength)
								{
									sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);

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

									if (sc->SamplingPosition <= sc->LoopBegin) // 8bb: non-IT2 edge-case safety for extremely high pitches
										sc->SamplingPosition = sc->LoopBegin + 1;
								}
							}
						}

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
					}
				}
				else if (sc->LoopMode == LOOP_FORWARDS)
				{
					while (MixBlockSize > 0)
					{
						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
							sc->SamplingPosition = sc->LoopBegin + ((uint32_t)(sc->SamplingPosition - sc->LoopEnd) % LoopLength);

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
					}
				}
				else // 8bb: no loop
				{
					while (MixBlockSize > 0)
					{
						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						{
							sc->Flags = SF_NOTE_STOP;
							if (!(sc->HostChnNum & CHN_DISOWNED))
								((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off

							break;
						}

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
					}
				}
			}

			if (OldMixOffset >= 2) // 8bb: if volramp was used, reset volumes
			{
				sc->LeftVolume  = sc->OldLeftVolume  = sc->CurrVolL;
				sc->RightVolume = sc->OldRightVolume = sc->CurrVolR;
			}
		}

		sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
		               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
		               SF_LOOP_CHANGED    | SF_PAN_CHANGED);
	}
}

static void SB16MMX_SetTempo(uint8_t Tempo)
{
	assert(Tempo >= LOWEST_BPM_POSSIBLE);
	BytesToMix = ((Driver.MixSpeed << 1) + (Driver.MixSpeed >> 1)) / Tempo;
}

static void SB16MMX_SetMixVolume(uint8_t vol)
{
	MixVolume = vol;
	RecalculateAllVolumes();
}

static void SB16MMX_ResetMixer(void) // 8bb: added this
{
	MixTransferRemaining = 0;
	MixTransferOffset = 0;
}

static int32_t SB16MMX_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput) // 8bb: added this
{
	const uint8_t SampleShiftValue = (Song.Header.Flags & ITF_STEREO) ? 12 : 13;

	int32_t SamplesTodo = (SamplesToOutput == 0) ? BytesToMix : SamplesToOutput;
	for (int32_t i = 0; i < SamplesTodo * 2; i++)
	{
		int32_t Sample = MixBuffer[MixTransferOffset++] >> SampleShiftValue;

		if (Sample < INT16_MIN)
			Sample = INT16_MIN;
		else if (Sample > INT16_MAX)
			Sample = INT16_MAX;

		*AudioOut16++ = (int16_t)Sample;
	}

	return SamplesTodo;
}

static void SB16MMX_Mix(int32_t numSamples, int16_t *audioOut) // 8bb: added this (original SB16 MMX driver uses IRQ callback)
{
	int32_t SamplesLeft = numSamples;
	while (SamplesLeft > 0)
	{
		if (MixTransferRemaining == 0)
		{
			Update();
			SB16MMX_MixSamples();
			MixTransferRemaining = BytesToMix;
		}

		int32_t SamplesToTransfer = SamplesLeft;
		if (SamplesToTransfer > MixTransferRemaining)
			SamplesToTransfer = MixTransferRemaining;

		SB16MMX_PostMix(audioOut, SamplesToTransfer);
		audioOut += SamplesToTransfer * 2;

		MixTransferRemaining -= SamplesToTransfer;
		SamplesLeft -= SamplesToTransfer;
	}
}

/* 8bb:
** Fixes sample end bytes for interpolation (yes, we have room after the data).
** Sustain loops are always handled as non-looping during fix in IT2.
*/
static void SB16MMX_FixSamples(void)
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
		if (HasLoop && s->LoopEnd-s->LoopBegin < 2)
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
				src = s->LoopBegin;
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

static void SB16MMX_CloseDriver(void)
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

bool SB16MMX_InitDriver(int32_t mixingFrequency)
{
	if (mixingFrequency < 8000)
		mixingFrequency = 8000;
	else if (mixingFrequency > 64000)
		mixingFrequency = 64000;

	const int32_t MaxSamplesToMix = (((mixingFrequency << 1) + (mixingFrequency >> 1)) / LOWEST_BPM_POSSIBLE) + 1;

	MixBuffer = (int32_t *)malloc(MaxSamplesToMix * 2 * sizeof (int32_t));
	if (MixBuffer == NULL)
		return false;

	Driver.Flags = DF_SUPPORTS_MIDI | DF_USES_VOLRAMP | DF_HAS_RESONANCE_FILTER;
	Driver.NumChannels = 128;
	Driver.MixSpeed = mixingFrequency;
	Driver.Type = DRIVER_SB16MMX;

	// 8bb: setup driver functions
	DriverClose = SB16MMX_CloseDriver;
	DriverMix = SB16MMX_Mix; // 8bb: added this (original driver uses IRQ callback)
	DriverSetTempo = SB16MMX_SetTempo;
	DriverSetMixVolume = SB16MMX_SetMixVolume;
	DriverFixSamples = SB16MMX_FixSamples;
	DriverResetMixer = SB16MMX_ResetMixer;
	DriverPostMix = SB16MMX_PostMix;
	DriverMixSamples = SB16MMX_MixSamples;

	/*
	** MixMode 0 = "MMX, Non-Interpolated"
	** MixMode 1 = "MMX, Interpolated"
	** MixMode 2 = "MMX, Volume Ramped"
	** MixMode 3 = "MMX, Filtered"
	*/
	Driver.MixMode = 3; // 8bb: "MMX, Filtered"

	return true;
}
