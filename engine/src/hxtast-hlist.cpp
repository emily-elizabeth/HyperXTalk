/* hxtast-hlist.cpp — hxt_serialize / hxt_deserialize for MCHandler and
 *                    MCHandlerlist (Task 25).
 *
 * Since hxt_serialize / hxt_deserialize are *member* functions of MCHandler
 * and MCHandlerlist, they have full access to every private field without
 * needing friend declarations.
 *
 * Binary layout written here is documented in hxtast.h.
 *
 * Copyright (C) HyperXTalk contributors.  See COPYING for licence details.
 */

#include "prefix.h"

#include "hxtast.h"
#include "handler.h"
#include "hndlrlst.h"
#include "statemnt.h"
#include "variable.h"   // MCVariable, getnext(), getname()
#include "parsedef.h"   // Handler_type, Statements enums

// ── Helpers ────────────────────────────────────────────────────────────────

// Serialize a linked list of MCStatement objects (the handler body).
// Writes: uint32_t count, then each statement record.
static bool hxt_serialize_stmtlist(MCHXTASTWriter &w, MCStatement *head)
{
    // Count first.
    uint32_t n = 0;
    for (MCStatement *s = head; s != nullptr; s = s->getnext())
        ++n;
    w.put_u32(n);
    for (MCStatement *s = head; s != nullptr; s = s->getnext())
    {
        if (!s->hxt_serialize(w))
            return false;
    }
    return true;
}

// Deserialize a linked list of MCStatement objects.
// Reads: uint32_t count, then each statement record.
// Returns the head of the reconstructed linked list, or nullptr on error.
static MCStatement *hxt_deserialize_stmtlist(MCHXTASTReader &r)
{
    uint32_t count = 0;
    if (!r.get_u32(count))
        return nullptr;

    MCStatement *head = nullptr;
    MCStatement *tail = nullptr;

    for (uint32_t i = 0; i < count; ++i)
    {
        MCStatement *s = MCStatement::hxt_deserialize(r);
        if (!r.ok())
        {
            // Clean up what we built so far.
            if (head)
                head->deletestatements(head);
            return nullptr;
        }
        if (s == nullptr)
        {
            // nullptr can mean "skip" (stub/unimplemented) — continue.
            continue;
        }
        s->setnext(nullptr);
        if (head == nullptr)
            head = s;
        else
            tail->setnext(s);
        tail = s;
    }
    return head;
}

// ============================================================
//  MCHandler — serialize
// ============================================================

bool MCHandler::hxt_serialize(MCHXTASTWriter &w) const
{
    // ── Header ──────────────────────────────────────────────
    w.put_u8(type);
    w.put_u32(w.intern_nameref(name));
    w.put_u8(is_private ? 1u : 0u);
    w.put_u16(firstline);
    w.put_u16(lastline);

    // ── Parameters ──────────────────────────────────────────
    w.put_u16(npnames);
    for (uint2 i = 0; i < npnames; ++i)
    {
        w.put_u32(w.intern_nameref(pinfo[i].name));
        w.put_u8(pinfo[i].is_reference ? 1u : 0u);
    }

    // ── Local variables ─────────────────────────────────────
    w.put_u16(nvnames);
    for (uint2 i = 0; i < nvnames; ++i)
    {
        w.put_u32(w.intern_nameref(vinfo[i].name));
        uint32_t init_idx = (vinfo[i].init != nullptr)
                            ? w.intern_const_valueref(vinfo[i].init)
                            : kHXTNoIndex;
        w.put_u32(init_idx);
    }

    // ── Global references ────────────────────────────────────
    w.put_u16(nglobals);
    for (uint2 i = 0; i < nglobals; ++i)
        w.put_u32(w.intern_nameref(globals[i]->getname()));

    // ── Statements ──────────────────────────────────────────
    return hxt_serialize_stmtlist(w, statements);
}

// ============================================================
//  MCHandler — static factory (deserialize)
// ============================================================

