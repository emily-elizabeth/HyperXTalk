#ifndef EDITTOOL_H
#define EDITTOOL_H

#include "typedefs.h"
#include "graphic.h"

enum MCEditMode
{
	kMCEditModeNone,
	kMCEditModeFillGradient,
	kMCEditModeStrokeGradient,
	kMCEditModePolygon,
};

class MCGraphic;

class MCEditTool
{
public:
	virtual ~MCEditTool(void) {};
	virtual bool mdown(int2 x, int2 y, uint2 which) = 0;
	virtual bool mfocus(int2 x, int2 y) = 0;
	virtual bool mup(int2 x, int2 y, uint2 which) = 0;
	virtual void drawhandles(MCDC *dc) = 0;
	virtual uint4 handle_under_point(int2 x, int2 y) = 0;
	virtual MCRectangle drawrect() = 0;
	virtual MCEditMode type() = 0;
};

class MCGradientEditTool : public MCEditTool
{
public:
	bool mdown(int2 x, int2 y, uint2 which);
	bool mfocus(int2 x, int2 y);
	bool mup(int2 x, int2 y, uint2 which);
	void drawhandles(MCDC *dc);
	uint4 handle_under_point(int2 x, int2 y);
	MCRectangle drawrect();
	MCEditMode type();

	MCGradientEditTool(MCGraphic *p_graphic, MCGradientFill *p_gradient, MCEditMode p_mode);
private:
	MCEditMode mode;
	MCGraphic *graphic;
	MCGradientFill *gradient;
	uint4 m_gradient_edit_point;
	int4 xoffset, yoffset;

	void gradient_rects(MCRectangle *rects);
};

class MCPolygonEditTool : public MCEditTool
{
public:
	bool mdown(int2 x, int2 y, uint2 which);
	bool mfocus(int2 x, int2 y);
	bool mup(int2 x, int2 y, uint2 which);
	void drawhandles(MCDC *dc);
	uint4 handle_under_point(int2 x, int2 y);
	MCRectangle drawrect();
	MCEditMode type();

	MCPolygonEditTool(MCGraphic *p_graphic);
private:
	MCGraphic *graphic;
	uint4 m_polygon_edit_point;
	uint4 m_path_start_point;
	int4 xoffset, yoffset;

	void point_rects(MCRectangle *rects);
};

#endif

