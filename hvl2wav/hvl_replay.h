#ifndef _HVL2WAV_HVL_REPLAY_H
#define _HVL2WAV_HVL_REPLAY_H

#include "types.h"
#include "../common/hvl_replay.h"

int32 hvl_FindLoudest( struct hvl_tune *ht, int32 maxframes, BOOL usesongend );

#endif
