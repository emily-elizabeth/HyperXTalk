#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"


#include "exec.h"
#include "dispatch.h"
#include "stack.h"
#include "image.h"
#include "util.h"
#include "date.h"
#include "player.h"

#include "sellst.h"
#include "stacklst.h"
#include "card.h"
#include "globals.h"
#include "osspec.h"

#include "w32dc.h"
#include "resource.h"
#include "meta.h"
#include "mode.h"

#include "w32text.h"
#include "w32compat.h"

#include "graphicscontext.h"
#include "graphics_util.h"
#include "redraw.h"
#include "mctheme.h"
#include "w32theme.h"

#ifndef CS_DROPSHADOW
#define CS_DROPSHADOW   0x00020000
#endif

uint4 MCScreenDC::image_inks[] =
    {
        BLACKNESS, SRCAND, SRCERASE,
        SRCCOPY, 0x00220326, 0x00AA0029, SRCINVERT, SRCPAINT, NOTSRCERASE,
        SRCINVERT, DSTINVERT, 0x00DD0228, NOTSRCCOPY, MERGEPAINT,
        0x007700E6, WHITENESS
    };

static MCColor vgapalette[16] =
    {
        {0x0000, 0x0000, 0x0000},
		{0x8080, 0x0000, 0x0000},
        {0x0000, 0x8080, 0x0000},
		{0x0000, 0x0000, 0x8080},
        {0x8080, 0x8080, 0x0000},
		{0x8080, 0x0000, 0x8080},
        {0x0000, 0x8080, 0x8080},
		{0x8080, 0x8080, 0x8080},
        {0xC0C0, 0xC0C0, 0xC0C0},
		{0xFFFF, 0x0000, 0x0000},
        {0x0000, 0xFFFF, 0x0000},
		{0x0000, 0x0000, 0xFFFF},
        {0xFFFF, 0xFFFF, 0x0000},
		{0xFFFF, 0x0000, 0xFFFF},
        {0x0000, 0xFFFF, 0xFFFF},
		{0xFFFF, 0xFFFF, 0xFFFF},
    };

// Forward declaration for MCplatformIsDarkMode, defined later in this file.
extern "C" bool MCplatformIsDarkMode(void);

void MCScreenDC::setstatus(MCStringRef status)
{ //No action
}

Boolean MCScreenDC::open()
{
	WNDCLASSA  wc;  //window class

	// Fill in window class structure with parameters that describe
	// the MAIN window.
	//
	// MW-2006-01-04: This used to have CS_CLASSDC - but this causes problems
	//   with certain kinds of window. Also, given we always double-buffer rendering
	//   now, the only thing we have to (potentially) setup a DC with is a palette.
	wc.style         = CS_DBLCLKS;
	wc.lpfnWndProc   = (WNDPROC)MCWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 4;
	wc.hInstance     = MChInst;
	wc.hIcon         = LoadIconA(MChInst, MAKEINTRESOURCEA(IDI_ICON1));
	wc.hCursor       = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = MC_APP_NAME;
	wc.lpszClassName = MC_WIN_CLASS_NAME;

	WNDCLASSW wwc;
	wwc.style         = CS_DBLCLKS;
	wwc.lpfnWndProc   = (WNDPROC)MCWindowProc;
	wwc.cbClsExtra    = 0;
	wwc.cbWndExtra    = 4;
	wwc.hInstance     = MChInst;
	wwc.hIcon         = LoadIconA(MChInst, MAKEINTRESOURCEA(IDI_ICON1));
	wwc.hCursor       = NULL;
	wwc.hbrBackground = NULL;
	wwc.lpszMenuName  = MC_APP_NAME_W;
	wwc.lpszClassName = MC_WIN_CLASS_NAME_W;

	// Register the window class and return success/failure code.
	if (RegisterClassW(&wwc) == 0)
		return FALSE;

	if (RegisterClassA(&wc) == 0)
		return FALSE;
	wc.style |= CS_SAVEBITS;
	wc.lpszClassName = MC_POPUP_WIN_CLASS_NAME;
	if (RegisterClassA(&wc) == 0)
		return FALSE;
	wc.lpszClassName = MC_MENU_WIN_CLASS_NAME;

	HINSTANCE tdll = LoadLibraryA("UxTheme.dll");
	if (tdll)
	{
		wc.style |= CS_DROPSHADOW;
		FreeLibrary(tdll);
	}
	if (RegisterClassA(&wc) == 0)
		return FALSE;
	// Wide version of the menu/popup class — used by CreateWindowExW for
	// popovers, pulldowns, and tooltips so they get the CS_DROPSHADOW style.
	wwc.style = wc.style; // inherits CS_DROPSHADOW if UxTheme was loaded
	wwc.lpszClassName = MC_MENU_WIN_CLASS_NAME_W;
	if (RegisterClassW(&wwc) == 0)
		return FALSE;
	// Define the VIDEO CLIP window. Has its own DC
	wc.style         = /*CS_OWNDC | */CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc   = (WNDPROC)MCPlayerWindowProc;
	wc.lpszClassName = MC_VIDEO_WIN_CLASS_NAME;  //video class
	if (RegisterClassA(&wc) == 0)
		return FALSE;


	// Define Snapshot window. Has its own window proc
	wc.style         = CS_DBLCLKS | CS_CLASSDC;
	wc.lpfnWndProc   = (WNDPROC)MCSnapshotWindowProc;
	wc.lpszClassName = MC_SNAPSHOT_WIN_CLASS_NAME;  //snapshot class
	if (RegisterClassA(&wc) == 0)
		return FALSE;

	// Define backdrop window class. Has its own window proc
	wc.style         = CS_CLASSDC;
	wc.lpfnWndProc   = (WNDPROC)MCBackdropWindowProc;
	wc.lpszClassName = MC_BACKDROP_WIN_CLASS_NAME;  //backdrop class
	if (RegisterClassA(&wc) == 0)
		return FALSE;

	f_src_dc = CreateCompatibleDC(NULL);
	f_dst_dc = CreateCompatibleDC(NULL);

	vis = new (nothrow) MCVisualInfo;

	ncolors = 0;
			redbits = greenbits = bluebits = 8;
			redshift = 16;
			greenshift = 8;
			blueshift = 0;
			vis->red_mask = 0x00FF0000;
			vis->green_mask = 0x0000FF00;
			vis->blue_mask = 0x000000FF;

	black_pixel.red = black_pixel.green = black_pixel.blue = 0;
	white_pixel.red = white_pixel.green = white_pixel.blue = 0xFFFF;

	MCselectioncolor = MCpencolor = black_pixel;
	
	MConecolor = MCbrushcolor = white_pixel;
	
	gray_pixel.red = gray_pixel.green = gray_pixel.blue = 0x8080;
	
	MChilitecolor.red = MChilitecolor.green = 0x0000;
	MChilitecolor.blue = 0x8080;

    MCExecContext ctxt(nil, nil, nil);
    MCStringRef t_key;
    t_key = MCSTR("HKEY_CURRENT_USER\\Control Panel\\Colors\\Hilight");
    MCAutoValueRef t_value;
    MCAutoStringRef t_type, t_error;
    /* UNCHECKED */ MCS_query_registry(t_key, &t_value, &t_type, &t_error);

	if (*t_value != nil && !MCValueIsEmpty(*t_value))
	{
		MCAutoStringRef t_string;
		/* UNCHECKED */ ctxt . ConvertToString(*t_value, &t_string);
		MCAutoStringRef t_string_mutable;
		MCStringMutableCopy(*t_string, &t_string_mutable);
		/* UNCHECKED */ MCStringFindAndReplaceChar(*t_string_mutable, ' ', ',', kMCCompareExact);
		/* UNCHECKED */ parsecolor(*t_string_mutable, MChilitecolor);
	}
	
	MCaccentcolor = MChilitecolor;
	
	background_pixel.red = background_pixel.green = background_pixel.blue = 0xC0C0;
    MCStringRef t_key2;

	if (MCmajorosversion > MCOSVersionMake(4,0,0))
		t_key2 = MCSTR("HKEY_CURRENT_USER\\Control Panel\\Colors\\MenuBar");
	else
		t_key2 = MCSTR("HKEY_CURRENT_USER\\Control Panel\\Colors\\Menu");

	MCAutoValueRef t_value2;
    MCAutoStringRef t_type2, t_error2;
    /* UNCHECKED */ MCS_query_registry(t_key2, &t_value2, &t_type2, &t_error2);

	if (*t_value != nil && !MCValueIsEmpty(*t_value2))
	{
		MCAutoStringRef t_string;
		/* UNCHECKED */ ctxt . ConvertToString(*t_value2, &t_string);
		MCAutoStringRef t_string_mutable;
		MCStringMutableCopy(*t_string, &t_string_mutable);
		/* UNCHECKED */ MCStringFindAndReplaceChar(*t_string_mutable, ' ', ',', kMCCompareExact);
		/* UNCHECKED */ parsecolor(*t_string_mutable, background_pixel);
	}

	SetBkMode(f_dst_dc, OPAQUE);
	SetBkColor(f_dst_dc, MCColorGetPixel(black_pixel));
	SetTextColor(f_dst_dc, MCColorGetPixel(white_pixel));
	SetBkMode(f_src_dc, OPAQUE);
	SetBkColor(f_src_dc, MCColorGetPixel(black_pixel));
	SetTextColor(f_src_dc, MCColorGetPixel(white_pixel));

	mousetimer = 0;
	grabbed = False;
	MCcursors[PI_NONE] = nil;

	//create an invisible main window for all the dlgs, secondary windows to use as their
	//parent window, so that they will not have icons show up in the desktop taskbar
	//use video class window which has it's own DC
	invisiblehwnd = CreateWindowA(MC_WIN_CLASS_NAME, "MCdummy",
	                             WS_POPUP, 0, 0, 8, 8,
	                             NULL, NULL, MChInst, NULL);

	// Prime uxtheme dark-mode NOW — before MCNativeTheme::load() later calls
	// OpenThemeData(invisiblehwnd, "Menu").  uxtheme only returns a dark HTHEME
	// when BOTH SetPreferredAppMode(AllowDark) AND AllowDarkModeForWindow(hwnd)
	// have been called first.  updatedesktop() handles this on WM_SETTINGCHANGE,
	// but that message never arrives at cold startup, so we do it here inline.
	// We call uxtheme ordinals directly to avoid forward-referencing the static
	// helpers (s_ensure_uxtheme_dark etc.) that are defined later in this file.
	{
		typedef BOOL (WINAPI *AllowDarkModeForWindowFn)(HWND, BOOL);
		typedef int  (WINAPI *SetPreferredAppModeFn)(int);
		typedef void (WINAPI *FlushMenuThemesFn)(void);

		HMODULE t_ux = GetModuleHandleA("uxtheme.dll");
		if (t_ux == NULL)
			t_ux = LoadLibraryA("uxtheme.dll");
		if (t_ux != NULL)
		{
			auto t_set_mode   = (SetPreferredAppModeFn)    GetProcAddress(t_ux, MAKEINTRESOURCEA(135));
			auto t_allow_dark = (AllowDarkModeForWindowFn) GetProcAddress(t_ux, MAKEINTRESOURCEA(133));
			auto t_flush      = (FlushMenuThemesFn)        GetProcAddress(t_ux, MAKEINTRESOURCEA(136));
			if (t_set_mode && t_allow_dark && t_flush)
			{
				BOOL t_dark = MCplatformIsDarkMode() ? TRUE : FALSE;
				t_set_mode(1 /* AllowDark */);
				t_allow_dark(invisiblehwnd, t_dark);
				// Apply DWM title-bar attribute via GetProcAddress — the rest of
				// the file also loads dwmapi.dll dynamically (no #include <dwmapi.h>).
				typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
				HMODULE t_dwm = GetModuleHandleA("dwmapi.dll");
				if (t_dwm == NULL)
					t_dwm = LoadLibraryA("dwmapi.dll");
				if (t_dwm != NULL)
				{
					auto t_dwm_set = (DwmSetWindowAttributeFn)
					    GetProcAddress(t_dwm, "DwmSetWindowAttribute");
					if (t_dwm_set)
					{
						DWORD t_attr = t_dark;
						t_dwm_set(invisiblehwnd,
						          20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
						          &t_attr, sizeof(t_attr));
					}
				}
				t_flush();
			}
		}
	}

	mutex = CreateMutexA(NULL, False, NULL);

	MCblinkrate = GetCaretBlinkTime();
	MCdoubletime = GetDoubleClickTime();
	opened++;

	/* Fetch any system metrics we need which are updated on WM_SETTINGCHANGE or 
	 * WM_DISPLAYCHANGE. */
	updatemetrics();

	MCDisplay const *t_displays;
	getdisplays(t_displays, false);
	MCwbr = t_displays[0] . workarea;
	owndnd = False;

	// The System and Input codepages are used to translate input characters.
	// A keyboard layout will present characters via WM_CHAR in the
	// input_codepage, while LiveCode is running in the system_codepage.
	//
	system_codepage = GetACP();

	char t_info[8];
	t_info[0] = '\0';
	GetLocaleInfoA(MAKELCID(LOWORD(GetKeyboardLayout(NULL)), SORT_DEFAULT), LOCALE_IDEFAULTANSICODEPAGE, t_info, 8);
	input_codepage = atoi(t_info);

	// OK-2007-08-03 : Bug 5265
	input_default_keyboard = GetKeyboardLayout(0);

	// Make sure we initialize the dragDelta property
	MCdragdelta = MCU_max(GetSystemMetrics(SM_CYDRAG), GetSystemMetrics(SM_CXDRAG));

	// MW-2009-12-10: Colorspace support
	PROFILE t_profile_info;
	t_profile_info . dwType = PROFILE_FILENAME;
	t_profile_info . pProfileData = (LPVOID)"sRGB Color Space Profile.icm";
	t_profile_info . cbDataSize = strlen((char *)t_profile_info . pProfileData) + 1;
	m_srgb_profile = OpenColorProfileA(&t_profile_info, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING);

	// Seed background_pixel / system_fore_pixel from the current Windows
	// appearance so that stacks without explicit colours are correct from the
	// very first draw, even if the OS is already in dark mode at startup.
	// The legacy MenuBar-colour path above sets background_pixel to a light grey
	// which is wrong in dark mode.
	updatesystemcolors();

	return True;
}

