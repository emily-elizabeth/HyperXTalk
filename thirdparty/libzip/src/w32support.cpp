// SN-2015-03-10: [[ Bug 14413 ]] We need few types (WCHAR)
// and function from the Windows API
#include "zip.h"
#include "zipint.h"

WCHAR* ConvertCStringToLpwstr(const char *p_input)
{
	int t_length;
	int t_error, t_allocated;
	WCHAR *t_utf16_fn;

	t_error = 0;
	t_allocated = 0;
	t_utf16_fn = NULL;

	if (!t_error)
	{
		t_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p_input, strlen(p_input), NULL, 0);
		t_error = t_length == 0;
	}

	if (!t_error)
	{
		t_utf16_fn = (WCHAR*)calloc(1, (t_length + 1) * sizeof(WCHAR));
		if (t_utf16_fn == NULL)
			t_error = 1;
		else
			t_allocated = 1;
	}

	if (!t_error)
	{
		t_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p_input, strlen(p_input), t_utf16_fn, (DWORD)t_length);
		t_error = t_length == 0;
	}

	if (t_error && t_allocated)
	{
		free(t_utf16_fn);
		t_utf16_fn = NULL;
	}

	return t_utf16_fn;
}
