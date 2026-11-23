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

#include "prefix.h"

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"
#include "mcstring.h"
#include "globals.h"
#include "mctheme.h"
#include "util.h"
#include "object.h"

#include "context.h"

Boolean MCTheme::load(void)
{
	return False;
}
uint2 MCTheme::getthemeid()
	{
	return 0;
	}
uint2 MCTheme::getthemefamilyid()
{
	return 0;
}
int4 MCTheme::getmetric(Widget_Metric wmetric)
{
	return 0;
}
Boolean MCTheme::iswidgetsupported(Widget_Type wtype)
{
	return False;
}

int4 MCTheme::getwidgetmetric(const MCWidgetInfo &winfo,Widget_Metric wmetric)
{
	return 0;
};

void MCTheme::getwidgetrect(const MCWidgetInfo &winfo, Widget_Metric wmetric, const MCRectangle &srect, MCRectangle &drect)
{
}

Boolean MCTheme::drawwidget(MCDC *dc, const MCWidgetInfo &winfo, const MCRectangle &d)
{
	return False;
}

Widget_Part MCTheme::hittest(const MCWidgetInfo &winfo, int2 mx, int2 my, const MCRectangle &drect)
{
	return WTHEME_PART_UNDEFINED;
}

Boolean MCTheme::getthemepropbool(Widget_ThemeProps themeprop)
{
	return False;
}
void MCTheme::getthemecolor(const MCWidgetInfo &winfo,Widget_Color ctype, MCStringRef &r_themecolor )
{
	r_themecolor = MCValueRetain(kMCEmptyString);
}

void MCTheme::unload()
{
}

////////////////////////////////////////////////////////////////////////////////

// MW-2011-09-14: [[ Bug 9719 ]] Default implementation has an empty MCThemeDrawInfo.
uint32_t MCTheme::getthemedrawinfosize(void)
{
	return 0;
}

int32_t MCTheme::fetchtooltipstartingheight(void)
{
	return 0;
}

bool MCTheme::applythemetotooltipwindow(Window window, const MCRectangle& rect)
{
	return false;
}

bool MCTheme::drawtooltipbackground(MCContext *context, const MCRectangle& rect)
{
	return false;
}

bool MCTheme::settooltiptextcolor(MCContext *context)
{
	return false;
}

bool MCTheme::drawmenubackground(MCContext *context, const MCRectangle& dirty, const MCRectangle& rect, bool with_gutter)
{
	return false;
}

bool MCTheme::drawmenubarbackground(MCContext *context, const MCRectangle& dirty, const MCRectangle& rect, bool is_active)
{
	return false;
}

bool MCTheme::drawmenuheaderbackground(MCContext *context, const MCRectangle& dirty, MCButton *button)
{
	return false;
}

bool MCTheme::drawmenuitembackground(MCContext *context, const MCRectangle& dirty, MCButton *button)
{
	return false;
}

bool MCTheme::drawfocusborder(MCContext *context, const MCRectangle& dirty, const MCRectangle& rect)
{
	return false;
}

bool MCTheme::drawmetalbackground(MCContext *context, const MCRectangle& dirty, const MCRectangle& rect, MCObject *object)
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// HXT colour themes: controls painted in a stack theme's colours.
//
// A native theme draws buttons, tabs, scrollbars and so on in the system's
// colours, which do not suit a window whose colours come from a colour theme,
// least of all when the theme is dark and the system light or the reverse.
// For an object that has a colour theme the control is painted here instead,
// in colours worked out from the theme. Used by the GTK theme; the Windows
// theme has its own copy of the same painting in w32theme.cpp.

extern bool MCInterfaceThemeLookupColor(MCObject *p_object, uint2 p_di, MCColor& r_color);
extern bool MCInterfaceThemeLookupFieldColor(MCObject *p_object, MCColor& r_color);
extern bool MCInterfaceThemeLookupTextOnHilite(MCObject *p_object, const MCColor& p_hilite, MCColor& r_color);

