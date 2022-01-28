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

static uint8_t decompBuffer[65536];
static int32_t ptrListOffset;

static void D_Decompress16BitData(int16_t *dst, uint8_t *src, uint32_t blockLen)
{
	uint8_t byte8, bitDepth, bitDepthInv, bitsRead;
	uint16_t bytes16, lastVal;
	uint32_t bytes32;

	lastVal = 0;
	bitDepth = 17;
	bitDepthInv = bitsRead = 0;

	blockLen >>= 1;
	while (blockLen != 0)
	{
		bytes32 = (*(uint32_t *)src) >> bitsRead;

		bitsRead += bitDepth;
		src += bitsRead >> 3;
		bitsRead &= 7;

		if (bitDepth <= 6)
		{
			bytes32 <<= bitDepthInv & 0x1F;

			bytes16 = (uint16_t)bytes32;
			if (bytes16 != 0x8000)
			{
				lastVal += (int16_t)bytes16 >> (bitDepthInv & 0x1F); // arithmetic shift
				*dst++ = lastVal;
				blockLen--;
			}
			else
			{
				byte8 = ((bytes32 >> 16) & 0xF) + 1;
				if (byte8 >= bitDepth)
					byte8++;
				bitDepth = byte8;

				bitDepthInv = 16;
				if (bitDepthInv < bitDepth)
					bitDepthInv++;
				bitDepthInv -= bitDepth;

				bitsRead += 4;
			}

			continue;
		}

		bytes16 = (uint16_t)bytes32;

		if (bitDepth <= 16)
		{
			uint16_t DX = 0xFFFF >> (bitDepthInv & 0x1F);
			bytes16 &= DX;
			DX = (DX >> 1) - 8;

			if (bytes16 > DX+16 || bytes16 <= DX)
			{
				bytes16 <<= bitDepthInv & 0x1F;
				bytes16 = (int16_t)bytes16 >> (bitDepthInv & 0x1F); // arithmetic shift
				lastVal += bytes16;
				*dst++ = lastVal;
				blockLen--;
				continue;
			}

			byte8 = (uint8_t)(bytes16 - DX);
			if (byte8 >= bitDepth)
				byte8++;
			bitDepth = byte8;

			bitDepthInv = 16;
			if (bitDepthInv < bitDepth)
				bitDepthInv++;
			bitDepthInv -= bitDepth;
			continue;
		}

		if (bytes32 & 0x10000)
		{
			bitDepth = (uint8_t)(bytes16 + 1);
			bitDepthInv = 16 - bitDepth;
		}
		else
		{
			lastVal += bytes16;
			*dst++ = lastVal;
			blockLen--;
		}
	}
}

static void D_Decompress8BitData(int8_t *dst, uint8_t *src, uint32_t blockLen)
{
	uint8_t lastVal, byte8, bitDepth, bitDepthInv, bitsRead;
	uint16_t bytes16;

	lastVal = 0;
	bitDepth = 9;
	bitDepthInv = bitsRead = 0;

	while (blockLen != 0)
	{
		bytes16 = (*(uint16_t *)src) >> bitsRead;

		bitsRead += bitDepth;
		src += (bitsRead >> 3);
		bitsRead &= 7;

		byte8 = bytes16 & 0xFF;

		if (bitDepth <= 6)
		{
			bytes16 <<= (bitDepthInv & 0x1F);
			byte8 = bytes16 & 0xFF;

			if (byte8 != 0x80)
			{
				lastVal += (int8_t)byte8 >> (bitDepthInv & 0x1F); // arithmetic shift
				*dst++ = lastVal;
				blockLen--;
				continue;
			}

			byte8 = (bytes16 >> 8) & 7;
			bitsRead += 3;
			src += (bitsRead >> 3);
			bitsRead &= 7;
		}
		else
		{
			if (bitDepth == 8)
			{
				if (byte8 < 0x7C || byte8 > 0x83)
				{
					lastVal += byte8;
					*dst++ = lastVal;
					blockLen--;
					continue;
				}
				byte8 -= 0x7C;
			}
			else if (bitDepth < 8)
			{
				byte8 <<= 1;
				if (byte8 < 0x78 || byte8 > 0x86)
				{
					lastVal += (int8_t)byte8 >> (bitDepthInv & 0x1F); // arithmetic shift
					*dst++ = lastVal;
					blockLen--;
					continue;
				}
				byte8 = (byte8 >> 1) - 0x3C;
			}
			else
			{
				bytes16 &= 0x1FF;
				if ((bytes16 & 0x100) == 0)
				{
					lastVal += byte8;
					*dst++ = lastVal;
					blockLen--;
					continue;
				}
			}
		}

		byte8++;
		if (byte8 >= bitDepth)
			byte8++;
		bitDepth = byte8;

		bitDepthInv = 8;
		if (bitDepthInv < bitDepth)
			bitDepthInv++;
		bitDepthInv -= bitDepth;
	}
}

