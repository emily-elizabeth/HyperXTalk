#ifndef LC_WINDOW_ANIMATION_H
#define LC_WINDOW_ANIMATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Animate a window resize, looking up the window by its windowNumber.
 *
 * x, y      : Cocoa bottom-left origin (Y-flip handled by the LCB side)
 * width     : new width
 * height    : new content-area height (title bar + toolbar added automatically)
 * duration  : animation duration in seconds (e.g. 0.3)
 *
 * Returns 1 on success, 0 if the window could not be found.
 */
int WAAnimateWindowResize(long windowNumber,
                          double x,
                          double y,
                          double width,
                          double height,
                          double duration);

/*
 * Returns the height of the main screen in points.
 * Used by the LCB side to flip LiveCode's top-left Y into Cocoa's bottom-left Y.
 */
double WAGetMainScreenHeight(void);

#ifdef __cplusplus
}
#endif

#endif /* LC_WINDOW_ANIMATION_H */
