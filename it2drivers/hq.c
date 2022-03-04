/*
** ---- Custom high quality floating-point driver, by 8bitbubsy ----
**
** Behaves like the SB16 MMX driver when it comes to filter clamping,
** volume ramp speed and bidi looping.
**
** Features:
** - 4-tap cubic spline interpolation
** - Stereo sample support
** - 32.32 fixed-point sampling precision (32.16 in the original drivers)
** - Ended non-looping samples are ramped out, like the WAV writer driver
**
** TODO:
** - Implement 8-tap windowed-sinc interpolation, and calculate "left/right"
**   edge tap buffers for voice on SF_LOOP_CHANGED (flag) in the mixer.
**
** Compiling for 64-bit is ideal, as 64-bit divisions are used.
** All of the comments in this file are my own (8bb).
*/

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../it_structs.h"
#include "../it_music.h" // Update()
#include "hq.h"
#include "hq_m.h"
#include "zerovol.h"

// fast 32-bit -> 16-bit clamp (XXX: Maybe not faster in 2022?)
#define CLAMP16(i) if ((int16_t)i != i) i = INT16_MAX ^ ((int32_t)i >> 31)

#define BPM_FRAC_BITS 31 /* absolute max for 32-bit arithmetics, don't change! */
#define BPM_FRAC_SCALE (1UL << BPM_FRAC_BITS)
#define BPM_FRAC_MASK (BPM_FRAC_SCALE-1)

static uint16_t MixVolume;
static int32_t RealBytesToMix, BytesToMix, MixTransferRemaining, MixTransferOffset;
static uint32_t BytesToMixFractional, CurrentFractional, RandSeed;
static float *fMixBuffer, fLastClickRemovalLeft, fLastClickRemovalRight;
static double dFreq2DeltaMul, dPrngStateL, dPrngStateR;

float *fCubicLUT;

static bool InitCubicSplineLUT(void)
{
	fCubicLUT = (float *)malloc(CUBIC_PHASES * CUBIC_WIDTH * sizeof (float));
	if (fCubicLUT == NULL)
		return false;

	float *fLUTPtr = fCubicLUT;
	for (int32_t i = 0; i < CUBIC_PHASES; i++)
	{
		const double x = i * (1.0 / CUBIC_PHASES);
		const double x2 = x * x; // x^2
		const double x3 = x2 * x; // x^3

		*fLUTPtr++ = (float)(-0.5 * x3 + 1.0 * x2 - 0.5 * x);
		*fLUTPtr++ = (float)( 1.5 * x3 - 2.5 * x2 + 1.0);
		*fLUTPtr++ = (float)(-1.5 * x3 + 2.0 * x2 + 0.5 * x);
		*fLUTPtr++ = (float)( 0.5 * x3 - 0.5 * x2);
	}

	return true;
}

