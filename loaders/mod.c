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
#include "../it_music.h"
#include "../it_structs.h"
#include "../it_d_rm.h"

#define MOD_ROWS 64

static const uint16_t FineTuneTable[16] =
{
	8363, 8413, 8463, 8529, 8581, 8651, 8723, 8757,
	7895, 7941, 7985, 8046, 8107, 8169, 8232, 8280
};

static const uint16_t MODPeriodTable[6 * 12] =
{
	1712, 1616, 1525, 1440, 1357, 1281, // Octave 0
	1209, 1141, 1077, 1017,  961,  907,
	 856,  808,  762,  720,  678,  640, // Octave 1
	 604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320, // Octave 2
	 302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160, // Octave 3
	 151,  143,  135,  127,  120,  113,
	 107,  101,   95,   90,   85,   80, // Octave 4
	  75,   71,   67,   63,   60,   56,
	  53,   50,   47,   45,   42,   40, // Octave 5
	  37,   35,   33,   31,   30,   28
};

static uint8_t PatData[MOD_ROWS*MAX_HOST_CHANNELS*4];
static uint8_t MODNumberOfChannels, MODNumberOfOrders, MODNumberOfInstruments;
static uint16_t MODOrderOffset, MODPatternOffset;

static bool GetChannelCount(MEMFILE *m) // 8bb: added this
{
	uint8_t ID[4];

	if (meof(m) || !ReadBytes(m, ID, 4))
		return false;

	MODNumberOfChannels = 4;
	if (!memcmp(ID, "6CHN", 4))
		MODNumberOfChannels = 6;
	else if (!memcmp(ID, "8CHN", 4))
		MODNumberOfChannels = 8;
	else if (ID[0] >= '1' && ID[1] <= '9' && ID[1] >= '0' && ID[1] <= '9' && ID[2] == 'C' && ID[3] == 'H')
		MODNumberOfChannels = ((ID[0] - '0') * 10) + (ID[1] - '0');

	return true;
}

static void TranslateMODCommand(uint8_t *Dst)
{
	if (Dst[3] == 0 && Dst[4] == 0)
		return;

	switch (Dst[3])
	{
		default: break;

		case 0x0: Dst[3] = 'J'-'@'; break;
		case 0x1: Dst[3] = 'F'-'@'; break;
		case 0x2: Dst[3] = 'E'-'@'; break;
		case 0x3: Dst[3] = 'G'-'@'; break;
		case 0x4: Dst[3] = 'H'-'@'; break;
	
		case 0x5:
		{
			if (Dst[4] != 0)
				Dst[3] = 'L'-'@';
			else
				Dst[3] = 'G'-'@';
		}
		break;

		case 0x6:
		{
			if (Dst[4] != 0)
				Dst[3] = 'K'-'@';
			else
				Dst[3] = 'H'-'@';
		}
		break;

		case 0x7: Dst[3] = 'R'-'@'; break;

		case 0x8:
		{
			if (Dst[4] != 0xA4)
			{
				Dst[3] = 'X'-'@';
			}
			else
			{
				Dst[3] = 'S'-'@';
				Dst[4] = 0x91;
			}
		}
		break;

		case 0x9: Dst[3] = 'O'-'@'; break;

		case 0xA:
		{
			Dst[3] = 'D'-'@';

			if ((Dst[4] & 0x0F) && (Dst[4] & 0xF0))
				Dst[4] &= 0xF0;

			if (Dst[4] == 0)
				Dst[3] = 0;
		}
		break;

		case 0xB: Dst[3] = 'B'-'@'; break;

		case 0xC:
		{
			if (Dst[4] > 64)
				Dst[4] = 64;

			Dst[2] = Dst[4]; // 8bb: use volume column instead

			Dst[3] = 0;
			Dst[4] = 0;
		}
		break;

		case 0xD:
		{
			Dst[3] = 'C'-'@';

			// 8bb: IT2's broken (?) way of converting between decimal/hex
			Dst[4] = (Dst[4] & 0x0F) + ((Dst[4] & 0xF0) >> 1) + ((Dst[4] & 0xF0) >> 3);
		}
		break;

		case 0xE:
		{
			if (!(Dst[4] & 0xF0))
			{
				if (Dst[4] & 0x0F)
				{
					Dst[3] = 'S'-'@';
				}
				else
				{
					Dst[3] = 0;
					Dst[4] = 0;
				}
			}
			else
			{
				switch (Dst[4] >> 4)
				{
					case 0x1:
					{
						if (!(Dst[4] & 0x0F))
						{
							Dst[3] = 0;
							Dst[4] = 0;
						}
						else
						{
							Dst[3] = 'F'-'@';
							Dst[4] = 0xF0 | (Dst[4] & 0x0F);
						}
					}
					break;

					case 0x2:
					{
						if (!(Dst[4] & 0x0F))
						{
							Dst[3] = 0;
							Dst[4] = 0;
						}
						else
						{
							Dst[3] = 'E'-'@';
							Dst[4] = 0xF0 | (Dst[4] & 0x0F);
						}
					}
					break;

					case 0x3:
					{
						Dst[3] = 'S'-'@';
						Dst[4] = 0x10 | (Dst[4] & 0x0F);
					}
					break;

					case 0x4:
					{
						Dst[3] = 'S'-'@';
						Dst[4] = 0x30 | (Dst[4] & 0x0F);
					}
					break;

					case 0x5:
					{
						Dst[3] = 'S'-'@';
						Dst[4] = 0x20 | (Dst[4] & 0x0F);
					}
					break;

					case 0x6:
					{
						Dst[3] = 'S'-'@';
						Dst[4] = 0xB0 | (Dst[4] & 0x0F);
					}
					break;

					case 0x7:
					{
						Dst[3] = 'S'-'@';
						Dst[4] = 0x40 | (Dst[4] & 0x0F);
					}
					break;

					case 0x8:
					{
						Dst[3] = 'S'-'@';
					}
					break;

					case 0x9:
					{
						if (!(Dst[4] & 0x0F))
						{
							Dst[3] = 0;
							Dst[4] = 0;
						}
						else
						{
							Dst[3] = 'Q'-'@';
							Dst[4] &= 0x0F;
						}
					}
					break;

					case 0xA:
					{
						if (!(Dst[4] & 0x0F))
						{
							Dst[3] = 0;
							Dst[4] = 0;
						}
						else
						{
							Dst[3] = 'D'-'@';
							Dst[4] <<= 4;
							Dst[4] |= 0x0F;
						}
					}
					break;

					case 0xB:
					{
						if (!(Dst[4] & 0x0F))
						{
							Dst[3] = 0;
							Dst[4] = 0;
						}
						else
						{
							Dst[3] = 'D'-'@';
							Dst[4] &= 0x0F;
							Dst[4] |= 0xF0;
						}
					}
					break;

					default: Dst[3] = 'S'-'@'; break;
				}
			}
		}
		break;

		case 0xF:
		{
			if (Dst[4] > 0x20)
				Dst[3] = 'T'-'@';
			else
				Dst[3] = 'A'-'@';
		}
		break;
	}
}

