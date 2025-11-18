/*
** ---- Custom (non-IT2) high quality floating-point driver ----
**
** Behaves like the SB16 MMX driver when it comes to filter clamping
** and bidi loop wrapping.
**
** Features:
** - 8-tap windowed-sinc interpolation
** - Stereo sample support
** - 32.32 fixed-point sampling precision (32.16 if 32-bit CPU, for speed)
** - Ended non-looping samples are ramped out, like the WAV writer driver
**
** Compiling for 64-bit is ideal as it results in higher precision and support
** for higher mixing frequencies than 48kHz.
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
#include "hq_m.h"
#include "hq_fixsample.h"
#include "zerovol.h"

#define PI 3.14159265358979323846264338327950288

// fast 32-bit -> 16-bit clamp
#define CLAMP16(i) if ((int16_t)i != i) i = INT16_MAX ^ ((int32_t)i >> 31)

#define BPM_FRAC_BITS 31 /* absolute max for 32-bit arithmetics, don't change! */
#define BPM_FRAC_SCALE (1UL << BPM_FRAC_BITS)
#define BPM_FRAC_MASK (BPM_FRAC_SCALE-1)

static uint16_t MixVolume;
static int32_t RealBytesToMix, BytesToMix, MixTransferRemaining, MixTransferOffset;
static uint32_t BytesToMixFractional, CurrentFractional, RandSeed;
static uint32_t SamplesPerTickInt[256-LOWEST_BPM_POSSIBLE], SamplesPerTickFrac[256-LOWEST_BPM_POSSIBLE];
static float *fMixBuffer, fLastClickRemovalLeft, fLastClickRemovalRight;
static double dFreq2DeltaMul;
#if !CPU_32BIT
static double dPrngStateL, dPrngStateR;
#endif

static inline double sinc(double x)
{
	if (x == 0.0)
	{
		return 1.0;
	}
	else
	{
		x *= PI;
		return sin(x) / x;
	}
}

static bool InitWindowedSincLUT(void)
{
	Driver.fSincLUT = (float *)malloc(SINC_PHASES * SINC_WIDTH * sizeof (float));
	if (Driver.fSincLUT == NULL)
		return false;

	for (int32_t i = 0; i < SINC_WIDTH * SINC_PHASES; i++)
	{
		const double x = i * (1.0 / (SINC_WIDTH*SINC_PHASES));
		const double n = (x - 0.5) * SINC_WIDTH;

		// Cosine-sum window (approximation of Kaiser-Bessel with beta=9.2)
		const double window = 0.40719122268357 -
		                     (0.49860425525392 * cos((2.0 * PI) * x)) +
		                     (0.09406372638699 * cos((4.0 * PI) * x));

		const float wsinc = (float)(sinc(n) * window);

		// rearrange LUT for faster access in mixer
		const uint32_t point = i >> SINC_PHASES_BITS;
		const uint32_t phase = i & (SINC_PHASES-1);
		Driver.fSincLUT[(((SINC_PHASES-1)-phase) << SINC_WIDTH_BITS) + point] = wsinc;
	}

	return true;
}

