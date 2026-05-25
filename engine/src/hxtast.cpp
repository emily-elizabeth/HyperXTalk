/* hxtast.cpp — MCHXTASTWriter and MCHXTASTReader implementation
 *
 * Copyright (C) HyperXTalk contributors.  See COPYING for licence details.
 */

#include "prefix.h"

#include "hxtast.h"
#include "foundation.h"   // MCValueRef, MCNameRef, MCStringRef, etc.

#include <cassert>
#include <cstring>

// ============================================================
//  Internal helpers
// ============================================================

namespace {

// Little-endian multi-byte put helpers (into an arbitrary vector).
static void le_put_u16(std::vector<uint8_t> &v, uint16_t x)
{
    v.push_back(uint8_t(x      ));
    v.push_back(uint8_t(x >> 8 ));
}
static void le_put_u32(std::vector<uint8_t> &v, uint32_t x)
{
    v.push_back(uint8_t(x      ));
    v.push_back(uint8_t(x >> 8 ));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 24));
}
static void le_put_f64(std::vector<uint8_t> &v, double x)
{
    uint8_t tmp[8];
    memcpy(tmp, &x, 8);
    v.insert(v.end(), tmp, tmp + 8);
}

// Little-endian multi-byte get helpers (from a raw span).
static bool le_get_u8(const uint8_t *d, size_t len, size_t &pos, uint8_t &out)
{
    if (pos + 1 > len) return false;
    out = d[pos++];
    return true;
}
static bool le_get_u16(const uint8_t *d, size_t len, size_t &pos, uint16_t &out)
{
    if (pos + 2 > len) return false;
    out = uint16_t(d[pos]) | (uint16_t(d[pos+1]) << 8);
    pos += 2;
    return true;
}
static bool le_get_u32(const uint8_t *d, size_t len, size_t &pos, uint32_t &out)
{
    if (pos + 4 > len) return false;
    out = uint32_t(d[pos  ])        |
          uint32_t(d[pos+1]) <<  8  |
          uint32_t(d[pos+2]) << 16  |
          uint32_t(d[pos+3]) << 24;
    pos += 4;
    return true;
}
static bool le_get_f64(const uint8_t *d, size_t len, size_t &pos, double &out)
{
    if (pos + 8 > len) return false;
    memcpy(&out, d + pos, 8);
    pos += 8;
    return true;
}

} // anonymous namespace

// ============================================================
//  MCHXTASTWriter
// ============================================================

MCHXTASTWriter::MCHXTASTWriter()
{
    // Index 0 in the string table is always the empty string.
    m_strings.push_back("");

    // Index 0 in the constant pool is always kHXTConst_Empty.
    ConstEntry e;
    e.type = kHXTConst_Empty;
    m_consts.push_back(std::move(e));
}

// ── Body primitive writers ────────────────────────────────────────────────────

void MCHXTASTWriter::put_u8(uint8_t v)   { m_body.push_back(v); }
void MCHXTASTWriter::put_u16(uint16_t v) { le_put_u16(m_body, v); }
void MCHXTASTWriter::put_u32(uint32_t v) { le_put_u32(m_body, v); }
void MCHXTASTWriter::put_f64(double v)   { le_put_f64(m_body, v); }
void MCHXTASTWriter::put_bytes(const uint8_t *data, size_t len)
{
    m_body.insert(m_body.end(), data, data + len);
}

void MCHXTASTWriter::body_put_u16(uint16_t v) { le_put_u16(m_body, v); }
void MCHXTASTWriter::body_put_u32(uint32_t v) { le_put_u32(m_body, v); }
void MCHXTASTWriter::body_put_f64(double v)   { le_put_f64(m_body, v); }

// ── String table ─────────────────────────────────────────────────────────────