static bool TranslateMODPattern(uint8_t *Src, int32_t Pattern, uint8_t NumChannels)
{
	ClearPatternData();

	uint8_t *OrigDst = PatternDataArea;
	for (int32_t i = 0; i < MOD_ROWS; i++)
	{
		uint8_t *Dst = OrigDst;
		for (int32_t j = 0; j < NumChannels; j++, Src += 4, Dst += 5)
		{
			uint16_t Period = ((Src[0] & 0x0F) << 8) | Src[1];
			if (Period > 0)
			{
				for (int32_t k = 0; k < 6*12; k++)
				{
					if (Period >= MODPeriodTable[k])
					{
						Dst[0] = (3 * 12) + (uint8_t)k;
						break;
					}
				}
			}

			Dst[1] = (Src[0] & 0xF0) | (Src[2] >> 4); // 8bb: sample
			// 8bb: skip volume column
			Dst[3] = Src[2] & 0x0F; // 8bb: effect
			Dst[4] = Src[3]; // 8bb: effect parameter

			TranslateMODCommand(Dst);
		}

		OrigDst += MAX_HOST_CHANNELS * 5;
	}

	uint16_t PackedLength;
	if (!GetPatternLength(MOD_ROWS, &PackedLength))
		return false;

	if (!Music_AllocatePattern(Pattern, PackedLength))
		return false;

	EncodePattern(&Song.Pat[Pattern], MOD_ROWS);
	return true;
}

