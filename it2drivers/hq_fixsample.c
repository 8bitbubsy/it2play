/* Routines for changing sample data before mixing (and reverting after mix) for
** interpolation taps to be read the correctly. This is a bit messy...
*/

#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"

/* TODO:
** The ping-pong sample fixer needs more work, some edge-cases are off.
** If I remember correctly, it's only bugged for extremely short loops.
*/
void fixSamplesPingpong(sample_t *s, slaveChn_t *sc)
{
	const int32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

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

			if (LoopLength >= 4)
			{
				loopBegin[-1] = loopBegin[0];
				loopBegin[-2] = loopBegin[1];
				loopBegin[-3] = loopBegin[2];
			}
			else
			{
				// loop is too short, some logic is needed (XXX: I think this is broken?)
				bool backwards = false;
				int32_t offset = sc->LoopBegin;
				for (int32_t i = 0; i < 3; i++)
				{
					if (backwards)
					{
						if (offset < sc->LoopBegin) { offset = sc->LoopBegin; backwards ^= 1; }
					}
					else
					{
						if (offset >= sc->LoopEnd) { offset = sc->LoopEnd - 1; backwards ^= 1; }
					}

					*--loopBegin = ptr[offset];

					if (backwards)
						offset--;
					else
						offset++;
				}
			}
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
			loopEnd[0] = loopEnd[-1];
			loopEnd[1] = loopEnd[-2];
			loopEnd[2] = loopEnd[-3];
			loopEnd[3] = loopEnd[-4];
		}
	}
}

void unfixSamplesPingpong(sample_t *s, slaveChn_t *sc)
{
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

			if (LoopLength >= 4)
			{
				loopBegin[-1] = loopEnd[-1];
				loopBegin[-2] = loopEnd[-2];
				loopBegin[-3] = loopEnd[-3];
			}
			else
			{
				// loop is too short, some logic is needed
				int32_t offset = sc->LoopEnd - 1;
				if (offset < sc->LoopBegin) offset += LoopLength;
				loopBegin[-1] = ptr[offset];
				offset--;
				if (offset < sc->LoopBegin) offset += LoopLength;
				loopBegin[-2] = ptr[offset];
				offset--;
				if (offset < sc->LoopBegin) offset += LoopLength;
				loopBegin[-3] = ptr[offset];
			}
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
		if (LoopLength >= 4)
		{
			loopEnd[1] = loopBegin[1];
			loopEnd[2] = loopBegin[2];
			loopEnd[3] = loopBegin[3];
		}
		else
		{
			// loop is too short, some logic is needed
			int32_t offset = sc->LoopBegin + 1;
			if (offset >= sc->LoopEnd) offset -= LoopLength;
			loopEnd[1] = ptr[offset];
			offset++;
			if (offset >= sc->LoopEnd) offset -= LoopLength;
			loopEnd[2] = ptr[offset];
			offset++;
			if (offset >= sc->LoopEnd) offset -= LoopLength;
			loopEnd[3] = ptr[offset];
		}

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

				if (LoopLength >= 4)
				{
					loopBegin[-1] = loopEnd[-1];
					loopBegin[-2] = loopEnd[-2];
					loopBegin[-3] = loopEnd[-3];
				}
				else
				{
					// loop is too short, some logic is needed
					int32_t offset = sc->LoopEnd - 1;
					if (offset < sc->LoopBegin) offset += LoopLength;
					loopBegin[-1] = ptr[offset];
					offset--;
					if (offset < sc->LoopBegin) offset += LoopLength;
					loopBegin[-2] = ptr[offset];
					offset--;
					if (offset < sc->LoopBegin) offset += LoopLength;
					loopBegin[-3] = ptr[offset];
				}
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
			if (LoopLength >= 4)
			{
				loopEnd[1] = loopBegin[1];
				loopEnd[2] = loopBegin[2];
				loopEnd[3] = loopBegin[3];
			}
			else
			{
				// loop is too short, some logic is needed
				int32_t offset = sc->LoopBegin + 1;
				if (offset >= sc->LoopEnd) offset -= LoopLength;
				loopEnd[1] = ptr[offset];
				offset++;
				if (offset >= sc->LoopEnd) offset -= LoopLength;
				loopEnd[2] = ptr[offset];
				offset++;
				if (offset >= sc->LoopEnd) offset -= LoopLength;
				loopEnd[3] = ptr[offset];
			}
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

			if (LoopLength >= 4)
			{
				loopBegin[-1] = loopEnd[-1];
				loopBegin[-2] = loopEnd[-2];
				loopBegin[-3] = loopEnd[-3];
			}
			else
			{
				// loop is too short, some logic is needed
				int32_t offset = sc->LoopEnd - 1;
				if (offset < sc->LoopBegin) offset += LoopLength;
				loopBegin[-1] = ptr[offset];
				offset--;
				if (offset < sc->LoopBegin) offset += LoopLength;
				loopBegin[-2] = ptr[offset];
				offset--;
				if (offset < sc->LoopBegin) offset += LoopLength;
				loopBegin[-3] = ptr[offset];
			}
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
		if (LoopLength >= 4)
		{
			loopEnd[1] = loopBegin[1];
			loopEnd[2] = loopBegin[2];
			loopEnd[3] = loopBegin[3];
		}
		else
		{
			// loop is too short, some logic is needed
			int32_t offset = sc->LoopBegin + 1;
			if (offset >= sc->LoopEnd) offset -= LoopLength;
			loopEnd[1] = ptr[offset];
			offset++;
			if (offset >= sc->LoopEnd) offset -= LoopLength;
			loopEnd[2] = ptr[offset];
			offset++;
			if (offset >= sc->LoopEnd) offset -= LoopLength;
			loopEnd[3] = ptr[offset];
		}

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

				if (LoopLength >= 4)
				{
					loopBegin[-1] = loopEnd[-1];
					loopBegin[-2] = loopEnd[-2];
					loopBegin[-3] = loopEnd[-3];
				}
				else
				{
					// loop is too short, some logic is needed
					int32_t offset = sc->LoopEnd - 1;
					if (offset < sc->LoopBegin) offset += LoopLength;
					loopBegin[-1] = ptr[offset];
					offset--;
					if (offset < sc->LoopBegin) offset += LoopLength;
					loopBegin[-2] = ptr[offset];
					offset--;
					if (offset < sc->LoopBegin) offset += LoopLength;
					loopBegin[-3] = ptr[offset];
				}
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
			if (LoopLength >= 4)
			{
				loopEnd[1] = loopBegin[1];
				loopEnd[2] = loopBegin[2];
				loopEnd[3] = loopBegin[3];
			}
			else
			{
				// loop is too short, some logic is needed
				int32_t offset = sc->LoopBegin + 1;
				if (offset >= sc->LoopEnd) offset -= LoopLength;
				loopEnd[1] = ptr[offset];
				offset++;
				if (offset >= sc->LoopEnd) offset -= LoopLength;
				loopEnd[2] = ptr[offset];
				offset++;
				if (offset >= sc->LoopEnd) offset -= LoopLength;
				loopEnd[3] = ptr[offset];
			}
		}
	}
}

