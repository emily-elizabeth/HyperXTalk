#ifndef __STACK_TILE__
#define __STACK_TILE__

#include "graphics.h"

bool MCStackTileInitialize();
void MCStackTileFinalize();

class MCStackTile
{
public:
    virtual bool Lock(void) = 0;
	virtual void Unlock(void) = 0;
    virtual void Render(void) = 0;
};

void MCStackTilePush(MCStackTile *tile);
void MCStackTileCollectAll(void);

#endif
