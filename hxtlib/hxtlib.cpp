/*
 * hxtlib.cpp — HyperXTalk compiled library format implementation
 *
 * On-disk layout
 * ──────────────
 *  [0..31]              Fixed header (32 bytes)
 *  [32..32+N×16−1]      Section directory (N × 16-byte entries)
 *  [dir_end..]          Sections in order: META · STRT · ASTN · HASH
 *
 * Fixed header (32 bytes, all LE):
 *   [0..7]   magic      8 bytes   0x89 'H' 'X' 'T' 0x0D 0x0A 0x1A 0x0A
 *   [8]      fmt_major  uint8
 *   [9]      fmt_minor  uint8
 *   [10..11] flags      uint16
 *   [12..13] eng_major  uint16
 *   [14..15] eng_minor  uint16
 *   [16..17] eng_patch  uint16
 *   [18..19] sec_count  uint16
 *   [20..27] compiled   uint64    Unix timestamp (seconds)
 *   [28..31] hdr_crc32  uint32    CRC32 of bytes [0..27]
 *
 * Section directory entry (16 bytes, all LE):
 *   [0..3]   type       uint32    FourCC
 *   [4..7]   offset     uint32    byte offset from start of file
 *   [8..11]  length     uint32    byte length of section data
 *   [12..15] crc32      uint32    CRC32 of section data
 *
 * Sections:
 *   META  UTF-8 "key=value\n" pairs  (name/version/author/identifier/description/min_engine)
 *   STRT  uint32 count + [uint32 length + UTF-8 bytes] × count
 *   ASTN  uint32 node_count + [uint16 type + uint16 child_count + uint32 pay_len + bytes] × count
 *   HASH  32 bytes  SHA-256 of section data bytes only (META+STRT+ASTN payloads,
 *                  i.e. bytes [hdr+dir_end, hash_section_offset)).
 *                  Covering header+dir would be circular (the HASH dir entry
 *                  holds the CRC32 of this digest).
 *
 * The HASH section is always last.  Unknown section types are silently skipped
 * by the loader, allowing future minor-version additions without breaking
 * older readers.
 */

#include "hxtlib.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <sstream>

// ============================================================
//  Private helpers
// ============================================================
namespace {

// ------------------------------------------------------------------ magic

static const uint8_t kMagicBytes[8] = {
    0x89, 'H', 'X', 'T', '\r', '\n', 0x1A, '\n'
};

// ------------------------------------------------------------------ CRC-32 (ISO 3309)

static uint32_t g_crc_table[256];
static bool     g_crc_inited = false;

static void crc_init()
{
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc_table[i] = c;
    }
    g_crc_inited = true;
}

