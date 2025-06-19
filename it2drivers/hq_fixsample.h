#pragma once

#include <stdint.h>
#include "../it_structs.h"

void fixSamplesPingpong(sample_t *s, slaveChn_t *sc);
void unfixSamplesPingpong(sample_t *s, slaveChn_t *sc);
void fixSamplesFwdLoop(sample_t *s, slaveChn_t *sc);
void unfixSamplesFwdLoop(sample_t *s, slaveChn_t *sc);
void fixSamplesNoLoop(sample_t *s, slaveChn_t *sc);
