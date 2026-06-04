#ifndef __MC_FIBER__
#define __MC_FIBER__

typedef struct MCFiber *MCFiberRef;
typedef void (*MCFiberCallback)(void *);

// Convert the current thread to a fiber.
bool MCFiberConvert(MCFiberRef& r_fiber);

// Create a new fiber with the given stack size.
bool MCFiberCreate(size_t stack_size, MCFiberRef& r_fiber);

// Destroy the given fiber. This implicitly jumps to the fiber and
// waits for termination.
void MCFiberDestroy(MCFiberRef fiber);

// Get the fiber that is currently active.
MCFiberRef MCFiberGetCurrent(void);

// Switch to the given fiber, pausing the current one.
void MCFiberMakeCurrent(MCFiberRef fiber);

// Invoke the given routine on the given fiber and wait for return.
void MCFiberCall(MCFiberRef fiber, void (*callback)(void *), void *context);

// Returns true if the current fiber is currently running.
bool MCFiberIsCurrentThread(MCFiberRef fiber);

#endif
