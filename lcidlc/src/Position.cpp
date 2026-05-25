#include <stdio.h>

#include "Position.h"

////////////////////////////////////////////////////////////////////////////////

Position PositionMake(uint32_t p_row, uint32_t p_column)
{
	return MCMin(1023U, p_column) | (p_row << 10);
}

const char *PositionDescribe(Position p_position)
{
	static char s_buffer[64];
	sprintf(s_buffer, "%04d:%04d", PositionGetRow(p_position) + 1, PositionGetColumn(p_position) + 1);
	return s_buffer;
}

uint32_t PositionGetRow(Position p_position)
{
	return 1 + p_position / 1024;
}

uint32_t PositionGetColumn(Position p_position)
{
	return 1 + p_position % 1024;
}

////////////////////////////////////////////////////////////////////////////////
