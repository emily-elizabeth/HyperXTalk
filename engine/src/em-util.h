#ifndef __MC_EMSCRIPTEN_UTIL_H__
#define __MC_EMSCRIPTEN_UTIL_H__

#include <foundation.h>

/* ----------------------------------------------------------------
 * Debugging macros
 * ---------------------------------------------------------------- */

inline void
__MCEmscriptenNotImplemented(const char *p_file,
                             uint32_t p_line,
                             const char *p_function)
{
#if defined(_DEBUG)
	__MCLog(p_file, p_line, "not implemented: %s", p_function);
#endif /* _DEBUG */
}

#define MCEmscriptenNotImplemented() __MCEmscriptenNotImplemented(__FILE__, __LINE__, __PRETTY_FUNCTION__)

#endif /* ! __MC_EMSCRIPTEN_UTIL_H__ */
