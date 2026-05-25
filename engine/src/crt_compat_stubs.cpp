/* crt_compat_stubs.cpp
 *
 * Stubs for old MSVC CRT functions referenced by prebuilt libraries
 * compiled with older toolchains (VS2010-VS2013).
 *
 * _chvalidator: used by isalpha()/isupper()/etc. macros in old MSVC CRT.
 * In VS2015+ these became inline; the symbol no longer lives in LIBCMT.
 * Implemented using the equivalent modern _isctype().
 */

#include <ctype.h>

extern "C" int __cdecl _chvalidator(int c, int mask)
{
    return _isctype(c, mask);
}
