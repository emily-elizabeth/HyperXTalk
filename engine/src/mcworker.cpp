/* Copyright (C) 2024 HyperXTalk Contributors

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

#include "prefix.h"

// Defined in libfoundation/src/foundation-value.cpp.
// Drains the calling thread's value-pool free list, returning each slot to the
// heap.  Must be called from worker threads before they exit to avoid leaks.
extern void __MCValueDrainPoolForThread(void);

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"
#include "mcio.h"

#include "globals.h"
#include "dispatch.h"
#include "stack.h"
#include "object.h"
#include "param.h"
#include "exec.h"
#include "variable.h"
#include "stacksecurity.h"
#include "osspec.h"
#include "securemode.h"
#include "notify.h"

#include "executionerrors.h"
#include "debug.h"     // MCnexecutioncontexts (thread_local)

#include "mcworker.h"

static void MCWorkerDeliverCallbackNow(MCWorkerCallback *t_cb);

// ---------------------------------------------------------------------------
// Thread-local: which MCWorker is currently running on this thread.
// Set by the worker thread before each handler dispatch; nullptr on the main
// thread and between messages.
// ---------------------------------------------------------------------------
static thread_local MCWorker *tls_current_worker = nullptr;

// ---------------------------------------------------------------------------
// MCWorkerParam helpers
// ---------------------------------------------------------------------------

bool MCWorkerMarshalParams(MCExecContext  &ctxt,
                           MCParameter    *p_params,
                           MCWorkerParam *&r_params,
                           uint32_t       &r_count)
{
    // Count parameters.
    uint32_t t_count = 0;
    for (MCParameter *p = p_params; p != nullptr; p = p->getnext())
        t_count++;

    r_count = t_count;

    if (t_count == 0)
    {
        r_params = nullptr;
        return true;
    }

    r_params = new (nothrow) MCWorkerParam[t_count];
    if (r_params == nullptr)
    {
        ctxt.LegacyThrow(EE_NO_MEMORY);
        return false;
    }

    uint32_t i = 0;
    for (MCParameter *p = p_params; p != nullptr; p = p->getnext(), i++)
    {
        MCAutoValueRef t_val;
        if (!p->eval(ctxt, &t_val))
        {
            // Free what we've retained so far and bail.
            for (uint32_t j = 0; j < i; j++)
                MCValueRelease(r_params[j].value);
            delete[] r_params;
            r_params = nullptr;
            r_count  = 0;
            return false;
        }

        // Deep-copy so the value is fully owned by this param and has no
        // shared mutable state that could race across the thread boundary.
        if (!MCValueCopy(*t_val, r_params[i].value))
        {
            for (uint32_t j = 0; j < i; j++)
                MCValueRelease(r_params[j].value);
            delete[] r_params;
            r_params = nullptr;
            r_count  = 0;
            ctxt.LegacyThrow(EE_NO_MEMORY);
            return false;
        }
    }

    return true;
}

void MCWorkerParamsFree(MCWorkerParam *p_params, uint32_t p_count)
{
    if (p_params == nullptr)
        return;
    for (uint32_t i = 0; i < p_count; i++)
        MCValueRelease(p_params[i].value);
    delete[] p_params;
}

// ---------------------------------------------------------------------------
// MCWorkerMessage
// ---------------------------------------------------------------------------

MCWorkerMessage *MCWorkerMessageNew(MCNameRef      p_message,
                                    MCWorkerParam *p_params,
                                    uint32_t       p_count,
                                    MCStack       *p_caller)
{
    MCWorkerMessage *t_msg = new (nothrow) MCWorkerMessage;
    if (t_msg == nullptr)
        return nullptr;

    t_msg->message      = MCValueRetain(p_message);
    t_msg->params       = p_params;
    t_msg->param_count  = p_count;
    t_msg->caller_stack = p_caller;
    t_msg->next         = nullptr;
    return t_msg;
}

void MCWorkerMessageFree(MCWorkerMessage *p_msg)
{
    if (p_msg == nullptr)
        return;
    MCValueRelease(p_msg->message);
    MCWorkerParamsFree(p_msg->params, p_msg->param_count);
    delete p_msg;
}

// ---------------------------------------------------------------------------
// MCWorkerCallback — delivered on main thread by MCNotify
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Callback delivery
// ---------------------------------------------------------------------------

// Actual delivery — should only run when MCnexecutioncontexts == 0.
// Safe-notification re-entrancy is prevented by the s_safe_dispatch_depth
// guard in notify.cpp, so this is normally guaranteed.  The check below is
// belt-and-suspenders in case some other code path calls us while nested.
static void MCWorkerDeliverCallbackNow(MCWorkerCallback *t_cb)
{
    // Look up the target stack by name; it may have been closed since the
    // callback was posted.
    MCStack *t_target_stack = MCdispatcher->findstackname(t_cb->target_name);
    if (t_target_stack == nullptr)
    {
        MCWorkerParamsFree(t_cb->params, t_cb->param_count);
        MCValueRelease(t_cb->target_name);
        MCValueRelease(t_cb->message);
        delete t_cb;
        return;
    }

    // Build an MCParameter list from the marshalled values.
    MCParameter *t_first = nullptr;
    MCParameter *t_last  = nullptr;
    for (uint32_t i = 0; i < t_cb->param_count; i++)
    {
        MCParameter *t_p = new (nothrow) MCParameter;
        if (t_p == nullptr)
            break;
        // setvalueref_argument retains the value internally; do not pre-retain.
        t_p->setvalueref_argument(t_cb->params[i].value);
        if (t_first == nullptr)
            t_first = t_last = t_p;
        else
        {
            t_last->setnext(t_p);
            t_last = t_p;
        }
    }

    // Dispatch the callback on the main thread.
    MCObjectPtr t_target;
    t_target.object  = t_target_stack;
    t_target.part_id = 0;

    // MCEngineExecDispatch writes to 'it' after every dispatch.  This context
    // has no enclosing handler, so give it a dedicated variable to absorb that
    // write (the result is intentionally discarded here).
    MCVariable *t_it_var = nullptr;
    /* UNCHECKED */ MCVariable::createwithname(MCN_it, t_it_var);
    MCVarref *t_it_ref = new (nothrow) MCVarref(t_it_var);

    MCExecContext t_ctxt;
    t_ctxt.SetWorkerIt(t_it_ref);
    MCEngineExecDispatch(t_ctxt, HT_MESSAGE, t_cb->message, &t_target, t_first);

    delete t_it_ref;
    delete t_it_var;

    // Free the parameter list.
    MCParameter *t_p = t_first;
    while (t_p != nullptr)
    {
        MCParameter *t_next = t_p->getnext();
        delete t_p;
        t_p = t_next;
    }

    MCWorkerParamsFree(t_cb->params, t_cb->param_count);
    MCValueRelease(t_cb->target_name);
    MCValueRelease(t_cb->message);
    delete t_cb;
}

