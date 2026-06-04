
#ifndef	PLAYER_H
#define	PLAYER_H

// SN-2014-07-03: [[ PlatformPlayer ]]
// Add the correct definition of the MCPlayer class
#ifdef FEATURE_PLATFORM_PLAYER
#include "player-platform.h"
#else
#include "player-legacy.h"
#endif

#endif 