uint32_t MCHXTASTWriter::intern_str(const char *utf8, size_t len)
{
    // Linear scan — script string tables are small (hundreds of entries).
    for (size_t i = 0; i < m_strings.size(); ++i)
    {
        const std::string &s = m_strings[i];
        if (s.size() == len && memcmp(s.data(), utf8, len) == 0)
            return uint32_t(i);
    }
    uint32_t idx = uint32_t(m_strings.size());
    m_strings.emplace_back(utf8, len);
    return idx;
}

uint32_t MCHXTASTWriter::intern_str(const char *utf8)
{
    return utf8 ? intern_str(utf8, strlen(utf8)) : 0u;
}

uint32_t MCHXTASTWriter::intern_str(const std::string &s)
{
    return intern_str(s.data(), s.size());
}

uint32_t MCHXTASTWriter::intern_nameref(MCNameRef name)
{
    if (name == nullptr)
        return 0u;

    MCStringRef t_str = MCNameGetString(name);
    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(t_str, t_cstr))
        return 0u;
    uint32_t idx = intern_str(t_cstr);
    MCMemoryDeleteArray(t_cstr);
    return idx;
}

// ── Constant pool ─────────────────────────────────────────────────────────────

uint32_t MCHXTASTWriter::intern_const_empty()
{
    return 0u;  // index 0 is always kHXTConst_Empty
}

uint32_t MCHXTASTWriter::intern_const_string(uint32_t str_idx)
{
    // Look for an existing kHXTConst_String entry with the same str_idx.
    for (size_t i = 0; i < m_consts.size(); ++i)
    {
        const ConstEntry &e = m_consts[i];
        if (e.type != kHXTConst_String || e.data.size() != 4) continue;
        uint32_t s = uint32_t(e.data[0]) | uint32_t(e.data[1])<<8 |
                     uint32_t(e.data[2])<<16 | uint32_t(e.data[3])<<24;
        if (s == str_idx) return uint32_t(i);
    }
    ConstEntry e;
    e.type = kHXTConst_String;
    le_put_u32(e.data, str_idx);
    uint32_t idx = uint32_t(m_consts.size());
    m_consts.push_back(std::move(e));
    return idx;
}

uint32_t MCHXTASTWriter::intern_const_real(double v)
{
    for (size_t i = 0; i < m_consts.size(); ++i)
    {
        const ConstEntry &e = m_consts[i];
        if (e.type != kHXTConst_Real || e.data.size() != 8) continue;
        double stored;
        memcpy(&stored, e.data.data(), 8);
        if (stored == v) return uint32_t(i);
    }
    ConstEntry e;
    e.type = kHXTConst_Real;
    le_put_f64(e.data, v);
    uint32_t idx = uint32_t(m_consts.size());
    m_consts.push_back(std::move(e));
    return idx;
}

uint32_t MCHXTASTWriter::intern_const_bool(bool v)
{
    uint8_t bv = v ? 1u : 0u;
    for (size_t i = 0; i < m_consts.size(); ++i)
    {
        const ConstEntry &e = m_consts[i];
        if (e.type == kHXTConst_Bool && e.data.size() == 1 && e.data[0] == bv)
            return uint32_t(i);
    }
    ConstEntry e;
    e.type = kHXTConst_Bool;
    e.data.push_back(bv);
    uint32_t idx = uint32_t(m_consts.size());
    m_consts.push_back(std::move(e));
    return idx;
}

uint32_t MCHXTASTWriter::intern_const_data(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < m_consts.size(); ++i)
    {
        const ConstEntry &e = m_consts[i];
        if (e.type != kHXTConst_Data) continue;
        // Stored as uint32_t length + bytes.
        if (e.data.size() < 4) continue;
        uint32_t slen = uint32_t(e.data[0]) | uint32_t(e.data[1])<<8 |
                        uint32_t(e.data[2])<<16 | uint32_t(e.data[3])<<24;
        if (slen != len) continue;
        if (memcmp(e.data.data() + 4, data, len) == 0) return uint32_t(i);
    }
    ConstEntry e;
    e.type = kHXTConst_Data;
    le_put_u32(e.data, uint32_t(len));
    e.data.insert(e.data.end(), data, data + len);
    uint32_t idx = uint32_t(m_consts.size());
    m_consts.push_back(std::move(e));
    return idx;
}

