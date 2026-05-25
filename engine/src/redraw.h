#ifndef __MC_REDRAW__
#define __MC_REDRAW__

bool MCRedrawIsScreenLocked(void);
void MCRedrawSaveLockScreen(uint2& r_lock);
void MCRedrawRestoreLockScreen(uint2 lock);

void MCRedrawLockScreen(void);
void MCRedrawUnlockScreen(void);
void MCRedrawUnlockScreenWithEffects(void);
void MCRedrawForceUnlockScreen(void);

bool MCRedrawIsScreenDirty(void);
void MCRedrawDirtyScreen(void);

void MCRedrawScheduleUpdateForStack(MCStack *stack);
void MCRedrawDoUpdateScreen(void);

bool MCRedrawIsScreenUpdateEnabled(void);
void MCRedrawDisableScreenUpdates(void);
void MCRedrawEnableScreenUpdates(void);

void MCRedrawDoUpdateScreen(void);

#endif
