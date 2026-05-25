/* hxtast-stmts.cpp — hxt_serialize / hxt_deserialize_body for MCStatement
 *                    subclasses.
 *
 * Tasks 26 (flow control), 27 (variable / arithmetic ops),
 *       28 (messaging / dispatch).
 *
 * Protocol
 * ────────
 * Each hxt_serialize() override MUST call w.begin_stmt(S_xxx, line, pos)
 * first, then write type-specific fields.  The static factory in
 * hxtast-base.cpp reads the common header (stmt_type, line, pos) and
 * calls hxt_deserialize_body(), which reads only the type-specific data.
 *
 * For unimplemented statement types the base-class stub in hxtast-base.cpp
 * writes a 0xFFFF placeholder; the factory turns that into a no-op
 * MCStatement so the list stays contiguous.
 *
 * Copyright (C) HyperXTalk contributors.  See COPYING for licence details.
 */

#include "prefix.h"

#include "hxtast.h"
#include "keywords.h"   // MCIf, MCRepeat, MCExit, MCPass, MCSwitch, MCTry, MCThrowKeyword, MCComref, MCFuncref
#include "cmds.h"       // MCGet, MCPut, MCReturn, MCSet, MCAdd/Subtract/Multiply/Divide, MCDo, MCMessage, MCDispatchCmd
#include "statemnt.h"
#include "express.h"
#include "chunk.h"
#include "property.h"   // MCProperty — must be included before any cast to/from MCProperty *
#include "variable.h"   // MCVarref
#include "param.h"      // MCParameter
#include "parsedef.h"
#include "newobj.h"     // MCN_new_function (for deserialization of MCFuncref)
#include "handler.h"    // MCHandref

// ── Shared helpers ─────────────────────────────────────────────────────────

// Free function declared in hxtast-base.cpp.
extern bool hxt_serialize_expr(MCHXTASTWriter &w, const MCExpression *expr);

// Serialize a linked MCStatement list preceded by a uint32_t count.
static bool write_stmtlist(MCHXTASTWriter &w, MCStatement *head)
{
    uint32_t n = 0;
    for (MCStatement *s = head; s != nullptr; s = s->getnext())
        ++n;
    w.put_u32(n);
    for (MCStatement *s = head; s != nullptr; s = s->getnext())
        if (!s->hxt_serialize(w))
            return false;
    return true;
}

// Deserialize a linked MCStatement list (count-prefixed).
static MCStatement *read_stmtlist(MCHXTASTReader &r)
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
            if (head) head->deletestatements(head);
            return nullptr;
        }
        if (!s) continue;   // skip / placeholder
        s->setnext(nullptr);
        if (!head) head = s;
        else       tail->setnext(s);
        tail = s;
    }
    return head;
}

// Serialize an MCParameter linked list (count-prefixed).
static bool write_params(MCHXTASTWriter &w, MCParameter *head)
{
    uint16_t n = 0;
    for (MCParameter *p = head; p != nullptr; p = p->getnext())
        ++n;
    w.put_u16(n);
    for (MCParameter *p = head; p != nullptr; p = p->getnext())
        if (!hxt_serialize_expr(w, p->getexp()))
            return false;
    return true;
}

// Deserialize an MCParameter linked list.
static MCParameter *read_params(MCHXTASTReader &r)
{
    uint16_t count = 0;
    if (!r.get_u16(count))
        return nullptr;

    MCParameter *head = nullptr;
    MCParameter *tail = nullptr;

    for (uint16_t i = 0; i < count; ++i)
    {
        MCExpression *e = MCExpression::hxt_deserialize(r);
        if (!r.ok())
        {
            // clean up
            MCParameter *p = head;
            while (p) { MCParameter *nx = p->getnext(); delete p; p = nx; }
            return nullptr;
        }
        MCParameter *param = new MCParameter;
        param->setexp(e);
        param->setnext(nullptr);
        if (!head) head = param;
        else       tail->setnext(param);
        tail = param;
    }
    return head;
}

