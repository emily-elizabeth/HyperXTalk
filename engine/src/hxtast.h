/* hxtast.h — HyperXTalk AST serialization for .hxtlib ASTN section
 *
 * This file defines:
 *   • The complete binary layout of the ASTN section
 *   • HXTExprType — compact expression-type discriminator (engine has no enum)
 *   • MCHXTASTWriter — append-only buffer builder with string/constant intern
 *   • MCHXTASTReader — bounds-checked cursor over a raw byte span
 *
 * Copyright (C) HyperXTalk contributors.  See COPYING for licence details.
 *
 * ============================================================
 *  ASTN SECTION BINARY LAYOUT
 * ============================================================
 *
 *  All multi-byte integers are little-endian.
 *  All sizes are in bytes unless noted.
 *
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  Magic + version  (12 bytes)                            │
 *  │  String table     (variable)                            │
 *  │  Constant pool    (variable)                            │
 *  │  Handler list     (variable)                            │
 *  └─────────────────────────────────────────────────────────┘
 *
 * ── Magic + version ─────────────────────────────────────────
 *
 *  [0..7]   magic   8 bytes   "HXTASTN\0"
 *  [8..9]   major   uint16    format major version (kASTNFmtMajor)
 *  [10..11] minor   uint16    format minor version (kASTNFmtMinor)
 *
 * ── String table ─────────────────────────────────────────────
 *
 *  Stores every MCNameRef and identifier as a UTF-8 string.
 *  All other records refer to strings by their uint32_t index here.
 *  Index 0 is the empty string (always present as the first entry).
 *
 *  uint32_t  count
 *  entry[count]:
 *    uint32_t  byte_length
 *    uint8_t   data[byte_length]    (UTF-8, no NUL terminator)
 *
 * ── Constant pool ─────────────────────────────────────────────
 *
 *  Stores every compile-time constant (MCValueRef from MCLiteral nodes).
 *  Referred to by uint32_t index.
 *  Index 0 is always kMCEmptyString (type=kHXTConst_Empty).
 *
 *  uint32_t  count
 *  entry[count]:
 *    uint8_t   type    (HXTConstType)
 *    <data>
 *
 *  HXTConstType values and their data:
 *    kHXTConst_Empty    = 0   (no data)
 *    kHXTConst_String   = 1   uint32_t str_idx  (into string table)
 *    kHXTConst_Real     = 2   float64  value     (IEEE 754 double, LE)
 *    kHXTConst_Bool     = 3   uint8_t  value     (0=false, 1=true)
 *    kHXTConst_Data     = 4   uint32_t byte_len + uint8_t data[byte_len]
 *
 * ── Handler list ──────────────────────────────────────────────
 *
 *  uint32_t  handler_count
 *  handler[handler_count]:
 *    <handler record>
 *
 * ── Handler record ────────────────────────────────────────────
 *
 *  uint8_t   handler_type      (Handler_type enum: HT_MESSAGE=1 .. HT_AFTER=6)
 *  uint32_t  name_str_idx      (string table index)
 *  uint8_t   is_private        (0 or 1)
 *
 *  uint16_t  param_count
 *  param[param_count]:
 *    uint32_t  name_str_idx
 *    uint8_t   is_reference    (0 or 1 — @param)
 *
 *  uint16_t  local_var_count
 *  local[local_var_count]:
 *    uint32_t  name_str_idx
 *    uint32_t  init_const_idx  (0xFFFFFFFF = no initialiser)
 *
 *  uint16_t  global_name_count
 *  global[global_name_count]:
 *    uint32_t  name_str_idx
 *
 *  uint32_t  statement_count
 *  statement[statement_count]:
 *    <statement record>
 *
 * ── Statement record ──────────────────────────────────────────
 *
 *  uint16_t  stmt_type    (Statements enum value, e.g. S_IF=81)
 *  uint16_t  line         (source line, for error messages)
 *  uint16_t  pos          (column position)
 *  <type-specific data>   (defined in each MCStatement::hxt_serialize)
 *
 * ── Expression record ─────────────────────────────────────────
 *
 *  uint8_t   expr_type    (HXTExprType)
 *
 *  If expr_type == kHXTExpr_Null (0):
 *    (nothing more — the field is absent/null)
 *
 *  Otherwise:
 *  uint16_t  line
 *  uint16_t  pos
 *  <type-specific data>
 *
 *  Callers always write expressions via MCHXTASTWriter::put_expr(), which
 *  handles the null sentinel.  Similarly, MCHXTASTReader::get_expr() returns
 *  nullptr when it reads kHXTExpr_Null.
 *
 * ── MCChunk record (HXTExprType = kHXTExpr_Chunk) ────────────
 *
 *  uint8_t   dest_type      (Dest_type enum)
 *  uint32_t  destvar_stridx (string table idx for destvar name; 0 = none)
 *  <expression>             source  (base object/variable expression)
 *  uint8_t   cref_count
 *  cref[cref_count]:
 *    uint8_t   etype    (Chunk_term — the chunk kind: line, word, item …)
 *    uint8_t   otype    (Chunk_term — object type modifier)
 *    uint8_t   ptype    (Chunk_term — part type modifier)
 *    <expression>  startpos
 *    <expression>  endpos
 *
 * ── MCVarref record (HXTExprType = kHXTExpr_Varref) ──────────
 *
 *  uint32_t  name_str_idx   (variable name)
 *  uint8_t   scope          (HXTVarScope: kHXTVarScope_Local / _Global /
 *                            _ScriptLocal / _Param)
 *  uint8_t   dimensions     (0 = plain scalar, N = N-dimensional array access)
 *  index_expression[dimensions]:
 *    <expression>
 *
 * ── MCFuncref record (HXTExprType = kHXTExpr_Funcref) ────────
 *
 *  uint32_t  name_str_idx
 *  uint16_t  param_count
 *  <expression>[param_count]
 *
 * ── MCProperty record (HXTExprType = kHXTExpr_Property) ──────
 *
 *  uint16_t  which              (Properties enum value)
 *  uint8_t   effective          (0 or 1)
 *  uint8_t   tocount            (Chunk_term — CT_UNDEFINED when not a count form)
 *  uint8_t   ptype              (Chunk_term — CT_UNDEFINED when not a count form)
 *  uint32_t  customprop_stridx  (0 = not a custom property)
 *  <expression>  customindex    (may be kHXTExpr_Null)
 *  <expression>  target         (MCChunk for the target object; may be Null)
 *
 *  Note: tocount and ptype MUST be persisted.  eval_ctxt() dispatches to
 *  eval_count_ctxt() only when tocount != CT_UNDEFINED.  Omitting these
 *  fields causes "the number of words in X" to evaluate via the wrong path
 *  (eval_object_property_ctxt) after ASTN deserialization, which will
 *  produce wrong results or a crash.
 *
 * ── MCBuiltinFunc record (HXTExprType = kHXTExpr_BuiltinFunc) ─
 *
 *  uint16_t  func_id        (Functions enum value)
 *  uint16_t  param_count
 *  <expression>[param_count]
 *
 * ── Binary operator records ───────────────────────────────────
 *
 *  (All share the same layout — left and right operands only.)
 *  <expression>  left
 *  <expression>  right
 *
 * ── Unary operator records ────────────────────────────────────
 *
 *  <expression>  operand
 */

