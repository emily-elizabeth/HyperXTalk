#ifndef __MC_EMSCRIPTEN_ASYNC_H__
#define __MC_EMSCRIPTEN_ASYNC_H__

#include <foundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Yield to browser and wait for an event.
 *
 * If an event does not occur within the specified p_timeout_s,
 * returns zero.  Otherwise, returns non-zero.
 *
 * If p_timeout_s is negative or infinite, MCEmscriptenAsyncYield()
 * will always wait for an event to occur.  p_timeout_s is a duration
 * in seconds.
 *
 * Defined in em-async.js
 */
int MCEmscriptenAsyncYield(real64_t p_timeout_s = -1);

/* Continue running the engine's main loop on receipt of an event.
 *
 * Returns when the engine next yields.
 */
void MCEmscriptenAsyncResume(void);

#if defined(__cplusplus)
}
#endif

#endif /* !__MC_EMSCRIPTEN_ASYNC_H__ */