static void HQ_MixSamples(void)
{
	MixTransferOffset = 0;

	RealBytesToMix = BytesToMix;

	CurrentFractional += BytesToMixFractional;
	if (CurrentFractional > BPM_FRAC_SCALE)
	{
		CurrentFractional &= BPM_FRAC_MASK;
		RealBytesToMix++;
	}

	// click removal (also clears buffer)

	float *fMixBufPtr = fMixBuffer;
	for (int32_t i = 0; i < RealBytesToMix; i++)
	{
		*fMixBufPtr++ = fLastClickRemovalLeft;
		*fMixBufPtr++ = fLastClickRemovalRight;

		// XXX: This may cause an issue if there is complete silence over a LONG time?
		fLastClickRemovalLeft  -= fLastClickRemovalLeft  * (1.0f / 4096.0f);
		fLastClickRemovalRight -= fLastClickRemovalRight * (1.0f / 4096.0f);
	}

	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
			continue;

		sample_t *s = sc->SmpPtr;
		assert(s != NULL);

		if (sc->Flags & SF_NOTE_STOP) // note cut
		{
			sc->Flags &= ~SF_CHAN_ON;

			sc->FinalVol32768 = 0;
			sc->Flags |= SF_UPDATE_MIXERVOL;
		}

		if (sc->Flags & SF_FREQ_CHANGE)
		{
			if ((uint32_t)sc->Frequency >= INT32_MAX/2) // non-IT2 limit, but required for safety
			{
				sc->Flags = SF_NOTE_STOP;
				if (!(sc->HostChnNum & CHN_DISOWNED))
					((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // turn off channel

				continue;
			}

#if CPU_32BIT
			sc->Delta32 = (int32_t)((int32_t)sc->Frequency * dFreq2DeltaMul); // mixer delta (16.16fp)
#else
			sc->Delta64 = (int64_t)((int32_t)sc->Frequency * dFreq2DeltaMul); // mixer delta (32.32fp)
#endif
		}

		if (sc->Flags & SF_NEW_NOTE)
		{
			sc->fOldLeftVolume = sc->fOldRightVolume = 0.0f;

			sc->fCurrVolL = sc->fCurrVolR = 0.0f; // ramp in current voice (old note is ramped out in another voice)

			// clear filter state and filter coeffs
			sc->fOldSamples[0] = sc->fOldSamples[1] = sc->fOldSamples[2] = sc->fOldSamples[3] = 0.0f;
			sc->fFiltera = 1.0f;
			sc->fFilterb = sc->fFilterc = 0.0f;
		}

		if (sc->Flags & (SF_UPDATE_MIXERVOL | SF_LOOP_CHANGED | SF_PAN_CHANGED))
		{
			uint8_t FilterQ;

			if (sc->HostChnNum & CHN_DISOWNED)
			{
				FilterQ = sc->MIDIBank >> 8; // if disowned, use channel filters
			}
			else
			{
				uint8_t filterCutOff = Driver.FilterParameters[sc->HostChnNum];
				FilterQ = Driver.FilterParameters[64+sc->HostChnNum];

				sc->VolEnvState.CurNode = (filterCutOff << 8) | (sc->VolEnvState.CurNode & 0x00FF);
				sc->MIDIBank = (FilterQ << 8) | (sc->MIDIBank & 0x00FF);
			}

			// FilterEnvVal (0..255) * CutOff (0..127)
			const uint16_t FilterFreqValue = (sc->MIDIBank & 0x00FF) * (uint8_t)((uint16_t)sc->VolEnvState.CurNode >> 8);
			if (FilterFreqValue != 127*255 || FilterQ != 0)
			{
				assert(FilterFreqValue <= 127*255 && FilterQ <= 127);
				const float r = powf(2.0f, (float)FilterFreqValue * Driver.FreqParameterMultiplier) * Driver.FreqMultiplier;
				const float p = Driver.QualityFactorTable[FilterQ];
				const float d = (p * r) + (p - 1.0f);
				const float e = r * r;

				sc->fFiltera = 1.0f / (1.0f + d + e);
				sc->fFilterb = (d + e + e) * sc->fFiltera;
				sc->fFilterc = 1.0f - sc->fFiltera - sc->fFilterb;
			}

			if (sc->Flags & SF_CHN_MUTED)
			{
				sc->fLeftVolume = sc->fRightVolume = 0.0f;
			}
			else
			{
				const int32_t Vol = sc->FinalVol32768 * MixVolume;
				if (!(Song.Header.Flags & ITF_STEREO)) // mono?
				{
					sc->fLeftVolume = sc->fRightVolume = Vol * (1.0f / (32768.0f * 128.0f));
				}
				else if (sc->FinalPan == PAN_SURROUND)
				{
					sc->fLeftVolume = sc->fRightVolume = Vol * (0.5f / (32768.0f * 128.0f));
				}
				else // normal (panned)
				{
					sc->fLeftVolume  = ((64-sc->FinalPan) * Vol) * (1.0f / (64.0f * 32768.0f * 128.0f));
					sc->fRightVolume = ((   sc->FinalPan) * Vol) * (1.0f / (64.0f * 32768.0f * 128.0f));
				}
			}
		}

		// just in case (shouldn't happen)
#if CPU_32BIT
		if (sc->Delta32 == 0)
#else
		if (sc->Delta64 == 0)
#endif
			continue;

		uint32_t MixBlockSize = RealBytesToMix;
		const bool FilterActive = (sc->fFilterb > 0.0f) || (sc->fFilterc > 0.0f);

		if (sc->fLeftVolume == 0.0f && sc->fRightVolume == 0.0f &&
			sc->fOldLeftVolume <= 0.000001f && sc->fOldRightVolume <= 0.000001f &&
			!FilterActive)
		{
			// use position update routine (zero voice volume and no filter)

			const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin; // also length for non-loopers
			if ((int32_t)LoopLength > 0)
			{
				if (sc->LoopMode == LOOP_PINGPONG)
					UpdatePingPongLoopHQ(sc, MixBlockSize);
				else if (sc->LoopMode == LOOP_FORWARDS)
					UpdateForwardsLoopHQ(sc, MixBlockSize);
				else
					UpdateNoLoopHQ(sc, MixBlockSize);
			}
		}
		else // regular mixing
		{
			const bool Surround = (sc->FinalPan == PAN_SURROUND);
			const bool Stereo = !!(s->Flags & SMPF_STEREO);
			
			MixFunc_t Mix = HQ_MixFunctionTables[(FilterActive << 3) + (Stereo << 2) + (Surround << 1) + sc->SmpIs16Bit];
			assert(Mix != NULL);

			const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin; // also actual length for non-loopers
			if ((int32_t)LoopLength > 0)
			{
				float *fMixBufferPtr = fMixBuffer;
				if (sc->LoopMode == LOOP_PINGPONG)
				{
					while (MixBlockSize > 0)
					{
						uint32_t NewLoopPos, SamplesToMix;

						if (sc->LoopDirection == DIR_BACKWARDS)
						{
							if (sc->SamplingPosition == sc->LoopBegin)
							{
								sc->LoopDirection = DIR_FORWARDS;
#if CPU_32BIT
								sc->Frac32 = (uint16_t)(0 - sc->Frac32);
#else
								sc->Frac64 = (uint32_t)(0 - sc->Frac64);
#endif
								SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
								if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
									SamplesToMix = UINT16_MAX;

								SamplesToMix = (((SamplesToMix << 16) | (uint16_t)(sc->Frac32 ^ UINT16_MAX)) / sc->Delta32) + 1;
								Driver.Delta32 = sc->Delta32;
#else
								SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
								Driver.Delta64 = sc->Delta64;
#endif
							}
							else
							{
								SamplesToMix = sc->SamplingPosition - (sc->LoopBegin + 1);
#if CPU_32BIT
								if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
									SamplesToMix = UINT16_MAX;

								SamplesToMix = (((SamplesToMix << 16) | (uint16_t)sc->Frac32) / sc->Delta32) + 1;
								Driver.Delta32 = 0 - sc->Delta32;
#else
								SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | (uint32_t)sc->Frac64) / sc->Delta64) + 1);
								Driver.Delta64 = 0 - sc->Delta64;
#endif
							}
						}
						else // forwards
						{
							SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
							if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
								SamplesToMix = UINT16_MAX;

							SamplesToMix = (((SamplesToMix << 16) | (uint16_t)(sc->Frac32 ^ UINT16_MAX)) / sc->Delta32) + 1;
							Driver.Delta32 = sc->Delta32;
#else
							SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
							Driver.Delta64 = sc->Delta64;
#endif
						}

						if (SamplesToMix > MixBlockSize)
							SamplesToMix = MixBlockSize;

						fixSamplesPingpong(s, sc); // for interpolation taps
						Mix(sc, fMixBufferPtr, SamplesToMix);
						unfixSamplesPingpong(s, sc); // for interpolation taps

						MixBlockSize -= SamplesToMix;
						fMixBufferPtr += SamplesToMix << 1;

						if (sc->LoopDirection == DIR_BACKWARDS)
						{
							if (sc->SamplingPosition <= sc->LoopBegin)
							{
								NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
								if (NewLoopPos >= LoopLength)
								{
									sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);

									if (sc->SamplingPosition == sc->LoopBegin)
									{
										sc->LoopDirection = DIR_FORWARDS;
#if CPU_32BIT
										sc->Frac32 = (uint16_t)(0 - sc->Frac32);
#else
										sc->Frac64 = (uint32_t)(0 - sc->Frac64);
#endif
									}
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

								sc->HasLooped = true;
							}
						}
						else // forwards
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
									sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
									if (sc->SamplingPosition != sc->LoopBegin)
									{
										sc->LoopDirection = DIR_BACKWARDS;
#if CPU_32BIT
										sc->Frac32 = (uint16_t)(0 - sc->Frac32);
#else
										sc->Frac64 = (uint32_t)(0 - sc->Frac64);
#endif
									}
								}

								sc->HasLooped = true;
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
						if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
							SamplesToMix = UINT16_MAX;

						SamplesToMix = (((SamplesToMix << 16) | (uint16_t)(sc->Frac32 ^ UINT16_MAX)) / sc->Delta32) + 1;
						Driver.Delta32 = sc->Delta32;
#else
						SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
						Driver.Delta64 = sc->Delta64;
#endif
						if (SamplesToMix > MixBlockSize)
							SamplesToMix = MixBlockSize;

						fixSamplesFwdLoop(s, sc); // for interpolation taps
						Mix(sc, fMixBufferPtr, SamplesToMix);
						unfixSamplesFwdLoop(s, sc); // for interpolation taps

						MixBlockSize -= SamplesToMix;
						fMixBufferPtr += SamplesToMix << 1;

						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						{
							sc->SamplingPosition = sc->LoopBegin + ((uint32_t)(sc->SamplingPosition - sc->LoopEnd) % LoopLength);
							sc->HasLooped = true;
						}
					}
				}
				else // no loop
				{
					while (MixBlockSize > 0)
					{
						uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
						if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
							SamplesToMix = UINT16_MAX;

						SamplesToMix = (((SamplesToMix << 16) | (uint16_t)(sc->Frac32 ^ UINT16_MAX)) / sc->Delta32) + 1;
						Driver.Delta32 = sc->Delta32;
#else
						SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
						Driver.Delta64 = sc->Delta64;
#endif
						if (SamplesToMix > MixBlockSize)
							SamplesToMix = MixBlockSize;

						fixSamplesNoLoop(s, sc); // for interpolation taps
						Mix(sc, fMixBufferPtr, SamplesToMix);

						MixBlockSize -= SamplesToMix;
						fMixBufferPtr += SamplesToMix << 1;

						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						{
							sc->Flags = SF_NOTE_STOP;
							if (!(sc->HostChnNum & CHN_DISOWNED))
								((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON;

							// sample ended, ramp out very last sample point for the remaining samples
							for (; MixBlockSize > 0; MixBlockSize--)
							{
								*fMixBufferPtr++ += Driver.fLastLeftValue;
								*fMixBufferPtr++ += Driver.fLastRightValue;

								Driver.fLastLeftValue  -= Driver.fLastLeftValue  * (1.0f / 4096.0f);
								Driver.fLastRightValue -= Driver.fLastRightValue * (1.0f / 4096.0f);
							}

							// update anti-click value for next mixing session
							fLastClickRemovalLeft  += Driver.fLastLeftValue;
							fLastClickRemovalRight += Driver.fLastRightValue;

							break;
						}
					}
				}
			}

			sc->fOldLeftVolume = sc->fCurrVolL;
			if (!Surround)
				sc->fOldRightVolume = sc->fCurrVolR;
		}

		sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
		               SF_UPDATE_MIXERVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
		               SF_LOOP_CHANGED    | SF_PAN_CHANGED);
	}
}