static uint32_t crc32_of(const uint8_t *data, size_t len)
{
    if (!g_crc_inited) crc_init();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = g_crc_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ------------------------------------------------------------------ SHA-256 (standalone, no deps)

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR32(x,n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA_CH(x,y,z)  (((x)&(y))^(~(x)&(z)))
#define SHA_MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define SHA_S0(x) (ROTR32(x,2)  ^ ROTR32(x,13) ^ ROTR32(x,22))
#define SHA_S1(x) (ROTR32(x,6)  ^ ROTR32(x,11) ^ ROTR32(x,25))
#define SHA_s0(x) (ROTR32(x,7)  ^ ROTR32(x,18) ^ ((x)>>3))
#define SHA_s1(x) (ROTR32(x,17) ^ ROTR32(x,19) ^ ((x)>>10))

struct SHA256Ctx {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buf[64];
    uint32_t buf_len;
};

static void sha256_init(SHA256Ctx &c)
{
    c.state[0]=0x6a09e667; c.state[1]=0xbb67ae85;
    c.state[2]=0x3c6ef372; c.state[3]=0xa54ff53a;
    c.state[4]=0x510e527f; c.state[5]=0x9b05688c;
    c.state[6]=0x1f83d9ab; c.state[7]=0x5be0cd19;
    c.bit_count = 0;
    c.buf_len   = 0;
}

static void sha256_block(SHA256Ctx &c, const uint8_t blk[64])
{
    uint32_t W[64];
    for (int i = 0; i < 16; ++i)
        W[i] = ((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|
               ((uint32_t)blk[i*4+2]<<8)|(uint32_t)blk[i*4+3];
    for (int i = 16; i < 64; ++i)
        W[i] = SHA_s1(W[i-2]) + W[i-7] + SHA_s0(W[i-15]) + W[i-16];

    uint32_t a=c.state[0],b=c.state[1],cc=c.state[2],d=c.state[3];
    uint32_t e=c.state[4],f=c.state[5],g=c.state[6],h=c.state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = h + SHA_S1(e) + SHA_CH(e,f,g) + SHA256_K[i] + W[i];
        uint32_t T2 = SHA_S0(a) + SHA_MAJ(a,b,cc);
        h=g; g=f; f=e; e=d+T1; d=cc; cc=b; b=a; a=T1+T2;
    }
    c.state[0]+=a; c.state[1]+=b; c.state[2]+=cc; c.state[3]+=d;
    c.state[4]+=e; c.state[5]+=f; c.state[6]+=g;  c.state[7]+=h;
}

static void sha256_update(SHA256Ctx &c, const uint8_t *data, size_t len)
{
    c.bit_count += (uint64_t)len * 8;
    while (len > 0) {
        uint32_t space = 64 - c.buf_len;
        uint32_t copy  = (uint32_t)(len < space ? len : space);
        memcpy(c.buf + c.buf_len, data, copy);
        c.buf_len += copy;
        data += copy;
        len  -= copy;
        if (c.buf_len == 64) { sha256_block(c, c.buf); c.buf_len = 0; }
    }
}

static void sha256_final(SHA256Ctx &c, uint8_t digest[32])
{
    uint64_t bits = c.bit_count;
    uint8_t  pad  = 0x80;
    sha256_update(c, &pad, 1);
    while (c.buf_len != 56) { uint8_t z=0; sha256_update(c, &z, 1); }
    uint8_t len_be[8];
    for (int i = 7; i >= 0; --i) { len_be[i] = (uint8_t)(bits & 0xFF); bits >>= 8; }
    sha256_update(c, len_be, 8);
    for (int i = 0; i < 8; ++i) {
        digest[i*4]   = (c.state[i]>>24)&0xFF;
        digest[i*4+1] = (c.state[i]>>16)&0xFF;
        digest[i*4+2] = (c.state[i]>>8) &0xFF;
        digest[i*4+3] =  c.state[i]     &0xFF;
    }
}

static void sha256_of(const uint8_t *data, size_t len, uint8_t digest[32])
{
    SHA256Ctx c;
    sha256_init(c);
    sha256_update(c, data, len);
    sha256_final(c, digest);
}

#undef ROTR32
#undef SHA_CH
#undef SHA_MAJ
#undef SHA_S0
#undef SHA_S1
#undef SHA_s0
#undef SHA_s1

// ------------------------------------------------------------------ byte buffer helpers

using Buf = std::vector<uint8_t>;

static void put_u8(Buf &b, uint8_t v)
{
    b.push_back(v);
}

static void put_u16(Buf &b, uint16_t v)
{
    b.push_back((uint8_t)(v     ));
    b.push_back((uint8_t)(v >> 8));
}

static void put_u32(Buf &b, uint32_t v)
{
    b.push_back((uint8_t)(v      ));
    b.push_back((uint8_t)(v >>  8));
    b.push_back((uint8_t)(v >> 16));
    b.push_back((uint8_t)(v >> 24));
}

static void put_u64(Buf &b, uint64_t v)
{
    for (int i = 0; i < 8; ++i) { b.push_back((uint8_t)(v & 0xFF)); v >>= 8; }
}

static void put_bytes(Buf &b, const uint8_t *data, size_t len)
{
    b.insert(b.end(), data, data + len);
}

static void put_str(Buf &b, const std::string &s)
{
    b.insert(b.end(), s.begin(), s.end());
}

// Patch a uint32 at a known offset (used to fix up CRC32 fields after the fact).
static void patch_u32(Buf &b, size_t offset, uint32_t v)
{
    assert(offset + 4 <= b.size());
    b[offset    ] = (uint8_t)(v      );
    b[offset + 1] = (uint8_t)(v >>  8);
    b[offset + 2] = (uint8_t)(v >> 16);
    b[offset + 3] = (uint8_t)(v >> 24);
}

// Safe little-endian readers (return false on truncation).
static bool get_u8(const uint8_t *d, size_t len, size_t &pos, uint8_t &out)
{
    if (pos + 1 > len) return false;
    out = d[pos++];
    return true;
}

static bool get_u16(const uint8_t *d, size_t len, size_t &pos, uint16_t &out)
{
    if (pos + 2 > len) return false;
    out = (uint16_t)d[pos] | ((uint16_t)d[pos+1] << 8);
    pos += 2;
    return true;
}

static bool get_u32(const uint8_t *d, size_t len, size_t &pos, uint32_t &out)
{
    if (pos + 4 > len) return false;
    out = (uint32_t)d[pos    ]        |
          (uint32_t)d[pos + 1] <<  8  |
          (uint32_t)d[pos + 2] << 16  |
          (uint32_t)d[pos + 3] << 24;
    pos += 4;
    return true;
}

static bool get_u64(const uint8_t *d, size_t len, size_t &pos, uint64_t &out)
{
    if (pos + 8 > len) return false;
    out = 0;
    for (int i = 0; i < 8; ++i) out |= (uint64_t)d[pos+i] << (i*8);
    pos += 8;
    return true;
}

// ------------------------------------------------------------------ file I/O

static bool file_read(const std::string &path, Buf &out)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long sz = ftell(f);
    if (sz < 0)                      { fclose(f); return false; }
    rewind(f);
    out.resize((size_t)sz);
    bool ok = (sz == 0) || (fread(out.data(), 1, (size_t)sz, f) == (size_t)sz);
    fclose(f);
    return ok;
}

static bool file_write(const std::string &path, const Buf &data)
{
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = (data.empty()) || (fwrite(data.data(), 1, data.size(), f) == data.size());
    fclose(f);
    return ok;
}

// ------------------------------------------------------------------ META section

static Buf build_meta(const hxtlib::Meta &m)
{
    Buf b;
    auto kv = [&](const char *key, const std::string &val) {
        if (!val.empty()) {
            put_str(b, key);
            put_u8(b, '=');
            put_str(b, val);
            put_u8(b, '\n');
        }
    };
    kv("name",        m.name);
    kv("version",     m.version);
    kv("author",      m.author);
    kv("identifier",  m.identifier);
    kv("description", m.description);
    kv("min_engine",  m.min_engine);
    return b;
}

static hxtlib::Error parse_meta(const uint8_t *data, size_t len, hxtlib::Meta &out)
{
    std::string text(reinterpret_cast<const char *>(data), len);
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // strip CR if present (defensive)
        if (!val.empty() && val.back() == '\r') val.pop_back();
        if      (key == "name")        out.name        = val;
        else if (key == "version")     out.version     = val;
        else if (key == "author")      out.author      = val;
        else if (key == "identifier")  out.identifier  = val;
        else if (key == "description") out.description = val;
        else if (key == "min_engine")  out.min_engine  = val;
        // unknown keys are silently ignored for forward compatibility
    }
    return hxtlib::Error::Ok;
}

// ------------------------------------------------------------------ STRT section

static Buf build_strt(const std::vector<std::string> &strings)
{
    Buf b;
    put_u32(b, (uint32_t)strings.size());
    for (const auto &s : strings) {
        put_u32(b, (uint32_t)s.size());
        put_bytes(b, reinterpret_cast<const uint8_t *>(s.data()), s.size());
    }
    return b;
}

static hxtlib::Error parse_strt(const uint8_t *data, size_t len,
                                 std::vector<std::string> &out)
{
    size_t   pos = 0;
    uint32_t count;
    if (!get_u32(data, len, pos, count))
        return hxtlib::Error::Truncated;

    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t slen;
        if (!get_u32(data, len, pos, slen))
            return hxtlib::Error::Truncated;
        if (pos + slen > len)
            return hxtlib::Error::Truncated;
        out.emplace_back(reinterpret_cast<const char *>(data + pos), slen);
        pos += slen;
    }
    return hxtlib::Error::Ok;
}

