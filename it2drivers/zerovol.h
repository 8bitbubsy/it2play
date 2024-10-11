#pragma once

#include <stdint.h>
#include "../it_structs.h"

void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples);
void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples);
void UpdatePingPongLoop(slaveChn_t *sc, uint32_t numSamples);
void UpdateNoLoopHQ(slaveChn_t *sc, uint32_t numSamples);
void UpdateForwardsLoopHQ(slaveChn_t *sc, uint32_t numSamples);
void UpdatePingPongLoopHQ(slaveChn_t *sc, uint32_t numSamples);