uint32_t MCHXTASTWriter::intern_const_valueref(MCValueRef v)
{
    if (v == nullptr || v == kMCNull)
        return intern_const_empty();

    MCValueTypeCode t_type = MCValueGetTypeCode(v);

    switch (t_type)
    {
        case kMCValueTypeCodeName:
        {
            // MCLiteral stores string literals as MCNameRef (the parser's token
            // type).  Extract the underlying string content and store it the
            // same way as a plain string constant so round-trips preserve text.
            MCStringRef t_str = MCNameGetString((MCNameRef)v);
            char *t_cstr = nullptr;
            if (!MCStringConvertToCString(t_str, t_cstr))
                return intern_const_empty();
            uint32_t si = intern_str(t_cstr);
            MCMemoryDeleteArray(t_cstr);
            return intern_const_string(si);
        }
        case kMCValueTypeCodeString:
        {
            MCStringRef t_str = (MCStringRef)v;
            char *t_cstr = nullptr;
            if (!MCStringConvertToCString(t_str, t_cstr))
                return intern_const_empty();
            uint32_t si = intern_str(t_cstr);
            MCMemoryDeleteArray(t_cstr);
            return intern_const_string(si);
        }
        case kMCValueTypeCodeNumber:
        {
            double d = MCNumberFetchAsReal((MCNumberRef)v);
            return intern_const_real(d);
        }
        case kMCValueTypeCodeBoolean:
        {
            bool b = (v == kMCTrue);
            return intern_const_bool(b);
        }
        case kMCValueTypeCodeData:
        {
            MCDataRef t_data = (MCDataRef)v;
            const uint8_t *bytes = MCDataGetBytePtr(t_data);
            uindex_t       blen  = MCDataGetLength(t_data);
            return intern_const_data(bytes, blen);
        }
        default:
            // Unsupported constant type — store as empty.
            return intern_const_empty();
    }
}

// ── Convenience: statement/expression headers ─────────────────────────────────

void MCHXTASTWriter::begin_stmt(uint16_t stmt_type, uint16_t line, uint16_t pos)
{
    put_u16(stmt_type);
    put_u16(line);
    put_u16(pos);
}

void MCHXTASTWriter::begin_expr(HXTExprType expr_type, uint16_t line, uint16_t pos)
{
    put_u8(uint8_t(expr_type));
    put_u16(line);
    put_u16(pos);
}

void MCHXTASTWriter::put_null_expr()
{
    put_u8(uint8_t(kHXTExpr_Null));
}

// ── String table serialisation ────────────────────────────────────────────────

void MCHXTASTWriter::write_string_table(std::vector<uint8_t> &out) const
{
    le_put_u32(out, uint32_t(m_strings.size()));
    for (const std::string &s : m_strings)
    {
        le_put_u32(out, uint32_t(s.size()));
        out.insert(out.end(), s.begin(), s.end());
    }
}

// ── Constant pool serialisation ───────────────────────────────────────────────

void MCHXTASTWriter::write_const_pool(std::vector<uint8_t> &out) const
{
    le_put_u32(out, uint32_t(m_consts.size()));
    for (const ConstEntry &e : m_consts)
    {
        out.push_back(uint8_t(e.type));
        out.insert(out.end(), e.data.begin(), e.data.end());
    }
}

// ── finalise ─────────────────────────────────────────────────────────────────

std::vector<uint8_t> MCHXTASTWriter::finalise()
{
    std::vector<uint8_t> out;
    out.reserve(12 + 64 + m_body.size());

    // Magic
    out.insert(out.end(), kASTNMagic, kASTNMagic + 8);
    // Version
    le_put_u16(out, kASTNFmtMajor);
    le_put_u16(out, kASTNFmtMinor);
    // String table
    write_string_table(out);
    // Constant pool
    write_const_pool(out);
    // Handler list body
    out.insert(out.end(), m_body.begin(), m_body.end());

    return out;
}

