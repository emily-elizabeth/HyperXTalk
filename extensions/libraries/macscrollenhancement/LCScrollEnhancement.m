/*
 * LCScrollEnhancement.m
 *
 * Plain-C/ObjC glue layer between LCB and Cocoa scroll event monitoring.
 *
 * Intercepts ALL precise trackpad scroll events application-wide (both active-
 * touch and inertial momentum phases). The engine never sees them, so it cannot
 * apply its coarse line-quantised scrolling. The accumulated pixel delta is
 * polled from LCB and applied directly to the target field's vScroll.
 *
 * Physical scroll wheel events (hasPreciseScrollingDeltas == NO) are passed
 * through untouched.
 *
 * When SESetScrollRect has been called with a non-zero rect, the monitor only
 * consumes events whose current mouse position (in LiveCode screen coordinates:
 * origin top-left, y increases downward) falls within that rect. Events outside
 * — e.g. over the handler list — are passed through to the engine unchanged.
 *
 * The rect is stored in LiveCode screen coordinates. The check converts
 * [NSEvent mouseLocation] (Cocoa screen: y-up) to LC coords using the primary
 * screen height, which keeps the LiveCode Script side simple — it just passes
 * `the rect of field` directly without any coordinate conversion.
 *
 * Build (via build_glue.sh):
 *   clang -x objective-c -fobjc-arc -dynamiclib -framework Cocoa \
 *         -arch arm64  -o scrollenhancement_glue_arm64.dylib  LCScrollEnhancement.m
 *   clang -x objective-c -fobjc-arc -dynamiclib -framework Cocoa \
 *         -arch x86_64 -o scrollenhancement_glue_x86_64.dylib LCScrollEnhancement.m
 *   lipo -create scrollenhancement_glue_arm64.dylib scrollenhancement_glue_x86_64.dylib \
 *        -output scrollenhancement_glue.dylib
 */

#import <Cocoa/Cocoa.h>

// Forward declarations
void   SEInstallMonitor(long windowId);
double SEGetAndClearMomentumDelta(void);
double SEGetAndClearDeltaX(void);
void   SESetScrollRect(long left, long top, long right, long bottom);
void   SESetScrollRectStr(const char *rectStr);
void   SERemoveMonitor(void);

// ---------------------------------------------------------------------------
// Scroll rect — stored in LiveCode screen coordinates (origin = top-left of
// primary screen, y increases downward). Compared against the mouse position
// after converting [NSEvent mouseLocation] from Cocoa to LC coords.
// sScrollRectSet == NO means intercept everywhere (no filter active).
// ---------------------------------------------------------------------------
static BOOL sScrollRectSet = NO;
static long sLCLeft  = 0;
static long sLCTop   = 0;
static long sLCRight  = 0;
static long sLCBottom = 0;

// ---------------------------------------------------------------------------
// State (accessed on main thread only — NSEvent local monitors fire there,
// same thread as LCB poll calls)
// ---------------------------------------------------------------------------

static id        sMonitor      = nil;
static double    sDeltaY       = 0.0;
static double    sDeltaX       = 0.0;
static NSInteger sTargetWinNum = 0;   // windowNumber of the script editor window

// ---------------------------------------------------------------------------
// SEInstallMonitor
//
// Installs an application-wide local NSEvent monitor that intercepts precise
// trackpad scroll events. The windowId passed by LiveCode Script is the
// NSWindow windowNumber of the script editor. We look it up once here so
// we can do a fast integer compare in the hot event handler.
// ---------------------------------------------------------------------------

void SEInstallMonitor(long windowId)
{
    SERemoveMonitor();
    sDeltaY = 0.0;
    sDeltaX = 0.0;

    // Popup/overlay detection is handled entirely in the LiveCode Script layer
    // (via seHandlerListObject visibility check). No C-level window comparison
    // is needed or reliable — LiveCode's windowId does not equal NSWindow.windowNumber,
    // and capturing windowNumberAtPoint at install time races with window compositing.
    // Always accumulate precise scroll events; LiveCode Script decides whether to apply.
    sTargetWinNum = 0;

    sMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                                     handler:^NSEvent *(NSEvent *event)
    {
        // Pass physical scroll wheels through unchanged.
        if (!event.hasPreciseScrollingDeltas) return event;

        // Accumulate raw pixel deltas.
        // Vertical: Cocoa negative = down; LC vScroll increases downward, so negate.
        // Horizontal: Cocoa negative = right; LC hScroll increases rightward, so negate.
        sDeltaY -= event.scrollingDeltaY;
        sDeltaX -= event.scrollingDeltaX;

        // Consume — the engine won't see this event.
        return nil;
    }];
}

// ---------------------------------------------------------------------------
// SEGetAndClearMomentumDelta
//
// Returns accumulated vertical scroll delta in points since the last call,
// then resets the accumulator. Positive = scroll down (add to vScroll).
// Returns 0.0 when no trackpad events have arrived.
// ---------------------------------------------------------------------------

double SEGetAndClearMomentumDelta(void)
{
    double delta = sDeltaY;
    sDeltaY = 0.0;
    return delta;
}

double SEGetAndClearDeltaX(void)
{
    double delta = sDeltaX;
    sDeltaX = 0.0;
    return delta;
}

// ---------------------------------------------------------------------------
// SESetScrollRect — restrict event interception to the given rect, expressed
// in LiveCode screen coordinates (left, top, right, bottom; origin top-left
// of primary screen, y increases downward). Pass all zeros to disable the
// filter and intercept everywhere.
//
// Call from LiveCode Script with: the rect of field "Script"
// No coordinate conversion needed on the LiveCode side.
// ---------------------------------------------------------------------------

void SESetScrollRect(long left, long top, long right, long bottom)
{
    if (left == 0 && top == 0 && right == 0 && bottom == 0) {
        sScrollRectSet = NO;
    } else {
        sLCLeft   = left;
        sLCTop    = top;
        sLCRight  = right;
        sLCBottom = bottom;
        sScrollRectSet = YES;
    }
}

// ---------------------------------------------------------------------------
// SESetScrollRectStr — convenience wrapper that parses a LiveCode-style rect
// string "left,top,right,bottom" using sscanf and calls SESetScrollRect.
// Called from LCB with a ZStringNative (single-parameter, avoids all LCB
// multi-param and type-coercion issues).
// ---------------------------------------------------------------------------

void SESetScrollRectStr(const char *rectStr)
{
    long l = 0, t = 0, r = 0, b = 0;
    sscanf(rectStr, "%ld,%ld,%ld,%ld", &l, &t, &r, &b);
    SESetScrollRect(l, t, r, b);
}

// ---------------------------------------------------------------------------
// SERemoveMonitor — stops interception and resets state.
// ---------------------------------------------------------------------------

void SERemoveMonitor(void)
{
    if (sMonitor) {
        [NSEvent removeMonitor:sMonitor];
        sMonitor = nil;
    }
    sDeltaY = 0.0;
    sDeltaX = 0.0;
    sScrollRectSet = NO;
    sTargetWinNum = 0;
}
