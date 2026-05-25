#ifndef __MC_EMSCRIPTEN_VIEW_H__
#define __MC_EMSCRIPTEN_VIEW_H__

bool MCEmscriptenViewInitialize(void);
void MCEmscriptenViewFinalize(void);

bool MCEmscriptenViewSetBounds(const MCRectangle & p_rect);
MCRectangle MCEmscriptenViewGetBounds(void);

#endif /* !__MC_EMSCRIPTEN_VIEW_H__ */