Boolean MCScreenDC::close(Boolean force)
{
	if (m_srgb_profile != nil)
	{
		CloseColorProfile(m_srgb_profile);
		m_srgb_profile = nil;
	}

	if (f_has_icon)
		configurestatusicon(0, nil, nil);

	disablebackdrop(true);
	disablebackdrop(false);
	finalisebackdrop();

	DeleteDC(f_src_dc);
	DeleteDC(f_dst_dc);

	if (m_printer_dc != NULL)
		DeleteDC(m_printer_dc);

	delete vis;
	opened = False;

	if (mousetimer != 0)
		if (MClowrestimers)
			KillTimer(NULL, mousetimer);
		else
			timeKillEvent(mousetimer);
	timeEndPeriod(1);
	opened = 0;

	DestroyWindow(invisiblehwnd);
	invisiblehwnd = NULL;

	return True;
}

MCNameRef MCScreenDC::getdisplayname()
{
	return MCN_local_win32;
}

void MCScreenDC::grabpointer(Window w)
{
	if (MCModeMakeLocalWindows())
	{
		if (w != nil)
			SetCapture((HWND)w->handle.window);
		grabbed = True;
	}
}

void MCScreenDC::ungrabpointer()
{
	if (MCModeMakeLocalWindows())
	{
		grabbed = False;
		ReleaseCapture();
	}
}

extern MCGFloat MCWin32GetLogicalToScreenScale();
uint2 MCScreenDC::platform_getwidth()
{
	HDC winhdc = GetDC(NULL);

	uint32_t t_width;
	t_width = GetDeviceCaps(winhdc, HORZRES);

	ReleaseDC(NULL, winhdc);

	// IM-2014-01-28: [[ HiDPI ]] Convert screen width to logical size
	t_width = t_width / MCWin32GetLogicalToScreenScale();

	return t_width;
}

uint2 MCScreenDC::platform_getheight()
{
	HDC winhdc = GetDC(NULL);

	uint32_t t_height;
	t_height = (uint2)GetDeviceCaps(winhdc, VERTRES);

	ReleaseDC(NULL, winhdc);

	// IM-2014-01-28: [[ HiDPI ]] Convert screen height to logical size
	t_height = t_height / MCWin32GetLogicalToScreenScale();

	return t_height;
}

uint2 MCScreenDC::getwidthmm()
{
	HDC winhdc = GetDC(NULL);
	uint2 widthmm = (uint2)GetDeviceCaps(winhdc, HORZSIZE);
	ReleaseDC(NULL, winhdc);
	return widthmm;
}

uint2 MCScreenDC::getheightmm()
{
	HDC winhdc = GetDC(NULL);
	uint2 heightmm = (uint2)GetDeviceCaps(winhdc, VERTSIZE);
	ReleaseDC(NULL, winhdc);
	return heightmm;
}

uint2 MCScreenDC::getdepth()
{
	return 32;
}

uint2 MCScreenDC::getmaxpoints()
{ //max points defined in a polygon
	return MAX_POINT_IN_POLYGON;
}

uint2 MCScreenDC::getvclass()
{
	return DirectColor;
}

// Forward declarations for dark-mode helpers defined later in this file.
extern "C" bool MCplatformIsDarkMode(void);
static void MCWin32ApplyDarkModeChrome(HWND p_hwnd, BOOL p_dark);