// ------------------------------------------------------------------ ASTN section
//
// The ASTN section now stores the raw output of MCHXTASTWriter::finalise(),
// which starts with the 8-byte magic "HXTASTN\0".  When astn_bytes is empty
// (no pre-parsed AST available) we write an empty section so the section
// directory remains structurally valid; the loader treats an empty or
// unrecognised ASTN as "fall back to SRCS".

static Buf build_astn(const std::vector<uint8_t> &astn_bytes)
{
    // Return the raw bytes as-is; an empty vector → empty section.
    return Buf(astn_bytes.begin(), astn_bytes.end());
}

static hxtlib::Error parse_astn(const uint8_t *data, size_t len,
                                 std::vector<uint8_t> &out)
{
    out.assign(data, data + len);
    return hxtlib::Error::Ok;
}

// ------------------------------------------------------------------ version comparison

// Parse "major.minor.patch" (missing components treated as 0).
static void parse_version(const std::string &s,
                           uint16_t &maj, uint16_t &min, uint16_t &pat)
{
    maj = min = pat = 0;
    int n = sscanf(s.c_str(), "%hu.%hu.%hu", &maj, &min, &pat);
    (void)n;
}

// Returns true if engine (maj,min,pat) >= required min_engine string.
static bool engine_ok(uint16_t e_maj, uint16_t e_min, uint16_t e_pat,
                       const std::string &min_engine)
{
    if (min_engine.empty()) return true;
    uint16_t r_maj, r_min, r_pat;
    parse_version(min_engine, r_maj, r_min, r_pat);
    if (e_maj != r_maj) return e_maj > r_maj;
    if (e_min != r_min) return e_min > r_min;
    return e_pat >= r_pat;
}