// p_from moved towards p_to by p_amount (0 .. 1).
static MCColor MCThemeMixColor(const MCColor& p_from, const MCColor& p_to, double p_amount)
{
	MCColor c;
	c . red = (uint2)(p_from . red + ((double)p_to . red - (double)p_from . red) * p_amount + 0.5);
	c . green = (uint2)(p_from . green + ((double)p_to . green - (double)p_from . green) * p_amount + 0.5);
	c . blue = (uint2)(p_from . blue + ((double)p_to . blue - (double)p_from . blue) * p_amount + 0.5);
	return c;
}

bool MCThemeGetThemedControlPalette(MCObject *p_object, MCThemedControlPalette& r_pal)
{
	MCColor t_white = {0xffff, 0xffff, 0xffff};
	MCColor t_black = {0x0000, 0x0000, 0x0000};

	MCColor t_bg;
	if (p_object == nil || !MCInterfaceThemeLookupColor(p_object, DI_BACK, t_bg))
		return false;

	bool t_dark = (0.2126 * t_bg . red + 0.7152 * t_bg . green + 0.0722 * t_bg . blue) / 65535.0 < 0.55;

	MCColor t_fg;
	if (!MCInterfaceThemeLookupColor(p_object, DI_FORE, t_fg))
		t_fg = t_dark ? t_white : t_black;

	r_pal . dark = t_dark;
	r_pal . ctrl = MCThemeMixColor(t_bg, t_white, t_dark ? 0.10 : 0.55);
	r_pal . ctrl_hover = MCThemeMixColor(t_bg, t_white, t_dark ? 0.17 : 0.80);
	r_pal . ctrl_press = MCThemeMixColor(t_bg, t_black, t_dark ? 0.20 : 0.10);
	r_pal . ctrl_dis = MCThemeMixColor(r_pal . ctrl, t_bg, 0.60);
	if (!MCInterfaceThemeLookupColor(p_object, DI_BORDER, r_pal . border))
		r_pal . border = MCThemeMixColor(t_bg, t_fg, 0.35);
	if (!MCInterfaceThemeLookupColor(p_object, DI_HILITE, r_pal . accent))
	{
		MCColor t_accent = {0x0000, 0x7878, 0xd4d4};
		r_pal . accent = t_accent;
	}
	if (!MCInterfaceThemeLookupTextOnHilite(p_object, r_pal . accent, r_pal . on_accent))
		r_pal . on_accent = t_white;
	if (!MCInterfaceThemeLookupFieldColor(p_object, r_pal . field))
		r_pal . field = t_dark ? MCThemeMixColor(t_bg, t_black, 0.20) : MCThemeMixColor(t_bg, t_white, 0.70);
	r_pal . field_border = r_pal . border;
	r_pal . tab_sel = r_pal . ctrl;
	r_pal . tab_unsel = MCThemeMixColor(t_bg, t_black, t_dark ? 0.12 : 0.05);
	r_pal . pane = t_bg;
	r_pal . group_border = r_pal . border;
	r_pal . track = t_dark ? MCThemeMixColor(t_bg, t_white, 0.06) : MCThemeMixColor(t_bg, t_black, 0.06);
	r_pal . thumb = MCThemeMixColor(t_bg, t_fg, 0.40);
	r_pal . themed = true;
	return true;
}