// MW-2007-07-05: [[ Bug 471 ]] Modal/Sheets do not enable/disable windows correctly.
void MCScreenDC::openwindow(Window w, Boolean override)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	MCStack *t_stack;
	t_stack = MCdispatcher -> findstackd(w);

    // If there is a mainwindow callback then disable the mainwindow.
    if (MCmainwindowcallback != NULL && !IsWindowVisible((HWND)w->handle.window))
    {
        MCAssert(m_main_window_depth < INT_MAX - 1);
        m_main_window_depth += 1;
        if (m_main_window_depth == 1)
        {
            m_main_window_current = (HWND)MCmainwindowcallback();
            EnableWindow(m_main_window_current, False);
        }
        SetWindowLongPtr((HWND)w->handle.window, GWLP_HWNDPARENT, (LONG_PTR)m_main_window_current);
    }

	// Apply dark/light title-bar chrome before the window becomes visible so
	// there is no flash of the wrong colour.
	MCWin32ApplyDarkModeChrome((HWND)w->handle.window, MCplatformIsDarkMode() ? TRUE : FALSE);

	if (override)
		ShowWindow((HWND)w->handle.window, SW_SHOWNA);
	else
		// CW-2015-09-28: [[ Bug 15873 ]] If the stack state is iconic, restore the window minimised.
		if (t_stack != NULL && t_stack -> getstate(CS_ICONIC))
			ShowWindow((HWND)w->handle.window, SW_SHOWMINIMIZED);
		else if (IsIconic((HWND)w->handle.window))
			ShowWindow((HWND)w->handle.window, SW_RESTORE);
		else
			ShowWindow((HWND)w->handle.window, SW_SHOW);

	// Give popover windows foreground focus immediately on open so that
	// WM_ACTIVATE/WA_INACTIVE fires when the user clicks elsewhere — without
	// requiring an initial click on the popover first.
	if (t_stack != NULL && t_stack->getmode() == WM_POPOVER)
		SetForegroundWindow((HWND)w->handle.window);

	if (t_stack != NULL)
	{
		if (t_stack -> getmode() == WM_SHEET || t_stack -> getmode() == WM_MODAL)
			MCstacks -> enableformodal(w, False);
	}
}

// MW-2007-07-05: [[ Bug 471 ]] Modal/Sheets do not enable/disable windows correctly.
void MCScreenDC::closewindow(Window w)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	MCStack *t_stack;
	t_stack = MCdispatcher -> findstackd(w);

    // If there is a mainwindow callback then re-enable the mainwindow if at
    // depth 1.
    if (MCmainwindowcallback != nullptr && IsWindowVisible((HWND)w->handle.window))
    {
        MCAssert(m_main_window_depth > 0);
        m_main_window_depth -= 1;
        if (m_main_window_depth == 0 &&
            m_main_window_current != nullptr)
        {
            EnableWindow(m_main_window_current, True);
            m_main_window_current = nullptr;
        }
    }

	// If we are a sheet or a modal dialog we need to ensure we enable the right windows
	// and activate the next obvious window.
	if (t_stack != NULL && (t_stack -> getmode() == WM_SHEET || t_stack -> getmode() == WM_MODAL))
	{
		// By default we will activate the (sheet's) parent...
		MCStack *t_parent;
		t_parent = MCdispatcher -> findstackd(t_stack -> getparentwindow());

		// However modals do not a have a parent so we search...
		MCStack *t_visual_parent;
		if (t_parent == NULL || !t_parent -> isvisible() || t_parent -> getwindow() == NULL)
		{
			MCStacknode *t_node;
			t_node = MCstacks -> topnode();

			t_visual_parent = NULL;

			// Iterate through the open stacks from top to bottom...
			if (t_node != NULL)
			{
				do
				{
					// Ignore the stack we are closing...
					if (t_node -> getstack() != t_stack)
					{
						// The stack should be activated if either:
						//   . it is a MODAL dialog, or
						//   . it is a SHEET dialog that has a MODAL in its ancestry
						//   . it has the topstack in its ancestry
						MCStack *t_parent;
						t_parent = t_node -> getstack();
						while(t_parent != NULL)
						{
							if (t_parent -> isvisible() && t_parent -> getwindow() != NULL)
							{
								if (t_parent -> getmode() == WM_MODAL || t_parent == MCtopstackptr)
								{
									t_visual_parent = t_node -> getstack();
									break;
								}
							}

							t_parent = MCdispatcher -> findstackd(t_parent -> getparentwindow());
						}

						if (t_visual_parent != NULL)
							break;
					}

					t_node = t_node -> next();
				}
				while(t_node != MCstacks -> topnode());
			}
		}
		else
			t_visual_parent = t_parent;
		
		// Ask the stacklist to iterate through and enable/disable windows as appropriate
		MCstacks -> enableformodal(w, True);

		// If the dialog is the active window then we have to ensure we activate
		// the next one in the chain - but atomically to stop window flicker!
		//
		// Here if t_parent_stack is NULL it means that there is no sensible
		// stack for us to fall back to. Therefore, we just fall through to
		// the default behaviour of just hiding the window.
		//
		if (GetActiveWindow() == (HWND)w -> handle . window && t_visual_parent != NULL)
		{
			HDWP t_dwp;
			t_dwp = BeginDeferWindowPos(2);
			DeferWindowPos(t_dwp, (HWND)w -> handle . window, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);
			DeferWindowPos(t_dwp, (HWND)t_visual_parent -> getwindow() -> handle . window, GetWindow((HWND)w -> handle . window, GW_HWNDPREV), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			EndDeferWindowPos(t_dwp);
			return;
		}
	}
	
	ShowWindow((HWND)w->handle.window, SW_HIDE);
}

void MCScreenDC::destroywindow(Window &w)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	DestroyWindow((HWND)w->handle.window);
	delete w;
	w = DNULL;
}

void MCScreenDC::raisewindow(Window w)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	if (w != nil)
		SetWindowPos((HWND)w->handle.window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

}

void MCScreenDC::iconifywindow(Window w)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	ShowWindow((HWND)w->handle.window, SW_MINIMIZE);
}

void MCScreenDC::uniconifywindow(Window w)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	ShowWindow((HWND)w->handle.window, SW_RESTORE);
}

// MW-2007-07-09: [[ Bug 3226 ]] Updated to convert UTF-8 title to appropriate
//   encoding.
// MW-2008-07-22: [[ Bug 6235 ]] Make sure we deassociate any players temporarily
//   around the SetWindowText call. Failing to do this causes the call to only
//   set the first character as its treated as partially non-Unicode.
// MW-2008-09-01: [[ Bug 6235 ]] Unfortunately my nifty association/deassociation
//   logic doesn't work :o(. Therefore I'm removing this for now and have instead
//   added a bit to 'revRuntimeBehaviour' which allows turning off creation of
//   unicode windows.
void MCScreenDC::setname(Window w, MCStringRef newname)
{
	// MW-2009-11-01: Do nothing if there is no window
	if (w == NULL)
		return;

	if (IsWindowUnicode((HWND)w -> handle . window))
	{
		// If the name begins with an RTL character, force windows to interpret
        // it as such by pre-pending an RTL embedding (RTL) control.
        MCAutoStringRef t_newname;
        if (MCBidiFirstStrongIsolate(newname, 0) != 0)
        {
            /* UNCHECKED */ MCStringMutableCopy(newname, &t_newname);
            /* UNCHECKED */ MCStringPrependChar(*t_newname, 0x202B);
        }
        else
        {
            t_newname = newname;
        }
        
        MCAutoStringRefAsWString t_newname_w;
		/* UNCHECKED */ t_newname_w . Lock(*t_newname);
		SetWindowTextW((HWND)w -> handle . window, *t_newname_w);
	}
	else
	{
		MCAutoStringRefAsCString t_newname_a;
		/* UNCHECKED */ t_newname_a . Lock(newname);
		SetWindowTextA((HWND)w->handle.window, *t_newname_a);
	}
}

void MCScreenDC::setcmap(MCStack *sptr)
{}

void MCScreenDC::sync(Window w)
{
	GdiFlush();
}

static BOOL IsRemoteSession(void)
{
   return GetSystemMetrics(SM_REMOTESESSION);
}

void MCScreenDC::beep()
{
	bool t_use_internal = false ;
	if ( m_sound_internal != NULL ) 
		t_use_internal = (MCString(m_sound_internal) == "internal") == True;
	
	if ( t_use_internal || IsRemoteSession())
		Beep(beeppitch, beepduration);
	else 
		MessageBeep(-1);
}

void MCScreenDC::setinputfocus(Window w)
{
	SetFocus((HWND)w->handle.window);
}

bool MCScreenDC::platform_getwindowgeometry(Window w, MCRectangle &drect)
{//get the client window's geometry in screen coord
	if (w == DNULL || w->handle.window == 0)
		return false;
	RECT wrect;
	GetClientRect((HWND)w->handle.window, &wrect);
	POINT p;
	p.x = wrect.left;
	p.y = wrect.top;
	ClientToScreen((HWND)w->handle.window, &p);

	MCRectangle t_rect;
	t_rect = MCRectangleMake(p.x, p.y, wrect.right - wrect.left, wrect.bottom - wrect.top);

	// IM-2014-01-28: [[ HiDPI ]] Convert screen to logical coords
	drect = screentologicalrect(t_rect);

	return true;
}

void MCScreenDC::setgraphicsexposures(Boolean on, MCStack *sptr)
{
	exposures = on;
}

