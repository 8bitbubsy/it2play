#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "it_structs.h"

#define SWAP16(value) \
( \
	(((uint16_t)((value) & 0x00FF)) << 8) | \
	(((uint16_t)((value) & 0xFF00)) >> 8)   \
)

enum
{
	FORMAT_UNKNOWN     = 0,
	FORMAT_IT          = 1,
	FORMAT_S3M         = 3,
	FORMAT_XM          = 4,
	FORMAT_669         = 5,
	FORMAT_MT          = 6,
	FORMAT_MOD15       = 7,
	FORMAT_PT          = 8,
	FORMAT_GENERIC_MOD = 9,

	// TODO: Add rest
};

// routines for handling data in RAM as a "FILE" type (IT2 doesn't have these)
typedef struct mem_t
{
	bool _eof;
	uint8_t *_ptr, *_base;
	uint32_t _cnt, _bufsiz;
} MEMFILE;

MEMFILE *mopen(const uint8_t *src, uint32_t length);
void mclose(MEMFILE **buf);
size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf);
size_t mtell(MEMFILE *buf);
int32_t meof(MEMFILE *buf);
void mseek(MEMFILE *buf, size_t offset, int32_t whence);
bool ReadBytes(MEMFILE *m, void *dst, uint32_t num);
// -------------------------------------------------------

void ClearEncodingInfo(void);
void ClearPatternData(void);
bool GetPatternLength(uint16_t Rows, uint16_t *LengthOut);
void EncodePattern(pattern_t *p, uint8_t Rows);
bool StorePattern(uint8_t NumRows, int32_t Pattern);
bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen);
bool Music_LoadFromFile(const char *Filename);
void Music_FreeSong(void);

extern uint8_t PatternDataArea[MAX_HOST_CHANNELS*200*5];