// Serialize an MCChunk* (which may be null).
// Writes: 0x00 if null, 0x01 + chunk record if non-null.
static bool write_chunk(MCHXTASTWriter &w, const MCChunk *c)
{
    if (c == nullptr)
    {
        w.put_u8(0);
        return true;
    }
    w.put_u8(1);
    return c->hxt_serialize(w);
}

// Deserialize an optional MCChunk.
// Returns nullptr if absent (flag byte == 0) or on error.
static MCChunk *read_chunk(MCHXTASTReader &r)
{
    uint8_t flag = 0;
    if (!r.get_u8(flag))
        return nullptr;
    if (flag == 0)
        return nullptr;

    // A chunk expression record starts with the begin_expr header.
    // Use MCExpression::hxt_deserialize and cast (MCChunk IS MCExpression).
    // static_cast is safe: the serializer always writes kHXTExpr_Chunk here.
    MCExpression *e = MCExpression::hxt_deserialize(r);
    if (!r.ok())
        return nullptr;
    return static_cast<MCChunk *>(e);
}

// Serialize an MCVarref* (as an expression, possibly null).
static bool write_varref(MCHXTASTWriter &w, const MCVarref *vr)
{
    return hxt_serialize_expr(w, vr);
}

// Deserialize an MCVarref (as an expression).
// static_cast is safe: the serializer always writes kHXTExpr_Varref here.
static MCVarref *read_varref(MCHXTASTReader &r)
{
    MCExpression *e = MCExpression::hxt_deserialize(r);
    if (!r.ok() || e == nullptr)
        return nullptr;
    return static_cast<MCVarref *>(e);
}

// ============================================================
//  Task 26 — Flow control
// ============================================================

// ── MCIf ──────────────────────────────────────────────────────────────────

bool MCIf::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_IF, line, pos);
    if (!hxt_serialize_expr(w, cond))           return false;
    if (!write_stmtlist(w, thenstatements))     return false;
    if (!write_stmtlist(w, elsestatements))     return false;
    return true;
}

bool MCIf::hxt_deserialize_body(MCHXTASTReader &r)
{
    cond           = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;
    thenstatements = read_stmtlist(r);
    if (!r.ok()) return false;
    elsestatements = read_stmtlist(r);
    return r.ok();
}

// ── MCRepeat ──────────────────────────────────────────────────────────────

bool MCRepeat::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_REPEAT, line, pos);
    w.put_u8(uint8_t(form));
    w.put_u8(uint8_t(each));
    w.put_f64(stepval);
    if (!hxt_serialize_expr(w, startcond))  return false;
    if (!hxt_serialize_expr(w, endcond))    return false;
    if (!write_varref(w, loopvar))          return false;
    if (!hxt_serialize_expr(w, step))       return false;
    if (!write_stmtlist(w, statements))     return false;
    return true;
}

bool MCRepeat::hxt_deserialize_body(MCHXTASTReader &r)
{
    uint8_t form_b = 0, each_b = 0;
    if (!r.get_u8(form_b) || !r.get_u8(each_b)) return false;
    form = Repeat_form(form_b);
    each = File_unit(each_b);

    if (!r.get_f64(stepval)) return false;

    startcond  = MCExpression::hxt_deserialize(r); if (!r.ok()) return false;
    endcond    = MCExpression::hxt_deserialize(r); if (!r.ok()) return false;
    loopvar    = read_varref(r);                   if (!r.ok()) return false;
    step       = MCExpression::hxt_deserialize(r); if (!r.ok()) return false;
    statements = read_stmtlist(r);
    return r.ok();
}

// ── MCExit ────────────────────────────────────────────────────────────────

bool MCExit::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_EXIT, line, pos);
    w.put_u8(uint8_t(exit));
    return true;
}