void MCScreenDC::copyarea(Drawable s, Drawable d, int2 depth, int2 sx, int2 sy,
                          uint2 sw, uint2 sh, int2 dx, int2 dy, uint4 rop)
{
	if (s == DNULL || d == DNULL)
		return;

	if (s == d && s->type == DC_WINDOW)
	{
		RECT wrect;
		wrect.left = sx;
		wrect.top = sy;
		wrect.right = sx + sw;
		wrect.bottom = sy + sh;
		if (ScrollWindowEx((HWND)s->handle.window, dx - sx, dy - sy, &wrect, NULL, NULL,
		                   NULL, exposures ? SW_INVALIDATE : 0) == COMPLEXREGION)
			expose();
	}
	else
	{
		HDC t_src_dc, t_dst_dc;
		HGDIOBJ t_old_src, t_old_dst;

		if (s -> type == DC_WINDOW)
			t_src_dc = GetDC((HWND)s -> handle . window); // Released
		else
			t_old_src = SelectObject(f_src_dc, s -> handle . pixmap), t_src_dc = f_src_dc;

		if (d -> type == DC_WINDOW)
			t_dst_dc = GetDC((HWND)d -> handle . window); // Released
		else
			t_old_dst = SelectObject(f_dst_dc, d -> handle . pixmap), t_dst_dc = f_dst_dc;

		BitBlt(t_dst_dc, dx, dy, sw, sh, t_src_dc, sx, sy, image_inks[rop]);

		if (d -> type == DC_WINDOW)
			ReleaseDC((HWND)d -> handle . window, t_dst_dc);
		else
			SelectObject(f_dst_dc, t_old_dst);

		if (s -> type == DC_WINDOW)
			ReleaseDC((HWND)s -> handle . window, t_src_dc);
		else
			SelectObject(f_src_dc, t_old_src);
	}
}

static void fixdata(uint4 *bits, uint2 width, uint2 height)
{
	uint4 mask = width == 16 ? 0xFFFF0000 : 0xFF000000;
	while (height--)
		*bits++ |= mask;
}

uintptr_t MCScreenDC::dtouint(Drawable d)
{
	if (d == DNULL)
		return 0;
	else
		if (d->type == DC_WINDOW)
			return (uintptr_t)(d->handle.window);
		else
			return (uintptr_t)(d->handle.pixmap);
}

Boolean MCScreenDC::uinttowindow(uintptr_t id, Window &w)
{
	w = new (nothrow) _Drawable;
	w->type = DC_WINDOW;
	w->handle.window = (MCSysWindowHandle)id;
	return True;
}

void MCScreenDC::getbeep(uint4 property, int4& r_value)
{
	switch (property)
	{
	case P_BEEP_LOUDNESS:
		r_value = 100;
		break;
	case P_BEEP_PITCH:
		r_value = beeppitch;
		break;
	case P_BEEP_DURATION:
		r_value = beepduration;
		break;
	}
}

void MCScreenDC::setbeep(uint4 which, int4 beep)
{
	switch (which)
	{
	case P_BEEP_LOUDNESS:
		break;
	case P_BEEP_PITCH:
		if (beep == -1)
			beeppitch = 1440;
		else
			beeppitch = beep;
		break;
	case P_BEEP_DURATION:
		if (beep == -1)
			beepduration = 1000;
		else
			beepduration = beep;
		break;
	}
}

MCNameRef MCScreenDC::getvendorname(void)
{
	return MCN_win32;
}

uint2 MCScreenDC::getpad()
{
	return 32;
}

Window MCScreenDC::getroot()
{
	static Meta::static_ptr_t<_Drawable> mydrawable;
	if (mydrawable == DNULL)
		mydrawable = new (nothrow) _Drawable;
	mydrawable->type = DC_WINDOW;
	mydrawable->handle.window = (MCSysWindowHandle)GetDesktopWindow();
	return mydrawable;
}

static Boolean snapdone;
static Boolean snapcancelled;
static Boolean rubbering;
static HDC snaphdc;
static HDC snapdesthdc;
static MCRectangle snaprect;
static int32_t snapoffsetx, snapoffsety;

typedef HRESULT (CALLBACK *DwmIsCompositionEnabledPtr)(BOOL *p_enabled);
typedef HRESULT (CALLBACK *DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID, DWORD);
static HMODULE s_dwmapi_library = NULL;
static DwmIsCompositionEnabledPtr s_dwm_is_composition_enabled = NULL;
static DwmSetWindowAttributePtr   s_dwm_set_window_attribute   = NULL;

// DWMWA_USE_IMMERSIVE_DARK_MODE: tells DWM to draw the title bar in dark mode.
// Value 20 is the SDK constant (Win10 20H1 / build 19041+).
// Value 19 is the undocumented equivalent for older Win10 insider builds.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static bool s_ensure_dwmapi(void)
{
	if (s_dwmapi_library != NULL)
		return true;

	s_dwmapi_library = LoadLibraryA("dwmapi.dll");
	if (s_dwmapi_library == NULL)
		return false;

	s_dwm_is_composition_enabled = (DwmIsCompositionEnabledPtr)GetProcAddress(s_dwmapi_library, "DwmIsCompositionEnabled");
	s_dwm_set_window_attribute   = (DwmSetWindowAttributePtr)  GetProcAddress(s_dwmapi_library, "DwmSetWindowAttribute");

	if (s_dwm_is_composition_enabled == NULL)
	{
		FreeLibrary(s_dwmapi_library);
		s_dwmapi_library = NULL;
		return false;
	}
	return true;
}

// ── Private uxtheme.dll dark-mode APIs ───────────────────────────────────────
// These ordinals are not in any public SDK header but have been stable since
// Windows 10 build 17763 (1809) and are used by many open-source apps
// (e.g. Mozilla, Chromium) to enable per-window and per-process dark mode for
// UxTheme-drawn controls (including LiveCode's custom menu bar).
//
//   Ordinal 133  AllowDarkModeForWindow(HWND, BOOL) -> BOOL
//   Ordinal 135  SetPreferredAppMode(int) -> int   (1903+; fallback: AllowDarkModeForApp)
//   Ordinal 136  FlushMenuThemes() -> void
typedef BOOL (WINAPI *AllowDarkModeForWindowPtr)(HWND hwnd, BOOL allow);
typedef int  (WINAPI *SetPreferredAppModePtr)(int mode);   // 0=Default, 1=AllowDark, 2=ForceDark, 3=ForceLight
typedef void (WINAPI *FlushMenuThemesPtr)(void);

static HMODULE                 s_uxtheme_library            = NULL;
static AllowDarkModeForWindowPtr s_allow_dark_mode_for_window = NULL;
static SetPreferredAppModePtr    s_set_preferred_app_mode     = NULL;
static FlushMenuThemesPtr        s_flush_menu_themes          = NULL;

static bool s_ensure_uxtheme_dark(void)
{
	if (s_uxtheme_library != NULL)
		return true;

	s_uxtheme_library = LoadLibraryA("uxtheme.dll");
	if (s_uxtheme_library == NULL)
		return false;

	s_allow_dark_mode_for_window = (AllowDarkModeForWindowPtr) GetProcAddress(s_uxtheme_library, MAKEINTRESOURCEA(133));
	s_set_preferred_app_mode     = (SetPreferredAppModePtr)    GetProcAddress(s_uxtheme_library, MAKEINTRESOURCEA(135));
	s_flush_menu_themes          = (FlushMenuThemesPtr)        GetProcAddress(s_uxtheme_library, MAKEINTRESOURCEA(136));

	// All three must be present; fall back gracefully on older Windows.
	if (s_allow_dark_mode_for_window == NULL ||
	    s_set_preferred_app_mode     == NULL ||
	    s_flush_menu_themes          == NULL)
	{
		// Not all functions available – clear so callers skip dark-mode logic.
		s_allow_dark_mode_for_window = NULL;
		s_set_preferred_app_mode     = NULL;
		s_flush_menu_themes          = NULL;
		// Keep the library handle so we don't keep retrying.
		return false;
	}
	return true;
}

static bool WindowsIsCompositionEnabled(void)
{
	if (MCmajorosversion < MCOSVersionMake(6,0,0))
		return false;

	if (!s_ensure_dwmapi())
		return false;

	BOOL t_enabled;
	if (s_dwm_is_composition_enabled(&t_enabled) != S_OK)
		return false;

	return t_enabled != FALSE;
}

// Apply (or remove) dark-mode chrome to a single HWND.
// Sets both the DWM title-bar colour and the uxtheme window-level dark mode
// flag so that OpenThemeData(hwnd, "Menu") returns the correct dark/light
// HTHEME for LiveCode's custom-drawn menu bar.
// Safe to call on any HWND; silently no-ops if the APIs are unavailable.
static void MCWin32ApplyDarkModeChrome(HWND p_hwnd, BOOL p_dark)
{
	if (p_hwnd == NULL)
		return;

	// Mark the window as dark/light for uxtheme so OpenThemeData honours the
	// setting.  Must be done before (re)opening any theme handles for this hwnd.
	if (s_ensure_uxtheme_dark() && s_allow_dark_mode_for_window != NULL)
		s_allow_dark_mode_for_window(p_hwnd, p_dark);

	// Paint the title bar dark/light via DWM.
	if (!s_ensure_dwmapi() || s_dwm_set_window_attribute == NULL)
		return;

	// Try the documented attribute (20) first; fall back to the pre-release
	// undocumented attribute (19) for older Win10 insider builds.
	if (FAILED(s_dwm_set_window_attribute(p_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &p_dark, sizeof(p_dark))))
		s_dwm_set_window_attribute(p_hwnd, 19, &p_dark, sizeof(p_dark));
}

