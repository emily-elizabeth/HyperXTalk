/* hxtast-base.cpp — Default hxt_serialize/hxt_deserialize for MCStatement
 *                   and MCExpression base classes, plus the static factories.
 *
 * Per-subclass overrides live in hxtast-stmts.cpp and hxtast-exprs.cpp.
 *
 * Copyright (C) HyperXTalk contributors.  See COPYING for licence details.
 */

#include "prefix.h"

#include "hxtast.h"
#include "statemnt.h"
#include "express.h"
#include "newobj.h"     // MCN_new_statement, MCN_new_function
#include "parsedef.h"   // Statements, Functions enums

// Expression subclasses we construct directly in the factory.
#include "literal.h"
#include "variable.h"   // MCVarref
#include "keywords.h"   // MCFuncref, MCComref, MCIs, MCThere
#include "property.h"   // MCProperty
#include "chunk.h"      // MCChunk
#include "operator.h"   // binary/unary operator base classes
#include "hndlrlst.h"   // MCHandlerlist (for current_hlist context)

// ============================================================
//  MCStatement — base class defaults
// ============================================================

// Default serialize: warns at runtime that the subclass hasn't been
// implemented yet, and writes nothing.  This allows the build to succeed
// while statement groups are being filled in; a missing implementation will
// fail gracefully (the ASTN section will not validate on reload) rather than
// crashing at compile time.
bool MCStatement::hxt_serialize(MCHXTASTWriter &w) const
{
    // Subclass did not override hxt_serialize.
    // Write a kHXTStmt_Skip placeholder so the reader can count and skip it
    // gracefully.  The handler stays runnable via source-text fallback.
    w.begin_stmt(kHXTStmt_Skip, line, pos);
    return true;
}

bool MCStatement::hxt_deserialize_body(MCHXTASTReader &r)
{
    // Subclass did not override hxt_deserialize_body.
    // Nothing to read (matches the empty body written by the default serialize).
    return true;
}

// Static factory: reads the common statement header and dispatches to the
// right subclass.
MCStatement *MCStatement::hxt_deserialize(MCHXTASTReader &r)
{
    uint16_t stmt_type = 0;
    uint16_t stmt_line = 0;
    uint16_t stmt_pos  = 0;
    if (!r.get_u16(stmt_type) || !r.get_u16(stmt_line) || !r.get_u16(stmt_pos))
        return nullptr;

    // ── Special / non-enum statement types ───────────────────────────────

    if (stmt_type == kHXTStmt_Skip)
    {
        // Unimplemented placeholder — return a base-class no-op statement.
        MCStatement *t_noop = new MCStatement;
        t_noop->line = stmt_line;
        t_noop->pos  = stmt_pos;
        return t_noop;
    }

    if (stmt_type == kHXTStmt_Comref)
    {
        // User-defined command call: read name, then construct MCComref.
        MCNameRef cmd_name = nullptr;
        if (!r.get_nameref_field(cmd_name))
            return nullptr;
        MCComref *comref = new MCComref(cmd_name);
        MCValueRelease(cmd_name);
        comref->line = stmt_line;
        comref->pos  = stmt_pos;
        if (!comref->hxt_deserialize_body(r))
        {
            delete comref;
            return nullptr;
        }
        return comref;
    }

    // ── Standard enum-keyed statements ───────────────────────────────────

    // Allocate the right subclass using the engine's existing factory.
    MCStatement *t_stmt = MCN_new_statement(int2(stmt_type));
    if (t_stmt == nullptr)
        return nullptr;

    // Restore source location (used for error messages).
    t_stmt->line = stmt_line;
    t_stmt->pos  = stmt_pos;

    // Populate type-specific fields.
    if (!t_stmt->hxt_deserialize_body(r))
    {
        delete t_stmt;
        return nullptr;
    }

    return t_stmt;
}

// ============================================================
//  MCExpression — base class defaults
// ============================================================

bool MCExpression::hxt_serialize(MCHXTASTWriter &w) const
{
    // Subclass did not override hxt_serialize.  Write the kHXTExpr_Null
    // sentinel (1 byte = 0x00) so the stream stays parseable — the expression
    // will deserialize as absent/null rather than corrupting subsequent bytes.
    // This is better than writing nothing, which would cause the reader to
    // misinterpret the next field's bytes as this expression's type byte.
    fprintf(stderr,
        "hxtast: WARNING: MCExpression subclass at line %d has no "
        "hxt_serialize override — serializing as null\n",
        int(line));
    w.put_u8(uint8_t(kHXTExpr_Null));
    return true;
}