bool MCExit::hxt_deserialize_body(MCHXTASTReader &r)
{
    uint8_t ex = 0;
    if (!r.get_u8(ex)) return false;
    exit = Exec_stat(ex);
    return true;
}

// ── MCPass ────────────────────────────────────────────────────────────────

bool MCPass::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_PASS, line, pos);
    w.put_u8(all ? 1u : 0u);
    return true;
}

bool MCPass::hxt_deserialize_body(MCHXTASTReader &r)
{
    uint8_t b = 0;
    if (!r.get_u8(b)) return false;
    all = b ? True : False;
    return true;
}

// ── MCSwitch ──────────────────────────────────────────────────────────────
//
// Format:
//   <expr>          cond
//   uint16_t        ncases
//   for each case:
//     <expr>        case expression
//     uint16_t      caseoffset (statement index of first stmt for this case)
//   int16_t         defaultcase (-1 if none)
//   <stmt-list>     all switch statements

bool MCSwitch::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_SWITCH, line, pos);
    if (!hxt_serialize_expr(w, cond)) return false;
    w.put_u16(ncases);
    for (uint16_t i = 0; i < ncases; ++i)
    {
        if (!hxt_serialize_expr(w, cases[i])) return false;
        w.put_u16(caseoffsets[i]);
    }
    w.put_u16(uint16_t(int16_t(defaultcase)));  // sign-preserving via reinterpret
    if (!write_stmtlist(w, statements)) return false;
    return true;
}

bool MCSwitch::hxt_deserialize_body(MCHXTASTReader &r)
{
    cond = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;

    uint16_t nc = 0;
    if (!r.get_u16(nc)) return false;
    ncases = nc;

    if (nc > 0)
    {
        cases       = new MCExpression*[nc];
        caseoffsets = new uint2[nc];
        for (uint16_t i = 0; i < nc; ++i)
        {
            cases[i] = MCExpression::hxt_deserialize(r);
            if (!r.ok()) return false;
            uint16_t off = 0;
            if (!r.get_u16(off)) return false;
            caseoffsets[i] = off;
        }
    }

    uint16_t dc = 0;
    if (!r.get_u16(dc)) return false;
    defaultcase = int16_t(dc);   // sign-extend

    statements = read_stmtlist(r);
    return r.ok();
}

// ── MCThrowKeyword ────────────────────────────────────────────────────────

bool MCThrowKeyword::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_THROW, line, pos);
    return hxt_serialize_expr(w, error);
}

bool MCThrowKeyword::hxt_deserialize_body(MCHXTASTReader &r)
{
    error = MCExpression::hxt_deserialize(r);
    return r.ok();
}

// ── MCTry ─────────────────────────────────────────────────────────────────

bool MCTry::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_TRY, line, pos);
    if (!write_stmtlist(w, trystatements))     return false;
    if (!write_varref(w, errorvar))            return false;
    if (!write_stmtlist(w, catchstatements))   return false;
    if (!write_stmtlist(w, finallystatements)) return false;
    return true;
}

bool MCTry::hxt_deserialize_body(MCHXTASTReader &r)
{
    trystatements     = read_stmtlist(r); if (!r.ok()) return false;
    errorvar          = read_varref(r);   if (!r.ok()) return false;
    catchstatements   = read_stmtlist(r); if (!r.ok()) return false;
    finallystatements = read_stmtlist(r);
    return r.ok();
}

// ============================================================
//  Task 27 — Variable / arithmetic ops
// ============================================================

// ── MCGet ─────────────────────────────────────────────────────────────────

bool MCGet::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_GET, line, pos);
    return hxt_serialize_expr(w, value);
}

bool MCGet::hxt_deserialize_body(MCHXTASTReader &r)
{
    value = MCExpression::hxt_deserialize(r);
    return r.ok();
}