// A small filled triangle in the middle of p_rect. p_direction: 0 up, 1 down,
// 2 left, 3 right.
static void MCThemeFillArrow(MCDC *dc, const MCRectangle& p_rect, int p_direction)
{
	int2 t_cx = p_rect . x + p_rect . width / 2;
	int2 t_cy = p_rect . y + p_rect . height / 2;
	int2 t_size = (p_rect . width < p_rect . height ? p_rect . width : p_rect . height) / 4;
	if (t_size < 2)
		t_size = 2;

	MCPoint t_points[3];
	switch (p_direction)
	{
	case 0:
		t_points[0] . x = t_cx - t_size; t_points[0] . y = t_cy + t_size / 2;
		t_points[1] . x = t_cx + t_size; t_points[1] . y = t_cy + t_size / 2;
		t_points[2] . x = t_cx;          t_points[2] . y = t_cy - t_size / 2 - 1;
		break;
	case 1:
		t_points[0] . x = t_cx - t_size; t_points[0] . y = t_cy - t_size / 2;
		t_points[1] . x = t_cx + t_size; t_points[1] . y = t_cy - t_size / 2;
		t_points[2] . x = t_cx;          t_points[2] . y = t_cy + t_size / 2 + 1;
		break;
	case 2:
		t_points[0] . x = t_cx + t_size / 2;     t_points[0] . y = t_cy - t_size;
		t_points[1] . x = t_cx + t_size / 2;     t_points[1] . y = t_cy + t_size;
		t_points[2] . x = t_cx - t_size / 2 - 1; t_points[2] . y = t_cy;
		break;
	default:
		t_points[0] . x = t_cx - t_size / 2;     t_points[0] . y = t_cy - t_size;
		t_points[1] . x = t_cx - t_size / 2;     t_points[1] . y = t_cy + t_size;
		t_points[2] . x = t_cx + t_size / 2 + 1; t_points[2] . y = t_cy;
		break;
	}
	dc->setfillstyle(FillSolid, nil, 0, 0);
	dc->fillpolygon(t_points, 3);
}

