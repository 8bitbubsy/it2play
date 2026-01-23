/* Routines for changing sample data before mixing (and reverting after mix) for
** interpolation taps to be read the correctly. This is a bit messy...
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../it_structs.h"

void fixSamplesPingpong(sample_t *s, slaveChn_t *sc)
{
	const int32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
	if (LoopLength <= 4) // This requires complicated logic. As this is rare, don't bother.
		return;

	if (sc->SmpIs16Bit)
	{
		int16_t *ptr = (int16_t *)s->Data;
		int16_t *loopBegin = (int16_t *)s->Data + sc->LoopBegin;
		int16_t *loopEnd = (int16_t *)s->Data + sc->LoopEnd;

		if (sc->HasLooped)
		{
			sc->leftTmpSamples16[0] = loopBegin[-1];
			sc->leftTmpSamples16[1] = loopBegin[-2];
			sc->leftTmpSamples16[2] = loopBegin[-3];
			loopBegin[-1] = loopBegin[0];
			loopBegin[-2] = loopBegin[1];
			loopBegin[-3] = loopBegin[2];
		}
		else
		{
			ptr[-1] = ptr[0];
			ptr[-2] = ptr[0];
			ptr[-3] = ptr[0];
		}

		sc->rightTmpSamples16[0] = loopEnd[0];
		sc->rightTmpSamples16[1] = loopEnd[1];
		sc->rightTmpSamples16[2] = loopEnd[2];
		sc->rightTmpSamples16[3] = loopEnd[3];
		loopEnd[0] = loopEnd[-1];
		loopEnd[1] = loopEnd[-2];
		loopEnd[2] = loopEnd[-3];
		loopEnd[3] = loopEnd[-4];

		if (s->DataR != NULL) // right sample (if present)
		{
			ptr = (int16_t *)s->DataR;
			loopBegin = (int16_t *)s->DataR + sc->LoopBegin;
			loopEnd = (int16_t *)s->DataR + sc->LoopEnd;

			if (sc->HasLooped)
			{
				sc->leftTmpSamples16_R[0] = loopBegin[-1];
				sc->leftTmpSamples16_R[1] = loopBegin[-2];
				sc->leftTmpSamples16_R[2] = loopBegin[-3];
				loopBegin[-1] = loopBegin[0];
				loopBegin[-2] = loopBegin[1];
				loopBegin[-3] = loopBegin[2];
			}
			else
			{
				ptr[-1] = ptr[0];
				ptr[-2] = ptr[0];
				ptr[-3] = ptr[0];
			}

			sc->rightTmpSamples16_R[0] = loopEnd[0];
			sc->rightTmpSamples16_R[1] = loopEnd[1];
			sc->rightTmpSamples16_R[2] = loopEnd[2];
			sc->rightTmpSamples16_R[3] = loopEnd[3];
			loopEnd[0] = loopEnd[-1];
			loopEnd[1] = loopEnd[-2];
			loopEnd[2] = loopEnd[-3];
			loopEnd[3] = loopEnd[-4];
		}
	}
	else
	{
		// 8-BIT SAMPLE

		int8_t *ptr = (int8_t *)s->Data;
		int8_t *loopBegin = (int8_t *)s->Data + sc->LoopBegin;
		int8_t *loopEnd = (int8_t *)s->Data + sc->LoopEnd;

		if (sc->HasLooped)
		{
			sc->leftTmpSamples8[0] = loopBegin[-1];
			sc->leftTmpSamples8[1] = loopBegin[-2];
			sc->leftTmpSamples8[2] = loopBegin[-3];
			loopBegin[-1] = loopBegin[0];
			loopBegin[-2] = loopBegin[1];
			loopBegin[-3] = loopBegin[2];
		}
		else
		{
			ptr[-1] = ptr[0];
			ptr[-2] = ptr[0];
			ptr[-3] = ptr[0];
		}

		sc->rightTmpSamples8[0] = loopEnd[0];
		sc->rightTmpSamples8[1] = loopEnd[1];
		sc->rightTmpSamples8[2] = loopEnd[2];
		sc->rightTmpSamples8[3] = loopEnd[3];
		loopEnd[0] = loopEnd[-1];
		loopEnd[1] = loopEnd[-2];
		loopEnd[2] = loopEnd[-3];
		loopEnd[3] = loopEnd[-4];

		if (s->DataR != NULL) // right sample (if present)
		{
			ptr = (int8_t *)s->DataR;
			loopBegin = (int8_t *)s->DataR + sc->LoopBegin;
			loopEnd = (int8_t *)s->DataR + sc->LoopEnd;

			if (sc->HasLooped)
			{
				sc->leftTmpSamples8_R[0] = loopBegin[-1];
				sc->leftTmpSamples8_R[1] = loopBegin[-2];
				sc->leftTmpSamples8_R[2] = loopBegin[-3];
				loopBegin[-1] = loopBegin[0];
				loopBegin[-2] = loopBegin[1];
				loopBegin[-3] = loopBegin[2];
		
			}
			else
			{
				ptr[-1] = ptr[0];
				ptr[-2] = ptr[0];
				ptr[-3] = ptr[0];
			}

			sc->rightTmpSamples8_R[0] = loopEnd[0];
			sc->rightTmpSamples8_R[1] = loopEnd[1];
			sc->rightTmpSamples8_R[2] = loopEnd[2];
			sc->rightTmpSamples8_R[3] = loopEnd[3];
			loopEnd[0] = loopEnd[-1];
			loopEnd[1] = loopEnd[-2];
			loopEnd[2] = loopEnd[-3];
			loopEnd[3] = loopEnd[-4];
		}
	}
}

void unfixSamplesPingpong(sample_t *s, slaveChn_t *sc)
{
	const int32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
	if (LoopLength <= 4) // This requires complicated logic. As this is rare, don't bother.
		return;

	if (sc->SmpIs16Bit)
	{
		int16_t *loopEnd = (int16_t *)s->Data + sc->LoopEnd;

		loopEnd[0] = sc->rightTmpSamples16[0];
		loopEnd[1] = sc->rightTmpSamples16[1];
		loopEnd[2] = sc->rightTmpSamples16[2];
		loopEnd[3] = sc->rightTmpSamples16[3];

		if (sc->HasLooped)
		{
			int16_t *loopBegin = (int16_t *)s->Data + sc->LoopBegin;
			loopBegin[-1] = sc->leftTmpSamples16[0];
			loopBegin[-2] = sc->leftTmpSamples16[1];
			loopBegin[-3] = sc->leftTmpSamples16[2];
		}

		if (s->DataR != NULL) // right sample (if present)
		{
			loopEnd = (int16_t *)s->DataR + sc->LoopEnd;

			loopEnd[0] = sc->rightTmpSamples16_R[0];
			loopEnd[1] = sc->rightTmpSamples16_R[1];
			loopEnd[2] = sc->rightTmpSamples16_R[2];
			loopEnd[3] = sc->rightTmpSamples16_R[3];

			if (sc->HasLooped)
			{
				int16_t *loopBegin = (int16_t *)s->DataR + sc->LoopBegin;
				loopBegin[-1] = sc->leftTmpSamples16_R[0];
				loopBegin[-2] = sc->leftTmpSamples16_R[1];
				loopBegin[-3] = sc->leftTmpSamples16_R[2];
			}
		}
	}
	else
	{
		int8_t *loopEnd = (int8_t *)s->Data + sc->LoopEnd;

		loopEnd[0] = sc->rightTmpSamples8[0];
		loopEnd[1] = sc->rightTmpSamples8[1];
		loopEnd[2] = sc->rightTmpSamples8[2];
		loopEnd[3] = sc->rightTmpSamples8[3];

		if (sc->HasLooped)
		{
			int8_t *loopBegin = (int8_t *)s->Data + sc->LoopBegin;
			loopBegin[-1] = sc->leftTmpSamples8[0];
			loopBegin[-2] = sc->leftTmpSamples8[1];
			loopBegin[-3] = sc->leftTmpSamples8[2];
		}

		if (s->DataR != NULL) // right sample (if present)
		{
			loopEnd = (int8_t *)s->DataR + sc->LoopEnd;

			loopEnd[0] = sc->rightTmpSamples8_R[0];
			loopEnd[1] = sc->rightTmpSamples8_R[1];
			loopEnd[2] = sc->rightTmpSamples8_R[2];
			loopEnd[3] = sc->rightTmpSamples8_R[3];

			if (sc->HasLooped)
			{
				int8_t *loopBegin = (int8_t *)s->DataR + sc->LoopBegin;
				loopBegin[-1] = sc->leftTmpSamples8_R[0];
				loopBegin[-2] = sc->leftTmpSamples8_R[1];
				loopBegin[-3] = sc->leftTmpSamples8_R[2];
			}
		}
	}
}

void fixSamplesFwdLoop(sample_t *s, slaveChn_t *sc)
{
	const int32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
	if (LoopLength <= 4) // This requires complicated logic. As this is rare, don't bother.
		return;

	if (sc->SmpIs16Bit)
	{
		int16_t *ptr = (int16_t *)s->Data;
		int16_t *loopBegin = (int16_t *)s->Data + sc->LoopBegin;
		int16_t *loopEnd = (int16_t *)s->Data + sc->LoopEnd;

		if (sc->HasLooped)
		{
			sc->leftTmpSamples16[0] = loopBegin[-1];
			sc->leftTmpSamples16[1] = loopBegin[-2];
			sc->leftTmpSamples16[2] = loopBegin[-3];
			loopBegin[-1] = loopEnd[-1];
			loopBegin[-2] = loopEnd[-2];
			loopBegin[-3] = loopEnd[-3];
		}
		else
		{
			ptr[-1] = ptr[0];
			ptr[-2] = ptr[0];
			ptr[-3] = ptr[0];
		}

		sc->rightTmpSamples16[0] = loopEnd[0];
		sc->rightTmpSamples16[1] = loopEnd[1];
		sc->rightTmpSamples16[2] = loopEnd[2];
		sc->rightTmpSamples16[3] = loopEnd[3];
		loopEnd[0] = loopBegin[0];
		loopEnd[1] = loopBegin[1];
		loopEnd[2] = loopBegin[2];
		loopEnd[3] = loopBegin[3];

		if (s->DataR != NULL) // right sample (if present)
		{
			ptr = (int16_t *)s->DataR;
			loopBegin = (int16_t *)s->DataR + sc->LoopBegin;
			loopEnd = (int16_t *)s->DataR + sc->LoopEnd;

			if (sc->HasLooped)
			{
				sc->leftTmpSamples16_R[0] = loopBegin[-1];
				sc->leftTmpSamples16_R[1] = loopBegin[-2];
				sc->leftTmpSamples16_R[2] = loopBegin[-3];
				loopBegin[-1] = loopEnd[-1];
				loopBegin[-2] = loopEnd[-2];
				loopBegin[-3] = loopEnd[-3];
			}
			else
			{
				ptr[-1] = ptr[0];
				ptr[-2] = ptr[0];
				ptr[-3] = ptr[0];
			}

			sc->rightTmpSamples16_R[0] = loopEnd[0];
			sc->rightTmpSamples16_R[1] = loopEnd[1];
			sc->rightTmpSamples16_R[2] = loopEnd[2];
			sc->rightTmpSamples16_R[3] = loopEnd[3];
			loopEnd[0] = loopBegin[0];
			loopEnd[1] = loopBegin[1];
			loopEnd[2] = loopBegin[2];
			loopEnd[3] = loopBegin[3];
		}
	}
	else
	{
		int8_t *ptr = (int8_t *)s->Data;
		int8_t *loopBegin = (int8_t *)s->Data + sc->LoopBegin;
		int8_t *loopEnd = (int8_t *)s->Data + sc->LoopEnd;

		if (sc->HasLooped)
		{
			sc->leftTmpSamples8[0] = loopBegin[-1];
			sc->leftTmpSamples8[1] = loopBegin[-2];
			sc->leftTmpSamples8[2] = loopBegin[-3];
			loopBegin[-1] = loopEnd[-1];
			loopBegin[-2] = loopEnd[-2];
			loopBegin[-3] = loopEnd[-3];
		}
		else
		{
			ptr[-1] = ptr[0];
			ptr[-2] = ptr[0];
			ptr[-3] = ptr[0];
		}

		sc->rightTmpSamples8[0] = loopEnd[0];
		sc->rightTmpSamples8[1] = loopEnd[1];
		sc->rightTmpSamples8[2] = loopEnd[2];
		sc->rightTmpSamples8[3] = loopEnd[3];
		loopEnd[0] = loopBegin[0];
		loopEnd[1] = loopBegin[1];
		loopEnd[2] = loopBegin[2];
		loopEnd[3] = loopBegin[3];

		if (s->DataR != NULL) // right sample (if present)
		{
			ptr = (int8_t *)s->DataR;
			loopBegin = (int8_t *)s->DataR + sc->LoopBegin;
			loopEnd = (int8_t *)s->DataR + sc->LoopEnd;

			if (sc->HasLooped)
			{
				sc->leftTmpSamples8_R[0] = loopBegin[-1];
				sc->leftTmpSamples8_R[1] = loopBegin[-2];
				sc->leftTmpSamples8_R[2] = loopBegin[-3];
				loopBegin[-1] = loopEnd[-1];
				loopBegin[-2] = loopEnd[-2];
				loopBegin[-3] = loopEnd[-3];
			}
			else
			{
				ptr[-1] = ptr[0];
				ptr[-2] = ptr[0];
				ptr[-3] = ptr[0];
			}

			sc->rightTmpSamples8_R[0] = loopEnd[0];
			sc->rightTmpSamples8_R[1] = loopEnd[1];
			sc->rightTmpSamples8_R[2] = loopEnd[2];
			sc->rightTmpSamples8_R[3] = loopEnd[3];
			loopEnd[0] = loopBegin[0];
			loopEnd[1] = loopBegin[1];
			loopEnd[2] = loopBegin[2];
			loopEnd[3] = loopBegin[3];
		}
	}
}

void unfixSamplesFwdLoop(sample_t *s, slaveChn_t *sc)
{
	const int32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
	if (LoopLength <= 4) // This requires complicated logic. As this is rare, don't bother.
		return;

	if (sc->SmpIs16Bit)
	{
		int16_t *loopEnd = (int16_t *)s->Data + sc->LoopEnd;

		loopEnd[0] = sc->rightTmpSamples16[0];
		loopEnd[1] = sc->rightTmpSamples16[1];
		loopEnd[2] = sc->rightTmpSamples16[2];
		loopEnd[3] = sc->rightTmpSamples16[3];

		if (sc->HasLooped)
		{
			int16_t *loopBegin = (int16_t *)s->Data + sc->LoopBegin;
			loopBegin[-1] = sc->leftTmpSamples16[0];
			loopBegin[-2] = sc->leftTmpSamples16[1];
			loopBegin[-3] = sc->leftTmpSamples16[2];
		}

		if (s->DataR != NULL) // right sample (if present)
		{
			loopEnd = (int16_t *)s->DataR + sc->LoopEnd;

			loopEnd[0] = sc->rightTmpSamples16_R[0];
			loopEnd[1] = sc->rightTmpSamples16_R[1];
			loopEnd[2] = sc->rightTmpSamples16_R[2];
			loopEnd[3] = sc->rightTmpSamples16_R[3];

			if (sc->HasLooped)
			{
				int16_t *loopBegin = (int16_t *)s->DataR + sc->LoopBegin;
				loopBegin[-1] = sc->leftTmpSamples16_R[0];
				loopBegin[-2] = sc->leftTmpSamples16_R[1];
				loopBegin[-3] = sc->leftTmpSamples16_R[2];
			}
		}
	}
	else
	{
		int8_t *loopEnd = (int8_t *)s->Data + sc->LoopEnd;

		loopEnd[0] = sc->rightTmpSamples8[0];
		loopEnd[1] = sc->rightTmpSamples8[1];
		loopEnd[2] = sc->rightTmpSamples8[2];
		loopEnd[3] = sc->rightTmpSamples8[3];

		if (sc->HasLooped)
		{
			int8_t *loopBegin = (int8_t *)s->Data + sc->LoopBegin;
			loopBegin[-1] = sc->leftTmpSamples8[0];
			loopBegin[-2] = sc->leftTmpSamples8[1];
			loopBegin[-3] = sc->leftTmpSamples8[2];
		}

		if (s->DataR != NULL) // right sample (if present)
		{
			loopEnd = (int8_t *)s->DataR + sc->LoopEnd;

			loopEnd[0] = sc->rightTmpSamples8_R[0];
			loopEnd[1] = sc->rightTmpSamples8_R[1];
			loopEnd[2] = sc->rightTmpSamples8_R[2];
			loopEnd[3] = sc->rightTmpSamples8_R[3];

			if (sc->HasLooped)
			{
				int8_t *loopBegin = (int8_t *)s->DataR + sc->LoopBegin;
				loopBegin[-1] = sc->leftTmpSamples8_R[0];
				loopBegin[-2] = sc->leftTmpSamples8_R[1];
				loopBegin[-3] = sc->leftTmpSamples8_R[2];
			}
		}
	}
}

void fixSamplesNoLoop(sample_t *s, slaveChn_t *sc)
{
	if (sc->SmpIs16Bit)
	{
		int16_t *data = (int16_t *)s->Data;
		int16_t *end = (int16_t *)s->Data + sc->LoopEnd;

		data[-1] = data[0];
		data[-2] = data[0];
		data[-3] = data[0];
		end[0] = end[-1];
		end[1] = end[-1];
		end[2] = end[-1];
		end[3] = end[-1];

		if (s->DataR != NULL) // right sample (if present)
		{
			data = (int16_t *)s->DataR;
			end = (int16_t *)s->DataR + sc->LoopEnd;

			data[-1] = data[0];
			data[-2] = data[0];
			data[-3] = data[0];
			end[0] = end[-1];
			end[1] = end[-1];
			end[2] = end[-1];
			end[3] = end[-1];
		}
	}
	else
	{
		int8_t *data = (int8_t *)s->Data;
		int8_t *end = (int8_t *)s->Data + sc->LoopEnd;

		data[-1] = data[0];
		data[-2] = data[0];
		data[-3] = data[0];
		end[0] = end[-1];
		end[1] = end[-1];
		end[2] = end[-1];
		end[3] = end[-1];

		if (s->DataR != NULL) // right sample (if present)
		{
			data = (int8_t *)s->DataR;
			end = (int8_t *)s->DataR + sc->LoopEnd;

			data[-1] = data[0];
			data[-2] = data[0];
			data[-3] = data[0];
			end[0] = end[-1];
			end[1] = end[-1];
			end[2] = end[-1];
			end[3] = end[-1];
		}
	}
}
