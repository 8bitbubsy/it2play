/*
** 8bb: IT2 module loading routines
**
** NOTE: This file is not directly ported from the IT2 code,
**       so routines have non-original names. All comments in
**       this file are by me (8bitbubsy).
*/

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "loaders/mmcmp/mmcmp.h"
#include "it_structs.h"
#include "it_music.h"
#include "it_d_rm.h"
#include "loaders/it.h"
#include "loaders/mod.h"

#define NO_NOTE 253

static bool firstTimeLoading = true;
static uint8_t EncodingInfo[MAX_HOST_CHANNELS*6];

uint8_t PatternDataArea[MAX_HOST_CHANNELS*200*5]; // 8bb: globalized

void ClearEncodingInfo(void)
{
	uint8_t *Enc = EncodingInfo;
	for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, Enc += 6)
	{
		Enc[0] = 0;       // mask
		Enc[1] = NO_NOTE; // note
		Enc[2] = 0;       // ins
		Enc[3] = 255;     // vol
		Enc[4] = 0;       // cmd
		Enc[5] = 0;       // value
	}
}

void ClearPatternData(void)
{
	uint8_t *Src = PatternDataArea;
	for (int32_t i = 0; i < 200; i++)
	{
		for (int32_t j = 0; j < MAX_HOST_CHANNELS; j++, Src += 5)
		{
			Src[0] = NO_NOTE; // note
			Src[1] = 0;       // ins
			Src[2] = 255;     // vol
			Src[3] = 0;       // cmd
			Src[4] = 0;       // value
		}
	}
}

bool GetPatternLength(uint16_t Rows, uint16_t *LengthOut)
{
	ClearEncodingInfo();

	uint8_t *Src = PatternDataArea;
	uint32_t Bytes = Rows; // End of row bytes added.

	for (int32_t i = 0; i < Rows; i++)
	{
		uint8_t *Enc = EncodingInfo;
		for (int32_t j = 0; j < MAX_HOST_CHANNELS; j++, Src += 5, Enc += 6)
		{
			if (Src[0] == NO_NOTE && Src[1] == 0 && Src[2] == 255 && Src[3] == 0 && Src[4] == 0)
				continue;

			Bytes++; // 1 byte for channel indication

			uint8_t Mask = 0;

			uint8_t Note = Src[0];
			if (Note != NO_NOTE)
			{
				if (Enc[1] != Note)
				{
					Enc[1] = Note;
					Bytes++;
					Mask |= 1;
				}
				else
				{
					Mask |= 16;
				}
			}

			uint8_t Instr = Src[1];
			if (Instr != 0)
			{
				if (Enc[2] != Instr)
				{
					Enc[2] = Instr;
					Bytes++;
					Mask |= 2;
				}
				else
				{
					Mask |= 32;
				}
			}

			uint8_t Vol = Src[2];
			if (Vol != 255)
			{
				if (Enc[3] != Vol)
				{
					Enc[3] = Vol;
					Bytes++;
					Mask |= 4;
				}
				else
				{
					Mask |= 64;
				}
			}

			uint16_t EfxAndParam = *(uint16_t *)&Src[3];
			if (EfxAndParam != 0)
			{
				if (*(uint16_t *)&Enc[4] != EfxAndParam)
				{
					*(uint16_t *)&Enc[4] = EfxAndParam;
					Bytes += 2;
					Mask |= 8;
				}
				else
				{
					Mask |= 128;
				}
			}

			if (Mask != Enc[0])
			{
				Enc[0] = Mask;
				Bytes++;
			}
		}
	}

	if (Bytes > 65535)
		return false;

	*LengthOut = (uint16_t)Bytes;
	return true;
}

