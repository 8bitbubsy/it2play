#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen);
bool Music_LoadFromFile(const char *Filename);
void Music_FreeSong(void);