static void IT_LoadCompressed16BitSample(MEMFILE *m, sample_t *s, bool deltaEncoded)
{
	uint16_t packedLen;

	int8_t *dstPtr = (int8_t *)s->Data;

	uint32_t i = s->Length;
	while (i > 0)
	{
		uint32_t bytesToUnpack = 32768;
		if (bytesToUnpack > i)
			bytesToUnpack = i;

		mread(&packedLen, sizeof (uint16_t), 1, m);
		mread(decompBuffer, 1, packedLen, m);

		D_Decompress16BitData((int16_t *)dstPtr, decompBuffer, bytesToUnpack);

		if (deltaEncoded) // convert from delta values to PCM
		{
			int16_t *ptr16 = (int16_t *)dstPtr;
			int16_t lastSmp16 = 0; // yes, reset this every block!

			const uint32_t len = bytesToUnpack >> 1;
			for (uint32_t j = 0; j < len; j++)
			{
				lastSmp16 += ptr16[j];
				ptr16[j] = lastSmp16;
			}
		}

		dstPtr += bytesToUnpack;
		i -= bytesToUnpack;
	}
}

static void IT_LoadCompressed8BitSample(MEMFILE *m, sample_t *s, bool deltaEncoded)
{
	uint16_t packedLen;
	int8_t *dstPtr = (int8_t *)s->Data;

	uint32_t i = s->Length;
	while (i > 0)
	{
		uint32_t bytesToUnpack = 32768;
		if (bytesToUnpack > i)
			bytesToUnpack = i;

		mread(&packedLen, sizeof (uint16_t), 1, m);
		mread(decompBuffer, 1, packedLen, m);

		D_Decompress8BitData(dstPtr, decompBuffer, bytesToUnpack);

		if (deltaEncoded) // convert from delta values to PCM
		{
			int8_t lastSmp8 = 0; // yes, reset this every block!

			const uint32_t len = bytesToUnpack;
			for (uint32_t j = 0; j < len; j++)
			{
				lastSmp8 += dstPtr[j];
				dstPtr[j] = lastSmp8;
			}
		}

		dstPtr += bytesToUnpack;
		i -= bytesToUnpack;
	}
}

