#ifndef __POSITION__
#define __POSITION__

#include "foundation.h"

typedef uint32_t Position;

Position PositionMake(uint32_t row, uint32_t column);

const char *PositionDescribe(Position position);

uint32_t PositionGetRow(Position position);
uint32_t PositionGetColumn(Position position);

#endif