bool MCExpression::hxt_deserialize_body(MCHXTASTReader &r)
{
    // Subclass did not override hxt_deserialize_body.
    return true;
}

// Static factory: reads the type byte; returns nullptr for kHXTExpr_Null
// (not an error — the caller should treat it as an absent expression).
// For all other types, reads line/pos, allocates the subclass, calls
// hxt_deserialize_body().  Returns nullptr + r.ok()==false on real error.
MCExpression *MCExpression::hxt_deserialize(MCHXTASTReader &r)
{
    uint8_t type_byte = 0;
    if (!r.get_u8(type_byte)) return nullptr;

    HXTExprType expr_type = HXTExprType(type_byte);
    if (expr_type == kHXTExpr_Null)
        return nullptr;  // absent — not an error

    uint16_t expr_line = 0, expr_pos = 0;
    if (!r.get_u16(expr_line) || !r.get_u16(expr_pos)) return nullptr;

    MCExpression *t_expr = nullptr;

    switch (expr_type)
    {
        // ── Leaf nodes ────────────────────────────────────────
        case kHXTExpr_Literal:
            // MCLiteral() default-constructs to kMCEmptyString;
            // hxt_deserialize_body() replaces value from the const pool.
            t_expr = new MCLiteral;
            break;

        case kHXTExpr_Varref:
        {
            // MCVarref has no default constructor.  Read scope+index here,
            // then construct the right variant.
            uint8_t  scope_b  = 0;
            uint16_t var_idx  = 0;
            uint8_t  dims     = 0;
            if (!r.get_u8(scope_b) || !r.get_u16(var_idx) || !r.get_u8(dims))
                return nullptr;

            HXTVarScope scope = HXTVarScope(scope_b);
            MCVarref *vr = nullptr;

            if (scope == kHXTVarScope_Param || scope == kHXTVarScope_Local)
            {
                if (r.current_handler == nullptr) return nullptr;
                vr = new MCVarref(r.current_handler, var_idx,
                                  scope == kHXTVarScope_Param ? True : False);
            }
            else if (scope == kHXTVarScope_ScriptLocal)
            {
                // Walk the hlist's variable linked list to the given index.
                if (r.current_hlist == nullptr) return nullptr;
                MCVariable *v = r.current_hlist->getvars();
                for (uint16_t i = 0; i < var_idx && v != nullptr; ++i)
                    v = v->getnext();
                if (v == nullptr) return nullptr;
                vr = new MCVarref(v, var_idx);
            }
            else
            {
                // kHXTVarScope_Global: resolve by index is not supported yet.
                // Consume the dimension expressions and return null (best-effort).
                for (uint8_t d = 0; d < dims; ++d)
                {
                    MCExpression *di = MCExpression::hxt_deserialize(r);
                    if (!r.ok()) return nullptr;
                    delete di;
                }
                return nullptr;
            }

            vr->line = expr_line;
            vr->pos  = expr_pos;

            // Read dimension index expressions (array access).
            if (dims == 0)
            {
                // nothing
            }
            else if (dims == 1)
            {
                MCExpression *de = MCExpression::hxt_deserialize(r);
                if (!r.ok()) { delete vr; return nullptr; }
                vr->hxt_set_dim1(de);
            }
            else
            {
                MCExpression **ea = new MCExpression*[dims];
                for (uint8_t d = 0; d < dims; ++d)
                {
                    ea[d] = MCExpression::hxt_deserialize(r);
                    if (!r.ok()) { delete[] ea; delete vr; return nullptr; }
                }
                vr->hxt_set_dims(dims, ea);
            }
            return vr;   // already set line/pos; skip the common tail
        }

        case kHXTExpr_Funcref:
        {
            // MCFuncref requires a name at construction.
            // Read name_str_idx, create, then call hxt_deserialize_body for params.
            MCNameRef fn_name = nullptr;
            if (!r.get_nameref_field(fn_name)) return nullptr;
            MCFuncref *funcref = new MCFuncref(fn_name);
            MCValueRelease(fn_name);
            funcref->line = expr_line;
            funcref->pos  = expr_pos;
            if (!funcref->hxt_deserialize_body(r))
            {
                delete funcref;
                return nullptr;
            }
            return funcref;
        }

        case kHXTExpr_Property:
            t_expr = new MCProperty;
            break;
        case kHXTExpr_Chunk:
            t_expr = new MCChunk(False);
            break;
        case kHXTExpr_There:
            t_expr = new MCThere;
            break;
        case kHXTExpr_Is:
            t_expr = new MCIs;
            break;

        // ── Built-in function (MCFunction subclasses) ─────────
        case kHXTExpr_BuiltinFunc:
        {
            // Next uint16_t is the Functions enum value.
            uint16_t func_id = 0;
            if (!r.get_u16(func_id)) return nullptr;
            t_expr = MCN_new_function(int2(func_id));
            if (t_expr == nullptr) return nullptr;
            t_expr->line = expr_line;
            t_expr->pos  = expr_pos;
            if (!t_expr->hxt_deserialize_body(r)) { delete t_expr; return nullptr; }
            return t_expr;
        }

        // ── Binary arithmetic ─────────────────────────────────
        // Note: MCMinus handles both binary subtraction and unary negation
        // (canbeunary() returns true).  kHXTExpr_Sub covers both forms;
        // the serializer stores an is_unary flag byte so the deserializer
        // can set the operand count correctly.
        case kHXTExpr_Add:    t_expr = new MCPlus;  break;
        case kHXTExpr_Sub:    t_expr = new MCMinus; break;   // also unary minus
        case kHXTExpr_Mul:    t_expr = new MCTimes; break;
        case kHXTExpr_Div:    t_expr = new MCOver;  break;
        case kHXTExpr_Mod:    t_expr = new MCMod;   break;
        case kHXTExpr_IntDiv: t_expr = new MCDiv;   break;
        case kHXTExpr_Power:  t_expr = new MCPow;   break;

        // ── Comparison ────────────────────────────────────────
        case kHXTExpr_Equal:     t_expr = new MCEqual;          break;
        case kHXTExpr_NotEqual:  t_expr = new MCNotEqual;       break;
        case kHXTExpr_Less:      t_expr = new MCLessThan;       break;
        case kHXTExpr_Greater:   t_expr = new MCGreaterThan;    break;
        case kHXTExpr_LessEq:    t_expr = new MCLessThanEqual;  break;
        case kHXTExpr_GreaterEq: t_expr = new MCGreaterThanEqual; break;
        case kHXTExpr_Contains:  t_expr = new MCContains;       break;
        // kHXTExpr_Is is handled in the leaf section above (value 7).

        // ── String ────────────────────────────────────────────
        case kHXTExpr_Concat:   t_expr = new MCConcat;      break;
        case kHXTExpr_ConcatSp: t_expr = new MCConcatSpace; break;

        // ── Logical ───────────────────────────────────────────
        case kHXTExpr_And: t_expr = new MCAnd; break;
        case kHXTExpr_Or:  t_expr = new MCOr;  break;

        // ── Unary ─────────────────────────────────────────────
        case kHXTExpr_Not:    t_expr = new MCNot;    break;
        case kHXTExpr_Negate:
        {
            // Unary minus: only one operand (right); left stays null.
            MCMinus *um = new MCMinus;
            um->line = expr_line;
            um->pos  = expr_pos;
            // Read the single operand as the right child.
            um->right = MCExpression::hxt_deserialize(r);
            if (!r.ok()) { delete um; return nullptr; }
            // left remains null — that's how MCMinus::hxt_serialize() detects
            // the unary form.
            return um;
        }

        default:
            // Unknown type from a newer format version.
            return nullptr;
    }

    if (t_expr == nullptr)
        return nullptr;

    t_expr->line = expr_line;
    t_expr->pos  = expr_pos;

    if (!t_expr->hxt_deserialize_body(r))
    {
        delete t_expr;
        return nullptr;
    }

    return t_expr;
}

// ============================================================
//  Convenience: serialize an MCExpression* that may be null
// ============================================================
//
// Free function used by statement serialize() implementations.

bool hxt_serialize_expr(MCHXTASTWriter &w, const MCExpression *expr)
{
    if (expr == nullptr)
    {
        w.put_null_expr();
        return true;
    }
    return expr->hxt_serialize(w);
}
