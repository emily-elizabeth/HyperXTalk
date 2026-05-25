/*

LiveCode is currently built agains Windows SDK 6.1.

To support features introduced in subsequent SDK versions, this header defines
structures and weak-linked functions required to use those newer features.

*/

////////////////////////////////////////////////////////////////////////////////

#include "foundation.h"

typedef enum __MCWin32ProcessDpiAwareness
{
	kMCWin32ProcessDpiUnaware,
	kMCWin32ProcessSystemDpiAware,
	kMCWin32ProcessPerMonitorDpiAware,
} MCWin32ProcessDpiAwareness;

typedef enum __MCWin32MonitorDpiType
{
	kMCWin32MDTEffectiveDpi,
	kMCWin32MDTAngularDpi,
	kMCWin32MDTRawDpi,
	kMCWin32MDTDefault = kMCWin32MDTEffectiveDpi,
} MCWin32MonitorDpiType;

////////////////////////////////////////////////////////////////////////////////

// Windows Vista, kernel32.dll
bool MCWin32QueryActCtxSettingsW(BOOL &r_result, DWORD p_flags, HANDLE p_act_ctx,
								 PCWSTR p_settings_namespace,
								 PCWSTR p_setting_name,
								 PWSTR p_buffer, SIZE_T p_buffer_size,
								 SIZE_T *r_out_size);

// Windows Vista, user32.dll
bool MCWin32SetProcessDPIAware(BOOL &r_result);

// Windows Vista, user32.dll
bool MCWin32IsProcessDPIAware(BOOL &r_result);

// Windows 8.1, shcore.dll
bool MCWin32GetProcessDpiAwareness(HRESULT &r_result, HANDLE p_hprocess,
								   MCWin32ProcessDpiAwareness *r_awareness);

// Windows 8.1, shcore.dll
bool MCWin32GetDpiForMonitor(HRESULT &r_result, HMONITOR p_monitor,
							 MCWin32MonitorDpiType p_dpi_type,
							 UINT *r_dpi_x, UINT *r_dpi_y);

////////////////////////////////////////////////////////////////////////////////

// Convienience functions

bool MCWin32QueryActCtxSettings(const unichar_t *p_settings_name, unichar_t *&r_value);