#pragma once

#include "typedefs.h"
#include "parsedef.h"   // Statements, Functions, Handler_type, Repeat_form, Dest_type, etc.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ============================================================
//  Format versioning
// ============================================================

static constexpr uint16_t kASTNFmtMajor = 1;
// Minor version history:
//   0 — initial release
//   1 — MCProperty record gains tocount (uint8) + ptype (uint8) after
//       the effective byte; MCDelete statement record added.
//   2 — MCFunction (kHXTExpr_BuiltinFunc) fully implemented: MCConstantFunction,
//       MCUnaryFunction, and MCParamFunction subclasses now serialize /
//       deserialize their parameter expressions correctly.
//   3 — MCChunkOffset (offset, lineOffset, wordOffset, itemOffset, etc.)
//       gains hxt_serialize/hxt_deserialize_body: part + whole + optional offset.
//       MCN_new_function now assigns m_hxt_func_id post-construction (no
//       thread_local dependency); fixes silent func_id=0 bug.
static constexpr uint16_t kASTNFmtMinor = 3;

static constexpr uint8_t kASTNMagic[8] = {
    'H', 'X', 'T', 'A', 'S', 'T', 'N', '\0'
};

// Sentinel for "no string" / "no constant".
static constexpr uint32_t kHXTNoIndex = 0xFFFFFFFFu;

// Special statement-type codes used for entities not in the Statements enum.
// Written by hxt_serialize() and recognised by the factory in hxtast-base.cpp.
static constexpr uint16_t kHXTStmt_Comref = 0xFFFEu; // MCComref (user cmd)
static constexpr uint16_t kHXTStmt_Skip   = 0xFFFFu; // unimplemented placeholder

// ============================================================
//  HXTExprType — expression-type discriminator
// ============================================================
//
// The engine identifies expression classes solely by vtable (class type).
// We define our own compact byte-wide enum to tag serialized nodes.

enum HXTExprType : uint8_t
{
    kHXTExpr_Null        = 0,   // absent / null pointer — no further bytes