// ============================================================
//  MCHXTASTReader
// ============================================================

const std::string MCHXTASTReader::kEmptyStr;

bool MCHXTASTReader::open(const uint8_t *data, size_t len)
{
    m_data  = data;
    m_len   = len;
    m_pos   = 0;
    m_error = false;

    // Magic
    if (len < 12)
    {
        m_error = true;
        return false;
    }
    if (memcmp(data, kASTNMagic, 8) != 0)
    {
        m_error = true;
        return false;
    }
    m_pos = 8;

    // Version
    if (!le_get_u16(m_data, m_len, m_pos, m_fmt_major) ||
        !le_get_u16(m_data, m_len, m_pos, m_fmt_minor))
    {
        m_error = true;
        return false;
    }
    if (m_fmt_major > kASTNFmtMajor)
    {
        m_error = true;
        return false;
    }

    if (!parse_string_table())  { m_error = true; return false; }
    if (!parse_const_pool())    { m_error = true; return false; }

    return true;
}

// ── Primitive readers ─────────────────────────────────────────────────────────

bool MCHXTASTReader::get_u8(uint8_t &out)
{
    if (m_error) return false;
    if (!le_get_u8(m_data, m_len, m_pos, out)) { m_error = true; return false; }
    return true;
}

bool MCHXTASTReader::get_u16(uint16_t &out)
{
    if (m_error) return false;
    if (!le_get_u16(m_data, m_len, m_pos, out)) { m_error = true; return false; }
    return true;
}

bool MCHXTASTReader::get_u32(uint32_t &out)
{
    if (m_error) return false;
    if (!le_get_u32(m_data, m_len, m_pos, out)) { m_error = true; return false; }
    return true;
}

bool MCHXTASTReader::get_f64(double &out)
{
    if (m_error) return false;
    if (!le_get_f64(m_data, m_len, m_pos, out)) { m_error = true; return false; }
    return true;
}

bool MCHXTASTReader::get_bytes(uint8_t *dest, size_t n)
{
    if (m_error) return false;
    if (m_pos + n > m_len) { m_error = true; return false; }
    memcpy(dest, m_data + m_pos, n);
    m_pos += n;
    return true;
}

// ── String table parsing ──────────────────────────────────────────────────────

bool MCHXTASTReader::parse_string_table()
{
    uint32_t count = 0;
    if (!le_get_u32(m_data, m_len, m_pos, count)) return false;
    m_strings.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t slen = 0;
        if (!le_get_u32(m_data, m_len, m_pos, slen)) return false;
        if (m_pos + slen > m_len) return false;
        m_strings[i].assign(reinterpret_cast<const char *>(m_data + m_pos), slen);
        m_pos += slen;
    }
    return true;
}

const std::string &MCHXTASTReader::get_string(uint32_t idx) const
{
    if (idx >= m_strings.size()) return kEmptyStr;
    return m_strings[idx];
}

bool MCHXTASTReader::get_str_field(std::string &out)
{
    uint32_t idx = 0;
    if (!get_u32(idx)) return false;
    out = get_string(idx);
    return true;
}

bool MCHXTASTReader::get_nameref_field(MCNameRef &out)
{
    uint32_t idx = 0;
    if (!get_u32(idx)) return false;
    const std::string &s = get_string(idx);
    MCStringRef t_str = nullptr;
    if (!MCStringCreateWithBytes(
            reinterpret_cast<const byte_t *>(s.data()), s.size(),
            kMCStringEncodingUTF8, false, t_str))
    {
        m_error = true;
        return false;
    }
    bool ok = MCNameCreate(t_str, out);
    MCValueRelease(t_str);
    if (!ok) { m_error = true; return false; }
    return true;
}

// ── Constant pool parsing ─────────────────────────────────────────────────────

