#include "prefix.h"

#include "w32text.h"
#include <foundation.h>

WideCString::WideCString(LPCSTR p_ansi_string, int p_length)
{
	int t_length;
	t_length = MultiByteToWideChar(CP_ACP, 0, p_ansi_string, p_length, NULL, 0);

	f_string = new (nothrow) WCHAR[t_length];
	if (f_string != NULL)
		MultiByteToWideChar(CP_ACP, 0, p_ansi_string, p_length, (LPWSTR)f_string, t_length);
}

AnsiCString::AnsiCString(LPCWSTR p_wide_string)
{
	int t_length;
	t_length = WideCharToMultiByte(CP_ACP, 0, p_wide_string, -1, NULL, 0, NULL, NULL);

	f_string = new (nothrow) CHAR[t_length];
	if (f_string != NULL)
		WideCharToMultiByte(CP_ACP, 0, p_wide_string, -1, (LPSTR)f_string, t_length, NULL, NULL);
}