bool D_LoadIT(MEMFILE *m)
{
	/*
	** ===================================
	** =========== LOAD HEADER ===========
	** ===================================
	*/

	mseek(m, 4, SEEK_SET); // set to offset 4 (after "IMPM" magic)
	if (!ReadBytes(m, Song.Header.SongName, 26)) return false;
	if (!ReadBytes(m, &Song.Header.PHiligt, 2)) return false;
	if (!ReadBytes(m, &Song.Header.OrdNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.InsNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.SmpNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.PatNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Cwtv, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Cmwt, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Flags, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Special, 2)) return false;
	if (!ReadBytes(m, &Song.Header.GlobalVol, 1)) return false;
	if (!ReadBytes(m, &Song.Header.MixVolume, 1)) return false;
	if (!ReadBytes(m, &Song.Header.InitialSpeed, 1)) return false;
	if (!ReadBytes(m, &Song.Header.InitialTempo, 1)) return false;
	if (!ReadBytes(m, &Song.Header.PanSep, 1)) return false;
	if (!ReadBytes(m, &Song.Header.PitchWheelDepth, 1)) return false;
	if (!ReadBytes(m, &Song.Header.MessageLength, 2)) return false;
	if (!ReadBytes(m, &Song.Header.MessageOffset, 4)) return false;
	mseek(m, 4, SEEK_CUR); // skip unwanted stuff
	if (!ReadBytes(m, Song.Header.ChnlPan, MAX_HOST_CHANNELS)) return false;
	if (!ReadBytes(m, Song.Header.ChnlVol, MAX_HOST_CHANNELS)) return false;

	// IT2 doesn't do this test, but I do it for safety.
	if (Song.Header.OrdNum > MAX_ORDERS+1 || Song.Header.InsNum > MAX_INSTRUMENTS ||
		Song.Header.SmpNum > MAX_SAMPLES  || Song.Header.PatNum > MAX_PATTERNS)
	{
		return false;
	}

	// IT2 doesn't do this, but let's do it for safety
	if (Song.Header.MessageLength > MAX_SONGMSG_LENGTH)
		Song.Header.MessageLength = MAX_SONGMSG_LENGTH;

	Song.Header.SongName[25] = '\0'; // just in case...

	/* *absolute* lowest possible initial tempo, we need to clamp
	** it for safety reasons (yes, IT2 can do 31 as initial tempo!).
	*/
	if (Song.Header.InitialTempo < 31)
		Song.Header.InitialTempo = 31;

	ptrListOffset = 192 + Song.Header.OrdNum;

	int16_t OrdersToLoad = Song.Header.OrdNum - 1; // IT2 does this (removes the count for the last 255 terminator)
	if (OrdersToLoad > 0)
	{
		if (!ReadBytes(m, Song.Orders, OrdersToLoad))
			return false;

		// fill rest of order list with 255
		if (OrdersToLoad < MAX_ORDERS)
			memset(&Song.Orders[OrdersToLoad], 255, MAX_ORDERS-OrdersToLoad);
	}
	else
	{
		memset(Song.Orders, 255, MAX_ORDERS);
	}

	mseek(m, 192 + Song.Header.OrdNum + ((Song.Header.InsNum + Song.Header.SmpNum + Song.Header.PatNum) * 4), SEEK_SET);

	// skip time data, if present
	if (Song.Header.Special & 2)
	{
		uint16_t NumTimerData;
		ReadBytes(m, &NumTimerData, 2);
		mseek(m, NumTimerData * 8, SEEK_CUR);
	}

	// read embedded MIDI configuration, if preset (needed for Zxx macros)
	char *MIDIDataArea = Music_GetMIDIDataArea();
	if (Song.Header.Special & 8)
	{
		ReadBytes(m, MIDIDataArea, (9+16+128)*32);
	}

	// load song message, if present
	if ((Song.Header.Special & 1) && Song.Header.MessageLength > 0 && Song.Header.MessageOffset > 0)
	{
		mseek(m, Song.Header.MessageOffset, SEEK_SET);
		mread(Song.Message, 1, Song.Header.MessageLength, m);
		Song.Message[MAX_SONGMSG_LENGTH] = '\0'; // just in case
	}

	/*
	** ===================================
	** ======== LOAD INSTRUMENTS =========
	** ===================================
	*/

	mseek(m, ptrListOffset, SEEK_SET);
	size_t insPtrOffset = mtell(m);

	instrument_t *ins = Song.Ins;
	for (uint32_t i = 0; i < Song.Header.InsNum; i++, ins++)
	{
		mseek(m, insPtrOffset + (i * 4), SEEK_SET);
		if (meof(m)) return false;

		uint32_t insOffset;
		if (!ReadBytes(m, &insOffset, 4)) return false;
		if (insOffset == 0) continue;

		mseek(m, insOffset, SEEK_SET);
		if (meof(m)) return false;

		if (Song.Header.Cmwt >= 0x200)
		{
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->DOSFilename, 13)) return false;
			if (!ReadBytes(m, &ins->NNA, 1)) return false;
			if (!ReadBytes(m, &ins->DCT, 1)) return false;
			if (!ReadBytes(m, &ins->DCA, 1)) return false;
			if (!ReadBytes(m, &ins->FadeOut, 2)) return false;
			if (!ReadBytes(m, &ins->PPS, 1)) return false;
			if (!ReadBytes(m, &ins->PPC, 1)) return false;
			if (!ReadBytes(m, &ins->GbV, 1)) return false;
			if (!ReadBytes(m, &ins->DfP, 1)) return false;
			if (!ReadBytes(m, &ins->RV, 1)) return false;
			if (!ReadBytes(m, &ins->RP, 1)) return false;
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->InstrumentName, 26)) return false;
			if (!ReadBytes(m, &ins->IFC, 1)) return false;
			if (!ReadBytes(m, &ins->IFR, 1)) return false;
			if (!ReadBytes(m, &ins->MCh, 1)) return false;
			if (!ReadBytes(m, &ins->MPr, 1)) return false;
			if (!ReadBytes(m, &ins->MIDIBnk, 2)) return false;
			if (!ReadBytes(m, &ins->SmpNoteTable, 2*120)) return false;

			// just in case
			ins->DOSFilename[12] = '\0';
			ins->InstrumentName[25] = '\0';

			// read envelopes
			for (uint32_t j = 0; j < 3; j++)
			{
				env_t *env;

				     if (j == 0) env = &ins->VEnvelope;
				else if (j == 1) env = &ins->PEnvelope;
				else             env = &ins->PtEnvelope;

				if (!ReadBytes(m, &env->Flags, 1)) return false;
				if (!ReadBytes(m, &env->Num, 1)) return false;
				if (!ReadBytes(m, &env->LpB, 1)) return false;
				if (!ReadBytes(m, &env->LpE, 1)) return false;
				if (!ReadBytes(m, &env->SLB, 1)) return false;
				if (!ReadBytes(m, &env->SLE, 1)) return false;

				envNode_t *node = env->NodePoints;
				for (uint32_t k = 0; k < 25; k++, node++)
				{
					if (!ReadBytes(m, &node->Magnitude, 1)) return false;
					if (!ReadBytes(m, &node->Tick, 2)) return false;
				}

				mseek(m, 1, SEEK_CUR); // skip unwanted stuff
			}
		}
		else // old instruments (v1.xx)
		{
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->DOSFilename, 13)) return false;
			if (!ReadBytes(m, &ins->VEnvelope.Flags, 1)) return false;
			if (!ReadBytes(m, &ins->VEnvelope.LpB, 1)) return false;
			if (!ReadBytes(m, &ins->VEnvelope.LpE, 1)) return false;
			if (!ReadBytes(m, &ins->VEnvelope.SLB, 1)) return false;
			if (!ReadBytes(m, &ins->VEnvelope.SLE, 1)) return false;
			mseek(m, 2, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, &ins->FadeOut, 2)) return false;
			if (!ReadBytes(m, &ins->NNA, 1)) return false;
			if (!ReadBytes(m, &ins->DCT, 1)) return false;
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->InstrumentName, 26)) return false;
			mseek(m, 6, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, &ins->SmpNoteTable, 2*120)) return false;

			ins->FadeOut *= 2;

			// just in case
			ins->DOSFilename[12] = '\0';
			ins->InstrumentName[25] = '\0';

			// set default values not present in old instrument
			ins->PPC = 60;
			ins->GbV = 128;
			ins->DfP = 32 + 128; // center + pan disabled

			mseek(m, 200, SEEK_CUR);

			// read volume envelope
			uint8_t j;
			for (j = 0; j < 25; j++)
			{
				uint16_t word;
				envNode_t *node = &ins->VEnvelope.NodePoints[j];

				if (!ReadBytes(m, &word, 2)) return false;
				if (word == 0xFFFF)
					break; // end of envelope

				node->Tick = word & 0xFF;
				node->Magnitude = word >> 8;
			}

			ins->VEnvelope.Num = j;

			ins->PEnvelope.Num = 2;
			ins->PEnvelope.NodePoints[1].Tick = 99;

			ins->PtEnvelope.Num = 2;
			ins->PtEnvelope.NodePoints[1].Tick = 99;
		}
	}

	/*
	** ===================================
	** ======= LOAD SAMPLE HEADERS =======
	** ===================================
	*/

	mseek(m, ptrListOffset + (Song.Header.InsNum * 4), SEEK_SET);
	size_t smpPtrOffset = mtell(m);

	for (uint32_t i = 0; i < Song.Header.SmpNum; i++)
	{
		mseek(m, smpPtrOffset + (i * 4), SEEK_SET);
		if (meof(m)) return false;

		uint32_t smpOffset;
		if (!ReadBytes(m, &smpOffset, 4)) return false;
		if (smpOffset == 0) continue;

		mseek(m, smpOffset, SEEK_SET);
		if (meof(m)) return false;

		sample_t *smp = &Song.Smp[i];

		mseek(m, 4, SEEK_CUR); // skip unwanted stuff
		if (!ReadBytes(m, smp->DOSFilename, 13)) return false;
		if (!ReadBytes(m, &smp->GvL, 1)) return false;
		if (!ReadBytes(m, &smp->Flags, 1)) return false;
		if (!ReadBytes(m, &smp->Vol, 1)) return false;
		if (!ReadBytes(m, smp->SampleName, 26)) return false;
		if (!ReadBytes(m, &smp->Cvt, 1)) return false;
		if (!ReadBytes(m, &smp->DfP, 1)) return false;
		if (!ReadBytes(m, &smp->Length, 4)) return false;
		if (!ReadBytes(m, &smp->LoopBeg, 4)) return false;
		if (!ReadBytes(m, &smp->LoopEnd, 4)) return false;
		if (!ReadBytes(m, &smp->C5Speed, 4)) return false;
		if (!ReadBytes(m, &smp->SusLoopBeg, 4)) return false;
		if (!ReadBytes(m, &smp->SusLoopEnd, 4)) return false;
		if (!ReadBytes(m, &smp->OffsetInFile, 4)) return false;
		if (!ReadBytes(m, &smp->ViS, 1)) return false;
		if (!ReadBytes(m, &smp->ViD, 1)) return false;
		if (!ReadBytes(m, &smp->ViR, 1)) return false;
		if (!ReadBytes(m, &smp->ViT, 1)) return false;

		// just in case
		smp->DOSFilename[12] = '\0';
		smp->SampleName[25] = '\0';
	}

	/* ===================================
	** ======== LOAD SAMPLE DATA =========
	** ===================================
	*/

	sample_t *s = Song.Smp;
	for (uint32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		if (s->OffsetInFile == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
			continue;

		mseek(m, s->OffsetInFile, SEEK_SET);
		if (meof(m))
			return false;

		bool isCompressed = !!(s->Flags & SMPF_COMPRESSED);
		bool is16Bit = !!(s->Flags & SMPF_16BIT);
		bool signedSamples = !!(s->Cvt & 1);
		bool isDeltaEncoded = !!(s->Cvt & 4);

		if (isDeltaEncoded && !isCompressed)
			return false; // not supported

		if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
			return true; // safely skip this sample

		if (s->Cvt & 0b11111010)
			return true; // not supported

		if (!Music_AllocateSample(i, s->Length * (1 + is16Bit))) return false;

		if (isCompressed)
		{
			if (is16Bit)
				IT_LoadCompressed16BitSample(m, s, isDeltaEncoded);
			else
				IT_LoadCompressed8BitSample(m, s, isDeltaEncoded);
		}
		else
		{
			mread(s->Data, 1, s->Length, m);
		}

		// convert unsigned sample to signed
		if (!signedSamples)
		{
			if (is16Bit)
			{
				int16_t *Data = (int16_t *)s->Data;
				for (uint32_t j = 0; j < s->Length; j++)
					Data[j] ^= 0x8000;
			}
			else
			{
				int8_t *Data = (int8_t *)s->Data;
				for (uint32_t j = 0; j < s->Length; j++)
					Data[j] ^= 0x80;
			}
		}

		if (is16Bit)
			s->Length >>= 1;
	}

	/*
	** ===================================
	** ========== LOAD PATTERNS ==========
	** ===================================
	*/

	mseek(m, ptrListOffset + (Song.Header.InsNum * 4) + (Song.Header.SmpNum * 4), SEEK_SET);
	size_t patPtrOffset = mtell(m);

	pattern_t *pat = Song.Pat;
	for (uint32_t i = 0; i < Song.Header.PatNum; i++, pat++)
	{
		mseek(m, patPtrOffset + (i * 4), SEEK_SET);
		if (meof(m))
			return false;

		uint32_t patOffset;
		if (!ReadBytes(m, &patOffset, 4)) return false;
		if (patOffset == 0)
			continue;

		mseek(m, patOffset, SEEK_SET);
		if (meof(m))
			return false;

		uint16_t patLength;
		if (!ReadBytes(m, &patLength, 2)) return false;
		if (!ReadBytes(m, &pat->Rows, 2)) return false;
		if (patLength == 0 || pat->Rows == 0) continue;
		if (!Music_AllocatePattern(i, patLength)) return false;

		mseek(m, 4, SEEK_CUR); // skip unwanted stuff

		if (!ReadBytes(m, pat->PackedData, patLength)) return false;
	}

	return true;
}