void HQ_MixSamples(void)
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

		fLastClickRemovalLeft  -= fLastClickRemovalLeft  * (1.0f / 4096.0f);
		fLastClickRemovalRight -= fLastClickRemovalRight * (1.0f / 4096.0f);
	}

	//memset(fMixBuffer, 0, RealBytesToMix * 2 * sizeof (float));

	slaveChn_t *sc = sChn;
	for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
			continue;

		sample_t *s = sc->SmpOffs;
		assert(s != NULL);

		if (sc->Flags & SF_NOTE_STOP) // note cut
		{
			sc->Flags &= ~SF_CHAN_ON;

			sc->vol16Bit = 0;
			sc->Flags |= SF_RECALC_FINALVOL;
		}

		if (sc->Flags & SF_FREQ_CHANGE)
		{
			if ((uint32_t)sc->Frequency > INT32_MAX) // added this as my own max limit
			{
				sc->Flags = SF_NOTE_STOP;
				if (!(sc->HCN & CHN_DISOWNED))
					((hostChn_t *)sc->HCOffst)->Flags &= ~HF_CHAN_ON; // turn off channel

				continue;
			}

			sc->Delta64 = (int64_t)((int32_t)sc->Frequency * dFreq2DeltaMul); // mixer delta (32.32fp)
		}

		if (sc->Flags & SF_NEW_NOTE)
		{
			sc->fCurrVolL = sc->fCurrVolR = 0.0f; // ramp in current voice (old note is ramped out in another voice)

			// clear filter state and filter coeffs
			sc->fOldSamples[0] = sc->fOldSamples[1] = sc->fOldSamples[2] = sc->fOldSamples[3] = 0.0f;
			sc->fFiltera = 1.0f;
			sc->fFilterb = sc->fFilterc = 0.0f;
		}

		if (sc->Flags & (SF_RECALC_FINALVOL | SF_LOOP_CHANGED | SF_PAN_CHANGED))
		{
			uint8_t FilterQ;

			if (sc->HCN & CHN_DISOWNED)
			{
				FilterQ = sc->MBank >> 8; // if disowned, use channel filters
			}
			else
			{
				uint8_t filterCutOff = Driver.FilterParameters[sc->HCN];
				FilterQ = Driver.FilterParameters[64+sc->HCN];

				sc->VEnvState.CurNode = (filterCutOff << 8) | (sc->VEnvState.CurNode & 0x00FF);
				sc->MBank = (FilterQ << 8) | (sc->MBank & 0x00FF);
			}

			// FilterEnvVal (0..255) * CutOff (0..127)
			const uint16_t FilterFreqValue = (sc->MBank & 0x00FF) * (uint8_t)((uint16_t)sc->VEnvState.CurNode >> 8);
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
				const int32_t Vol = sc->vol16Bit * MixVolume;
				if (!(Song.Header.Flags & ITF_STEREO)) // mono?
				{
					sc->fLeftVolume = sc->fRightVolume = Vol * (1.0f / (32768.0f * 128.0f));
				}
				else if (sc->FPP == PAN_SURROUND)
				{
					sc->fLeftVolume = sc->fRightVolume = Vol * (0.5f / (32768.0f * 128.0f));
				}
				else // normal (panned)
				{
					sc->fLeftVolume  = ((64-sc->FPP) * Vol) * (1.0f / (64.0f * 32768.0f * 128.0f));
					sc->fRightVolume = ((   sc->FPP) * Vol) * (1.0f / (64.0f * 32768.0f * 128.0f));
				}
			}
		}

		if (sc->Delta64 == 0)
			continue;

		uint32_t MixBlockSize = RealBytesToMix;

		const bool Surround = (sc->FPP == PAN_SURROUND);
		const bool Sample16Bit = !!(sc->Bit & SMPF_16BIT);
		const bool Stereo = !!(s->Flags & SMPF_STEREO);
		const bool FilterActive = (sc->fFilterb > 0.0f) || (sc->fFilterc > 0.0f);
		MixFunc_t Mix = HQ_MixFunctionTables[(FilterActive << 3) + (Stereo << 2) + (Surround << 1) + Sample16Bit];
		assert(Mix != NULL);

		// prepare volume ramp
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBeg; // also length for non-loopers
		if ((int32_t)LoopLength > 0)
		{
			float *fMixBufferPtr = fMixBuffer;
			if (sc->LpM == LOOP_PINGPONG)
			{
				while (MixBlockSize > 0)
				{
					uint32_t NewLoopPos;

					uint32_t SamplesToMix;
					if (sc->LpD == DIR_BACKWARDS)
					{
						SamplesToMix = sc->SamplingPosition - (sc->LoopBeg + 1);

						SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | (uint32_t)sc->Frac64) / sc->Delta64) + 1);
						Driver.Delta64 = 0 - sc->Delta64;
					}
					else // 8bb: forwards
					{
						SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;

						SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
						Driver.Delta64 = sc->Delta64;
					}

					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Mix(sc, fMixBufferPtr, SamplesToMix);

					MixBlockSize -= SamplesToMix;
					fMixBufferPtr += SamplesToMix << 1;

					if (sc->LpD == DIR_BACKWARDS)
					{
						if (sc->SamplingPosition <= sc->LoopBeg)
						{
							NewLoopPos = (uint32_t)(sc->LoopBeg - sc->SamplingPosition) % (LoopLength << 1);
							if (NewLoopPos >= LoopLength)
							{
								sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);
							}
							else
							{
								sc->LpD = DIR_FORWARDS;
								sc->SamplingPosition = sc->LoopBeg + NewLoopPos;
								sc->Frac64 = (uint32_t)(0 - sc->Frac64);
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
								sc->SamplingPosition = sc->LoopBeg + (NewLoopPos - LoopLength);
							}
							else
							{
								sc->LpD = DIR_BACKWARDS;
								sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
								sc->Frac64 = (uint32_t)(0 - sc->Frac64);
							}
						}
					}
				}
			}
			else if (sc->LpM == LOOP_FORWARDS)
			{
				while (MixBlockSize > 0)
				{
					uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;

					SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Driver.Delta64 = sc->Delta64;
					Mix(sc, fMixBufferPtr, SamplesToMix);

					MixBlockSize -= SamplesToMix;
					fMixBufferPtr += SamplesToMix << 1;

					if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						sc->SamplingPosition = sc->LoopBeg + ((uint32_t)(sc->SamplingPosition - sc->LoopEnd) % LoopLength);
				}
			}
			else // 8bb: no loop
			{
				while (MixBlockSize > 0)
				{
					uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;

					SamplesToMix = (uint32_t)(((((uint64_t)SamplesToMix << 32) | ((uint32_t)sc->Frac64 ^ UINT32_MAX)) / sc->Delta64) + 1);
					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Driver.Delta64 = sc->Delta64;
					Mix(sc, fMixBufferPtr, SamplesToMix);

					MixBlockSize -= SamplesToMix;
					fMixBufferPtr += SamplesToMix << 1;

					if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
					{
						sc->Flags = SF_NOTE_STOP;
						if (!(sc->HCN & CHN_DISOWNED))
							((hostChn_t *)sc->HCOffst)->Flags &= ~HF_CHAN_ON;

						// sample ended, ramp out very last sample point for the remaining samples
						for (; MixBlockSize > 0; MixBlockSize--)
						{
							*fMixBufferPtr++ += fLastLeftValue;
							*fMixBufferPtr++ += fLastRightValue;

							fLastLeftValue  -= fLastLeftValue  * (1.0f / 4096.0f);
							fLastRightValue -= fLastRightValue * (1.0f / 4096.0f);
						}

						// update anti-click value for next mixing session
						fLastClickRemovalLeft  += fLastLeftValue;
						fLastClickRemovalRight += fLastRightValue;

						break;
					}
				}
			}
		}

		sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
		               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
		               SF_LOOP_CHANGED    | SF_PAN_CHANGED);
	}
}

