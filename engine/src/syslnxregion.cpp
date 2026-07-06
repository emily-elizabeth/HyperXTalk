/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "lnxprefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "util.h"
#include "globals.h"
#include "region.h"

#include "lnxdc.h"


// -- tperry 12-11-2025: GTK3 uses cairo_region_t instead of GdkRegion
bool MCLinuxRegionCreate(cairo_region_t* &r_region)
{
	cairo_region_t *t_rgn;
	t_rgn = cairo_region_create();
	if (t_rgn == nil)
		return false;

	r_region = t_rgn;
	return true;
}

void MCLinuxRegionDestroy(cairo_region_t* p_region)
{
	if (p_region == nil)
		return;
    
	cairo_region_destroy(p_region);
}

bool MCLinuxRegionIncludeRect(cairo_region_t* p_region, const MCGIntegerRectangle& p_rect)
{
	cairo_rectangle_int_t t_rect;
	t_rect . x = p_rect.origin.x;
	t_rect . y = p_rect.origin.y;
	t_rect . width = p_rect.size.width;
	t_rect . height = p_rect.size.height;
    cairo_region_union_rectangle(p_region, &t_rect);
	return true;
}

static bool MCLinuxMCGRegionToRegionCallback(void *p_context, const MCGIntegerRectangle &p_rect)
{
	return MCLinuxRegionIncludeRect(*static_cast<cairo_region_t**>(p_context), p_rect);
}

bool MCLinuxMCGRegionToRegion(MCGRegionRef p_region, cairo_region_t* &r_region)
{
	bool t_success;
	t_success = true;
	
	cairo_region_t* t_region;
	t_region = nil;
	
	t_success = MCLinuxRegionCreate(t_region);
	
	if (t_success)
		t_success = MCGRegionIterate(p_region, MCLinuxMCGRegionToRegionCallback, &t_region);
	
	if (t_success)
	{
		r_region = t_region;
		return true;
	}
	
	MCLinuxRegionDestroy(t_region);
	
	return false;
}

bool MCLinuxRegionToMCGRegion(cairo_region_t *p_region, MCGRegionRef &r_region)
{
    MCGRegionCreate(r_region);
    
    // -- tperry 12-11-2025: cairo_region_t uses different API than GdkRegion
    int t_nrects = cairo_region_num_rectangles(p_region);
    for (int i = 0; i < t_nrects; i++)
    {
        cairo_rectangle_int_t t_cairo_rect;
        cairo_region_get_rectangle(p_region, i, &t_cairo_rect);
        
        MCGIntegerRectangle t_rect;
        t_rect.origin.x = t_cairo_rect.x;
        t_rect.origin.y = t_cairo_rect.y;
        t_rect.size.width = t_cairo_rect.width;
        t_rect.size.height = t_cairo_rect.height;
        MCGRegionAddRect(r_region, t_rect);
    }
    
    return true;
}