bool MCHXTASTReader::parse_const_pool()
{
    uint32_t count = 0;
    if (!le_get_u32(m_data, m_len, m_pos, count)) return false;
    m_const_types.resize(count);
    m_const_data.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t type_byte = 0;
        if (!le_get_u8(m_data, m_len, m_pos, type_byte)) return false;
        m_const_types[i] = HXTConstType(type_byte);

        switch (m_const_types[i])
        {
            case kHXTConst_Empty:
                // No data.
                break;
            case kHXTConst_String:
            {
                // uint32_t str_idx
                if (m_pos + 4 > m_len) return false;
                m_const_data[i].insert(m_const_data[i].end(),
                                        m_data + m_pos, m_data + m_pos + 4);
                m_pos += 4;
                break;
            }
            case kHXTConst_Real:
            {
                if (m_pos + 8 > m_len) return false;
                m_const_data[i].insert(m_const_data[i].end(),
                                        m_data + m_pos, m_data + m_pos + 8);
                m_pos += 8;
                break;
            }
            case kHXTConst_Bool:
            {
                if (m_pos + 1 > m_len) return false;
                m_const_data[i].push_back(m_data[m_pos++]);
                break;
            }
            case kHXTConst_Data:
            {
                uint32_t dlen = 0;
                if (!le_get_u32(m_data, m_len, m_pos, dlen)) return false;
                if (m_pos + dlen > m_len) return false;
                m_const_data[i].reserve(4 + dlen);
                le_put_u32(m_const_data[i], dlen);
                m_const_data[i].insert(m_const_data[i].end(),
                                        m_data + m_pos, m_data + m_pos + dlen);
                m_pos += dlen;
                break;
            }
            default:
                // Unknown constant type — format is from a newer version.
                // We can't know how many bytes to skip, so fail.
                return false;
        }
    }
    return true;
}

bool MCHXTASTReader::get_const_field(MCValueRef &out)
{
    out = nullptr;
    uint32_t idx = 0;
    if (!get_u32(idx)) return false;
    if (idx >= m_const_types.size()) { m_error = true; return false; }

    HXTConstType type = m_const_types[idx];
    const std::vector<uint8_t> &data = m_const_data[idx];

    switch (type)
    {
        case kHXTConst_Empty:
            out = MCValueRetain(kMCEmptyString);
            return true;

        case kHXTConst_String:
        {
            if (data.size() < 4) { m_error = true; return false; }
            uint32_t si = uint32_t(data[0]) | uint32_t(data[1])<<8 |
                          uint32_t(data[2])<<16 | uint32_t(data[3])<<24;
            const std::string &s = get_string(si);
            MCStringRef t_str = nullptr;
            if (!MCStringCreateWithBytes(
                    reinterpret_cast<const byte_t *>(s.data()), s.size(),
                    kMCStringEncodingUTF8, false, t_str))
            {
                m_error = true;
                return false;
            }
            out = t_str;
            return true;
        }

        case kHXTConst_Real:
        {
            if (data.size() < 8) { m_error = true; return false; }
            double d;
            memcpy(&d, data.data(), 8);
            MCNumberRef t_num = nullptr;
            if (!MCNumberCreateWithReal(d, t_num)) { m_error = true; return false; }
            out = t_num;
            return true;
        }

        case kHXTConst_Bool:
        {
            if (data.empty()) { m_error = true; return false; }
            out = MCValueRetain(data[0] ? kMCTrue : kMCFalse);
            return true;
        }

        case kHXTConst_Data:
        {
            if (data.size() < 4) { m_error = true; return false; }
            uint32_t dlen = uint32_t(data[0]) | uint32_t(data[1])<<8 |
                            uint32_t(data[2])<<16 | uint32_t(data[3])<<24;
            if (data.size() < 4 + dlen) { m_error = true; return false; }
            MCDataRef t_data = nullptr;
            if (!MCDataCreateWithBytes(data.data() + 4, dlen, t_data))
            {
                m_error = true;
                return false;
            }
            out = t_data;
            return true;
        }

        default:
            m_error = true;
            return false;
    }
}