// ── MCPut ─────────────────────────────────────────────────────────────────
//
// Format:
//   <expr>       source
//   uint8_t      prep      (Preposition_type)
//   uint8_t      is_unicode
//   uint8_t      has_dest  (0 = no destination, 1 = has MCChunk dest)
//   [chunk]      dest       (if has_dest)

bool MCPut::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_PUT, line, pos);
    if (!hxt_serialize_expr(w, source)) return false;
    w.put_u8(uint8_t(prep));
    w.put_u8(is_unicode ? 1u : 0u);
    if (!write_chunk(w, dest)) return false;
    return true;
}

bool MCPut::hxt_deserialize_body(MCHXTASTReader &r)
{
    source = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;

    uint8_t prep_b = 0, uni_b = 0;
    if (!r.get_u8(prep_b) || !r.get_u8(uni_b)) return false;
    prep       = Preposition_type(prep_b);
    is_unicode = (uni_b != 0);

    dest = read_chunk(r);
    return r.ok();
}

// ── MCReturn ──────────────────────────────────────────────────────────────
//
// Format:
//   uint8_t   kind  (MCReturn::Kind enum)
//   <expr>    source
//   <expr>    extra_source

bool MCReturn::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_RETURN, line, pos);
    w.put_u8(uint8_t(kind));
    if (!hxt_serialize_expr(w, source))       return false;
    if (!hxt_serialize_expr(w, extra_source)) return false;
    return true;
}

bool MCReturn::hxt_deserialize_body(MCHXTASTReader &r)
{
    uint8_t k = 0;
    if (!r.get_u8(k)) return false;
    kind         = Kind(k);
    source       = MCExpression::hxt_deserialize(r); if (!r.ok()) return false;
    extra_source = MCExpression::hxt_deserialize(r);
    return r.ok();
}

// ── MCSet ─────────────────────────────────────────────────────────────────
//
// Format:
//   <expr-property>  target
//   <expr>           value

bool MCSet::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_SET, line, pos);
    // MCProperty IS an MCExpression.
    if (!hxt_serialize_expr(w, target)) return false;
    if (!hxt_serialize_expr(w, value))  return false;
    return true;
}

bool MCSet::hxt_deserialize_body(MCHXTASTReader &r)
{
    MCExpression *t = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;
    // static_cast is safe: the serializer always writes kHXTExpr_Property here.
    target = static_cast<MCProperty *>(t);
    value = MCExpression::hxt_deserialize(r);
    return r.ok();
}

// ── MCAdd / MCSubtract / MCMultiply / MCDivide ────────────────────────────
//
// Common format:
//   <expr>   source
//   uint8_t  has_varref    (1 = simple variable dest, 0 = chunk dest)
//   [varref] destvar        (if has_varref)
//   [chunk]  dest           (if !has_varref)

static bool write_arith_op(MCHXTASTWriter &w, uint16_t stmt_type,
                            uint16_t sline, uint16_t spos,
                            const MCExpression *source,
                            const MCVarref *destvar,
                            const MCChunk  *dest)
{
    w.begin_stmt(stmt_type, sline, spos);
    if (!hxt_serialize_expr(w, source)) return false;
    if (destvar != nullptr)
    {
        w.put_u8(1);
        if (!hxt_serialize_expr(w, destvar)) return false;
    }
    else
    {
        w.put_u8(0);
        if (!write_chunk(w, dest)) return false;
    }
    return true;
}

static bool read_arith_op(MCHXTASTReader &r,
                           MCExpression *&source_out,
                           MCVarref    *&destvar_out,
                           MCChunk     *&dest_out)
{
    source_out  = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;

    uint8_t has_varref = 0;
    if (!r.get_u8(has_varref)) return false;

    if (has_varref)
    {
        destvar_out = read_varref(r);
        dest_out    = nullptr;
    }
    else
    {
        destvar_out = nullptr;
        dest_out    = read_chunk(r);
    }
    return r.ok();
}