void HQ_SetTempo(uint8_t Tempo)
{
	assert(Tempo >= LOWEST_BPM_POSSIBLE);

	double dSamplesToMix = ((int32_t)Driver.MixSpeed * 2.5) / Tempo;
	double dSamplesToMixFrac = dSamplesToMix - (int32_t)dSamplesToMix;

	BytesToMix = (int32_t)dSamplesToMix;
	BytesToMixFractional = (int32_t)((dSamplesToMixFrac * BPM_FRAC_SCALE) + 0.5); // rounded 0.31fp
}

void HQ_SetMixVolume(uint8_t Vol)
{
	MixVolume = Vol;
	RecalculateAllVolumes();
}

void HQ_ResetMixer(void)
{
	MixTransferRemaining = 0;
	MixTransferOffset = 0;
	CurrentFractional = 0;
	RandSeed = 0x12345000;
	dPrngStateL = dPrngStateR = 0.0;
	fLastClickRemovalLeft = fLastClickRemovalRight = 0.0f;
}

static inline int32_t Random32(void)
{
	// LCG 32-bit random
	RandSeed *= 134775813;
	RandSeed++;

	return (int32_t)RandSeed;
}

int32_t HQ_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput)
{
	int32_t out32;
	double dOut, dPrng;

	int32_t SamplesTodo = (SamplesToOutput == 0) ? RealBytesToMix : SamplesToOutput;
	for (int32_t i = 0; i < SamplesTodo; i++)
	{
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
	}

	return SamplesTodo;
}

void HQ_Mix(int32_t numSamples, int16_t *audioOut)
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

