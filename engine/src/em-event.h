#ifndef __MC_EMSCRIPTEN_EVENT_H__
#define __MC_EMSCRIPTEN_EVENT_H__

/* Register HTML5 event handlers */
bool MCEmscriptenEventInitialize(void);

/* Remove HTML5 event handlers */
void MCEmscriptenEventFinalize(void);

#endif /* !__MC_EMSCRIPTEN_EVENT_H__ */