// Paints the control described by winfo into p_rect. False if this kind of
// control is not painted here, in which case the native theme should draw it.
bool MCThemeDrawThemedControl(MCDC *dc, const MCWidgetInfo& winfo, const MCRectangle& p_rect, const MCThemedControlPalette& p_pal)
{
	bool t_handled = true;
	switch (winfo.type)
	{
	// ── Push / bevel / option buttons ─────────────────────────────────
	case WTHEME_TYPE_PUSHBUTTON:
	case WTHEME_TYPE_BEVELBUTTON:
	case WTHEME_TYPE_RECTANGLE_BUTTON:
	case WTHEME_TYPE_PULLDOWN:
	case WTHEME_TYPE_COMBOBUTTON:
	case WTHEME_TYPE_OPTIONBUTTONARROW:
	case WTHEME_TYPE_OPTIONBUTTONTEXT:
	{
		// The arrow of an option menu sits on the face already painted
		// for the menu as a whole.
		if (winfo.type == WTHEME_TYPE_OPTIONBUTTONARROW)
		{
			dc->setforeground((winfo.state & WTHEME_STATE_DISABLED) ? p_pal . border : p_pal . thumb);
			MCThemeFillArrow(dc, p_rect, 1);
			break;
		}

		MCColor t_bg;
		if (winfo.state & WTHEME_STATE_DISABLED)
			t_bg = p_pal . ctrl_dis;
		else if (winfo.state & WTHEME_STATE_PRESSED || winfo.state & WTHEME_STATE_HILITED)
			t_bg = p_pal . ctrl_press;
		else if (winfo.state & WTHEME_STATE_HOVER)
			t_bg = p_pal . ctrl_hover;
		else
			t_bg = p_pal . ctrl;
		dc->setforeground(t_bg);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		// Border: use focus-ring colour when focused, standard border otherwise
		MCColor t_border;
		if (winfo.state & WTHEME_STATE_HASFOCUS)
			t_border = p_pal . accent;
		else if (p_pal . themed && (winfo.state & WTHEME_STATE_HASDEFAULT) && !(winfo.state & WTHEME_STATE_DISABLED))
			t_border = p_pal . accent;
		else
			t_border = p_pal . border;
		dc->setforeground(t_border);
		dc->drawrect(p_rect, false);
		if (winfo.type == WTHEME_TYPE_COMBOBUTTON)
		{
			dc->setforeground((winfo.state & WTHEME_STATE_DISABLED) ? p_pal . border : p_pal . thumb);
			MCThemeFillArrow(dc, p_rect, 1);
		}
		break;
	}

	// ── Checkbox ──────────────────────────────────────────────────────
	case WTHEME_TYPE_CHECKBOX:
	{
		// Draw the indicator box in dark colours
		bool t_checked = (winfo.state & WTHEME_STATE_HILITED) != 0;
		bool t_enabled = (winfo.state & WTHEME_STATE_DISABLED) == 0;
		MCColor t_bg;
		if (!t_enabled)
			t_bg = p_pal . ctrl_dis;
		else if (t_checked)
			t_bg = p_pal . accent;
		else
			t_bg = p_pal . field;
		dc->setforeground(t_bg);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		MCColor t_border;
		if ((winfo.state & WTHEME_STATE_HASFOCUS) || (t_checked && t_enabled))
			t_border = p_pal . accent;
		else
			t_border = p_pal . border;
		dc->setforeground(t_border);
		dc->drawrect(p_rect, false);
		// If checked, draw a simple check mark in a colour that shows on the box
		if (t_checked)
		{
			MCColor t_check = t_enabled ? p_pal . on_accent : p_pal . border;
			dc->setforeground(t_check);
			// Two line segments forming a "tick": left→mid-low→top-right
			int2 cx = p_rect.x + p_rect.width / 2;
			int2 cy = p_rect.y + p_rect.height / 2;
			dc->drawline(p_rect.x + 2,               cy - 1,
			             cx - 1,                    p_rect.y + p_rect.height - 3);
			dc->drawline(cx - 1,                    p_rect.y + p_rect.height - 3,
			             p_rect.x + p_rect.width - 3, p_rect.y + 2);
		}
		break;
	}

	// ── Radio button ──────────────────────────────────────────────────
	case WTHEME_TYPE_RADIOBUTTON:
	{
		MCColor t_bg;
		if (winfo.state & WTHEME_STATE_DISABLED)
			t_bg = p_pal . ctrl_dis;
		else
			t_bg = p_pal . field;
		dc->setforeground(t_bg);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillarc(p_rect, 0, 360, false);
		MCColor t_border;
		if (winfo.state & WTHEME_STATE_HASFOCUS)
			t_border = p_pal . accent;
		else
			t_border = p_pal . border;
		dc->setforeground(t_border);
		dc->drawarc(p_rect, 0, 360);
		// If selected, draw a small filled dot in the centre
		if (winfo.state & WTHEME_STATE_HILITED)
		{
			MCColor t_dot = p_pal . accent;
			dc->setforeground(t_dot);
			dc->setfillstyle(FillSolid, nil, 0, 0);
			int2 inset = p_rect.width / 4;
			MCRectangle dot = MCU_reduce_rect(p_rect, inset);
			dc->fillarc(dot, 0, 360, false);
		}
		break;
	}

	// ── Text fields ───────────────────────────────────────────────────
	case WTHEME_TYPE_TEXTFIELD:
	case WTHEME_TYPE_TEXTFIELD_FRAME:
	case WTHEME_TYPE_COMBOTEXT:
	case WTHEME_TYPE_COMBOFRAME:
	{
		// The native frame is up to three pixels deep, and the field only
		// fills inside that: paint the rest of the ring in the field's
		// colour so no strip of the window shows between border and text.
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->setforeground(p_pal . field);
		if (p_rect . width > 6 && p_rect . height > 6)
		{
			dc->drawrect(MCU_reduce_rect(p_rect, 1), false);
			dc->drawrect(MCU_reduce_rect(p_rect, 2), false);
		}
		dc->setforeground(p_pal . field_border);
		dc->drawrect(p_rect, false);
		break;
	}

	case WTHEME_TYPE_TEXTFIELD_FILL:
	{
		// Fill the content interior of the text field.
		dc->setforeground(p_pal . field);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		break;
	}

	// ── Tab buttons ───────────────────────────────────────────────────
	case WTHEME_TYPE_TAB:
	{
		MCColor t_bg;
		if (winfo.state & WTHEME_STATE_HILITED)
			t_bg = p_pal . tab_sel;
		else
			t_bg = p_pal . tab_unsel;
		dc->setforeground(t_bg);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		dc->setforeground(p_pal . border);
		dc->drawrect(p_rect, false);
		break;
	}

	// ── Tab pane ──────────────────────────────────────────────────────
	case WTHEME_TYPE_TABPANE:
	{
		dc->setforeground(p_pal . pane);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		dc->setforeground(p_pal . border);
		dc->drawrect(p_rect, false);
		break;
	}

	// ── Group / secondary-group frame ─────────────────────────────────
	case WTHEME_TYPE_GROUP_FRAME:
	case WTHEME_TYPE_SECONDARYGROUP_FRAME:
	{
		dc->setforeground(p_pal . group_border);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->drawrect(p_rect, false);
		break;
	}

	// ── Group / secondary-group fill ──────────────────────────────────
	case WTHEME_TYPE_GROUP_FILL:
	case WTHEME_TYPE_SECONDARYGROUP_FILL:
	{
		dc->setforeground(p_pal . pane);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		break;
	}

	// ── Scrollbar ─────────────────────────────────────────────────────
	case WTHEME_TYPE_SCROLLBAR_TRACK_VERTICAL:
	case WTHEME_TYPE_SCROLLBAR_TRACK_HORIZONTAL:
	{
		dc->setforeground(p_pal . track);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		break;
	}

	case WTHEME_TYPE_SCROLLBAR_BUTTON_UP:
	case WTHEME_TYPE_SCROLLBAR_BUTTON_DOWN:
	case WTHEME_TYPE_SCROLLBAR_BUTTON_LEFT:
	case WTHEME_TYPE_SCROLLBAR_BUTTON_RIGHT:
	{
		dc->setforeground(p_pal . track);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		if (winfo.state & WTHEME_STATE_DISABLED)
			dc->setforeground(p_pal . border);
		else
			dc->setforeground(p_pal . thumb);
		MCThemeFillArrow(dc, p_rect,
		             winfo.type == WTHEME_TYPE_SCROLLBAR_BUTTON_UP ? 0 :
		             winfo.type == WTHEME_TYPE_SCROLLBAR_BUTTON_DOWN ? 1 :
		             winfo.type == WTHEME_TYPE_SCROLLBAR_BUTTON_LEFT ? 2 : 3);
		break;
	}

	case WTHEME_TYPE_SCROLLBAR_THUMB_VERTICAL:
	case WTHEME_TYPE_SCROLLBAR_THUMB_HORIZONTAL:
	{
		dc->setforeground(p_pal . track);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		if (p_rect . width > 0 && p_rect . height > 0)
		{
			// a bar narrower than the track, with rounded ends
			MCRectangle t_bar = p_rect;
			if (winfo.type == WTHEME_TYPE_SCROLLBAR_THUMB_VERTICAL)
			{
				if (t_bar . width > 8) { t_bar . x += 3; t_bar . width -= 6; }
			}
			else
			{
				if (t_bar . height > 8) { t_bar . y += 3; t_bar . height -= 6; }
			}
			MCColor t_thumb = p_pal . thumb;
			if (winfo.state & (WTHEME_STATE_PRESSED | WTHEME_STATE_HOVER))
				t_thumb = MCThemeMixColor(p_pal . thumb, p_pal . accent, 0.35);
			dc->setforeground(t_thumb);
			uint2 t_radius = (t_bar . width < t_bar . height ? t_bar . width : t_bar . height) / 2;
			dc->fillroundrect(t_bar, t_radius);
		}
		break;
	}

	case WTHEME_TYPE_SCROLLBAR_GRIPPER_VERTICAL:
	case WTHEME_TYPE_SCROLLBAR_GRIPPER_HORIZONTAL:
		// the thumb has no grip marks
		break;

	// ── Spin (small scrollbar) buttons ────────────────────────────────
	case WTHEME_TYPE_SPIN:
	{
		MCColor t_bg;
		if (winfo.state & WTHEME_STATE_DISABLED)
			t_bg = p_pal . ctrl_dis;
		else if (winfo.state & WTHEME_STATE_PRESSED)
			t_bg = p_pal . ctrl_press;
		else
			t_bg = p_pal . ctrl;
		dc->setforeground(t_bg);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		dc->setforeground(p_pal . border);
		dc->drawrect(p_rect, false);
		if (winfo.state & WTHEME_STATE_DISABLED)
			dc->setforeground(p_pal . border);
		else
			dc->setforeground(p_pal . thumb);
		MCThemeFillArrow(dc, p_rect,
		             winfo.part == WTHEME_PART_SPIN_ARROW_UP ? 0 :
		             winfo.part == WTHEME_PART_SPIN_ARROW_DOWN ? 1 :
		             winfo.part == WTHEME_PART_SPIN_ARROW_LEFT ? 2 : 3);
		break;
	}

	// ── Progress bar ──────────────────────────────────────────────────
	case WTHEME_TYPE_PROGRESSBAR_HORIZONTAL:
	case WTHEME_TYPE_PROGRESSBAR_VERTICAL:
	{
		dc->setforeground(p_pal . track);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		dc->setforeground(p_pal . border);
		dc->drawrect(p_rect, false);
		break;
	}

	case WTHEME_TYPE_PROGRESSBAR_CHUNK:
	case WTHEME_TYPE_PROGRESSBAR_CHUNK_VERTICAL:
	{
		if (winfo.state & WTHEME_STATE_DISABLED)
			dc->setforeground(p_pal . border);
		else
			dc->setforeground(p_pal . accent);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		break;
	}

	// ── Option menu drawn as one control (GTK) ────────────────────────
	case WTHEME_TYPE_OPTIONBUTTON:
	{
		MCColor t_bg;
		if (winfo.state & WTHEME_STATE_DISABLED)
			t_bg = p_pal . ctrl_dis;
		else if (winfo.state & WTHEME_STATE_PRESSED)
			t_bg = p_pal . ctrl_press;
		else if (winfo.state & WTHEME_STATE_HOVER)
			t_bg = p_pal . ctrl_hover;
		else
			t_bg = p_pal . ctrl;
		dc->setforeground(t_bg);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		dc->setforeground((winfo.state & WTHEME_STATE_HASFOCUS) ? p_pal . accent : p_pal . border);
		dc->drawrect(p_rect, false);
		MCRectangle t_arrow = p_rect;
		if (t_arrow . width > 22)
		{
			t_arrow . x += t_arrow . width - 20;
			t_arrow . width = 16;
		}
		dc->setforeground((winfo.state & WTHEME_STATE_DISABLED) ? p_pal . border : p_pal . thumb);
		MCThemeFillArrow(dc, t_arrow, 1);
		break;
	}

	// ── List box frame ────────────────────────────────────────────────
	case WTHEME_TYPE_LISTBOX:
	case WTHEME_TYPE_LISTBOX_FRAME:
	{
		dc->setforeground(p_pal . field_border);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->drawrect(p_rect, false);
		break;
	}

	// ── Slider ────────────────────────────────────────────────────────
	case WTHEME_TYPE_SLIDER_TRACK_HORIZONTAL:
	case WTHEME_TYPE_SLIDER_TRACK_VERTICAL:
	case WTHEME_TYPE_SLIDER_FILL_HORIZONTAL:
	case WTHEME_TYPE_SLIDER_FILL_VERTICAL:
	{
		// a thin groove along the middle of the rect
		bool t_fill = winfo.type == WTHEME_TYPE_SLIDER_FILL_HORIZONTAL || winfo.type == WTHEME_TYPE_SLIDER_FILL_VERTICAL;
		bool t_vertical = winfo.type == WTHEME_TYPE_SLIDER_TRACK_VERTICAL || winfo.type == WTHEME_TYPE_SLIDER_FILL_VERTICAL;
		MCRectangle t_groove = p_rect;
		if (t_vertical)
		{
			if (t_groove . width > 4) { t_groove . x += (t_groove . width - 4) / 2; t_groove . width = 4; }
		}
		else
		{
			if (t_groove . height > 4) { t_groove . y += (t_groove . height - 4) / 2; t_groove . height = 4; }
		}
		if (t_groove . width > 0 && t_groove . height > 0)
		{
			dc->setforeground(t_fill ? p_pal . accent : MCThemeMixColor(p_pal . track, p_pal . thumb, 0.35));
			dc->setfillstyle(FillSolid, nil, 0, 0);
			dc->fillroundrect(t_groove, 2);
		}
		break;
	}

	case WTHEME_TYPE_SLIDER_THUMB_HORIZONTAL:
	case WTHEME_TYPE_SLIDER_THUMB_VERTICAL:
	{
		// a round knob in the middle of the rect
		MCRectangle t_knob = p_rect;
		uint2 t_size = t_knob . width < t_knob . height ? t_knob . width : t_knob . height;
		if (t_size > 16)
			t_size = 16;
		t_knob . x += (t_knob . width - t_size) / 2;
		t_knob . y += (t_knob . height - t_size) / 2;
		t_knob . width = t_knob . height = t_size;
		if (t_size > 0)
		{
			MCColor t_knob_color;
			if (winfo.state & WTHEME_STATE_DISABLED)
				t_knob_color = p_pal . ctrl_dis;
			else
				t_knob_color = p_pal . dark ? p_pal . thumb : p_pal . ctrl;
			dc->setforeground(t_knob_color);
			dc->setfillstyle(FillSolid, nil, 0, 0);
			dc->fillarc(t_knob, 0, 360, false);
			dc->setforeground((winfo.state & (WTHEME_STATE_PRESSED | WTHEME_STATE_HOVER)) ? p_pal . accent : p_pal . border);
			dc->drawarc(t_knob, 0, 360);
		}
		break;
	}

	// ── Small scrollbar (stepper) drawn as one control (GTK) ──────────
	case WTHEME_TYPE_SMALLSCROLLBAR:
	{
		dc->setforeground((winfo.state & WTHEME_STATE_DISABLED) ? p_pal . ctrl_dis : p_pal . ctrl);
		dc->setfillstyle(FillSolid, nil, 0, 0);
		dc->fillrect(p_rect);
		dc->setforeground(p_pal . border);
		dc->drawrect(p_rect, false);
		dc->setforeground((winfo.state & WTHEME_STATE_DISABLED) ? p_pal . border : p_pal . thumb);
		MCRectangle t_first = p_rect, t_second = p_rect;
		if (winfo.attributes & WTHEME_ATT_SBVERTICAL)
		{
			t_first . height = p_rect . height / 2;
			t_second . y += t_first . height;
			t_second . height = p_rect . height - t_first . height;
			MCThemeFillArrow(dc, t_first, 0);
			MCThemeFillArrow(dc, t_second, 1);
		}
		else
		{
			t_first . width = p_rect . width / 2;
			t_second . x += t_first . width;
			t_second . width = p_rect . width - t_first . width;
			MCThemeFillArrow(dc, t_first, 2);
			MCThemeFillArrow(dc, t_second, 3);
		}
		break;
	}

	default:
		t_handled = false;
		break;
	}
	return t_handled;
}