// MCNotify entry point for worker callbacks.
//
// The s_safe_dispatch_depth guard in notify.cpp ensures this is never called
// re-entrantly from within a safe-notification dispatch (e.g. from the modal
// event loop that 'answer' runs).  Pending safe notifications — including
// subsequent worker callbacks — stay in s_safe_notifications and are delivered
// by the outer MCNotifyDispatch call once the modal dialog closes.
//
// The MCnexecutioncontexts check below is belt-and-suspenders protection
// against hypothetical other re-entrancy paths.
void MCWorkerDeliverCallback(void *p_opaque)
{
    MCWorkerCallback *t_cb = static_cast<MCWorkerCallback *>(p_opaque);

    if (MCnexecutioncontexts > 0)
    {
        // Should not normally happen thanks to the s_safe_dispatch_depth guard
        // in notify.cpp.  If it does, drop rather than risk nested modal dialogs.
        MCWorkerParamsFree(t_cb->params, t_cb->param_count);
        MCValueRelease(t_cb->target_name);
        MCValueRelease(t_cb->message);
        delete t_cb;
        return;
    }

    MCWorkerDeliverCallbackNow(t_cb);
}

// ---------------------------------------------------------------------------
// MCWorker
// ---------------------------------------------------------------------------

MCWorker::MCWorker(MCStringRef p_name)
    : next(nullptr),
      m_stack(nullptr),
      m_current_caller(nullptr),
      m_queue_head(nullptr),
      m_queue_tail(nullptr),
      m_stop(false)
{
    m_name = MCValueRetain(p_name);
    // std::mutex and std::condition_variable are default-initialised — no
    // explicit init calls needed.
}