/* Fixes sample end bytes for interpolation (yes, we have room after the data).
** Samples with sustain loop are not fixed (too complex to get right).
*/
void HQ_FixSamples(void)
{
	sample_t *s = Song.Smp;
	for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		if (s->Data == NULL || s->Length == 0)
			continue;

		const bool Sample16Bit = !!(s->Flags & SMPF_16BIT);
		const bool HasLoop = !!(s->Flags & SMPF_USE_LOOP);

		int16_t *Data16 = (int16_t *)s->Data;
		int16_t *Data16R = (int16_t *)s->DataR;
		int8_t *Data8 = (int8_t *)s->Data;
		int8_t *Data8R = (int8_t *)s->DataR;

		/* All negative taps should be equal to the first sample point when at sampling
		** position #0 (on sample trigger).
		*/
		if (Sample16Bit)
		{
			Data16[-1] = Data16[0];
			if (Data16R != NULL) // right sample (if present)
				Data16R[-1] = Data16R[0];
		}
		else
		{
			Data8[-1] = Data8[0];
			if (Data8R != NULL) // right sample (if present)
				Data8R[-1] = Data8R[0];
		}

		if (Sample16Bit)
		{
			// 16 bit

			if (HasLoop)
			{
				if (s->Flags & SMPF_LOOP_PINGPONG)
				{
					int32_t LastSample = s->LoopEnd;
					if (LastSample < 0)
						LastSample = 0;

					Data16[s->LoopEnd+0] = Data16[LastSample];
					if (LastSample > 0)
						Data16[s->LoopEnd+1] = Data16[LastSample-1];
					else
						Data16[s->LoopEnd+1] = Data16[LastSample];

					// right sample (if present)
					if (Data16R != NULL)
					{
						Data16R[s->LoopEnd+0] = Data16R[LastSample];
						if (LastSample > 0)
							Data16R[s->LoopEnd+1] = Data16R[LastSample-1];
						else
							Data16R[s->LoopEnd+1] = Data16R[LastSample];
					}

					/* For bidi loops:
					** The loopstart point is never read after having looped once.
					** IT2 behaves like that. It loops loopstart+1 to loopend-1.
					** As such, there's no point in modifying the -1 point.
					** We already set the -1 point to the 0 point above.
					*/
				}
				else
				{
					if (s->LoopBeg == 0)
						Data16[-1] = Data16[s->LoopEnd-1];

					Data16[s->LoopEnd+0] = Data16[s->LoopBeg+0];
					Data16[s->LoopEnd+1] = Data16[s->LoopBeg+1];

					// right sample (if present)
					if (Data16R != NULL)
					{
						if (s->LoopBeg == 0)
							Data16R[-1] = Data16R[s->LoopEnd-1];

						Data16R[s->LoopEnd+0] = Data16R[s->LoopBeg+0];
						Data16R[s->LoopEnd+1] = Data16R[s->LoopBeg+1];
					}
				}
			}
			else
			{
				Data16[s->Length+0] = Data16[s->Length-1];
				Data16[s->Length+1] = Data16[s->Length-1];

				// right sample (if present)
				if (Data16R != NULL)
				{
					Data16R[s->Length+0] = Data16R[s->Length-1];
					Data16R[s->Length+1] = Data16R[s->Length-1];
				}
			}
		}
		else
		{
			// 8 bit

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

					// right sample (if present)
					if (Data8R != NULL)
					{
						Data8R[s->LoopEnd+0] = Data8R[LastSample];
						if (LastSample > 0)
							Data8R[s->LoopEnd+1] = Data8R[LastSample-1];
						else
							Data8R[s->LoopEnd+1] = Data8R[LastSample];

					}

					/* For bidi loops:
					** The loopstart point is never read after having looped once.
					** IT2 behaves like that. It loops loopstart+1 to loopend-1.
					** As such, there's no point in modifying the -1 point.
					** We already set the -1 point to the 0 point above.
					*/
				}
				else
				{
					if (s->LoopBeg == 0)
						Data8[-1] = Data8[s->LoopEnd-1];

					Data8[s->LoopEnd+0] = Data8[s->LoopBeg+0];
					Data8[s->LoopEnd+1] = Data8[s->LoopBeg+1];

					// right sample (if present)
					if (Data8R != NULL)
					{
						if (s->LoopBeg == 0)
							Data8R[-1] = Data8R[s->LoopEnd-1];

						Data8R[s->LoopEnd+0] = Data8R[s->LoopBeg+0];
						Data8R[s->LoopEnd+1] = Data8R[s->LoopBeg+1];
					}
				}
			}
			else
			{
				Data8[s->Length+0] = Data8[s->Length-1];
				Data8[s->Length+1] = Data8[s->Length-1];

				// right sample (if present)
				if (Data8R != NULL)
				{
					Data8R[s->Length+0] = Data8R[s->Length-1];
					Data8R[s->Length+1] = Data8R[s->Length-1];
				}
			}
		}
	}
}

bool HQ_InitSound(int32_t mixingFrequency)
{
	if (mixingFrequency < 8000)
		mixingFrequency = 8000;
	else if (mixingFrequency > 768000)
		mixingFrequency = 768000;

	const int32_t MaxSamplesToMix = (int32_t)(((mixingFrequency * 2.5) / LOWEST_BPM_POSSIBLE) + 1.0);

	fMixBuffer = (float *)malloc(MaxSamplesToMix * 2 * sizeof (float));
	if (fMixBuffer == NULL)
		return false;

	Driver.MixSpeed = mixingFrequency;
	Driver.Type = DRIVER_HQ;

	dFreq2DeltaMul = (double)(UINT32_MAX+1.0) / mixingFrequency;
	return InitCubicSplineLUT();
}

void HQ_UninitSound(void)
{
	if (fMixBuffer != NULL)
	{
		free(fMixBuffer);
		fMixBuffer = NULL;
	}

	if (fCubicLUT != NULL)
	{
		free(fCubicLUT);
		fCubicLUT = NULL;
	}
}
