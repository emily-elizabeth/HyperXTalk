/*
 * LCWindowAnimation.m
 *
 * Plain-C glue layer between LCB and Cocoa window animation APIs.
 * Exports only primitive types so LCB foreign handlers need no struct marshalling.
 *
 * Build (via build_glue.sh):
 *   clang -x objective-c -fobjc-arc -dynamiclib -framework Cocoa \
 *         -arch arm64  -o windowanimation_glue_arm64.dylib  LCWindowAnimation.m
 *   clang -x objective-c -fobjc-arc -dynamiclib -framework Cocoa \
 *         -arch x86_64 -o windowanimation_glue_x86_64.dylib LCWindowAnimation.m
 *   lipo -create windowanimation_glue_arm64.dylib windowanimation_glue_x86_64.dylib \
 *        -output windowanimation_glue.dylib
 */

#import <Cocoa/Cocoa.h>
#import "LCWindowAnimation.h"

// ---------------------------------------------------------------------------
// WAGetMainScreenHeight
// ---------------------------------------------------------------------------

double WAGetMainScreenHeight(void) {
    return NSScreen.mainScreen.frame.size.height;
}

// ---------------------------------------------------------------------------
// WAAnimateWindowResize
// Looks up the NSWindow fresh on every call via windowWithWindowNumber:.
// The windowNumber passed in should be obtained from the LiveCode side as
// `the windowId of stack "StackName"` — always by explicit name, never
// via `this stack` in a toolbar callback as that resolves unreliably.
// ---------------------------------------------------------------------------

int WAAnimateWindowResize(long windowNumber,
                          double x,
                          double y,
                          double width,
                          double height,
                          double duration)
{
    NSWindow *win = [NSApp windowWithWindowNumber:(NSInteger)windowNumber];
    if (!win) return 0;

    NSRect liveFrame   = win.frame;
    NSRect liveContent = [win contentRectForFrameRect:liveFrame];
    CGFloat chrome     = liveFrame.size.height - liveContent.size.height;

    CGFloat toolbar = 0;
    if (win.toolbar && win.toolbar.isVisible) {
        toolbar = liveContent.size.height - win.contentLayoutRect.size.height;
    }

    NSRect newFrame = NSMakeRect(x, y, width, height + chrome + toolbar);

    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:duration];
    [[win animator] setFrame:newFrame display:YES];
    [NSAnimationContext endGrouping];

    return 1;
}