void EncodePattern(pattern_t *p, uint8_t Rows)
{
	ClearEncodingInfo();

	p->Rows = Rows;

	uint8_t *Src = PatternDataArea;
	uint8_t *Dst = p->PackedData;

	for (int32_t i = 0; i < Rows; i++)
	{
		uint8_t *Enc = EncodingInfo;
		for (uint8_t ch = 0; ch < MAX_HOST_CHANNELS; ch++, Src += 5, Enc += 6)
		{
			if (Src[0] == NO_NOTE && Src[1] == 0 && Src[2] == 255 && Src[3] == 0 && Src[4] == 0)
				continue;

			uint8_t Mask = 0;

			uint8_t Note = Src[0];
			if (Note != NO_NOTE)
			{
				if (Enc[1] != Note)
				{
					Enc[1] = Note;
					Mask |= 1;
				}
				else
				{
					Mask |= 16;
				}
			}

			uint8_t Ins = Src[1];
			if (Src[1] != 0)
			{
				if (Enc[2] != Ins)
				{
					Enc[2] = Ins;
					Mask |= 2;
				}
				else
				{
					Mask |= 32;
				}
			}

			uint8_t Vol = Src[2];
			if (Vol != 255)
			{
				if (Enc[3] != Vol)
				{
					Enc[3] = Vol;
					Mask |= 4;
				}
				else
				{
					Mask |= 64;
				}
			}

			uint16_t EfxAndParam = *(uint16_t *)&Src[3];
			if (EfxAndParam != 0)
			{
				if (EfxAndParam != *(uint16_t *)&Enc[4])
				{
					*(uint16_t *)&Enc[4] = EfxAndParam;
					Mask |= 8;
				}
				else
				{
					Mask |= 128;
				}
			}

			if (Enc[0] != Mask)
			{
				Enc[0] = Mask;

				*Dst++ = (ch + 1) | 128; // read another mask...
				*Dst++ = Mask;
			}
			else
			{
				*Dst++ = ch + 1;
			}

			if (Mask & 1)
				*Dst++ = Note;

			if (Mask & 2)
				*Dst++ = Ins;

			if (Mask & 4)
				*Dst++ = Vol;

			if (Mask & 8)
			{
				*(uint16_t *)Dst = EfxAndParam;
				Dst += 2;
			}
		}

		*Dst++ = 0;
	}
}

static int8_t GetModuleType(uint8_t *Data, uint32_t DataLen) // 8bb: added this function
{
	// 8bb: order of testing has been modified for less potential false positives

	if (DataLen >= 17 && !memcmp(&Data[0], "Extended Module: ", 17)) // .XM
		return FORMAT_XM;

	if (DataLen >= 1080+4) // .MOD (31-sample)
	{
		char *ID = (char *)&Data[1080];

		if (!memcmp(ID, "M.K.", 4) || !memcmp(ID, "M!K!", 4))
			return FORMAT_PT;

		if (!memcmp(ID, "6CHN", 4) || !memcmp(ID, "8CHN", 4))
			return FORMAT_GENERIC_MOD;

		if (ID[0] >= '1' && ID[1] <= '9' && ID[1] >= '0' && ID[1] <= '9' && ID[2] == 'C' && ID[3] == 'H')
			return FORMAT_GENERIC_MOD;
	}

	if (DataLen >= 4 && !memcmp(&Data[0], "IMPM", 4)) // .IT
		return FORMAT_IT;

	if (DataLen >= 4 && !memcmp(&Data[0], "SCRM", 4)) // .S3M
		return FORMAT_S3M;

	if (DataLen >= 3 && !memcmp(&Data[0], "MTM", 3)) // MTM (?)
		return FORMAT_MT;

	if (DataLen >= 2 && (!memcmp(&Data[0], "if", 2) || !memcmp(Data, "JN", 2))) // .669 (poor)
		return FORMAT_669;

	if (DataLen >= 471+1 && Data[471] == 0x78) // .MOD (15-sample)
		return FORMAT_MOD15;

	return FORMAT_UNKNOWN;
}

bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen)
{
	bool WasCompressed = false;
	if (DataLen >= 4+4) // find out if module is MMCMP compressed
	{
		uint32_t Sig1 = *(uint32_t *)&Data[0];
		uint32_t Sig2 = *(uint32_t *)&Data[4];
		if (Sig1 == 0x4352697A && Sig2 == 0x61694E4F) // Sig1 = "ziRCONia"
		{
			if (unpackMMCMP(&Data, &DataLen))
				WasCompressed = true;
			else
				return false;
		}
	}

	MEMFILE *m = mopen(Data, DataLen);
	if (m == NULL)
		return false;

	uint8_t Format = GetModuleType(Data, DataLen);
	if (Format == FORMAT_UNKNOWN)
		goto Error;

	if (firstTimeLoading)
	{
		memset(&Song, 0, sizeof (Song));
		firstTimeLoading = false;
	}
	else
	{
		Music_FreeSong();
	}

	Music_SetDefaultMIDIDataArea();

	bool WasLoaded = false;
	switch (Format)
	{
		default: goto Error;

		case FORMAT_IT:
			WasLoaded = D_LoadIT(m);
			break;

		case FORMAT_PT:
		case FORMAT_GENERIC_MOD:
		case FORMAT_MOD15:
			WasLoaded = D_LoadMOD(m, false);
			break;
	}

	if (!WasLoaded)
		goto Error;

	mclose(&m);
	if (WasCompressed)
		free(Data);

	DriverSetMixVolume(Song.Header.MixVolume);
	DriverFixSamples();

	Song.Loaded = true;
	return true;

Error:
	if (m != NULL)
		mclose(&m);

	if (WasCompressed)
		free(Data);

	Music_FreeSong();
	return false;
}

bool Music_LoadFromFile(const char *Filename)
{
	FILE *f = fopen(Filename, "rb");
	if (f == NULL)
		return false;

	fseek(f, 0, SEEK_END);
	uint32_t FileSize = ftell(f);
	rewind(f);

	if (FileSize == 0)
	{
		fclose(f);
		return false;
	}

	uint8_t *Data = (uint8_t *)malloc(FileSize);
	if (Data == NULL)
	{
		fclose(f);
		return false;
	}

	if (fread(Data, 1, FileSize, f) != FileSize)
	{
		fclose(f);
		return false;
	}

	fclose(f);

	bool Result = Music_LoadFromData(Data, FileSize);
	free(Data);

	return Result;
}

// routines for handling data in RAM as a "FILE" type

MEMFILE *mopen(const uint8_t *src, uint32_t length)
{
	MEMFILE *b;

	if (src == NULL || length == 0)
		return NULL;

	b = (MEMFILE *)malloc(sizeof (MEMFILE));
	if (b == NULL)
		return NULL;

	b->_base = (uint8_t *)src;
	b->_ptr = (uint8_t *)src;
	b->_cnt = length;
	b->_bufsiz = length;
	b->_eof = false;

	return b;
}

void mclose(MEMFILE **buf)
{
	if (*buf != NULL)
	{
		free(*buf);
		*buf = NULL;
	}
}

size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf)
{
	int32_t pcnt;
	size_t wrcnt;

	if (buf == NULL || buf->_ptr == NULL)
		return 0;

	wrcnt = size * count;
	if (size == 0 || buf->_eof)
		return 0;

	pcnt = (buf->_cnt > (uint32_t)wrcnt) ? (uint32_t)wrcnt : buf->_cnt;
	memcpy(buffer, buf->_ptr, pcnt);

	buf->_cnt -= pcnt;
	buf->_ptr += pcnt;

	if (buf->_cnt <= 0)
	{
		buf->_ptr = buf->_base + buf->_bufsiz;
		buf->_cnt = 0;
		buf->_eof = true;
	}

	return pcnt / size;
}

size_t mtell(MEMFILE *buf)
{
	return (buf->_ptr - buf->_base);
}

int32_t meof(MEMFILE *buf)
{
	if (buf == NULL)
		return true;

	return buf->_eof;
}

void mseek(MEMFILE *buf, size_t offset, int32_t whence)
{
	if (buf == NULL)
		return;

	if (buf->_base)
	{
		switch (whence)
		{
			case SEEK_SET: buf->_ptr = buf->_base + offset; break;
			case SEEK_CUR: buf->_ptr += offset; break;
			case SEEK_END: buf->_ptr = buf->_base + buf->_bufsiz + offset; break;
			default: break;
		}

		buf->_eof = false;
		if (buf->_ptr >= buf->_base+buf->_bufsiz)
		{
			buf->_ptr = buf->_base + buf->_bufsiz;
			buf->_eof = true;
		}

		buf->_cnt = (uint32_t)((buf->_base + buf->_bufsiz) - buf->_ptr);
	}
}

bool ReadBytes(MEMFILE *m, void *dst, uint32_t num)
{
	if ((m == NULL) || meof(m))
		return false;

	if (mread(dst, 1, num, m) != num)
		return false;

	return true;
}