MCWorker::~MCWorker()
{
    MCValueRelease(m_name);
    // std::mutex / std::condition_variable clean themselves up via RAII.

    // Drain any messages that were queued but never delivered.
    MCWorkerMessage *t_msg = m_queue_head;
    while (t_msg != nullptr)
    {
        MCWorkerMessage *t_next = t_msg->next;
        MCWorkerMessageFree(t_msg);
        t_msg = t_next;
    }
}

void MCWorker::PostMessage(MCWorkerMessage *p_msg)
{
    {
        std::lock_guard<std::mutex> t_lock(m_queue_mutex);
        if (m_queue_tail == nullptr)
            m_queue_head = m_queue_tail = p_msg;
        else
        {
            m_queue_tail->next = p_msg;
            m_queue_tail       = p_msg;
        }
    }
    m_queue_cond.notify_one();
}

bool MCWorker::StartThread()
{
    if (m_thread.joinable())
        return true;

    // With -fno-exceptions a failed std::thread construction calls
    // std::terminate(), so there is no catchable failure path here.
    m_thread = std::thread(ThreadEntry, this);
    return true;
}

void MCWorker::StopThread()
{
    if (!m_thread.joinable())
        return;

    {
        std::lock_guard<std::mutex> t_lock(m_queue_mutex);
        m_stop = true;
    }
    m_queue_cond.notify_one();

    m_thread.join();
}

/*static*/ void MCWorker::ThreadEntry(MCWorker *p_worker)
{
    p_worker->RunLoop();
}