    // ── Leaf nodes ────────────────────────────────────────────
    kHXTExpr_Literal     = 1,   // MCLiteral       → uint32_t const_idx
    kHXTExpr_Varref      = 2,   // MCVarref        → see MCVarref record above
    kHXTExpr_Funcref     = 3,   // MCFuncref       → see MCFuncref record above
    kHXTExpr_Property    = 4,   // MCProperty      → see MCProperty record above
    kHXTExpr_Chunk       = 5,   // MCChunk         → see MCChunk record above
    kHXTExpr_There       = 6,   // MCThere         → uint8_t form + <chunk>
    kHXTExpr_Is          = 7,   // MCIs            → uint8_t form + <left> + <right>
    kHXTExpr_BuiltinFunc = 8,   // MCFunction      → uint16_t func_id + N exprs

    // ── Binary arithmetic ─────────────────────────────────────
    kHXTExpr_Add        = 10,   // <left> + <right>
    kHXTExpr_Sub        = 11,
    kHXTExpr_Mul        = 12,
    kHXTExpr_Div        = 13,   // real division (/)
    kHXTExpr_Mod        = 14,   // mod
    kHXTExpr_IntDiv     = 15,   // integer div (div)
    kHXTExpr_Power      = 16,   // ^

    // ── Comparison ────────────────────────────────────────────
    kHXTExpr_Equal      = 20,
    kHXTExpr_NotEqual   = 21,
    kHXTExpr_Less       = 22,
    kHXTExpr_Greater    = 23,
    kHXTExpr_LessEq     = 24,
    kHXTExpr_GreaterEq  = 25,
    kHXTExpr_Contains   = 26,
    // Note: is / is not / is in / is a — all handled by MCIs (kHXTExpr_Is = 7
    // in the leaf section above).  No separate value needed here.

    // ── String ────────────────────────────────────────────────
    kHXTExpr_Concat     = 40,   // &
    kHXTExpr_ConcatSp   = 41,   // &&

    // ── Logical ───────────────────────────────────────────────
    kHXTExpr_And        = 50,
    kHXTExpr_Or         = 51,

    // ── Unary ─────────────────────────────────────────────────
    kHXTExpr_Not        = 60,   // not <operand>  → MCNot
    // Unary minus reuses MCMinus (canbeunary() returns true).
    // kHXTExpr_Negate serializes as MCMinus with an is_unary=1 flag byte.
    kHXTExpr_Negate     = 61,   // - <operand>    → MCMinus (unary form)
};

// ============================================================
//  HXTConstType — constant pool entry type tags
// ============================================================

enum HXTConstType : uint8_t
{
    kHXTConst_Empty  = 0,   // kMCEmptyString  (no data)
    kHXTConst_String = 1,   // uint32_t str_idx (into string table)
    kHXTConst_Real   = 2,   // double (8 bytes, IEEE 754 LE)
    kHXTConst_Bool   = 3,   // uint8_t (0=false, 1=true)
    kHXTConst_Data   = 4,   // uint32_t length + uint8_t data[length]
};

// ============================================================
//  HXTVarScope — variable scope tags used in MCVarref records
// ============================================================

enum HXTVarScope : uint8_t
{
    kHXTVarScope_Local       = 0,   // handler-local variable
    kHXTVarScope_Global      = 1,   // global (declared via 'global' or referenced)
    kHXTVarScope_ScriptLocal = 2,   // script-level local variable
    kHXTVarScope_Param       = 3,   // formal parameter
};

// ============================================================
//  MCHXTASTWriter — serialization buffer
// ============================================================

class MCHXTASTWriter
{
public:
    MCHXTASTWriter();

    // ── Primitive writers ─────────────────────────────────────

    void put_u8(uint8_t v);
    void put_u16(uint16_t v);
    void put_u32(uint32_t v);
    void put_f64(double v);
    void put_bytes(const uint8_t *data, size_t len);

    // ── String table ──────────────────────────────────────────
    // Intern a UTF-8 string and return its uint32_t index.
    // Identical strings always get the same index.  Index 0 is always empty.
    uint32_t intern_str(const char *utf8, size_t len);
    uint32_t intern_str(const char *utf8);          // NUL-terminated overload
    uint32_t intern_str(const std::string &s);

    // Intern an MCNameRef (the engine's immutable interned string type).
    uint32_t intern_nameref(MCNameRef name);

    // ── Constant pool ─────────────────────────────────────────
    // Intern a constant and return its uint32_t index.
    // Index 0 is always kHXTConst_Empty.
    uint32_t intern_const_empty();
    uint32_t intern_const_string(uint32_t str_idx);
    uint32_t intern_const_real(double v);
    uint32_t intern_const_bool(bool v);
    uint32_t intern_const_data(const uint8_t *data, size_t len);
    uint32_t intern_const_valueref(MCValueRef v);

