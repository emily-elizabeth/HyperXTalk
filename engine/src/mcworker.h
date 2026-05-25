/* Copyright (C) 2024 HyperXTalk Contributors

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

#ifndef __MC_WORKER_H__
#define __MC_WORKER_H__

#include <mutex>
#include <condition_variable>
#include <thread>

class MCStack;
class MCExecContext;
class MCParameter;

// ---------------------------------------------------------------------------
// Cross-thread value passing
// ---------------------------------------------------------------------------

// A single parameter value that has been evaluated and deep-copied on the
// sender's thread so it can be safely transferred to another thread.
struct MCWorkerParam
{
    MCValueRef value; // retained; the receiving side owns and must release it
};

// Allocate and populate an MCWorkerParam array by evaluating every parameter
// in p_params on the caller's thread.  Returns true on success.  The caller
// owns the returned array and must free it with MCWorkerParamsFree.
bool MCWorkerMarshalParams(MCExecContext    &ctxt,
                           MCParameter      *p_params,
                           MCWorkerParam   *&r_params,
                           uint32_t         &r_count);

// Release each MCValueRef in the array and delete the array itself.
void MCWorkerParamsFree(MCWorkerParam *p_params, uint32_t p_count);

// ---------------------------------------------------------------------------
// Worker message (main thread → worker thread)
// ---------------------------------------------------------------------------

struct MCWorkerMessage
{
    MCNameRef        message;
    MCWorkerParam   *params;       // owned; param_count entries
    uint32_t         param_count;
    MCStack         *caller_stack; // raw pointer — valid at dispatch time
    MCWorkerMessage *next;
};

MCWorkerMessage *MCWorkerMessageNew(MCNameRef       p_message,
                                    MCWorkerParam  *p_params,
                                    uint32_t        p_count,
                                    MCStack        *p_caller);
void             MCWorkerMessageFree(MCWorkerMessage *p_msg);

// ---------------------------------------------------------------------------
// Worker callback (worker thread → main thread, via MCNotifyPush)
// ---------------------------------------------------------------------------

struct MCWorkerCallback
{
    MCNameRef       target_name;   // retained name of the calling stack; looked
                                   // up via MCdispatcher->findstackname() on delivery
    MCNameRef       message;
    MCWorkerParam  *params;
    uint32_t        param_count;
};

// Called by the MCNotify system on the main thread to deliver a callback that
// a worker posted with MCNotifyPush.
void MCWorkerDeliverCallback(void *p_opaque);

// ---------------------------------------------------------------------------
// MCWorker
// ---------------------------------------------------------------------------

/* MCWorker
 *
 * Represents a named worker — an isolated script context that runs on its
 * own background thread.  Workers are created with:
 *
 *   create worker <name> [from <scriptfile>]
 *
 * Messages are sent to a worker asynchronously:
 *
 *   dispatch <message> to worker <name> [with <params>]
 *
 * The call returns immediately; the worker handles the message on its own
 * thread.  To send a result back, the worker's handler uses:
 *
 *   dispatch <message> to caller [with <params>]
 *
 * which posts the message to the main thread via MCNotifyPush so it is
 * delivered safely at the next script-execution point.
 *
 * Workers are registered in a global registry and looked up by name.
 */
class MCWorker
{
public:
    MCWorker(MCStringRef p_name);
    ~MCWorker();

    MCStringRef GetName()  const { return m_name; }
    MCStack    *GetStack() const { return m_stack; }
    void        SetStack(MCStack *p_stack) { m_stack = p_stack; }

    // The caller stack for the message currently being handled.  Set by the
    // worker thread itself before each dispatch; nil between messages.
    MCStack    *GetCurrentCaller() const          { return m_current_caller; }
    void        SetCurrentCaller(MCStack *p_stack){ m_current_caller = p_stack; }

    // Post a pre-marshalled message.  Takes ownership of p_msg.
    // Safe to call from the main thread only.
    void PostMessage(MCWorkerMessage *p_msg);

    // Start the worker's background thread.  Called by MCWorkerExecCreate.
    bool StartThread();

    // Signal the thread to stop and wait for it to exit.
    // Called by MCWorkerExecDestroy and MCWorkerRegistryFinalize.
    void StopThread();

    // Intrusive linked list for the registry.
    MCWorker *next;

private:
    MCStringRef  m_name;
    MCStack     *m_stack;           // not owned — managed by MCdispatcher
    MCStack     *m_current_caller;  // set transiently during handler execution

    // Background thread
    std::thread              m_thread;

    // Incoming message queue (written by main thread, drained by worker)
    std::mutex               m_queue_mutex;
    std::condition_variable  m_queue_cond;
    MCWorkerMessage         *m_queue_head;
    MCWorkerMessage         *m_queue_tail;
    bool                     m_stop;

    static void ThreadEntry(MCWorker *p_worker);
    void         RunLoop();
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

void      MCWorkerRegistryInitialize();
void      MCWorkerRegistryFinalize();

MCWorker *MCWorkerFind(MCStringRef p_name);
void      MCWorkerAdd(MCWorker *p_worker);
void      MCWorkerRemove(MCStringRef p_name);

// Returns the worker currently executing on THIS thread, or nullptr.
// Used by MCWorkerExecDispatchToCaller to identify the caller.
MCWorker *MCWorkerGetCurrent();

// ---------------------------------------------------------------------------
// Engine exec helpers — called from cmdsc.cpp and cmdse.cpp
// ---------------------------------------------------------------------------

// 'create worker <name> [from <scriptfile>]'
void MCWorkerExecCreate(MCExecContext &ctxt,
                        MCStringRef    p_name,
                        MCStringRef    p_script_file);

// 'dispatch <message> to worker <name> [with <params>]'
// Non-blocking: evaluates params, posts to the worker queue, returns immediately.
void MCWorkerExecDispatch(MCExecContext &ctxt,
                          MCStringRef    p_worker_name,
                          MCNameRef      p_message,
                          bool           p_is_function,
                          MCParameter   *p_params);

// 'dispatch <message> to caller [with <params>]'
// Posts to the main thread via MCNotifyPush; does not block.
void MCWorkerExecDispatchToCaller(MCExecContext &ctxt,
                                  MCNameRef      p_message,
                                  bool           p_is_function,
                                  MCParameter   *p_params);

// 'destroy worker <name>'
void MCWorkerExecDestroy(MCExecContext &ctxt,
                         MCStringRef    p_name);

#endif /* __MC_WORKER_H__ */
