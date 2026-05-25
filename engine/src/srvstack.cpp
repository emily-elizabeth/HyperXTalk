#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"


#include "dispatch.h"
#include "stack.h"
#include "card.h"
#include "group.h"
#include "image.h"
#include "field.h"
#include "stacklst.h"
#include "cardlst.h"
#include "sellst.h"
#include "handler.h"
#include "mcerror.h"
#include "param.h"
#include "util.h"
#include "debug.h"
#include "player.h"
#include "visual.h"

#include "globals.h"
#include "graphics_util.h"

////////////////////////////////////////////////////////////////////////////////

void MCResPlatformInitPixelScaling(void)
{
}

// IM-2014-01-31: [[ HiDPI ]] Pixel scaling not supported on server
bool MCResPlatformSupportsPixelScaling(void)
{
	return false;
}

// IM-2014-01-31: [[ HiDPI ]] Pixel scaling not supported on server
bool MCResPlatformCanChangePixelScaling(void)
{
	return false;
}

// IM-2014-01-31: [[ HiDPI ]] Pixel scaling not supported on server
bool MCResPlatformCanSetPixelScale(void)
{
	return false;
}

// IM-2014-01-31: [[ HiDPI ]] Pixel scaling not supported on server
MCGFloat MCResPlatformGetDefaultPixelScale(void)
{
	return 1.0;
}

// IM-2014-03-14: [[ HiDPI ]] Pixel scaling not supported on server
MCGFloat MCResPlatformGetUIDeviceScale(void)
{
	return 1.0;
}

// IM-2014-01-31: [[ HiDPI ]] Pixel scaling not supported on server
void MCResPlatformHandleScaleChange(void)
{
}

////////////////////////////////////////////////////////////////////////////////

MCStack *MCStack::findstackd(Window w)
{
	if (w == window)
		return this;
	if (substacks != NULL)
	{
		MCStack *tptr = substacks;
		do
		{
			if (w == tptr->window)
				return tptr;
			tptr = (MCStack *)tptr->next();
		}
		while (tptr != substacks);
	}
	return NULL;
}


void MCStack::realize(void)
{
	start_externals();
}

void MCStack::setsizehints(void)
{
}

void MCStack::sethints(void)
{
}

void MCStack::destroywindowshape(void)
{
}

bool MCStack::view_platform_dirtyviewonresize() const
{
	return false;
}

MCRectangle MCStack::view_platform_setgeom(const MCRectangle &p_rect)
{
	return MCRectangleMake(0,0,0,0);
}

void MCStack::setgeom(void)
{
	state &= ~CS_NEED_RESIZE;
}

void MCStack::start_externals()
{
	loadexternals();
}

void MCStack::stop_externals()
{
	unloadexternals();
}

void MCStack::platform_openwindow(Boolean override)
{
	MCscreen -> openwindow(window, override);
}

void MCStack::setopacity(unsigned char p_level)
{
}

void MCStack::updatemodifiedmark(void)
{
}

// MERG-2014-06-02: [[ IgnoreMouseEvents ]] Stub for ignoreMouseEvents.
void MCStack::updateignoremouseevents(void)
{
}

void MCStack::view_platform_updatewindow(MCRegionRef region)
{
}

void MCStack::clearscroll(void)
{
}

void MCStack::applyscroll(void)
{
}

void MCStack::effectrect(const MCRectangle& p_rect, Boolean& r_abort)
{
	MCEffectList *t_effects = MCcur_effects;
	MCcur_effects = NULL;
	
	// There are no visual effects in server mode
	while(t_effects != NULL)
	{
		MCEffectList *t_effect;
		t_effect = t_effects;
		t_effects = t_effects -> next;
		delete t_effect;
	}
}

// MM-2013-03-06: [[ RefactorGraphics ]] Added to allow server engines to compile.
MCRectangle MCStack::view_platform_getwindowrect() const
{
	return rect;	
}

void MCStack::redrawicon(void)
{
}

void MCStack::enablewindow(bool enable)
{
}

// MERG-2015-10-12: [[ DocumentFilename ]] Stub for documentFilename.
void MCStack::updatedocumentfilename(void)
{
}

////////////////////////////////////////////////////////////////////////////////

bool MCStack::configure_window_buffer()
{
	return true;
}

void MCStack::release_window_buffer()
{
}

////////////////////////////////////////////////////////////////////////////////