void unfixSamplesFwdLoop(sample_t *s, slaveChn_t *sc)
{
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
		int16_t *begin = (int16_t *)s->Data;
		int16_t *end = (int16_t *)s->Data + sc->LoopEnd;

		begin[-1] = begin[0];
		begin[-2] = begin[0];
		begin[-3] = begin[0];
		end[0] = end[-1];
		end[1] = end[-1];
		end[2] = end[-1];
		end[3] = end[-1];

		if (s->DataR != NULL) // right sample (if present)
		{
			begin = (int16_t *)s->DataR;
			end = (int16_t *)s->DataR + sc->LoopEnd;

			begin[-1] = begin[0];
			begin[-2] = begin[0];
			begin[-3] = begin[0];
			end[0] = end[-1];
			end[1] = end[-1];
			end[2] = end[-1];
			end[3] = end[-1];
		}
	}
	else
	{
		int8_t *begin = (int8_t *)s->Data;
		int8_t *end = (int8_t *)s->Data + sc->LoopEnd;

		begin[-1] = begin[0];
		begin[-2] = begin[0];
		begin[-3] = begin[0];
		end[0] = end[-1];
		end[1] = end[-1];
		end[2] = end[-1];
		end[3] = end[-1];

		if (s->DataR != NULL) // right sample (if present)
		{
			begin = (int8_t *)s->DataR;
			end = (int8_t *)s->DataR + sc->LoopEnd;

			begin[-1] = begin[0];
			begin[-2] = begin[0];
			begin[-3] = begin[0];
			end[0] = end[-1];
			end[1] = end[-1];
			end[2] = end[-1];
			end[3] = end[-1];
		}
	}
}