bool MCAdd::hxt_serialize(MCHXTASTWriter &w) const
{
    return write_arith_op(w, S_ADD, line, pos, source, destvar, dest);
}
bool MCAdd::hxt_deserialize_body(MCHXTASTReader &r)
{
    return read_arith_op(r, source, destvar, dest);
}

bool MCSubtract::hxt_serialize(MCHXTASTWriter &w) const
{
    return write_arith_op(w, S_SUBTRACT, line, pos, source, destvar, dest);
}
bool MCSubtract::hxt_deserialize_body(MCHXTASTReader &r)
{
    return read_arith_op(r, source, destvar, dest);
}

bool MCMultiply::hxt_serialize(MCHXTASTWriter &w) const
{
    return write_arith_op(w, S_MULTIPLY, line, pos, source, destvar, dest);
}
bool MCMultiply::hxt_deserialize_body(MCHXTASTReader &r)
{
    return read_arith_op(r, source, destvar, dest);
}

bool MCDivide::hxt_serialize(MCHXTASTWriter &w) const
{
    return write_arith_op(w, S_DIVIDE, line, pos, source, destvar, dest);
}
bool MCDivide::hxt_deserialize_body(MCHXTASTReader &r)
{
    return read_arith_op(r, source, destvar, dest);
}

// ============================================================
//  Task 28 — Messaging / dispatch
// ============================================================

// ── MCMessage / MCCall / MCSend ───────────────────────────────────────────
//
// MCCall and MCSend both derive from MCMessage; they share the same on-disk
// format.  The Statements enum assigns them different type codes (S_CALL,
// S_SEND) which the factory uses to create the right subclass.
//
// Format:
//   uint8_t   send_flag   (0=MCCall, 1=MCSend)
//   <expr>    message
//   <chunk>   target      (may be absent)
//   <expr>    in          (may be null = no delay)
//   uint8_t   units       (Functions enum, cast to u8)
//   uint8_t   flags       (program|reply|script packed into bits)

bool MCMessage::hxt_serialize(MCHXTASTWriter &w) const
{
    // Emit the right statement-type code.
    uint16_t stmt_type = send ? S_SEND : S_CALL;
    w.begin_stmt(stmt_type, line, pos);

    w.put_u8(send ? 1u : 0u);
    if (!hxt_serialize_expr(w, message.Get())) return false;
    if (!write_chunk(w, target.Get()))         return false;
    if (!hxt_serialize_expr(w, in.Get()))      return false;
    w.put_u8(uint8_t(units));
    uint8_t flags = (program ? 1u : 0u)
                  | (reply   ? 2u : 0u)
                  | (script  ? 4u : 0u);
    w.put_u8(flags);
    return true;
}

bool MCMessage::hxt_deserialize_body(MCHXTASTReader &r)
{
    uint8_t send_b = 0;
    if (!r.get_u8(send_b)) return false;
    send = send_b ? True : False;

    MCExpression *msg = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;
    message.Reset(msg);

    MCChunk *tgt = read_chunk(r);
    if (!r.ok()) return false;
    target.Reset(tgt);

    MCExpression *in_e = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;
    in.Reset(in_e);

    uint8_t units_b = 0, flags_b = 0;
    if (!r.get_u8(units_b) || !r.get_u8(flags_b)) return false;
    units   = Functions(units_b);
    program = (flags_b & 1u) ? True : False;
    reply   = (flags_b & 2u) ? True : False;
    script  = (flags_b & 4u) ? True : False;
    return true;
}

// ── MCDispatchCmd ────────────────────────────────────────────────────────
//
// Format:
//   <expr>   message
//   uint8_t  flags    (is_function | to_worker<<1 | to_caller<<2)
//   <chunk>  target   (if !to_worker && !to_caller)
//   <expr>   worker_name (if to_worker)
//   <params> params

