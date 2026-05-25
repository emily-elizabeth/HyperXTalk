#include "globdefs.h"

#include <CoreGraphics/CoreGraphics.h>

// ARM/modern macOS: Carbon/Carbon.h removed.
// Resource fork APIs (MCS_mac_openresourcefile_with_path, MCS_mac_closeresourcefile)
// have been removed - FSOpenResFile and related APIs are unavailable on arm64.
// AppleEvent types are available via <CoreServices/CoreServices.h> where needed.

// ARM64/modern macOS: Carbon HITheme types are unavailable.
// Provide minimal stubs so legacy osx* files compile.
#if defined(__arm64__) || defined(__aarch64__)

// HIToolbox / Carbon theme stubs
typedef UInt32 ThemeTrackKind;
typedef UInt32 ThemeDrawState;
typedef UInt32 ThemeButtonKind;
typedef UInt32 ThemeButtonAdornment;
typedef UInt32 ThemeButtonValue;
typedef CGRect HIRect;

typedef struct {
    CGFloat version;
    ThemeTrackKind kind;
    HIRect bounds;
    SInt32 min;
    SInt32 max;
    SInt32 value;
    UInt32 attributes;
    ThemeDrawState enableState;
    SInt32 trackInfo;
} HIThemeTrackDrawInfo;

typedef struct {
    CGFloat version;
    ThemeButtonKind kind;
    ThemeDrawState state;
    ThemeButtonValue value;
    ThemeButtonAdornment adornment;
} HIThemeButtonDrawInfo;

#endif // __arm64__
