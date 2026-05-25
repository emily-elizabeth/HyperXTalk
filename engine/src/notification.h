//
// Cross-platform user notification support.
//
// Usage from HyperXTalk scripts:
//
//   requestNotificationPermission          -- macOS/iOS/Android 13+ only;
//                                          -- no-op on Windows and Linux
//   showNotification "Title" "Body" ["Tag"]
//   cancelNotification "Tag"
//   cancelAllNotifications
//   get notificationPermission()           -- "granted" | "denied" | "unknown"
//
// Messages fired back to the current card of the default stack:
//   notificationPermissionGranted
//   notificationPermissionDenied
//   notificationClicked pTag
//

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "mcstring.h"

// ── Platform entry points (implemented in mac/w32/lnx-notification.*) ────────

// Kick off an async permission request.  On platforms that do not require a
// permission step (Windows, Linux) this should call
// MCNotificationDispatchPermissionGranted() synchronously.
void MCPlatformRequestNotificationPermission();

// Post a notification.  p_tag may be empty; a duplicate tag replaces the
// previous notification on platforms that support it.
void MCPlatformShowNotification(MCStringRef p_title,
                                MCStringRef p_body,
                                MCStringRef p_tag);

// Cancel a single notification by tag, or cancel every pending notification.
void MCPlatformCancelNotification(MCStringRef p_tag);
void MCPlatformCancelAllNotifications();

// Return the current permission state without triggering a prompt.
// Sets r_permission to one of: "granted", "denied", "unknown".
// Caller is responsible for releasing the returned string.
void MCPlatformGetNotificationPermission(MCStringRef& r_permission);

// ── Engine callbacks (called on the main thread by platform code) ─────────────

// Called by the platform after the OS permission dialog is dismissed.
void MCNotificationDispatchPermissionGranted();
void MCNotificationDispatchPermissionDenied();

// Called by the platform when the user clicks a delivered notification.
// p_tag is the tag that was supplied to MCPlatformShowNotification.
void MCNotificationDispatchClicked(MCStringRef p_tag);

#endif // NOTIFICATION_H