bool MCDispatchCmd::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_DISPATCH, line, pos);
    if (!hxt_serialize_expr(w, message)) return false;

    uint8_t flags = (is_function ? 1u : 0u)
                  | (to_worker   ? 2u : 0u)
                  | (to_caller   ? 4u : 0u);
    w.put_u8(flags);

    if (!write_chunk(w, target))              return false;
    if (!hxt_serialize_expr(w, worker_name)) return false;
    if (!write_params(w, params))             return false;
    return true;
}

bool MCDispatchCmd::hxt_deserialize_body(MCHXTASTReader &r)
{
    message = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;

    uint8_t flags = 0;
    if (!r.get_u8(flags)) return false;
    is_function = (flags & 1u) != 0;
    to_worker   = (flags & 2u) != 0;
    to_caller   = (flags & 4u) != 0;

    target      = read_chunk(r);              if (!r.ok()) return false;
    worker_name = MCExpression::hxt_deserialize(r); if (!r.ok()) return false;
    params      = read_params(r);
    return r.ok();
}

// ── MCDo ─────────────────────────────────────────────────────────────────
//
// Format:
//   <expr>   source
//   uint8_t  flags   (browser | debug<<1 | caller<<2)
//   <expr>   alternatelang (null if none)
//   <chunk>  widget        (null if none)

bool MCDo::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_DO, line, pos);
    if (!hxt_serialize_expr(w, source)) return false;
    uint8_t flags = (browser ? 1u : 0u)
                  | (debug   ? 2u : 0u)
                  | (caller  ? 4u : 0u);
    w.put_u8(flags);
    if (!hxt_serialize_expr(w, alternatelang)) return false;
    if (!write_chunk(w, widget))               return false;
    return true;
}

bool MCDo::hxt_deserialize_body(MCHXTASTReader &r)
{
    source = MCExpression::hxt_deserialize(r);
    if (!r.ok()) return false;

    uint8_t flags = 0;
    if (!r.get_u8(flags)) return false;
    browser = (flags & 1u) != 0;
    debug   = (flags & 2u) != 0;
    caller  = (flags & 4u) != 0;

    alternatelang = MCExpression::hxt_deserialize(r); if (!r.ok()) return false;
    widget        = read_chunk(r);
    return r.ok();
}

// ── MCComref ──────────────────────────────────────────────────────────────
//
// MCComref is a user-defined command call.  It is NOT in the Statements enum;
// hxtast-base.cpp uses kHXTStmt_Comref (0xFFFE) and handles factory
// construction there — it reads the name, calls new MCComref(name), then
// calls hxt_deserialize_body() which reads only the parameter list.
//
// Format (after the 0xFFFE + line + pos header written by begin_stmt):
//   uint32_t  name_str_idx   command name in string table
//   <params>  MCParameter* list

bool MCComref::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(kHXTStmt_Comref, line, pos);
    // command.getname() returns MCHandref::name via the getter added to MCHandref.
    w.put_u32(w.intern_nameref(command.getname()));
    MCParameter *p = *command.getparams();
    return write_params(w, p);
}

bool MCComref::hxt_deserialize_body(MCHXTASTReader &r)
{
    // The factory (hxtast-base.cpp kHXTStmt_Comref case) reads the name and
    // passes it to new MCComref(name), which stores it in command.
    // We only need to deserialize the parameter list here.
    MCParameter *p = read_params(r);
    *command.getparams() = p;
    return r.ok();
}

// ── MCFuncref (expression) ────────────────────────────────────────────────
//
// MCFuncref requires a name at construction; the factory in hxtast-base.cpp
// handles kHXTExpr_Funcref specially: reads name_str_idx, calls
// new MCFuncref(name), then calls hxt_deserialize_body() which reads params.
//
// Format (after begin_expr header written by hxt_serialize):
//   uint32_t   name_str_idx
//   <params>

bool MCFuncref::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_expr(kHXTExpr_Funcref, line, pos);
    w.put_u32(w.intern_nameref(function.getname()));
    MCParameter *p = *function.getparams();
    return write_params(w, p);
}

