#include "prefix.h"

#include "em-view.h"

#include "parsedef.h"
#include "util.h"

#include <emscripten.h>

/* Performs initial setup of the Emscripten view, using SDL. */
bool
MCEmscriptenViewInitialize()
{
    return true;
}

/* Clean up the SDL video state */
void
MCEmscriptenViewFinalize()
{
}

/* Resize the canvas and update the SDL video mode */
bool
MCEmscriptenViewSetBounds(const MCRectangle & p_rect)
{
	int t_canvas_width = p_rect.width;
	int t_canvas_height = p_rect.height;

	/* Attempt to resize the canvas */
	emscripten_set_canvas_size(t_canvas_width, t_canvas_height);

    return true;
}

/* Return the size of the Emscripten view as a rectangle. */
MCRectangle
MCEmscriptenViewGetBounds()
{
	int t_canvas_width, t_canvas_height, t_is_fullscreen;
	emscripten_get_canvas_size(&t_canvas_width,
	                           &t_canvas_height,
	                           &t_is_fullscreen);

	MCRectangle t_result;
	MCU_set_rect(t_result, 0, 0, t_canvas_width, t_canvas_height);

	return t_result;
}

