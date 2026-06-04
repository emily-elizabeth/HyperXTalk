////////////////////////////////////////////////////////////////////////////////

struct MCColorVector2
{
	MCGFloat x;
	MCGFloat y;
};

struct MCColorVector3
{
	MCGFloat x;
	MCGFloat y;
	MCGFloat z;
};

struct MCColorMatrix3x3
{
	MCGFloat m[3][3];
};

inline MCColorVector2 MCColorVector2Make(MCGFloat x, MCGFloat y)
{
	MCColorVector2 t_vector;
	t_vector.x = x;
	t_vector.y = y;
	return t_vector;
}

inline MCColorVector3 MCColorVector3Make(MCGFloat x, MCGFloat y, MCGFloat z)
{
	MCColorVector3 t_vector;
	t_vector.x = x;
	t_vector.y = y;
	t_vector.z = z;
	return t_vector;
}

//////////

bool MCColorTransformLinearRGBToXYZ(const MCColorVector2 &p_white, const MCColorVector2 &p_red, const MCColorVector2 &p_green, const MCColorVector2 &p_blue,
									MCColorVector3 &r_white, MCColorMatrix3x3 &r_matrix);

////////////////////////////////////////////////////////////////////////////////