    // ── Final output ──────────────────────────────────────────
    // Serialise magic+version, string table, constant pool, then append
    // the handler-list bytes already written into the body buffer.
    // The returned vector is the complete ASTN section payload.
    std::vector<uint8_t> finalise();

    // Body writer — used by MCHandlerlist::hxt_serialize() and friends.
    // Call the put_* methods above; they accumulate into the body buffer
    // until finalise() is called.
    //
    // Convenience: write a statement/expression type tag, line, pos.
    void begin_stmt(uint16_t stmt_type, uint16_t line, uint16_t pos);
    void begin_expr(HXTExprType expr_type, uint16_t line, uint16_t pos);
    void put_null_expr();   // writes kHXTExpr_Null (single byte)

private:
    // ── Internal ─────────────────────────────────────────────

    // The body buffer receives all handler/statement/expression bytes.
    // The string table and constant pool are built separately and prepended
    // by finalise().
    std::vector<uint8_t> m_body;

    // String table: parallel arrays (value → index).
    std::vector<std::string>  m_strings;    // m_strings[i] is string i
    // (lookup by linear scan — string tables for a script are small)

    // Constant pool entries stored in serialised form directly.
    struct ConstEntry {
        HXTConstType type;
        std::vector<uint8_t> data;
    };
    std::vector<ConstEntry> m_consts;

    void body_put_u8(uint8_t v)   { m_body.push_back(v); }
    void body_put_u16(uint16_t v);
    void body_put_u32(uint32_t v);
    void body_put_f64(double v);
    void body_put_bytes(const uint8_t *d, size_t n)
        { m_body.insert(m_body.end(), d, d + n); }

    void write_string_table(std::vector<uint8_t> &out) const;
    void write_const_pool(std::vector<uint8_t> &out) const;
    // le_put_u16 / le_put_u32 are anonymous-namespace helpers in hxtast.cpp;
    // they are not part of the public or private interface here.
};

// Forward declarations so MCHXTASTReader can carry deserialization context.
class MCHandler;
class MCHandlerlist;

// ============================================================
//  MCHXTASTReader — deserialisation cursor
// ============================================================

class MCHXTASTReader
{
public:
    // Construct from raw ASTN section bytes.  Returns false (and sets the
    // error flag) if the magic or version header is invalid.
    bool open(const uint8_t *data, size_t len);

    bool ok() const { return !m_error; }
    bool at_end() const { return m_pos >= m_len; }

    // ── Deserialization context ───────────────────────────────
    // Set by MCHandler::hxt_deserialize() before reading statements/
    // expressions, so that MCVarref deserialization can reconstruct
    // handler-relative or script-local variable references.
    MCHandler     *current_handler = nullptr;
    MCHandlerlist *current_hlist   = nullptr;

    // ── Primitive readers ─────────────────────────────────────

    bool get_u8(uint8_t &out);
    bool get_u16(uint16_t &out);
    bool get_u32(uint32_t &out);
    bool get_f64(double &out);
    // Read exactly n bytes into dest.
    bool get_bytes(uint8_t *dest, size_t n);

    // ── String table lookup ───────────────────────────────────
    // Return the UTF-8 string at the given index (bounds-checked).
    // Returns "" on error.
    const std::string &get_string(uint32_t idx) const;

    // Convenience: read a uint32_t index and look up the string.
    bool get_str_field(std::string &out);

    // Create an MCNameRef from an interned string index field.
    // The caller is responsible for MCValueRelease.
    bool get_nameref_field(MCNameRef &out);

    // ── Constant pool lookup ──────────────────────────────────
    // Read a uint32_t const index and return the corresponding MCValueRef.
    // Returns kMCNull on error.  Caller must MCValueRelease.
    bool get_const_field(MCValueRef &out);

    // ── Version info (available after open()) ─────────────────
    uint16_t fmt_major() const { return m_fmt_major; }
    uint16_t fmt_minor() const { return m_fmt_minor; }

private:
    const uint8_t *m_data    = nullptr;
    size_t         m_len     = 0;
    size_t         m_pos     = 0;
    bool           m_error   = false;

    uint16_t m_fmt_major = 0;
    uint16_t m_fmt_minor = 0;

    std::vector<std::string>          m_strings;
    std::vector<std::vector<uint8_t>> m_const_data;   // raw const payloads
    std::vector<HXTConstType>         m_const_types;

    bool parse_string_table();
    bool parse_const_pool();

    uint8_t  le_u8()  { uint8_t  v = 0; get_u8(v);  return v; }
    uint16_t le_u16() { uint16_t v = 0; get_u16(v); return v; }
    uint32_t le_u32() { uint32_t v = 0; get_u32(v); return v; }

    static const std::string kEmptyStr;
};
