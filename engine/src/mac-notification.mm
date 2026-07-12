/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

//
// macOS/iOS notification backend using UNUserNotificationCenter.
// Requires macOS 10.14+ / iOS 10+.
//

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include "prefix.h"
#include "mcstring.h"
#include "notification.h"

// MCStringConvertToAutoreleasedNSString
#include "foundation-objc.h"

////////////////////////////////////////////////////////////////////////////////
// Utility: MCStringRef ↔ NSString

static NSString *_mcstr_to_ns(MCStringRef p_str)
{
    if (p_str == nil || MCStringIsEmpty(p_str))
        return @"";
    return MCStringConvertToAutoreleasedNSString(p_str);
}

static MCStringRef _ns_to_mcstr(NSString *p_str)
{
    MCStringRef t_result;
    /* UNCHECKED */ MCStringCreateWithCFStringRef((__bridge CFStringRef)p_str, t_result);
    return t_result;  // caller owns
}

////////////////////////////////////////////////////////////////////////////////
// UNUserNotificationCenter delegate
//
// Receives foreground-display options and notification click events.

@interface MCNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation MCNotificationDelegate

// Allow notifications to be shown even when the app is in the foreground.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    completionHandler(UNNotificationPresentationOptionAlert |
                      UNNotificationPresentationOptionSound);
}

// User tapped the notification.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)(void))completionHandler
{
    NSString *t_tag = response.notification.request.identifier;
    MCStringRef t_mcstr = _ns_to_mcstr(t_tag ? t_tag : @"");

    dispatch_async(dispatch_get_main_queue(), ^{
        MCNotificationDispatchClicked(t_mcstr);
        MCValueRelease(t_mcstr);
    });

    completionHandler();
}

@end

////////////////////////////////////////////////////////////////////////////////
// Singleton delegate

static MCNotificationDelegate *s_delegate = nil;

static UNUserNotificationCenter *_center()
{
    UNUserNotificationCenter *t_center = [UNUserNotificationCenter currentNotificationCenter];
    if (s_delegate == nil)
    {
        s_delegate = [[MCNotificationDelegate alloc] init];
        t_center.delegate = s_delegate;
    }
    return t_center;
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

void MCPlatformRequestNotificationPermission()
{
    UNAuthorizationOptions t_opts =
        UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge;

    [_center() requestAuthorizationWithOptions:t_opts
                             completionHandler:^(BOOL granted, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (granted)
                MCNotificationDispatchPermissionGranted();
            else
                MCNotificationDispatchPermissionDenied();
        });
    }];
}

void MCPlatformShowNotification(MCStringRef p_title, MCStringRef p_body, MCStringRef p_tag)
{
    UNMutableNotificationContent *t_content = [[UNMutableNotificationContent alloc] init];
    t_content.title = _mcstr_to_ns(p_title);
    t_content.body  = _mcstr_to_ns(p_body);
    t_content.sound = [UNNotificationSound defaultSound];

    // Use the supplied tag as the request identifier, or generate a unique one.
    NSString *t_id;
    if (p_tag != nil && !MCStringIsEmpty(p_tag))
        t_id = _mcstr_to_ns(p_tag);
    else
        t_id = [[NSUUID UUID] UUIDString];

    // Trigger immediately (nil trigger = deliver right away).
    UNNotificationRequest *t_req =
        [UNNotificationRequest requestWithIdentifier:t_id
                                             content:t_content
                                             trigger:nil];

    [_center() addNotificationRequest:t_req withCompletionHandler:nil];
}

void MCPlatformCancelNotification(MCStringRef p_tag)
{
    NSString *t_id = _mcstr_to_ns(p_tag);
    [_center() removePendingNotificationRequestsWithIdentifiers:@[t_id]];
    [_center() removeDeliveredNotificationsWithIdentifiers:@[t_id]];
}

void MCPlatformCancelAllNotifications()
{
    [_center() removeAllPendingNotificationRequests];
    [_center() removeAllDeliveredNotifications];
}

void MCPlatformGetNotificationPermission(MCStringRef& r_permission)
{
    // getNotificationSettingsWithCompletionHandler is async; we want to return
    // a synchronous result.
    //
    // On recent macOS versions the completion block is delivered on the calling
    // thread's run-loop / queue rather than an independent secondary thread.
    // Calling this from the main thread and then waiting on a semaphore on that
    // same thread would deadlock — the block could never fire because the queue
    // is blocked.
    //
    // Fix: issue the query from a background queue.  That guarantees the
    // completion block is delivered independently of the main thread, so the
    // semaphore wait here (on the main thread) will always resolve.
    __block UNAuthorizationStatus t_status = UNAuthorizationStatusNotDetermined;
    dispatch_semaphore_t t_sem = dispatch_semaphore_create(0);

    // Capture the notification center reference on the main thread before
    // dispatching, since _center() must only be called from the main thread.
    UNUserNotificationCenter *t_center = _center();

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [t_center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
            t_status = settings.authorizationStatus;
            dispatch_semaphore_signal(t_sem);
        }];
    });

    // Wait at most 2 seconds.  In practice this resolves in < 10 ms.
    dispatch_semaphore_wait(t_sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));

    switch (t_status)
    {
        case UNAuthorizationStatusAuthorized:
        case UNAuthorizationStatusProvisional:
#if TARGET_OS_IOS
        // UNAuthorizationStatusEphemeral is iOS 14+ only; unavailable on macOS.
        case UNAuthorizationStatusEphemeral:
#endif
            /* UNCHECKED */ MCStringCreateWithCString("granted", r_permission);
            break;
        case UNAuthorizationStatusDenied:
            /* UNCHECKED */ MCStringCreateWithCString("denied", r_permission);
            break;
        default:
            /* UNCHECKED */ MCStringCreateWithCString("unknown", r_permission);
            break;
    }
}