// Apply the current dark/light mode to every visible top-level stack window.
static void MCWin32SyncAllWindowChrome(void)
{
	BOOL t_dark = MCplatformIsDarkMode() ? TRUE : FALSE;
	MCStacknode *t_node = MCstacks->topnode();
	if (t_node == nullptr)
		return;
	MCStacknode *t_first = t_node;
	do
	{
		MCStack *t_stack = t_node->getstack();
		if (t_stack != nullptr)
		{
			Window t_win = t_stack->getwindow();
			if (t_win != nullptr && t_win->handle.window != nullptr)
				MCWin32ApplyDarkModeChrome((HWND)t_win->handle.window, t_dark);
		}
		t_node = t_node->next();
	}
	while (t_node != t_first);
}

// MW-2014-02-20: [[ Bug 11811 ]] Updated to scale snapshot to requested size.
bool create_temporary_dib(HDC p_dc, uint4 p_width, uint4 p_height, HBITMAP& r_bitmap, void*& r_bits);

MCImageBitmap *MCScreenDC::snapshot(MCRectangle &r, uint4 window, MCStringRef displayname, MCPoint *size)
{
	bool t_is_composited;
	t_is_composited = WindowsIsCompositionEnabled();

	expose();
	//make the parent window to be the invisible window, so that the snapshot window icon
	//does not show up in the desktop taskbar.
	
	MCDisplay const *t_displays;
	uint4 t_display_count;
	MCRectangle t_virtual_viewport;
	t_display_count = getdisplays(t_displays, false);
	t_virtual_viewport = t_displays[0] . viewport;
	for(uint4 t_index = 1; t_index < t_display_count; ++t_index)
		t_virtual_viewport = MCU_union_rect(t_virtual_viewport, t_displays[t_index] . viewport);

	// IM-2014-04-02: [[ Bug 12109 ]] Convert screenrect to screen coords
	MCRectangle t_device_viewport;
	t_device_viewport = logicaltoscreenrect(t_virtual_viewport);

	DWORD t_ex_options = WS_EX_TRANSPARENT | WS_EX_TOPMOST;
	// use layered if we don't need to select the snapshot area via cursor
	if (window != 0 || r.x != -32768)
	{
		t_ex_options |= WS_EX_LAYERED;
	}

	HWND hwndsnap = CreateWindowExA(t_ex_options,
	                               MC_SNAPSHOT_WIN_CLASS_NAME,"", WS_POPUP, t_device_viewport . x, t_device_viewport . y,
	                               t_device_viewport . width, t_device_viewport . height, invisiblehwnd,
	                               NULL, MChInst, NULL);
	SetWindowPos(hwndsnap, HWND_TOPMOST,
	             t_device_viewport . x, t_device_viewport . y, t_device_viewport . width, t_device_viewport . height, SWP_NOACTIVATE | SWP_SHOWWINDOW
	             | SWP_DEFERERASE | SWP_NOREDRAW);

	if (t_is_composited)
	{
		snaphdc = GetDC(NULL);
		snapoffsetx = t_device_viewport . x;
		snapoffsety = t_device_viewport . y;
	}
	else
	{
		snaphdc = GetDC(hwndsnap); // Released
		snapoffsetx = 0;
		snapoffsety = 0;
	}

	// IM-2014-04-02: [[ Bug 12109 ]] Calculate snapshot rect in screen coords
	MCRectangle t_device_snaprect;

	snapdone = False;
	snapcancelled = False;
	snapdesthdc = f_dst_dc;
	if (window == 0 && r.x == -32768)
	{
		snapdone = False;
		rubbering = False;
		SetFocus(hwndsnap);
		setcursor(nil, MCcursors[PI_CROSS]);
		SetROP2(snaphdc, R2_NOT);
		SelectObject(snaphdc, GetStockObject(HOLLOW_BRUSH));
		MSG msg;
		while (!snapdone && GetMessageW(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		t_device_snaprect = snaprect;
	}
	else
	{
		if (r.x == -32768)
			r.x = r.y = 0;

		// IM-2014-04-02: [[ Bug 12109 ]] Convert snapshot rect to screen coords
		t_device_snaprect = logicaltoscreenrect(r);

		if (window != 0 || r.width == 0 || r.height == 0)
		{
			HWND w;
			if (window == 0)
			{
				POINT p;
				p.x = t_device_snaprect.x;
				p.y = t_device_snaprect.y;

				ShowWindow(hwndsnap, SW_HIDE);
				if ((w = WindowFromPoint(p)) == NULL)
				{
					ReleaseDC(hwndsnap, snaphdc);
					DestroyWindow(hwndsnap);
					return NULL;
				}
				ShowWindow(hwndsnap, SW_SHOWNA);
			}
			else
				w = (HWND)window;
			RECT cr;
			GetClientRect(w, &cr);
			POINT p;
			p.x = p.y = 0;
			ClientToScreen(w, &p);
			if (r.width == 0 || r.height == 0)
				t_device_snaprect = MCRectangleMake(p.x, p.y, cr.right - cr.left, cr.bottom - cr.top);
			else
				t_device_snaprect = MCRectangleOffset(t_device_snaprect, p.x, p.y);
		}
	}
	
	int t_width, t_height;
	HBITMAP newimage = NULL;
	void *t_bits = nil;
	if (!snapcancelled)
	{
		t_device_snaprect = MCU_intersect_rect(t_device_snaprect, t_device_viewport);
		
		// IM-2014-04-02: [[ Bug 12109 ]] Convert screen coords back to logical
		r = screentologicalrect(t_device_snaprect);

		if (size != nil)
		{
			t_width = size -> x;
			t_height = size -> y;
		}
		else
		{
			t_width = r . width;
			t_height = r . height;
		}
		
		if (r.width != 0 && r.height != 0)
		{
			if (create_temporary_dib(snapdesthdc, t_width, t_height, newimage, t_bits))
			{
				HBITMAP obm = (HBITMAP)SelectObject(snapdesthdc, newimage);
				
				// MW-2012-09-19: [[ Bug 4173 ]] Add the 'CAPTUREBLT' flag to make sure
				//   layered windows are included.
				if (t_is_composited)
					StretchBlt(snapdesthdc, 0, 0, t_width, t_height,
								snaphdc, t_device_snaprect . x, t_device_snaprect . y, t_device_snaprect . width, t_device_snaprect . height, CAPTUREBLT | SRCCOPY);
				else
				{
					StretchBlt(snapdesthdc, 0, 0, t_width, t_height,
						snaphdc, t_device_snaprect.x - t_device_viewport . x, t_device_snaprect.y - t_device_viewport . y, t_device_snaprect . width, t_device_snaprect . height, CAPTUREBLT | SRCCOPY);
				}
				SelectObject(snapdesthdc, obm);
			}
		}
	}

	MCRectangle t_screenrect;
	t_screenrect = MCRectangleMake(0, 0, getwidth(), getheight());

	// IM-2014-01-28: [[ HiDPI ]] Convert logical to screen coords
	t_screenrect = logicaltoscreenrect(t_screenrect);

	SetWindowPos(hwndsnap, HWND_TOPMOST,
		t_screenrect.x, t_screenrect.y, t_screenrect.width, t_screenrect.height,
		SWP_HIDEWINDOW | SWP_DEFERERASE | SWP_NOREDRAW);
	if (t_is_composited)
		ReleaseDC(NULL, snaphdc);
	else
		ReleaseDC(hwndsnap, snaphdc);
	DestroyWindow(hwndsnap);

	MCImageBitmap *t_bitmap = nil;
	if (newimage != NULL)
	{
		/* UNCHECKED */ MCImageBitmapCreate(t_width, t_height, t_bitmap);
		BITMAPINFOHEADER t_out_fmt;
		MCMemoryClear(&t_out_fmt, sizeof(BITMAPINFOHEADER));
		t_out_fmt.biSize = sizeof(BITMAPINFOHEADER);
		t_out_fmt.biWidth = t_width;
		t_out_fmt.biHeight = -(int32_t)t_height;
		t_out_fmt.biPlanes = 1;
		t_out_fmt.biBitCount = 32;
		t_out_fmt.biCompression = BI_RGB;
		GetDIBits(snapdesthdc, newimage, 0, t_height, t_bitmap->data, (BITMAPINFO*)&t_out_fmt, DIB_RGB_COLORS);
		DeleteObject(newimage);
		MCImageBitmapSetAlphaValue(t_bitmap, 0xFF);
	}
	return t_bitmap;
}

LRESULT CALLBACK MCSnapshotWindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam)
{
	static int2 sx, sy;

	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			if (rubbering)
				Rectangle(snaphdc, snaprect.x, snaprect.y,
				          snaprect.x + snaprect.width, snaprect.y + snaprect.height);
			snapdone = True;
			snapcancelled = True;
		}
		break;
	case WM_MOUSEMOVE:
		if (rubbering)
		{
			Rectangle(snaphdc, snaprect.x, snaprect.y,
			          snaprect.x + snaprect.width, snaprect.y + snaprect.height);
			snaprect = MCU_compute_rect(sx, sy, LOWORD(lParam) + snapoffsetx, HIWORD(lParam) + snapoffsety);
			Rectangle(snaphdc, snaprect.x, snaprect.y,
			          snaprect.x + snaprect.width, snaprect.y + snaprect.height);
		}
		break;
	case WM_LBUTTONDOWN:
		sx = LOWORD(lParam) + snapoffsetx;
		sy = HIWORD(lParam) + snapoffsety;
		snaprect = MCU_compute_rect(sx, sy, sx, sy);
		Rectangle(snaphdc, snaprect.x, snaprect.y,
		          snaprect.x + snaprect.width, snaprect.y + snaprect.height);
		rubbering = True;
		break;
	case WM_LBUTTONUP:
		if (rubbering)
		{
			Rectangle(snaphdc, snaprect.x, snaprect.y,
			          snaprect.x + snaprect.width, snaprect.y + snaprect.height);
			snaprect = MCU_compute_rect(sx, sy, LOWORD(lParam) + snapoffsetx, HIWORD(lParam) + snapoffsety);
			if (snaprect.width < 4 && snaprect.height < 4)
				snaprect.width = snaprect.height = 0;
			snapdone = True;
		}
		break;
	case WM_SETCURSOR:
		MCscreen -> setcursor(nil, MCcursors[PI_CROSS]);
		return 0;
	default:
		return DefWindowProcA(hwnd, msg, wParam, lParam);
	}
	return 0;
}

