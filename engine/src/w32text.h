#ifndef __W32TEXT__
#define __W32TEXT__

class WideCString
{
	LPCWSTR f_string;

public:
	WideCString(LPCSTR p_ansi_string, int t_length = -1);
	~WideCString(void);

	operator LPCWSTR (void) const;
};

inline WideCString::~WideCString(void)
{
	if (f_string != NULL)
		delete[] f_string;
}

inline WideCString::operator LPCWSTR(void) const
{
	return f_string;
}

class AnsiCString
{
	LPCSTR f_string;

public:
	AnsiCString(LPCWSTR p_wide_string);
	~AnsiCString(void);

	operator LPCSTR (void) const;
};

inline AnsiCString::~AnsiCString(void)
{
	if (f_string != NULL)
		delete[] f_string;
}

inline AnsiCString::operator LPCSTR(void) const
{
	return f_string;
}

#endif
