/* hxt-cmds.cpp — HyperXTalk engine command implementations
 *
 * Currently contains:
 *   MCExecSaveStackAsLibrary  —  exec body for "save <stack> as library [to <path>]"
 *
 * Copyright (C) HyperXTalk contributors.  See COPYING for licence details.
 */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "exec.h"
#include "stack.h"
#include "dispatch.h"
#include "object.h"
#include "handler.h"
#include "hndlrlst.h"
#include "mcerror.h"
#include "globals.h"

#include "hxtast.h"
#include "hxtlib.h"

#include "revbuild.h"   // MC_BUILD_ENGINE_*_VERSION

// ============================================================
//  MCExecSaveStackAsLibrary
// ============================================================
//
// Called by MCSave::exec_ctxt() when the "as library" flag is set.
//
// Syntax: save <stack> as library [to <path>]
//
// If <path> is omitted (kMCEmptyString), the result is set to the
// output path chosen by the caller (the IDE handler supplies it from
// the ask-file dialog and always passes an explicit path, so the
// empty case simply errors).
//
// On success: the result is cleared.
// On failure: the result holds a human-readable error string.

void MCExecSaveStackAsLibrary(MCExecContext &ctxt, MCStack *p_stack,
                              MCStringRef p_path)
{
    // ── Validate path ─────────────────────────────────────────────────────
    if (MCStringIsEmpty(p_path))
    {
        ctxt.SetTheResultToCString("save as library: no output path specified");
        return;
    }

    char *t_path_cstr = nullptr;
    if (!MCStringConvertToCString(p_path, t_path_cstr))
    {
        ctxt.SetTheResultToCString("save as library: could not convert path");
        return;
    }
    std::string t_output_path(t_path_cstr);
    MCMemoryDeleteArray(t_path_cstr);

    // Ensure .hxtlib extension.
    if (t_output_path.size() < 7 ||
        t_output_path.substr(t_output_path.size() - 7) != ".hxtlib")
    {
        t_output_path += ".hxtlib";
    }

    // ── Build hxtlib::Document ────────────────────────────────────────────

    hxtlib::Document t_doc;

    // Engine version stamp.
    t_doc.eng_major = MC_BUILD_ENGINE_MAJOR_VERSION;
    t_doc.eng_minor = MC_BUILD_ENGINE_MINOR_VERSION;
    t_doc.eng_patch = MC_BUILD_ENGINE_POINT_VERSION;

    // META: pull display name from the stack.
    {
        char *t_name_cstr = nullptr;
        if (MCStringConvertToCString(MCNameGetString(p_stack->getname()), t_name_cstr))
        {
            t_doc.meta.name = t_name_cstr;
            MCMemoryDeleteArray(t_name_cstr);
        }
    }
    // Leave version/author/identifier/min_engine empty — the IDE handler
    // can set custom properties on the stack and pass them via a future
    // options parameter if needed.

    // ── ASTN: serialize the handler list ─────────────────────────────────

    const MCHandlerlist *t_hlist = p_stack->gethandlers();
    if (t_hlist != nullptr)
    {
        if (!MCDispatch::hxtlib_serialize_hlist(t_hlist, t_doc))
        {
            // Serialization failed — warn but continue so SRCS still works.
            // The loader will fall back to re-parsing SRCS.
            t_doc.astn_bytes.clear();
        }
    }

    // SRCS is intentionally omitted.  The source script is never written
    // into a library produced by the IDE — that is the whole point of the
    // compiled format.  The ASTN binary section is the sole representation;
    // if serialization above failed, astn_bytes is empty and the library
    // will load with no handlers (rather than exposing source text).

    // ── Write ──────────────────────────────────────────────────────────────

    hxtlib::Error t_err = hxtlib::write(t_doc, t_output_path);
    if (t_err != hxtlib::Error::Ok)
    {
        ctxt.SetTheResultToCString(hxtlib::strerror(t_err));
        return;
    }

    // Success — clear the result (HyperXTalk convention: empty = ok).
    ctxt.SetTheResultToEmpty();
}