bool D_LoadMOD(MEMFILE *m, bool Format15Samples)
{
	if (Format15Samples)
	{
		MODNumberOfInstruments = 15;
		MODOrderOffset = 472;
		MODPatternOffset = 600;

		mseek(m, 470, SEEK_SET);
		if (!ReadBytes(m, &MODNumberOfOrders, 1)) return false;
	}
	else
	{
		MODNumberOfInstruments = 31;
		MODOrderOffset = 952;
		MODPatternOffset = 1084;

		mseek(m, 950, SEEK_SET);
		if (!ReadBytes(m, &MODNumberOfOrders, 1)) return false;
	}

	if (!GetChannelCount(m) || MODNumberOfChannels < 4 || MODNumberOfChannels > MAX_HOST_CHANNELS)
		return false;

	Song.Header.Flags = ITF_STEREO | ITF_OLD_EFFECTS | ITF_COMPAT_GXX;
	Song.Header.GlobalVol = 128;
	Song.Header.MixVolume = 48;
	Song.Header.InitialSpeed = 6;
	Song.Header.InitialTempo = 125;

	/* 8bb:
	** IT2 sets a panning separation of 50% (0 = minimum, 128 = max) to soften the hard Amiga panning.
	** However, Jeffrey probably forgot that this should only be applied to Amiga modules.
	** What ends up happening is that panning commands in multi-channel modules will have 50% less
	** separation too. This is not ideal, but I'll keep the behavior regardless.
	*/
	Song.Header.PanSep = 64;

	if (MODNumberOfChannels <= 8)
	{
		// 8bb: Amiga pan (softened by Song.Header.PanSep)
		for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
		{
			if (i < MODNumberOfChannels)
				Song.Header.ChnlPan[i] = ((i+1) & 2) ? 64 : 0; // 8bb: 100% right or 100% left
			else
				Song.Header.ChnlPan[i] = 32 | 128; // 8bb: center + disable channel
		}
	}
	else
	{
		// 8bb: center pan
		for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
		{
			if (i < MODNumberOfChannels)
				Song.Header.ChnlPan[i] = 32;
			else
				Song.Header.ChnlPan[i] = 32 | 128;
		}
	}

	for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
		Song.Header.ChnlVol[i] = 64;

	// Orders.
	memset(Song.Orders, 255, MAX_ORDERS);
	mseek(m, MODOrderOffset, SEEK_SET);
	if (!ReadBytes(m, Song.Orders, MODNumberOfOrders)) return false;

	// Get number of patterns to load
	uint8_t PatNum = 0;
	mseek(m, MODOrderOffset, SEEK_SET);
	for (int32_t i = 0; i < 128; i++)
	{
		uint8_t Order;
		if (!ReadBytes(m, &Order, 1)) return false;

		if (Order > PatNum)
			PatNum = Order;
	}
	PatNum++;

	// setup inst. headers
	mseek(m, 20, SEEK_SET);
	sample_t *s = Song.Smp;
	for (int32_t i = 0; i < MODNumberOfInstruments; i++, s++)
	{
		uint16_t Length16, LoopBeg16, LoopLen16;
		uint8_t FineTune, Volume;

		if (!ReadBytes(m, s->SampleName, 22)) return false;
		if (!ReadBytes(m, &Length16, 2)) return false;
		if (!ReadBytes(m, &FineTune, 1)) return false;
		if (!ReadBytes(m, &Volume, 1)) return false;
		if (!ReadBytes(m, &LoopBeg16, 2)) return false;
		if (!ReadBytes(m, &LoopLen16, 2)) return false;

		Length16  = SWAP16(Length16);
		LoopBeg16 = SWAP16(LoopBeg16);
		LoopLen16 = SWAP16(LoopLen16);

		if (Length16 > 1)
		{
			if (!Music_AllocateSample(i, Length16 * 2))
				return false;
		}

		if (LoopLen16 > 1)
			s->Flags |= SMPF_USE_LOOP;

		uint32_t Length = Length16 * 2;
		uint32_t LoopBeg = LoopBeg16;
		uint32_t LoopLen = LoopLen16;

		if (!Format15Samples)
		{
			LoopBeg *= 2;
			LoopLen *= 2;
		}

		if (LoopBeg >= Length)
			LoopBeg = 0;

		uint32_t LoopEnd = LoopBeg + LoopLen;
		if (LoopEnd >= Length)
			LoopEnd = Length;

		s->LoopBeg = LoopBeg;
		s->LoopEnd = LoopEnd;
		s->Length = Length;
		s->DfP = 32;
		s->GvL = 64;
		s->Vol = Volume;
		s->C5Speed = FineTuneTable[FineTune];
	}

	// Now to load mod patterns.
	mseek(m, MODPatternOffset, SEEK_SET);
	for (int32_t i = 0; i < PatNum; i++)
	{
		if (!ReadBytes(m, PatData, MOD_ROWS * MODNumberOfChannels * 4))
			return false;

		if (!TranslateMODPattern(PatData, i, MODNumberOfChannels))
			return false;
	}

	// Finished loading patterns. Time to load samples.
	s = Song.Smp;
	for (int32_t i = 0; i < MODNumberOfInstruments; i++, s++)
	{
		if (s->Length > 0)
			mread(s->Data, 1, s->Length, m);
	}

	Song.Header.OrdNum = MODNumberOfOrders + 1;
	Song.Header.PatNum = PatNum;
	Song.Header.SmpNum = MODNumberOfInstruments;

	return true;
}
