#ifndef __MOBILE_NOTIFICATION__
#define __MOBILE_NOTIFICATION__

#include "mblsyntax.h"

void MCNotificationPostLocalNotificationEvent(MCStringRef p_payload);
void MCNotificationPostPushNotificationEvent(MCStringRef p_payload);
void MCNotificationPostPushRegistered (MCStringRef p_registration_text);
void MCNotificationPostPushRegistrationError (MCStringRef p_error_text);
void MCNotificationPostUrlWakeUp (MCStringRef p_url_wake_up_text);
void MCNotificationPostLaunchDataChanged();
void MCNotificationPostSystemAppearanceChanged();
bool MCNotificationPostCustom(MCNameRef p_message, uint32_t p_param_count, ...);

#endif
