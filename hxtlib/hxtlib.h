/*
 * hxtlib.h — HyperXTalk compiled library format (.hxtlib)
 *
 * .hxtlib is a binary container for pre-compiled HyperXTalk script-only
 * stack libraries.  The source script is parsed into an AST by the engine
 * or compiler tool, serialised here, and later reconstructed by the engine
 * loader — skipping the parse step and keeping source text out of the file.
 *
 * All multi-byte integers are little-endian.
 * See hxtlib.cpp for the detailed on-disk layout.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hxtlib {

// ------------------------------------------------------------------ version

static constexpr uint8_t kFmtMajor = 1;   // increment on breaking changes
static constexpr uint8_t kFmtMinor = 0;   // increment on backward-compat additions

// ------------------------------------------------------------------ flags (header word)

static constexpr uint16_t kFlagCompressed = 1u << 0;  // ASTN section is zlib-deflated
static constexpr uint16_t kFlagEncrypted  = 1u << 1;  // ASTN section is AES-256-CBC encrypted
static constexpr uint16_t kFlagMangled    = 1u << 2;  // identifier names in STRT are obfuscated

// ------------------------------------------------------------------ section FourCCs (little-endian)

static constexpr uint32_t kSectionMeta = 0x4154454Du; // 'META'
static constexpr uint32_t kSectionStrt = 0x54525453u; // 'STRT'
static constexpr uint32_t kSectionAstn = 0x4E545341u; // 'ASTN'
static constexpr uint32_t kSectionSrcs = 0x53435253u; // 'SRCS' — UTF-8 source script (interim until AST layer)
static constexpr uint32_t kSectionHash = 0x48534148u; // 'HASH'

// ------------------------------------------------------------------ errors

enum class Error : int {
    Ok               = 0,
    BadMagic         = 1,  // first 8 bytes don't match expected magic
    BadFormatVersion = 2,  // fmt_major > kFmtMajor (loader too old)
    BadCRC           = 3,  // header_crc32 or a section crc32 mismatch
    BadHash          = 4,  // SHA-256 of file body doesn't match HASH section
    EngineVersionTooOld = 5, // running engine < library's min_engine
    Truncated        = 6,  // file ended before expected data
    OutOfMemory      = 7,
    IOError          = 8,  // file could not be opened / read / written
    InvalidStructure = 9,  // section data has internal inconsistencies
};

// Human-readable description of an error code.
const char *strerror(Error err) noexcept;

// ------------------------------------------------------------------ metadata

// Contents of the META section.  All fields are UTF-8 strings.
struct Meta {
    std::string name;           // display name, e.g. "My Library"
    std::string version;        // semver string, e.g. "1.0.0"
    std::string author;         // free-form author string
    std::string identifier;     // reverse-domain ID, e.g. "com.example.mylib"
    std::string description;    // short description (single line preferred)
    std::string min_engine;     // minimum engine version, e.g. "0.9.13"
};

// (ASTNode — removed in format version 1.1.  The ASTN section now contains
//  the raw output of MCHXTASTWriter::finalise(), identifiable by the 8-byte
//  magic "HXTASTN\0" at the start of the section data.  An empty astn_bytes
//  field means no pre-parsed AST is stored; the loader falls back to SRCS.)

// ------------------------------------------------------------------ document

// Top-level in-memory representation of one .hxtlib file.
// Populate this before calling write(); it is filled out by read().
struct Document {
    // --- header fields ---
    uint16_t flags       = 0;  // combination of kFlag* constants
    uint16_t eng_major   = 0;  // engine version that compiled this file
    uint16_t eng_minor   = 0;
    uint16_t eng_patch   = 0;
    uint64_t compiled_at = 0;  // Unix timestamp (seconds); 0 → set to now()

    // --- sections ---
    Meta                     meta;
    std::vector<std::string> string_table;  // STRT: high-level identifier strings

    // ASTN: raw MCHXTASTWriter output (starts with "HXTASTN\0" when present).
    // Populate via MCHandlerlist::hxt_serialize() before calling write().
    // Empty means "no pre-parsed AST"; the loader falls back to SRCS.
    std::vector<uint8_t> astn_bytes;

    // SRCS: raw UTF-8 source script — legacy fallback only.
    // The IDE's "Save as Library" leaves this empty; source is never stored
    // in IDE-produced libraries.  Populated only by the standalone hxtc tool
    // (which cannot link against the engine to produce ASTN bytes) and by any
    // older .hxtlib files already in the field.  The loader uses it only when
    // astn_bytes is absent or fails to deserialise.
    std::string source_script;

    // Intern a string into the string table and return its index.
    // If the string is already present the existing index is returned.
    uint32_t intern(const std::string &s);

    // Look up a string by index.  Returns an empty string if out of range.
    const std::string &get_string(uint32_t index) const;
};

// ------------------------------------------------------------------ I/O

// Serialise doc to path, creating or overwriting the file.
// If doc.compiled_at == 0 it is set to the current Unix time before writing.
Error write(const Document &doc, const std::string &path);

// Deserialise path into doc_out, fully reconstructing meta, string table,
// and ASTN bytes.  engine_{major,minor,patch} are the running engine's version;
// if the file's min_engine metadata exceeds them, EngineVersionTooOld is
// returned and doc_out is left in an unspecified state.
Error read(const std::string  &path,
           Document           &doc_out,
           uint16_t            engine_major,
           uint16_t            engine_minor,
           uint16_t            engine_patch);

// Like read() but stops after validating the header and metadata — the AST
// is not deserialised.  Use this for quick compatibility checks at load time.
Error validate(const std::string &path,
               uint16_t           engine_major,
               uint16_t           engine_minor,
               uint16_t           engine_patch);

} // namespace hxtlib