// ------------------------------------------------------------------ section directory entry

struct SecEntry {
    uint32_t type;
    uint32_t offset;
    uint32_t length;
    uint32_t crc32;
};

static void put_sec_entry(Buf &b, const SecEntry &e)
{
    put_u32(b, e.type);
    put_u32(b, e.offset);
    put_u32(b, e.length);
    put_u32(b, e.crc32);
}

static bool get_sec_entry(const uint8_t *d, size_t len, size_t &pos, SecEntry &e)
{
    return get_u32(d, len, pos, e.type)   &&
           get_u32(d, len, pos, e.offset) &&
           get_u32(d, len, pos, e.length) &&
           get_u32(d, len, pos, e.crc32);
}

} // anonymous namespace

// ============================================================
//  Public API
// ============================================================
namespace hxtlib {

// ------------------------------------------------------------------ strerror

const char *strerror(Error err) noexcept
{
    switch (err) {
    case Error::Ok:                  return "ok";
    case Error::BadMagic:            return "bad magic — not a .hxtlib file";
    case Error::BadFormatVersion:    return "format version too new for this loader";
    case Error::BadCRC:              return "CRC32 mismatch — file is corrupt";
    case Error::BadHash:             return "SHA-256 integrity check failed";
    case Error::EngineVersionTooOld: return "engine version too old for this library";
    case Error::Truncated:           return "file is truncated";
    case Error::OutOfMemory:         return "out of memory";
    case Error::IOError:             return "I/O error";
    case Error::InvalidStructure:    return "invalid internal structure";
    default:                         return "unknown error";
    }
}

// ------------------------------------------------------------------ Document helpers

uint32_t Document::intern(const std::string &s)
{
    for (uint32_t i = 0; i < (uint32_t)string_table.size(); ++i)
        if (string_table[i] == s) return i;
    string_table.push_back(s);
    return (uint32_t)(string_table.size() - 1);
}

static const std::string kEmptyString;

const std::string &Document::get_string(uint32_t index) const
{
    if (index >= (uint32_t)string_table.size()) return kEmptyString;
    return string_table[index];
}

// ------------------------------------------------------------------ write

Error write(const Document &doc, const std::string &path)
{
    // Build section data buffers.
    Buf meta_buf = build_meta(doc.meta);
    Buf strt_buf = build_strt(doc.string_table);
    Buf astn_buf = build_astn(doc.astn_bytes);

    // SRCS section: raw UTF-8 source script.
    // IDE-produced libraries leave source_script empty (source is never stored).
    // hxtc-produced libraries populate it as a loader fallback while hxtc
    // cannot produce ASTN bytes.  The section is always written (possibly empty)
    // so the section count stays constant and readers need no version negotiation.
    Buf srcs_buf(doc.source_script.begin(), doc.source_script.end());

    // HASH is always 32 bytes (SHA-256, computed at end).

    // Section count: META + STRT + ASTN + SRCS + HASH = 5.
    static constexpr uint16_t kSecCount = 5;
    static constexpr size_t   kHdrSize  = 32;
    static constexpr size_t   kDirSize  = kSecCount * 16;

    const uint32_t meta_offset = (uint32_t)(kHdrSize + kDirSize);
    const uint32_t strt_offset = (uint32_t)(meta_offset + meta_buf.size());
    const uint32_t astn_offset = (uint32_t)(strt_offset + strt_buf.size());
    const uint32_t srcs_offset = (uint32_t)(astn_offset + astn_buf.size());
    const uint32_t hash_offset = (uint32_t)(srcs_offset + srcs_buf.size());

    Buf b;
    b.reserve(hash_offset + 32 + 16);

    // ---- Fixed header (bytes 0-31) ----
    put_bytes(b, kMagicBytes, 8);             // [0..7]
    put_u8(b, kFmtMajor);                     // [8]
    put_u8(b, kFmtMinor);                     // [9]
    put_u16(b, doc.flags);                    // [10..11]
    put_u16(b, doc.eng_major);                // [12..13]
    put_u16(b, doc.eng_minor);                // [14..15]
    put_u16(b, doc.eng_patch);                // [16..17]
    put_u16(b, kSecCount);                    // [18..19]
    uint64_t ts = doc.compiled_at != 0 ? doc.compiled_at
                                        : (uint64_t)time(nullptr);
    put_u64(b, ts);                           // [20..27]
    put_u32(b, 0);                            // [28..31]  hdr_crc32 placeholder

    // ---- Section directory ----
    put_sec_entry(b, {kSectionMeta, meta_offset, (uint32_t)meta_buf.size(),
                      crc32_of(meta_buf.data(), meta_buf.size())});
    put_sec_entry(b, {kSectionStrt, strt_offset, (uint32_t)strt_buf.size(),
                      crc32_of(strt_buf.data(), strt_buf.size())});
    put_sec_entry(b, {kSectionAstn, astn_offset, (uint32_t)astn_buf.size(),
                      crc32_of(astn_buf.data(), astn_buf.size())});
    put_sec_entry(b, {kSectionSrcs, srcs_offset, (uint32_t)srcs_buf.size(),
                      crc32_of(srcs_buf.data(), srcs_buf.size())});
    // HASH entry — offset and length known, crc32 = 0 for now (patched below).
    const size_t hash_dir_offset = b.size();
    put_sec_entry(b, {kSectionHash, hash_offset, 32u, 0u});

    // ---- Section data ----
    put_bytes(b, meta_buf.data(), meta_buf.size());
    put_bytes(b, strt_buf.data(), strt_buf.size());
    put_bytes(b, astn_buf.data(), astn_buf.size());
    put_bytes(b, srcs_buf.data(), srcs_buf.size());

    // ---- Header CRC (bytes 28-31 cover bytes 0-27, no section data involved) ----
    patch_u32(b, 28, crc32_of(b.data(), 28));

    // ---- SHA-256 of section data only (META + STRT + ASTN bytes) ----
    //
    // Covering the full file up to hash_offset would be circular: the section
    // directory entry for HASH contains the CRC32 of the digest, which is not
    // yet known when the digest is being computed.  Limiting coverage to the
    // section payload bytes (offset meta_offset..hash_offset-1) sidesteps this
    // entirely — those bytes contain no CRC fields.
    uint8_t digest[32];
    sha256_of(b.data() + meta_offset, hash_offset - meta_offset, digest);

    // Patch HASH section directory entry crc32 (at hash_dir_offset + 12).
    patch_u32(b, hash_dir_offset + 12, crc32_of(digest, 32));

    // Append HASH section data.
    put_bytes(b, digest, 32);

    if (!file_write(path, b))
        return Error::IOError;

    return Error::Ok;
}

// ------------------------------------------------------------------ internal read helper

// Reads the file, validates magic/header/section-CRCs/SHA-256, populates
// sec_entries and file_buf.  Stops before parsing section content.
static Error read_raw(const std::string       &path,
                      Buf                     &file_buf,
                      uint16_t                &flags_out,
                      uint16_t                &eng_maj_out,
                      uint16_t                &eng_min_out,
                      uint16_t                &eng_pat_out,
                      uint64_t                &compiled_at_out,
                      std::vector<SecEntry>   &secs_out)
{
    // Quick magic pre-check: peek at first 8 bytes only.
    // This avoids reading multi-MB binary stack files for the common case where
    // the file is not an .hxtlib file at all.
    {
        FILE *pf = fopen(path.c_str(), "rb");
        if (!pf) return Error::IOError;
        uint8_t probe[8] = {};
        size_t  n = fread(probe, 1, 8, pf);
        fclose(pf);
        if (n < 8 || memcmp(probe, kMagicBytes, 8) != 0)
            return Error::BadMagic;
    }

    if (!file_read(path, file_buf))
        return Error::IOError;

    const uint8_t *d   = file_buf.data();
    const size_t   len = file_buf.size();

    // Magic.
    if (len < 8 || memcmp(d, kMagicBytes, 8) != 0)
        return Error::BadMagic;

    // Header (32 bytes minimum).
    if (len < 32)
        return Error::Truncated;

    size_t pos = 8;
    uint8_t  fmt_major, fmt_minor;
    uint16_t flags, eng_maj, eng_min, eng_pat, sec_count;
    uint64_t compiled_at;
    uint32_t hdr_crc;

    (void)get_u8(d,len,pos,fmt_major);
    (void)get_u8(d,len,pos,fmt_minor);
    (void)get_u16(d,len,pos,flags);
    (void)get_u16(d,len,pos,eng_maj);
    (void)get_u16(d,len,pos,eng_min);
    (void)get_u16(d,len,pos,eng_pat);
    (void)get_u16(d,len,pos,sec_count);
    (void)get_u64(d,len,pos,compiled_at);
    (void)get_u32(d,len,pos,hdr_crc);
    // pos == 32 here

    // Header CRC covers bytes 0-27.
    if (crc32_of(d, 28) != hdr_crc)
        return Error::BadCRC;

    // Format version check.
    if (fmt_major > kFmtMajor)
        return Error::BadFormatVersion;

    // Section directory.
    if (len < 32 + (size_t)sec_count * 16)
        return Error::Truncated;

    secs_out.resize(sec_count);
    for (uint16_t i = 0; i < sec_count; ++i) {
        if (!get_sec_entry(d, len, pos, secs_out[i]))
            return Error::Truncated;
    }

    // Locate and validate HASH section — must be the last section.
    const SecEntry *hash_sec = nullptr;
    for (const auto &se : secs_out)
        if (se.type == kSectionHash) { hash_sec = &se; break; }

    if (!hash_sec)
        return Error::InvalidStructure;

    // Every section's data must be within the file.
    for (const auto &se : secs_out) {
        if ((size_t)se.offset + se.length > len)
            return Error::Truncated;
    }

    // Validate per-section CRC32.
    for (const auto &se : secs_out) {
        if (crc32_of(d + se.offset, se.length) != se.crc32)
            return Error::BadCRC;
    }

    // SHA-256: covers section data (META+STRT+ASTN bytes).
    // Section data begins immediately after the fixed header + section directory.
    // Using sec_count to compute the directory size avoids hard-coding kSecCount.
    if (hash_sec->length != 32)
        return Error::InvalidStructure;
    const size_t data_start = 32 + (size_t)sec_count * 16;
    if (hash_sec->offset < data_start)
        return Error::InvalidStructure;
    uint8_t expected[32];
    sha256_of(d + data_start, hash_sec->offset - data_start, expected);
    if (memcmp(expected, d + hash_sec->offset, 32) != 0)
        return Error::BadHash;

    flags_out       = flags;
    eng_maj_out     = eng_maj;
    eng_min_out     = eng_min;
    eng_pat_out     = eng_pat;
    compiled_at_out = compiled_at;
    return Error::Ok;
}

// ------------------------------------------------------------------ validate

Error validate(const std::string &path,
               uint16_t engine_major, uint16_t engine_minor, uint16_t engine_patch)
{
    Buf                  file_buf;
    uint16_t             flags, em, emi, ep;
    uint64_t             ts;
    std::vector<SecEntry> secs;

    Error err = read_raw(path, file_buf, flags, em, emi, ep, ts, secs);
    if (err != Error::Ok) return err;

    // Find META and check min_engine without full deserialisation.
    for (const auto &se : secs) {
        if (se.type != kSectionMeta) continue;
        Meta meta;
        err = parse_meta(file_buf.data() + se.offset, se.length, meta);
        if (err != Error::Ok) return err;
        if (!engine_ok(engine_major, engine_minor, engine_patch, meta.min_engine))
            return Error::EngineVersionTooOld;
        break;
    }

    return Error::Ok;
}

// ------------------------------------------------------------------ read

Error read(const std::string  &path,
           Document           &doc_out,
           uint16_t            engine_major,
           uint16_t            engine_minor,
           uint16_t            engine_patch)
{
    Buf                   file_buf;
    std::vector<SecEntry> secs;
    Error err = read_raw(path, file_buf,
                         doc_out.flags,
                         doc_out.eng_major, doc_out.eng_minor, doc_out.eng_patch,
                         doc_out.compiled_at, secs);
    if (err != Error::Ok) return err;

    const uint8_t *d = file_buf.data();

    // Helper: find a section by type (returns nullptr if absent).
    auto find_sec = [&](uint32_t type) -> const SecEntry * {
        for (const auto &se : secs)
            if (se.type == type) return &se;
        return nullptr;
    };

    // META (required).
    const SecEntry *meta_se = find_sec(kSectionMeta);
    if (!meta_se) return Error::InvalidStructure;
    err = parse_meta(d + meta_se->offset, meta_se->length, doc_out.meta);
    if (err != Error::Ok) return err;

    // Engine version check.
    if (!engine_ok(engine_major, engine_minor, engine_patch, doc_out.meta.min_engine))
        return Error::EngineVersionTooOld;

    // STRT (required).
    const SecEntry *strt_se = find_sec(kSectionStrt);
    if (!strt_se) return Error::InvalidStructure;
    err = parse_strt(d + strt_se->offset, strt_se->length, doc_out.string_table);
    if (err != Error::Ok) return err;

    // ASTN (required).
    const SecEntry *astn_se = find_sec(kSectionAstn);
    if (!astn_se) return Error::InvalidStructure;
    err = parse_astn(d + astn_se->offset, astn_se->length, doc_out.astn_bytes);
    if (err != Error::Ok) return err;

    // SRCS (optional — absent in files compiled without source, e.g. for
    // distribution; present in the default hxtc output so the engine can
    // call SetScript() to populate the handler list).
    const SecEntry *srcs_se = find_sec(kSectionSrcs);
    if (srcs_se && srcs_se->length > 0)
    {
        doc_out.source_script.assign(
            reinterpret_cast<const char *>(d + srcs_se->offset),
            srcs_se->length);
    }

    return Error::Ok;
}

} // namespace hxtlib