bool MCFuncref::hxt_deserialize_body(MCHXTASTReader &r)
{
    // Factory already created MCFuncref(name); just read params.
    MCParameter *p = read_params(r);
    *function.getparams() = p;
    return r.ok();
}

// ── MCDelete ──────────────────────────────────────────────────────────────
//
// Binary layout (after begin_stmt header):
//   uint8_t  form
//     0 = chunk targets (delete <chunk>)
//     1 = file          (delete file <expr>, directory flag follows)
//     2 = url           (delete url <expr>)
//     3 = var           (delete variable <varref>)
//     4 = session       (delete session)
//     5 = worker        (delete worker <expr>)
//     6 = worker-self   (delete worker, no name)
//
//   For form 0:  <expr>   targets chunk
//   For form 1:  uint8_t  is_directory + <expr>  file
//   For form 2:  <expr>   url expression
//   For form 3:  <expr>   var reference
//   For form 4:  (nothing)
//   For form 5:  <expr>   worker_name expression
//   For form 6:  (nothing)
//
// The most common form in .hxtlib worker scripts is form 0
// (e.g. "delete the last char of tResults").

bool MCDelete::hxt_serialize(MCHXTASTWriter &w) const
{
    w.begin_stmt(S_DELETE, line, pos);

    if (targets != nullptr)
    {
        w.put_u8(0);  // form: chunk targets
        hxt_serialize_expr(w, targets);
    }
    else if (url)
    {
        // url=True AND file=urlExpression (both set by parser).
        // Must check url before the generic file != nullptr test below.
        w.put_u8(2);
        hxt_serialize_expr(w, file);
    }
    else if (file != nullptr)
    {
        w.put_u8(1);  // form: file (or directory)
        w.put_u8(directory ? 1u : 0u);
        hxt_serialize_expr(w, file);
    }
    else if (var != nullptr)
    {
        w.put_u8(3);  // form: var
        hxt_serialize_expr(w, var);
    }
    else if (session)
    {
        w.put_u8(4);  // form: session (no extra data)
    }
    else if (worker_name != nullptr)
    {
        w.put_u8(5);  // form: delete named worker
        hxt_serialize_expr(w, worker_name);
    }
    else
    {
        w.put_u8(6);  // form: delete current worker
    }
    return true;
}

bool MCDelete::hxt_deserialize_body(MCHXTASTReader &r)
{
    uint8_t form_b = 0;
    if (!r.get_u8(form_b)) return false;

    switch (form_b)
    {
        case 0:  // chunk targets
        {
            MCExpression *tgt_expr = MCExpression::hxt_deserialize(r);
            if (!r.ok()) return false;
            // The serializer always writes an MCChunk here; static_cast is safe.
            targets = static_cast<MCChunk *>(tgt_expr);
            break;
        }
        case 1:  // file or directory
        {
            uint8_t is_dir = 0;
            if (!r.get_u8(is_dir)) return false;
            directory = is_dir ? True : False;
            file = MCExpression::hxt_deserialize(r);
            if (!r.ok()) return false;
            break;
        }
        case 2:  // url
        {
            url  = True;
            file = MCExpression::hxt_deserialize(r);
            if (!r.ok()) return false;
            break;
        }
        case 3:  // var
        {
            MCExpression *var_expr = MCExpression::hxt_deserialize(r);
            if (!r.ok()) return false;
            // The serializer always writes an MCVarref here; static_cast is safe.
            var = static_cast<MCVarref *>(var_expr);
            break;
        }
        case 4:  // session (no extra data)
            session = true;
            break;
        case 5:  // delete named worker
            worker = true;
            worker_name = MCExpression::hxt_deserialize(r);
            if (!r.ok()) return false;
            break;
        case 6:  // delete current worker
            worker = true;
            break;
        default:
            return false;
    }
    return r.ok();
}