static void HQ_SetTempo(uint8_t Tempo)
{
	if (Tempo < LOWEST_BPM_POSSIBLE)
		Tempo = LOWEST_BPM_POSSIBLE;

	const uint32_t index = Tempo - LOWEST_BPM_POSSIBLE;

	BytesToMix = SamplesPerTickInt[index];
	BytesToMixFractional = SamplesPerTickFrac[index];
}

static void HQ_SetMixVolume(uint8_t Vol)
{
	MixVolume = Vol;
	RecalculateAllVolumes();
}

static void HQ_ResetMixer(void)
{
	MixTransferRemaining = 0;
	MixTransferOffset = 0;
	CurrentFractional = 0;
	RandSeed = 0x12345000;
	fLastClickRemovalLeft = fLastClickRemovalRight = 0.0f;

#if !CPU_32BIT
	dPrngStateL = dPrngStateR = 0.0;
#endif
}

static inline int32_t Random32(void)
{
	// LCG 32-bit random
	RandSeed *= 134775813;
	RandSeed++;

	return (int32_t)RandSeed;
}

static int32_t HQ_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput)
{
	int32_t out32;
#if !CPU_32BIT
	double dOut, dPrng;
#endif

	int32_t SamplesTodo = (SamplesToOutput == 0) ? RealBytesToMix : SamplesToOutput;
	for (int32_t i = 0; i < SamplesTodo; i++)
	{
#if CPU_32BIT // if 32-bit CPU, use single-precision float + no dithering (for speed)
		// left channel
		out32 = (int32_t)(fMixBuffer[MixTransferOffset++] * 32768.0f);
		CLAMP16(out32);
		*AudioOut16++ = (int16_t)out32;

		// right channel
		out32 = (int32_t)(fMixBuffer[MixTransferOffset++] * 32768.0f);
		CLAMP16(out32);
		*AudioOut16++ = (int16_t)out32;
#else
		// left channel - 1-bit triangular dithering
		dPrng = Random32() * (0.5 / INT32_MAX); // -0.5 .. 0.5
		dOut = (double)fMixBuffer[MixTransferOffset++] * 32768.0;
		dOut = (dOut + dPrng) - dPrngStateL;
		dPrngStateL = dPrng;
		out32 = (int32_t)dOut;
		CLAMP16(out32);
		*AudioOut16++ = (int16_t)out32;

		// right channel - 1-bit triangular dithering
		dPrng = Random32() * (0.5 / INT32_MAX); // -0.5 .. 0.5
		dOut = (double)fMixBuffer[MixTransferOffset++] * 32768.0;
		dOut = (dOut + dPrng) - dPrngStateR;
		dPrngStateR = dPrng;
		out32 = (int32_t)dOut;
		CLAMP16(out32);
		*AudioOut16++ = (int16_t)out32;
#endif
	}

	return SamplesTodo;
}

