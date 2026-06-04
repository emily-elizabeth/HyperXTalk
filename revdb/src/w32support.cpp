///////////////////////////////////////////////////////////////////////////////
// Windows support function implementations

#include "w32support.h"

HINSTANCE hInstance = NULL;

void MCU_path2std(char *p_path)
{
  if (p_path == NULL || !*p_path)
    return;

  do 
  {
	 if (*p_path == '/')
	 {
      *p_path = '\\';
	 }
    else
	{
      if (*p_path == '\\')
		  *p_path = '/';
	 }
  } while (*++p_path);
}

void MCU_path2native(char *p_path)
{
	if (p_path == NULL || !*p_path)
		return;
	do 
	{
		if (*p_path == '/')
		{
			*p_path = '\\';
		}
		else
		{
			if (*p_path == '\\')
				*p_path = '/';
		}
  } while (*++p_path);
}

void MCU_fix_path(char *cstr)
{
  char *fptr = cstr;
  while (*fptr) 
  {
    if (*fptr == '/' && *(fptr + 1) == '.' && *(fptr + 2) == '.' && *(fptr + 3) == '/') 
	{
		if (fptr == cstr)
		  strcpy(fptr, fptr + 3);
		else
		{
			char *bptr = fptr - 1;
			while (True)
			{
				if (*bptr == '/')
				{
					strcpy(bptr, fptr + 3);
					fptr = bptr;
					break;
				}
				else
					bptr--;
			}
		}
	}
    else
      if (*fptr == '/' && *(fptr + 1) == '.' && *(fptr + 2) == '/')
		  strcpy(fptr, fptr + 2);
      else
		  if (fptr != cstr && *fptr == '/' && *(fptr + 1) == '/')
			  strcpy(fptr, fptr + 1);
	else
	  fptr++;
  }
}

char *MCS_getcurdir(void)
{
  char *t_path = new (nothrow) char[PATH_MAX + 2];
  GetCurrentDirectory(PATH_MAX +1, (LPTSTR)t_path);
  MCU_path2std(t_path);
  return t_path;
}

#define strclone istrdup

char *MCS_resolvepath(const char *p_path)
{				
  if (p_path == NULL)
  {
    char *t_path = MCS_getcurdir();
    MCU_path2native(t_path);
    return t_path;
  }

  char *cstr = strclone(p_path);
  MCU_path2native(cstr);
  return cstr;
}