void MCWorker::RunLoop()
{
    // -----------------------------------------------------------------------
    // Bootstrap thread-local engine state for this worker.
    //
    // Each of these is thread_local (declared in globals.h), so we are
    // setting up our own private copies — the main thread's copies are
    // completely unaffected.
    // -----------------------------------------------------------------------

    // Seed the C-stack bottom for this thread.  MCstackbottom is now
    // thread_local; without this it would hold the main thread's stack
    // address, making the recursion-depth check in MCObject::exechandler
    // always fire and return ES_ERROR immediately.
    char t_stack_sentinel;
    MCstackbottom = &t_stack_sentinel;

    MCdefaultstackptr = m_stack->GetHandle();
    MCerrorptr        = nil;
    MCtargetptr       = nullptr;
    MCexitall         = False;
    MCabortscript     = False;
    MCresultmode      = kMCExecResultModeReturn;

    // Create a fresh result variable for this thread using the same factory
    // the main thread uses (MCVariable's default constructor is protected).
    /* UNCHECKED */ MCVariable::createwithname(MCNAME("MCresult"), MCresult);

    // Create the worker thread's 'it' variable.  MCEngineExecDispatch writes
    // to 'it' after every dispatch; without an enclosing handler we need to
    // give the context a dedicated variable to absorb those writes.
    // GetIt() returns MCVarref*, so wrap the variable in a simple MCVarref.
    MCVariable *t_it_var = nullptr;
    /* UNCHECKED */ MCVariable::createwithname(MCN_it, t_it_var);
    MCVarref *t_it_ref = new (nothrow) MCVarref(t_it_var);

    // Register as the current worker on this thread.
    tls_current_worker = this;

    for (;;)
    {
        // -----------------------------------------------------------------------
        // Wait for the next message.
        // -----------------------------------------------------------------------
        MCWorkerMessage *t_msg;
        {
            std::unique_lock<std::mutex> t_lock(m_queue_mutex);
            m_queue_cond.wait(t_lock,
                [this]{ return m_queue_head != nullptr || m_stop; });

            if (m_stop && m_queue_head == nullptr)
                break;

            t_msg        = m_queue_head;
            m_queue_head = t_msg->next;
            if (m_queue_head == nullptr)
                m_queue_tail = nullptr;
        }

        // -----------------------------------------------------------------------
        // Expose the caller stack so 'dispatch to caller' can find it.
        // -----------------------------------------------------------------------
        m_current_caller = t_msg->caller_stack;

        // -----------------------------------------------------------------------
        // Build an MCParameter list from the pre-evaluated marshalled values.
        // -----------------------------------------------------------------------
        MCParameter *t_first = nullptr;
        MCParameter *t_last  = nullptr;
        for (uint32_t i = 0; i < t_msg->param_count; i++)
        {
            MCParameter *t_p = new (nothrow) MCParameter;
            if (t_p == nullptr)
                break;
            // setvalueref_argument retains the value internally; do not pre-retain.
            t_p->setvalueref_argument(t_msg->params[i].value);
            if (t_first == nullptr)
                t_first = t_last = t_p;
            else
            {
                t_last->setnext(t_p);
                t_last = t_p;
            }
        }

        // -----------------------------------------------------------------------
        // Dispatch the message into the worker's backing stack.
        // -----------------------------------------------------------------------
        MCObjectPtr t_target;
        t_target.object  = m_stack;
        t_target.part_id = 0;

        // Clear the result before each dispatch so stale values don't leak.
        MCresult->clear();
        MCexitall     = False;
        MCabortscript = False;

        MCExecContext t_ctxt;
        t_ctxt.SetWorkerIt(t_it_ref);
        MCEngineExecDispatch(t_ctxt, HT_MESSAGE, t_msg->message,
                             &t_target, t_first);

        // Free the parameter list.
        MCParameter *t_p = t_first;
        while (t_p != nullptr)
        {
            MCParameter *t_nxt = t_p->getnext();
            delete t_p;
            t_p = t_nxt;
        }

        m_current_caller = nullptr;
        MCWorkerMessageFree(t_msg);
    }

    // -----------------------------------------------------------------------
    // Tear down this thread's engine state.
    // -----------------------------------------------------------------------
    tls_current_worker = nullptr;
    delete t_it_ref;
    delete t_it_var;
    delete MCresult;
    MCresult          = nullptr;
    MCdefaultstackptr = nil;

    // Return any values still cached in this thread's pool back to the heap.
    // This must happen after all ValueRef-holding objects are destroyed above
    // so that no pool slot is re-allocated during teardown.
    __MCValueDrainPoolForThread();
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

static MCWorker *s_worker_list = nullptr;

void MCWorkerRegistryInitialize()
{
    s_worker_list = nullptr;
}

void MCWorkerRegistryFinalize()
{
    MCWorker *t_worker = s_worker_list;
    while (t_worker != nullptr)
    {
        MCWorker *t_next = t_worker->next;

        // Stop the background thread before destroying the stack so the
        // thread is not mid-dispatch when we pull the stack away.
        t_worker->StopThread();

        if (t_worker->GetStack() != nullptr)
            MCdispatcher->destroystack(t_worker->GetStack(), True);

        delete t_worker;
        t_worker = t_next;
    }
    s_worker_list = nullptr;
}

MCWorker *MCWorkerFind(MCStringRef p_name)
{
    for (MCWorker *t_w = s_worker_list; t_w != nullptr; t_w = t_w->next)
        if (MCStringIsEqualTo(t_w->GetName(), p_name, kMCStringOptionCompareCaseless))
            return t_w;
    return nullptr;
}

void MCWorkerAdd(MCWorker *p_worker)
{
    p_worker->next = s_worker_list;
    s_worker_list  = p_worker;
}

void MCWorkerRemove(MCStringRef p_name)
{
    MCWorker **t_prev = &s_worker_list;
    for (MCWorker *t_w = s_worker_list; t_w != nullptr; t_w = t_w->next)
    {
        if (MCStringIsEqualTo(t_w->GetName(), p_name, kMCStringOptionCompareCaseless))
        {
            *t_prev = t_w->next;
            delete t_w;
            return;
        }
        t_prev = &t_w->next;
    }
}

MCWorker *MCWorkerGetCurrent()
{
    return tls_current_worker;
}

// ---------------------------------------------------------------------------
// Exec helpers
// ---------------------------------------------------------------------------

void MCWorkerExecCreate(MCExecContext &ctxt,
                        MCStringRef    p_name,
                        MCStringRef    p_script_file)
{
    if (MCWorkerFind(p_name) != nullptr)
    {
        ctxt.UserThrow(MCSTR("create worker: a worker with that name already exists"));
        return;
    }

    // ── Choose the right stack-creation path ─────────────────────────────────
    //
    // If p_script_file ends in ".hxtlib", delegate entirely to the same loader
    // path used by "start using" / the stack-open machinery.  That path reads
    // the binary container, validates the magic, and calls
    // MCStack::setascompiledlib_from_astn() to reconstruct hlist from the ASTN
    // section — exactly what is needed here.
    //
    // For any other file (or no file) we follow the original plain-text-script
    // path: create a blank script-only stack and optionally load a .livecodescript
    // source file into it.

    MCStack *t_stack = nullptr;

    bool t_is_hxtlib = false;
    if (p_script_file != nullptr && !MCStringIsEmpty(p_script_file))
    {
        // Case-insensitive suffix check for ".hxtlib".
        t_is_hxtlib = MCStringEndsWith(p_script_file, MCSTR(".hxtlib"),
                                        kMCStringOptionCompareCaseless);
    }

    if (t_is_hxtlib)
    {
        // ── .hxtlib path ─────────────────────────────────────────────────────
        // trytoreadhxtlibstack creates the MCStack, populates its hlist from
        // the ASTN section, and sets the compiled-lib / script-only flags.
        const char *t_err = nullptr;
        IO_stat t_io = MCdispatcher->trytoreadhxtlibstack(
                            p_script_file, MCdispatcher->gethome(),
                            t_stack, t_err);

        if (t_io == IO_ERROR || t_stack == nullptr)
        {
            if (t_err != nullptr)
            {
                MCStringRef t_err_str = nullptr;
                if (MCStringCreateWithCString(t_err, t_err_str))
                {
                    ctxt.UserThrow(t_err_str);
                    MCValueRelease(t_err_str);
                }
                else
                    ctxt.UserThrow(MCSTR("create worker: failed to load .hxtlib file"));
            }
            else
            {
                ctxt.UserThrow(MCSTR("create worker: failed to load .hxtlib file"));
            }
            return;
        }

        // trytoreadhxtlibstack does NOT append to the dispatcher's stack list
        // (that is done by the caller in the normal open-stack path), so we do
        // it here ourselves — same as the text-script path below.
        MCdispatcher->appendstack(t_stack);
        t_stack->setflag(False, F_VISIBLE);
    }
    else
    {
        // ── Plain text-script path ────────────────────────────────────────────
        MCStackSecurityCreateStack(t_stack);
        if (t_stack == nullptr)
        {
            ctxt.UserThrow(MCSTR("create worker: failed to create backing stack"));
            return;
        }

        MCdispatcher->appendstack(t_stack);
        t_stack->setparent(MCdispatcher->gethome());
        t_stack->setflag(False, F_VISIBLE);
        t_stack->setasscriptonly(kMCEmptyString);

        if (p_script_file != nullptr && !MCStringIsEmpty(p_script_file))
        {
            MCAutoStringRef t_script;
            bool t_loaded = MCS_loadtextfile(p_script_file, &t_script);
            if (t_loaded)
                t_stack->setstringprop(ctxt, 0, P_SCRIPT, False, *t_script);
        }
    }

    // ── Name the backing stack ────────────────────────────────────────────────
    MCAutoStringRef t_stack_name;
    if (!MCStringFormat(&t_stack_name, "__worker_%@", p_name))
    {
        MCdispatcher->destroystack(t_stack, True);
        ctxt.LegacyThrow(EE_NO_MEMORY);
        return;
    }
    t_stack->setstringprop(ctxt, 0, P_NAME, False, *t_stack_name);

    // ── Start the worker thread ───────────────────────────────────────────────
    MCWorker *t_worker = new (nothrow) MCWorker(p_name);
    if (t_worker == nullptr)
    {
        MCdispatcher->destroystack(t_stack, True);
        ctxt.LegacyThrow(EE_NO_MEMORY);
        return;
    }
    t_worker->SetStack(t_stack);

    if (!t_worker->StartThread())
    {
        MCdispatcher->destroystack(t_stack, True);
        delete t_worker;
        ctxt.UserThrow(MCSTR("create worker: failed to start worker thread"));
        return;
    }

    MCWorkerAdd(t_worker);
    ctxt.SetItToValue(p_name);
}

void MCWorkerExecDispatch(MCExecContext &ctxt,
                          MCStringRef    p_worker_name,
                          MCNameRef      p_message,
                          bool           p_is_function,
                          MCParameter   *p_params)
{
    MCWorker *t_worker = MCWorkerFind(p_worker_name);
    if (t_worker == nullptr)
    {
        ctxt.UserThrow(MCSTR("dispatch to worker: named worker not found"));
        return;
    }

    // Evaluate and marshal all parameters on the calling (main) thread before
    // handing off to the worker thread.
    MCWorkerParam *t_params  = nullptr;
    uint32_t       t_count   = 0;
    if (!MCWorkerMarshalParams(ctxt, p_params, t_params, t_count))
        return; // ctxt already has the error

    // Capture the calling stack so the worker can dispatch back to us.
    MCStack *t_caller = MCdefaultstackptr;

    MCWorkerMessage *t_msg = MCWorkerMessageNew(p_message, t_params, t_count,
                                                 t_caller);
    if (t_msg == nullptr)
    {
        MCWorkerParamsFree(t_params, t_count);
        ctxt.LegacyThrow(EE_NO_MEMORY);
        return;
    }

    // Post to the worker's queue and return immediately — non-blocking.
    t_worker->PostMessage(t_msg);
}

void MCWorkerExecDispatchToCaller(MCExecContext &ctxt,
                                  MCNameRef      p_message,
                                  bool           p_is_function,
                                  MCParameter   *p_params)
{
    // Must be executing inside a worker handler.
    MCWorker *t_worker = tls_current_worker;
    if (t_worker == nullptr || t_worker->GetCurrentCaller() == nullptr)
    {
        ctxt.UserThrow(MCSTR("dispatch to caller: not currently executing inside a worker"));
        return;
    }

    // Evaluate and marshal parameters on the worker thread.
    MCWorkerParam *t_params = nullptr;
    uint32_t       t_count  = 0;
    if (!MCWorkerMarshalParams(ctxt, p_params, t_params, t_count))
        return;

    MCWorkerCallback *t_cb = new (nothrow) MCWorkerCallback;
    if (t_cb == nullptr)
    {
        MCWorkerParamsFree(t_params, t_count);
        ctxt.LegacyThrow(EE_NO_MEMORY);
        return;
    }
    // Capture the caller's name for use at delivery time (on the main thread).
    // We store a name rather than a raw pointer so MCdispatcher->findstackname()
    // can verify the stack still exists — findstackid() fails for stacks whose
    // obj_id is 0 (not explicitly assigned).
    t_cb->target_name = MCValueRetain(t_worker->GetCurrentCaller()->getname());
    t_cb->message     = MCValueRetain(p_message);
    t_cb->params      = t_params;
    t_cb->param_count = t_count;

    // Queue the callback for delivery on the main thread.  safe=true means
    // it will be dispatched at the next script-safe point.
    MCNotifyPush(MCWorkerDeliverCallback, t_cb, false, true);
    MCNotifyPing(false);
}

void MCWorkerExecDestroy(MCExecContext &ctxt,
                         MCStringRef    p_name)
{
    MCWorker *t_worker = MCWorkerFind(p_name);
    if (t_worker == nullptr)
    {
        ctxt.UserThrow(MCSTR("destroy worker: named worker not found"));
        return;
    }

    // Stop the thread first so it is not mid-dispatch when we destroy the
    // backing stack.
    t_worker->StopThread();

    MCStack *t_stack = t_worker->GetStack();
    if (t_stack != nullptr)
        MCdispatcher->destroystack(t_stack, True);

    MCWorkerRemove(p_name);
}