static void HQ_Mix(int32_t numSamples, int16_t *audioOut)
{
	int32_t SamplesLeft = numSamples;
	while (SamplesLeft > 0)
	{
		if (MixTransferRemaining == 0)
		{
			Update();
			HQ_MixSamples();
			MixTransferRemaining = RealBytesToMix;
		}

		int32_t SamplesToTransfer = SamplesLeft;
		if (SamplesToTransfer > MixTransferRemaining)
			SamplesToTransfer = MixTransferRemaining;

		HQ_PostMix(audioOut, SamplesToTransfer);
		audioOut += SamplesToTransfer * 2;

		MixTransferRemaining -= SamplesToTransfer;
		SamplesLeft -= SamplesToTransfer;
	}
}

static void HQ_CloseDriver(void)
{
	if (fMixBuffer != NULL)
	{
		free(fMixBuffer);
		fMixBuffer = NULL;
	}

	if (Driver.fSincLUT != NULL)
	{
		free(Driver.fSincLUT);
		Driver.fSincLUT = NULL;
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

bool HQ_InitDriver(int32_t mixingFrequency)
{
	if (mixingFrequency < 8000)
		mixingFrequency = 8000;

#if CPU_32BIT
	if (mixingFrequency > 64000)
		mixingFrequency = 64000;
#else
	if (mixingFrequency > 768000)
		mixingFrequency = 768000;
#endif

	const int32_t MaxSamplesToMix = (int32_t)ceil((mixingFrequency * 2.5) / LOWEST_BPM_POSSIBLE) + 1;

	fMixBuffer = (float *)malloc(MaxSamplesToMix * 2 * sizeof (float));
	if (fMixBuffer == NULL)
		return false;

	Driver.Flags = DF_SUPPORTS_MIDI | DF_USES_VOLRAMP | DF_HAS_RESONANCE_FILTER;
	Driver.NumChannels = 256;
	Driver.MixSpeed = mixingFrequency;
	Driver.Type = DRIVER_HQ;

	// calculate samples-per-tick tables
	for (int32_t i = LOWEST_BPM_POSSIBLE; i <= 255; i++)
	{
		const double dHz = i * (1.0 / 2.5);
		const double dSamplesPerTick = Driver.MixSpeed / dHz;

		// break into int/frac parts
		double dInt;
		const double dFrac = modf(dSamplesPerTick, &dInt);

		const uint32_t index = i - LOWEST_BPM_POSSIBLE;
		SamplesPerTickInt[index] = (uint32_t)dInt;
		SamplesPerTickFrac[index] = (uint32_t)((dFrac * BPM_FRAC_SCALE) + 0.5);
	}

	// setup driver functions
	DriverClose = HQ_CloseDriver;
	DriverMix = HQ_Mix;
	DriverSetTempo = HQ_SetTempo;
	DriverSetMixVolume = HQ_SetMixVolume;
	DriverFixSamples = NULL; // not used, we do it in realtime instead for accuracy
	DriverResetMixer = HQ_ResetMixer;
	DriverPostMix = HQ_PostMix;
	DriverMixSamples = HQ_MixSamples;

#if CPU_32BIT
	dFreq2DeltaMul = (double)(UINT16_MAX+1.0) / Driver.MixSpeed; // .16fp
#else
	dFreq2DeltaMul = (double)(UINT32_MAX+1.0) / Driver.MixSpeed; // .32fp
#endif

	return InitWindowedSincLUT();
}