void MCScreenDC::settaskbarstate(bool p_visible)
{
	HWND taskbarwnd = FindWindowA("Shell_traywnd", "");
	SetWindowPos(taskbarwnd, 0, 0, 0, 0, 0, SWP_NOACTIVATE | (p_visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));

	if (MCmajorosversion >= MCOSVersionMake(6,0,0))
	{
		HWND t_start_window;
		t_start_window = FindWindowExA(NULL, NULL, "Button", "Start");
		SetWindowPos(t_start_window, 0, 0, 0, 0, 0, SWP_NOACTIVATE | (p_visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
		if (p_visible)
		{
			InvalidateRect(t_start_window, NULL, TRUE);
			UpdateWindow(t_start_window);
		}
	}

	// Don't notify or update fonts when changing titlebar visibility
	processdesktopchanged(false, false);
}

// ── Windows dark-mode appearance helpers ──────────────────────────────────
// These provide the strong definitions declared as extern "C" in desktop.cpp.
// They are called from MCPlatformHandleSystemAppearanceChanged() to supply the
// three parameters sent with the systemAppearanceChanged message.

// Read the current dark/light mode from the Themes\Personalize registry key.
//
// Windows 10 has TWO separate toggles in Settings → Personalisation → Colours:
//   • "Choose your default app mode"     → AppsUseLightTheme
//   • "Choose your default Windows mode" → SystemUsesLightTheme
//
// A value of 0 means dark; 1 (or absent) means light.
//
// We treat dark mode as active if EITHER key is 0, so that:
//   - toggling the "Windows mode" slider (SystemUsesLightTheme) is detected, AND
//   - toggling the "App mode" slider (AppsUseLightTheme) is detected.
// On Windows 11 22H2+ there is a single combined slider that writes both keys,
// so the behaviour is unchanged there.
extern "C" bool MCplatformIsDarkMode(void)
{
    static const LPCWSTR k_subkey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

    DWORD t_value = 1;
    DWORD t_size  = sizeof(t_value);

    // Check app mode first (most widely used).
    RegGetValueW(HKEY_CURRENT_USER, k_subkey, L"AppsUseLightTheme",
                 RRF_RT_REG_DWORD, nullptr, &t_value, &t_size);
    if (t_value == 0)
        return true;

    // Check system/Windows mode as a fallback.
    t_value = 1;
    t_size  = sizeof(t_value);
    RegGetValueW(HKEY_CURRENT_USER, k_subkey, L"SystemUsesLightTheme",
                 RRF_RT_REG_DWORD, nullptr, &t_value, &t_size);
    return t_value == 0;
}

static void s_colorref_to_hex(COLORREF p_color, char *p_buf, size_t p_buflen)
{
    if (p_buflen < 8) return;
    snprintf(p_buf, p_buflen, "#%02x%02x%02x",
             (unsigned)GetRValue(p_color),
             (unsigned)GetGValue(p_color),
             (unsigned)GetBValue(p_color));
}

// Return the window background colour as a LiveCode hex string ("#rrggbb").
// In dark mode we use the canonical Windows 11 dark surface colour (#1e1e1e);
// in light mode we read the live system colour so custom themes are respected.
extern "C" void MCplatformGetWindowBackgroundColor(char *p_buf, size_t p_buflen)
{
    if (MCplatformIsDarkMode())
        snprintf(p_buf, p_buflen, "#1e1e1e");
    else
        s_colorref_to_hex(GetSysColor(COLOR_WINDOW), p_buf, p_buflen);
}

// Return the primary label (text) colour as a LiveCode hex string ("#rrggbb").
extern "C" void MCplatformGetLabelColor(char *p_buf, size_t p_buflen)
{
    if (MCplatformIsDarkMode())
        snprintf(p_buf, p_buflen, "#ffffff");
    else
        s_colorref_to_hex(GetSysColor(COLOR_WINDOWTEXT), p_buf, p_buflen);
}

// ── Windows MCScreenDC::getsystemappearance ───────────────────────────────
// Overrides MCUIDC::getsystemappearance (which always returns "light").
// "custom" = Windows High Contrast accessibility theme is active.
// "dark"   = AppsUseLightTheme registry value is 0.
// "light"  = default.
void MCScreenDC::getsystemappearance(MCSystemAppearance &r_appearance)
{
    HIGHCONTRAST t_hc = {};
    t_hc.cbSize = sizeof(t_hc);
    if (SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(t_hc), &t_hc, 0) &&
        (t_hc.dwFlags & HCF_HIGHCONTRASTON))
    {
        r_appearance = kMCSystemAppearanceCustom;
    }
    else
    {
        r_appearance = MCplatformIsDarkMode() ? kMCSystemAppearanceDark : kMCSystemAppearanceLight;
    }
}

void MCScreenDC::getsystemwindowcolor(MCStringRef &r_color)
{
    char t_buf[8] = "#ffffff";
    MCplatformGetWindowBackgroundColor(t_buf, sizeof(t_buf));
    /* UNCHECKED */ MCStringCreateWithCString(t_buf, r_color);
}

void MCScreenDC::getsystemtextcolor(MCStringRef &r_color)
{
    char t_buf[8] = "#000000";
    MCplatformGetLabelColor(t_buf, sizeof(t_buf));
    /* UNCHECKED */ MCStringCreateWithCString(t_buf, r_color);
}

// Refresh background_pixel and system_fore_pixel from the current Windows
// appearance.  Called at startup and from MCPlatformHandleSystemAppearanceChanged.
// Mirrors the desktop-dc.cpp implementation (which is excluded from the Windows
// build) but uses GetSysColor() directly to avoid pulling in a hex parser.
void MCScreenDC::updatesystemcolors(void)
{
    // Scale an 8-bit channel to the 16-bit range LiveCode uses (×257 = 0x0101).
    auto colorref_to_mc = [](COLORREF c, MCColor &r)
    {
        r.red   = (uint2)(GetRValue(c) * 257);
        r.green = (uint2)(GetGValue(c) * 257);
        r.blue  = (uint2)(GetBValue(c) * 257);
    };

    if (MCplatformIsDarkMode())
    {
        // Canonical Windows 11 dark-surface (#1e1e1e) / label (#ffffff).
        colorref_to_mc(RGB(0x1e, 0x1e, 0x1e), background_pixel);
        colorref_to_mc(RGB(0xff, 0xff, 0xff), system_fore_pixel);
    }
    else
    {
        colorref_to_mc(GetSysColor(COLOR_WINDOW),     background_pixel);
        colorref_to_mc(GetSysColor(COLOR_WINDOWTEXT), system_fore_pixel);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void MCScreenDC::processdesktopchanged(bool p_notify, bool p_update_fonts)
{
	// ── Dark / light mode change detection ───────────────────────────────────
	// This is called whenever the invisible window receives WM_SETTINGCHANGE or
	// WM_DISPLAYCHANGE.  We compare the current dark-mode state against the last
	// known state.  If it has changed (or if this is the first call), we call
	// MCPlatformHandleSystemAppearanceChanged() directly — no string matching,
	// no ANSI/Unicode ambiguity.
	//
	// This acts as the DEFINITIVE trigger for appearance changes on Windows,
	// overriding all the WM_SETTINGCHANGE "ImmersiveColorSet" string-matching
	// attempts which are fragile due to ANSI/Unicode thunking.
	{
		static bool s_appearance_initialised = false;
		static bool s_last_dark_mode         = false;

		bool t_dark = MCplatformIsDarkMode();

		if (!s_appearance_initialised || t_dark != s_last_dark_mode)
		{
			s_last_dark_mode         = t_dark;
			s_appearance_initialised = true;

			void MCPlatformHandleSystemAppearanceChanged(void);
			MCPlatformHandleSystemAppearanceChanged();
		}
	}

	// Refresh background_pixel / system_fore_pixel for any non-appearance
	// desktop changes (DPI, display count, etc.) — MCPlatformHandleSystemAppearanceChanged
	// already does this when dark mode changes, but we need it here too for the
	// general case.
	updatesystemcolors();

	// IM-2014-01-28: [[ HiDPI ]] Use updatedisplayinfo() method to update & compare display details
	bool t_changed;
	t_changed = false;

	/* Update any system metrics which are used often and are cached. */
	updatemetrics();

	updatedisplayinfo(t_changed);

	if (t_changed && (backdrop_active || backdrop_hard))
	{
		uint4 t_display_count;
		const MCDisplay *t_displays;

		t_display_count = getdisplays(t_displays, false);
		SetWindowPos(backdrop_window, NULL, t_displays[0] . workarea . x, t_displays[0] . workarea . y, t_displays[0] . workarea . width, t_displays[0] . workarea . height, 0);
	}

    // Force a recompute of fonts as they may have changed
	if (p_update_fonts)
	    MCdispatcher->recomputefonts(NULL, true);
    //MCRedrawDirtyScreen();

	// Sync dark/light title-bar chrome on all open windows whenever desktop
	// settings change (covers both ImmersiveColorSet and display changes).
	MCWin32SyncAllWindowChrome();

	// ── Dark-mode menu bar refresh ────────────────────────────────────────────
	// LiveCode draws its menu bar with UxTheme (OpenThemeData / DrawThemeBackground).
	// The HTHEME for the "Menu" class must be (re)opened against a window that
	// has AllowDarkModeForWindow applied; otherwise Windows always returns the
	// light-mode theme even when the system is in dark mode.
	//
	// Sequence:
	//  1. Tell Windows this process honours dark mode (SetPreferredAppMode).
	//  2. Mark the invisible window as dark/light (AllowDarkModeForWindow).
	//  3. Flush uxtheme's internal menu-theme cache (FlushMenuThemes).
	//  4. Cycle MCcurtheme so it closes the stale HTHEME handles and re-opens
	//     them – the new "Menu" handle will now be dark when appropriate.
	//  5. Repaint everything.
	if (s_ensure_uxtheme_dark())
	{
		// 1. Process-level preference: allow dark mode when the system requests it.
		s_set_preferred_app_mode(1 /* AllowDark */);

		// 2. Mark the invisible window so OpenThemeData(invisiblehwnd, "Menu")
		//    returns the dark HTHEME.  MCNativeTheme::load() will use this hwnd.
		BOOL t_dark = MCplatformIsDarkMode() ? TRUE : FALSE;
		MCWin32ApplyDarkModeChrome(invisiblehwnd, t_dark);

		// 3. Invalidate uxtheme's cached menu theme data.
		s_flush_menu_themes();

		// 4. Cycle the native theme so it re-opens all HTHEME handles
		//    (including mMenuTheme) against the now-configured window.
		if (MCcurtheme && MCcurtheme->getthemeid() == LF_NATIVEWIN)
		{
			MCcurtheme->unload();
			MCcurtheme->load();
		}

		// 5. Repaint all windows with fresh theme data.
		MCRedrawDirtyScreen();
	}

	if (p_notify && t_changed)
		MCscreen -> delaymessage(MCdefaultstackptr -> getcurcard(), MCM_desktop_changed);
}

void MCScreenDC::showtaskbar()
{
	if (!taskbarhidden && !(backdrop_active || backdrop_hard)) //if the menu bar is already showing, bail out
		return;

	taskbarhidden = False;
	settaskbarstate(true);
}

void MCScreenDC::hidetaskbar()
{
	if (taskbarhidden && !(backdrop_active || backdrop_hard))
		return;

	taskbarhidden = True;
	settaskbarstate(false);
}

// MW-2006-05-26: [[ Bug 3642 ]] Changed global allocation routines to use malloc
HRGN MCScreenDC::BitmapToRegion(MCImageBitmap *p_bitmap)
{
	HRGN hRgn = NULL;
	uint8_t *t_src_ptr = NULL;
	HRGN h = NULL;
	RGNDATA *pData = NULL;

	if (p_bitmap)
	{
		// For better performances, we will use the ExtCreateRegion() function
		// to create the region. This function take a RGNDATA structure on
		// entry. We will add rectangles by amount of ALLOC_UNIT number in
		// this structure.
#define ALLOC_UNIT 100

		DWORD maxRects = ALLOC_UNIT;
		pData = (RGNDATA *)malloc(sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects));
		if (pData == NULL)
			goto no_mem_error;

		pData->rdh.dwSize = sizeof(RGNDATAHEADER);
		pData->rdh.iType = RDH_RECTANGLES;
		pData->rdh.nCount = pData->rdh.nRgnSize = 0;
		SetRect(&pData->rdh.rcBound, MAXLONG, MAXLONG, 0, 0);

		// Scan each bitmap row from top to bottom
		t_src_ptr = (uint8_t*)p_bitmap->data;
		for (int y = 0; y < p_bitmap->height; y++)
		{
			uint32_t *t_src_row = (uint32_t*)t_src_ptr;
			// Scan each bitmap pixel from left to right
			for (int x = 0; x < p_bitmap->width; x++)
			{
				// Search for a continuous range of "non transparent pixels"
				int x0 = x;
				uint32_t *t_src_pixel = t_src_row + x;
				while (x < p_bitmap->width)
				{
					uint8_t t_alpha = *t_src_pixel++ >> 24;
					if (t_alpha == 0)
						// This pixel is "transparent"
						break;
					x++;
				}

				if (x > x0)
				{
					// Add the pixels (x0, y) to (x, y+1) as a new
					// rectangle in the region
					if (pData->rdh.nCount >= maxRects)
					{
						maxRects += ALLOC_UNIT;
						pData = (RGNDATA *)realloc(pData, sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects));
						if (pData == NULL)
							goto no_mem_error;
					}
					RECT *pr = (RECT *)&pData->Buffer;
					SetRect(&pr[pData->rdh.nCount], x0, y, x, y+1);
					if (x0 < pData->rdh.rcBound.left)
						pData->rdh.rcBound.left = x0;
					if (y < pData->rdh.rcBound.top)
						pData->rdh.rcBound.top = y;
					if (x > pData->rdh.rcBound.right)
						pData->rdh.rcBound.right = x;
					if (y+1 > pData->rdh.rcBound.bottom)
						pData->rdh.rcBound.bottom = y+1;
					pData->rdh.nCount++;

					// On Windows98, ExtCreateRegion() may fail if the
					// number of rectangles is too large (ie: >
					// 4000). Therefore, we have to create the region by
					// multiple steps.
					if (pData->rdh.nCount == 2000)
					{
						h = ExtCreateRegion(NULL, sizeof(RGNDATAHEADER)
						                         + (sizeof(RECT) * maxRects), pData);
						if (hRgn)
						{
							CombineRgn(hRgn, hRgn, h, RGN_OR);
							DeleteObject(h);
						}
						else
							hRgn = h;
						pData->rdh.nCount = 0;
						SetRect(&pData->rdh.rcBound, MAXLONG, MAXLONG, 0, 0);
					}
				}
			}

			// Go to next row
			t_src_ptr += p_bitmap->stride;
		}

		// Create or extend the region with the remaining rectangles
		h = ExtCreateRegion(NULL, sizeof(RGNDATAHEADER)
		                         + (sizeof(RECT) * maxRects), pData);
		if (hRgn)
		{
			CombineRgn(hRgn, hRgn, h, RGN_OR);
			DeleteObject(h);
		}
		else
			hRgn = h;

		// Clean up
no_mem_error:
		if (pData != NULL)
			free(pData);
		else if (hRgn != NULL)
		{
			DeleteObject(hRgn);
			hRgn = NULL;
		}
	}


	return hRgn;
}

void MCScreenDC::enablebackdrop(bool p_hard)
{
	if (!MCModeMakeLocalWindows())
		return;
	
	bool t_error;
	t_error = false;
	
	if (p_hard && backdrop_hard)
		return;

	if (!p_hard && backdrop_active)
		return;
	
	if (p_hard)
		backdrop_hard = true;
	else
		backdrop_active = True;
		
	if (backdrop_window != NULL)
	{
		finalisebackdrop();
		backdrop_window = NULL;
	}

	if (backdrop_window == NULL)
		t_error = !initialisebackdrop();
	
	if (!t_error)
	{
		InvalidateRect(backdrop_window, NULL, TRUE);
		ShowWindow(backdrop_window, SW_SHOW);
	}
	else
	{
		backdrop_active = false;
		finalisebackdrop();
	}
}

void MCScreenDC::disablebackdrop(bool p_hard)
{
	if (!MCModeMakeLocalWindows())
		return;
	
	if (!backdrop_hard && p_hard)
		return;

	if (!backdrop_active && !p_hard)
		return;

	if (p_hard)
		backdrop_hard = false;
	else
		backdrop_active = False;

	if (!backdrop_active && !backdrop_hard)
	{
		ShowWindow(backdrop_window, SW_HIDE);
	}
	else
		InvalidateRect(backdrop_window, NULL, TRUE);
}

// MM-2014-04-08: [[ Bug 12058 ]] Update prototype to take a MCPatternRef.
void MCScreenDC::configurebackdrop(const MCColor& p_colour, MCPatternRef p_pattern, MCImage *p_badge)
{
	if (backdrop_badge != p_badge || backdrop_pattern != p_pattern || backdrop_colour . red != p_colour . red || backdrop_colour . green != p_colour . green || backdrop_colour . blue != p_colour . blue)
	{
		backdrop_badge = p_badge;
		backdrop_pattern = p_pattern;
		backdrop_colour = p_colour;
	
		if (backdrop_active || backdrop_hard)
			InvalidateRect(backdrop_window, NULL, TRUE);
	}
}

void MCScreenDC::assignbackdrop(Window_mode p_mode, Window p_window)
{
}

bool MCScreenDC::initialisebackdrop(void)
{
	uint4 t_display_count;
	const MCDisplay *t_displays;

	t_display_count = getdisplays(t_displays, false);

	MCRectangle t_rect;

	// MW-2012-10-08: [[ Bug 10286 ]] If the taskbar is hidden then show it and then
	//   present the backdrop as fullscreen.
	if (taskbarhidden)
	{
		settaskbarstate(true);
		taskbarhidden = false;
		t_rect = t_displays[0].viewport;
	}
	else
	{
		t_rect = t_displays[0].workarea;
		if (t_rect.height == t_displays[0].viewport . height)
			t_rect.height -= 2;
	}

	// IM-2014-04-17: [[ Bug 12223 ]] Record the backdrop rect and scale
	m_backdrop_rect = MCRectangleMake(0, 0, t_rect.width, t_rect.height);
	m_backdrop_scale = t_displays[0].pixel_scale;

	// IM-2014-04-17: [[ Bug 12223 ]] Convert window rect to screen coords
	t_rect = logicaltoscreenrect(m_backdrop_rect);

	backdrop_window = CreateWindowExA(0, MC_BACKDROP_WIN_CLASS_NAME, "", WS_POPUP, t_rect . x, t_rect . y, t_rect . width, t_rect . height, invisiblehwnd, NULL, MChInst, NULL);

	return true;
}

void MCScreenDC::finalisebackdrop(void)
{
	if (backdrop_window != NULL)
	{
		DestroyWindow(backdrop_window);
		backdrop_window = NULL;
	}
}

void MCScreenDC::redrawbackdrop(void)
{
	bool t_success = true;

	RECT t_winrect;
	HBITMAP t_bitmap = nil;
	void *t_bits;
	MCGContextRef t_context = nil;
	uint32_t t_width, t_height;

	// IM-2014-04-17: [[ Bug 12223 ]] Account for pixelScale when drawing backdrop
	MCGAffineTransform t_transform;
	t_transform = MCGAffineTransformMakeScale(m_backdrop_scale, m_backdrop_scale);

	GetClientRect(backdrop_window, &t_winrect);
	t_width = t_winrect.right - t_winrect.left;
	t_height = t_winrect.bottom - t_winrect.top;

	t_success = create_temporary_dib(getdsthdc(), t_width, t_height, t_bitmap, t_bits);
	if (t_success)
		t_success = MCGContextCreateWithPixels(t_width, t_height, t_width * sizeof(uint32_t), t_bits, true, t_context);

	if (t_success)
	{
		MCGContextConcatCTM(t_context, t_transform);

		// MM-2014-01-27: [[ UpdateImageFilters ]] Updated to use new libgraphics image filter types (was nearest).
		// MM-2014-04-08: [[ Bug 12058 ]] Update back_pattern to be a MCPatternRef.
		if (backdrop_pattern != nil)
		{
			MCGImageRef t_image;
			MCGAffineTransform t_pattern_transform;
			// IM-2014-05-13: [[ HiResPatterns ]] Update pattern access to use lock function
			if (MCPatternLockForContextTransform(backdrop_pattern, t_transform, t_image, t_pattern_transform))
			{
				MCGContextSetFillPattern(t_context, t_image, t_pattern_transform, kMCGImageFilterNone);
				MCPatternUnlock(backdrop_pattern, t_image);
			}
		}
		else
			MCGContextSetFillRGBAColor(t_context, backdrop_colour.red / 65535.0, backdrop_colour.green / 65535.0, backdrop_colour.blue / 65535.0, 1.0);
		MCGContextAddRectangle(t_context, MCRectangleToMCGRectangle(m_backdrop_rect));
		MCGContextFill(t_context);

		if (backdrop_badge != NULL && backdrop_hard)
		{
			MCContext *t_gfxcontext = nil;
			t_success = nil != (t_gfxcontext = new (nothrow) MCGraphicsContext(t_context));

			if (t_success)
			{
				MCRectangle t_rect;
				t_rect = backdrop_badge -> getrect();
				backdrop_badge -> drawme(t_gfxcontext, 0, 0, t_rect . width, t_rect . height, 32, m_backdrop_rect.height - 32 - t_rect . height, t_rect . width, t_rect . height);
			}

			delete t_gfxcontext;
		}
	}

	MCGContextRelease(t_context);

	// draw to backdrop window
	HDC t_dc = nil;
	HDC t_src_dc = nil;
	if (t_success)
		t_success = nil != (t_dc = GetDC(backdrop_window));
	if (t_success)
		t_success = nil != (t_src_dc = getsrchdc());

	if (t_success)
	{
		HGDIOBJ t_old_obj;
		t_old_obj = SelectObject(t_src_dc, t_bitmap);
		BitBlt(t_dc, 0, 0, t_width, t_height, t_src_dc, 0, 0, SRCCOPY);
		SelectObject(t_src_dc, t_old_obj);
	}

	if (t_dc != nil)
		ReleaseDC(backdrop_window, t_dc);

	if (t_bitmap != nil)
		DeleteObject(t_bitmap);
}

void MCScreenDC::enactraisewindows(void)
{
}

LRESULT CALLBACK MCBackdropWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	uint4 mbutton;

	switch(msg)
	{
		case WM_ERASEBKGND:
		{
			((MCScreenDC *)MCscreen) -> redrawbackdrop();
		}
		return 1;

		case WM_WINDOWPOSCHANGING:
		{
			((MCScreenDC *)MCscreen) -> restackwindows(hwnd, msg, wParam, lParam);
			return DefWindowProcA(hwnd, msg, wParam, lParam);
		}

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			if (MCtopstackptr != NULL && MCtopstackptr -> getwindow() != NULL && IsWindowVisible((HWND)MCtopstackptr -> getwindow() -> handle . window))
				SetActiveWindow((HWND)MCtopstackptr -> getwindow() -> handle . window);
			else
			{
				MCStacknode *t_node;
				t_node = MCstacks -> topnode();
				do
				{
					if (t_node -> getstack() -> getwindow() != NULL && IsWindowVisible((HWND)t_node -> getstack() -> getwindow() -> handle . window))
					{
						SetActiveWindow((HWND)t_node -> getstack() -> getwindow() -> handle . window);
						break;
					}

					t_node = t_node -> next();
				}
				while(t_node != MCstacks -> topnode());
			}

			if (msg == WM_LBUTTONDOWN)
				mbutton = 1;
			else if (msg = WM_MBUTTONDOWN)
				mbutton = 2;
			else
				mbutton = 3;
			MCdefaultstackptr->getcard()->message_with_args(MCM_mouse_down_in_backdrop, mbutton);
		return 0;
	
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			if (msg == WM_LBUTTONUP)
				mbutton = 1;
			else if (msg = WM_MBUTTONUP)
				mbutton = 2;
			else
				mbutton = 3;
			MCdefaultstackptr->getcard()->message_with_args(MCM_mouse_up_in_backdrop, mbutton);
		return 0;

		case WM_SETCURSOR:
		{
			// MW-2008-01-11: [[ Bug 4336 ]] Resize cursor sticks over backdrop on Windows
			MCCursorRef t_cursor;
			if (MCwatchcursor)
				t_cursor = MCcursors[PI_WATCH];
			else if (MCcursor != None)
				t_cursor = MCcursor;
			else if (MCdefaultcursorid)
				t_cursor = MCdefaultcursor;
			else
				t_cursor = MCcursors[PI_HAND];
			MCscreen -> setcursor(NULL, t_cursor);
		}
		return 0;
	}

	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

////////////////////////////////////////////////////////////////////////////////