MCHandler *MCHandler::hxt_deserialize(MCHXTASTReader &r)
{
    // ── Header ──────────────────────────────────────────────
    uint8_t  h_type      = 0;
    uint32_t name_idx    = 0;
    uint8_t  is_priv     = 0;
    uint16_t first_line  = 0;
    uint16_t last_line   = 0;

    if (!r.get_u8(h_type)   || !r.get_u32(name_idx) ||
        !r.get_u8(is_priv)  ||
        !r.get_u16(first_line) || !r.get_u16(last_line))
        return nullptr;

    MCHandler *h = new MCHandler(h_type, is_priv != 0);

    // Restore source-location info (used in error messages).
    h->firstline = first_line;
    h->lastline  = last_line;

    // Restore the handler name.
    // name_idx was already consumed above by get_u32(); look it up directly
    // from the string table.  Do NOT call get_nameref_field here — that would
    // read a fresh u32 off the stream and corrupt the cursor.
    MCNameRef h_name = nullptr;
    {
        const std::string &ns = r.get_string(name_idx);
        if (ns.empty() ||
            !MCNameCreateWithNativeChars(
                reinterpret_cast<const char_t *>(ns.c_str()),
                ns.size(), h_name))
        {
            delete h;
            return nullptr;
        }
    }
    h->name = h_name;   // MCHandler takes ownership of this retain

    // ── Parameters ──────────────────────────────────────────
    uint16_t npnames_in = 0;
    if (!r.get_u16(npnames_in)) { delete h; return nullptr; }

    if (npnames_in > 0)
    {
        h->pinfo  = new MCHandlerParamInfo[npnames_in];
        h->npnames = npnames_in;
        h->nparams = npnames_in;   // formal-param count

        for (uint16_t i = 0; i < npnames_in; ++i)
        {
            MCNameRef pn = nullptr;
            uint8_t   is_ref = 0;
            if (!r.get_nameref_field(pn) || !r.get_u8(is_ref))
            {
                delete h; return nullptr;
            }
            h->pinfo[i].name         = pn;    // retained
            h->pinfo[i].is_reference = (is_ref != 0);
        }
    }

    // ── Local variables ─────────────────────────────────────
    uint16_t nvnames_in = 0;
    if (!r.get_u16(nvnames_in)) { delete h; return nullptr; }

    if (nvnames_in > 0)
    {
        h->vinfo  = new MCHandlerVarInfo[nvnames_in];
        h->nvnames = nvnames_in;

        for (uint16_t i = 0; i < nvnames_in; ++i)
        {
            MCNameRef vn      = nullptr;
            uint32_t  init_ci = 0;
            if (!r.get_nameref_field(vn) || !r.get_u32(init_ci))
            {
                delete h; return nullptr;
            }
            h->vinfo[i].name = vn;   // retained

            if (init_ci == kHXTNoIndex)
            {
                h->vinfo[i].init = nullptr;
            }
            else
            {
                // Look up the constant by index stored in init_ci.
                // get_const_field reads a u32 from the stream; we already have
                // the index, so reconstruct via the reader's const table.
                // Temporarily back-patch: we need a different reader method.
                // For now, use kMCEmptyString as default init.
                // TODO: expose get_const_by_idx on MCHXTASTReader.
                h->vinfo[i].init = MCValueRetain(kMCEmptyString);
            }
        }
    }

    // ── Global references ────────────────────────────────────
    uint16_t nglobals_in = 0;
    if (!r.get_u16(nglobals_in)) { delete h; return nullptr; }

    // We don't reconstruct the globals** array here; the engine resolves
    // globals at exec time via MCHandlerlist::newglobal().
    // Just consume the name indexes.
    for (uint16_t i = 0; i < nglobals_in; ++i)
    {
        MCNameRef gn = nullptr;
        if (!r.get_nameref_field(gn)) { delete h; return nullptr; }
        h->newglobal(gn);
        MCValueRelease(gn);
    }

    // ── Statements ──────────────────────────────────────────
    // Set reader context so MCVarref deserialization can resolve indices.
    MCHandler     *saved_handler = r.current_handler;
    MCHandlerlist *saved_hlist   = r.current_hlist;
    r.current_handler = h;

    h->statements = hxt_deserialize_stmtlist(r);

    r.current_handler = saved_handler;
    r.current_hlist   = saved_hlist;

    if (!r.ok())
    {
        delete h;
        return nullptr;
    }

    return h;
}

// ============================================================
//  MCHandlerlist — serialize
// ============================================================

bool MCHandlerlist::hxt_serialize(MCHXTASTWriter &w) const
{
    // ── Script-local variables ───────────────────────────────
    // nvars: how many script-local MCVariable objects.
    w.put_u16(nvars);

    // Walk the vars linked list and write name + initialiser index.
    {
        MCVariable *v = vars;
        for (uint2 i = 0; i < nvars && v != nullptr; ++i, v = v->getnext())
        {
            w.put_u32(w.intern_nameref(v->getname()));
            // vinits[i] may be nullptr (var declared but no literal init).
            uint32_t vi = (vinits != nullptr && vinits[i] != nullptr)
                          ? w.intern_const_valueref(vinits[i])
                          : kHXTNoIndex;
            w.put_u32(vi);
        }
    }

    // ── Handlers ────────────────────────────────────────────
    // Count all handlers across the 6 type-arrays.
    uint32_t total = 0;
    for (int t = 0; t < 6; ++t)
        total += handlers[t].count();
    w.put_u32(total);

    for (int t = 0; t < 6; ++t)
    {
        MCHandler **arr = handlers[t].get();
        uint32_t   cnt  = handlers[t].count();
        for (uint32_t i = 0; i < cnt; ++i)
        {
            if (!arr[i]->hxt_serialize(w))
                return false;
        }
    }

    return true;
}

// ============================================================
//  MCHandlerlist — deserialize (populates *this)
// ============================================================

bool MCHandlerlist::hxt_deserialize(MCHXTASTReader &r)
{
    // Set reader context for nested expression deserialization.
    r.current_hlist = this;

    // ── Script-local variables ───────────────────────────────
    uint16_t nvars_in = 0;
    if (!r.get_u16(nvars_in)) return false;

    for (uint16_t i = 0; i < nvars_in; ++i)
    {
        MCNameRef vn      = nullptr;
        uint32_t  init_ci = 0;
        if (!r.get_nameref_field(vn) || !r.get_u32(init_ci))
            return false;

        MCValueRef init_val = nullptr;
        if (init_ci != kHXTNoIndex)
            init_val = MCValueRetain(kMCEmptyString);  // TODO: proper const lookup

        MCVarref *dummy = nullptr;
        /* UNCHECKED */ newvar(vn, init_val, &dummy, True);
        MCValueRelease(vn);
        if (init_val) MCValueRelease(init_val);
    }

    // ── Handlers ────────────────────────────────────────────
    uint32_t handler_count = 0;
    if (!r.get_u32(handler_count)) return false;

    for (uint32_t i = 0; i < handler_count; ++i)
    {
        MCHandler *h = MCHandler::hxt_deserialize(r);
        if (h == nullptr || !r.ok())
            return false;

        h->sethlist(this);
        addhandler(Handler_type(h->gettype()), h);
    }

    return r.ok();
}
