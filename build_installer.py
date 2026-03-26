#!/usr/bin/env python3
"""
build_installer.py — Assembles HyperXTalkInstaller-1_0_0-Mac.app

Replicates what `_internal deploy macosx` + toolsBuilderMakeInstaller do:
  1. Reads the Installer.app engine binary (Mach-O, arm64)
  2. Builds a HyperXTalk capsule from:
       - installer.livecode (main installer stack)
       - revliburl.livecodescript  (auxiliary script-only stack)
       - installer_utilities.livecodescript (auxiliary script-only stack)
       - startup script string
  3. Deflate-compresses the capsule and patches the __PROJECT Mach-O segment
  4. Creates the full .app bundle structure with Info.plist, Installer.icns, payload
  5. Ad-hoc code-signs the result
  6. Places the finished app in _build/final/output/

Usage:
  python3 build_installer.py
"""

import hashlib
import os
import shutil
import struct
import subprocess
import sys
import zlib

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
REPO = os.path.dirname(os.path.abspath(__file__))
BUILD_MODE       = os.environ.get("MODE", "debug").capitalize()   # Debug or Release
# Code-signing identity.  Defaults to ad-hoc ("-").
# Override with your Developer ID, e.g.:
#   export CODESIGN_IDENTITY="Developer ID Application: Emily Howard (XXXXXXXXXX)"
# Never commit this value — set it in your shell profile or CI secrets.
CODESIGN_IDENTITY = os.environ.get("CODESIGN_IDENTITY", "-")
ENGINE_APP  = f"{REPO}/_build/mac/{BUILD_MODE}/Installer.app"
ENGINE_BIN  = f"{ENGINE_APP}/Contents/MacOS/Installer"
INSTALLER_STACK  = f"{REPO}/builder/installer.livecode"
REVLIBURL_STACK  = f"{REPO}/ide-support/revliburl.livecodescript"
INSTALLER_UTILS  = f"{REPO}/builder/installer_utilities.livecodescript"
INSTALLER_ICNS   = f"{ENGINE_APP}/Contents/Resources/Installer.icns"
DESCRIPTION_TXT  = f"{REPO}/Installer/description.txt"
LICENSE_TXT      = f"{REPO}/ide/License Agreement.txt"

VERSION   = "1_0_0"
EDITION   = "Community"
PLATFORM  = "macosx"
APP_NAME  = f"HyperXTalkInstaller-{VERSION}-Mac"
OUT_DIR   = f"{REPO}/_build/final/output"
WORK_DIR  = f"{REPO}/_build/final/work"
DEST      = OUT_DIR

# ---------------------------------------------------------------------------
# Capsule section type constants  (see capsule.h)
# ---------------------------------------------------------------------------
kEpilogue          = 0
kPrologue          = 1
kDigest            = 2
kMainStack         = 3
kScriptOnlyMain    = 4
kAuxStack          = 7
kScriptOnlyAux     = 8
kStartupScript     = 10
kEditionFlags      = 13   # 1-byte edition marker: 0x01 = Community

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def be32(v):
    return struct.pack(">I", v)

def le32(v):
    return struct.pack("<I", v)

def le64(v):
    return struct.pack("<Q", v)

def read_le32(data, off):
    return struct.unpack_from("<I", data, off)[0]

def read_le64(data, off):
    return struct.unpack_from("<Q", data, off)[0]

def pad4(n):
    """Return number of padding bytes needed to align n to 4-byte boundary."""
    return (4 - n % 4) % 4

# ---------------------------------------------------------------------------
# Build the uncompressed capsule stream
# ---------------------------------------------------------------------------
def make_capsule_stream(main_stack_data, aux_stacks, startup_script_str):
    """
    Returns (uncompressed_bytes, md5_digest_bytes) where md5_digest_bytes
    is the MD5 of everything BEFORE the Digest section.

    Section format (small sections, type < 128, length < 2^24):
        [uint32_be: (type << 24) | length] [data...] [pad-to-4-bytes]
    """
    buf = bytearray()
    md5 = hashlib.md5()

    def write_section(stype, data):
        header = struct.pack(">I", (stype << 24) | len(data))
        buf.extend(header)
        md5.update(header)
        buf.extend(data)
        md5.update(data)
        p = pad4(len(data))
        if p:
            buf.extend(b'\x00' * p)
            md5.update(b'\x00' * p)

    # 1. Prologue: banner_timeout=0, program_timeout=0  (8 bytes, big-endian)
    prologue_data = struct.pack(">II", 0, 0)
    write_section(kPrologue, prologue_data)

    # 1b. Edition flags: 0x01 = Community (matches original distributed installer)
    write_section(kEditionFlags, b'\x01')

    # 2. Main stack (binary .rev format → kMainStack)
    write_section(kMainStack, main_stack_data)

    # 3. Auxiliary stacks (script-only → kScriptOnlyAux)
    for aux_data in aux_stacks:
        write_section(kScriptOnlyAux, aux_data)

    # 4. Startup script (null-terminated UTF-8 string)
    startup_bytes = startup_script_str.encode("utf-8") + b'\x00'
    write_section(kStartupScript, startup_bytes)

    # 5. Digest: MD5 of everything written so far
    digest = md5.digest()          # 16 bytes
    digest_header = struct.pack(">I", (kDigest << 24) | 16)
    buf.extend(digest_header)
    # (We don't need to track MD5 from here on)
    buf.extend(digest)

    # 6. Epilogue (zero length, zero data)
    buf.extend(struct.pack(">I", 0))

    return bytes(buf)

# ---------------------------------------------------------------------------
# Compress the capsule stream
# ---------------------------------------------------------------------------
def compress_capsule(raw_bytes):
    """
    Raw deflate (windowBits=-15, same as zlib deflateInit2 -15).

    Trailing padding rules (two independent requirements both satisfied):

    1.  4-byte security stub:  MCCapsuleRead keeps a 4-byte margin at the end
        of its inflate input buffer (`avail_in = frontier - 4`).  Without at
        least 4 bytes after the deflate end-of-stream marker, inflate never
        sees the final bits of the stream and the capsule read stalls.

    2.  4-byte alignment:  MCCapsuleReadBuckets rounds its last read DOWN to a
        multiple of 4 (`t_total = buckets_available & ~3`).  Any remainder
        bytes stay in the bucket.  After processing the Epilogue section,
        MCCapsuleProcess checks `self->buckets != nil` and returns false
        (without setting MCresult → "Unknown error occurred") if any bytes are
        left.  So the total compressed+stub length must be divisible by 4.

    Combined fix: append at least 4 zero bytes, then pad further so the
    total is a multiple of 4.
    """
    compressor = zlib.compressobj(
        level=zlib.Z_DEFAULT_COMPRESSION,
        method=zlib.DEFLATED,
        wbits=-15,          # raw deflate, no header
        memLevel=8,
        strategy=zlib.Z_DEFAULT_STRATEGY,
    )
    compressed = compressor.compress(raw_bytes) + compressor.flush()
    # stub_size >= 4, and (len(compressed) + stub_size) % 4 == 0
    stub_size = 4 + ((-len(compressed)) % 4)
    return compressed + b'\x00' * stub_size

# ---------------------------------------------------------------------------
# Parse the 64-bit Mach-O binary and patch __PROJECT
# ---------------------------------------------------------------------------
MH_MAGIC_64 = 0xFEEDFACF

LC_SEGMENT_64 = 0x19

def macho_align(x, align=0x4000):
    """Align to the page size used by the Mach-O builder.
    Apple Silicon (arm64) uses 16 KiB pages; segments must be 0x4000-aligned.
    Using 0x1000 (4 KiB) causes the kernel to SIGKILL the binary on launch."""
    return (x + align - 1) & ~(align - 1)

class Segment64:
    # sizeof(segment_command_64) = 4+4+16+8+8+8+8+4+4+4+4 = 72 bytes
    HDR_SIZE = 72
    SECTION_SIZE = 80  # sizeof(section_64)

    def __init__(self, data, offset):
        self.offset = offset
        (self.cmd, self.cmdsize, segname_raw,
         self.vmaddr, self.vmsize,
         self.fileoff, self.filesize,
         self.maxprot, self.initprot,
         self.nsects, self.flags) = struct.unpack_from("<II16sQQQQIIII", data, offset)
        self.segname = segname_raw.rstrip(b'\x00').decode('ascii', errors='replace')
        # Sections start immediately after the 72-byte segment_command_64 header
        self.sections = []
        sec_off = offset + self.HDR_SIZE
        for _ in range(self.nsects):
            self.sections.append(sec_off)
            sec_off += self.SECTION_SIZE

def patch_macho(engine_data, project_data):
    """
    Replicate MCDeployToMacOSXMainBody for a 64-bit little-endian Mach-O.

    project_data = uint32_le(project_size) + compressed_capsule

    Returns the patched binary bytes.
    """
    data = bytearray(engine_data)

    magic = read_le32(data, 0)
    assert magic == MH_MAGIC_64, f"Not a 64-bit Mach-O (magic=0x{magic:08x})"

    # Mach header fields
    # struct mach_header_64: magic(4), cputype(4), cpusubtype(4), filetype(4),
    #                        ncmds(4), sizeofcmds(4), flags(4), reserved(4)
    ncmds      = read_le32(data, 16)
    sizeofcmds = read_le32(data, 20)

    # Walk load commands
    cmd_offset = 32  # sizeof(mach_header_64)
    project_seg = None
    linkedit_seg = None

    for _ in range(ncmds):
        cmd  = read_le32(data, cmd_offset)
        size = read_le32(data, cmd_offset + 4)
        if cmd == LC_SEGMENT_64:
            seg = Segment64(data, cmd_offset)
            if seg.segname == "__PROJECT":
                project_seg = seg
            elif seg.segname == "__LINKEDIT":
                linkedit_seg = seg
        cmd_offset += size

    assert project_seg  is not None, "__PROJECT segment not found"
    assert linkedit_seg is not None, "__LINKEDIT segment not found"

    # --- Build output ---
    out = bytearray()

    # Part 1: everything up to __PROJECT.fileoff
    proj_start = project_seg.fileoff
    out.extend(data[:proj_start])

    # Part 2: the new project data
    new_project_size = macho_align(len(project_data))
    out.extend(project_data)
    # Pad to page-aligned size
    out.extend(b'\x00' * (new_project_size - len(project_data)))

    # Part 3: __LINKEDIT and everything after (from the original binary)
    old_linkedit_start = linkedit_seg.fileoff
    out.extend(data[old_linkedit_start:])

    # --- Update load commands in the output buffer ---
    # Delta between where __LINKEDIT now lives vs where it used to
    new_linkedit_start = proj_start + new_project_size
    file_delta = new_linkedit_start - old_linkedit_start

    # Update __PROJECT segment header
    _patch_segment64(out, project_seg, new_project_size, new_project_size, new_project_size)

    # Update all load commands at/after __LINKEDIT
    cmd_offset = 32
    for _ in range(ncmds):
        cmd  = read_le32(out, cmd_offset)
        size = read_le32(out, cmd_offset + 4)
        if cmd == LC_SEGMENT_64:
            seg = Segment64(out, cmd_offset)
            if seg.segname == "__LINKEDIT":
                _shift_segment64(out, cmd_offset, file_delta)
            # dyld info, symtab, dysymtab, etc. all store offsets into __LINKEDIT
        elif cmd == 0x02:   # LC_SYMTAB
            _shift_linkedit_cmd(out, cmd_offset, file_delta, 8)   # symoff
            _shift_linkedit_cmd(out, cmd_offset, file_delta, 16)  # stroff
        elif cmd == 0x0B:   # LC_DYSYMTAB
            _shift_dysymtab(out, cmd_offset, file_delta)
        elif cmd in (0x22, 0x80000022):  # LC_DYLD_INFO, LC_DYLD_INFO_ONLY
            _shift_dyld_info(out, cmd_offset, file_delta)
        elif cmd in (0x26, 0x29, 0x2B,   # LC_FUNCTION_STARTS, LC_DATA_IN_CODE, LC_DYLIB_CODE_SIGN_DRS
                     0x1D,               # LC_CODE_SIGNATURE
                     0x80000033,         # LC_DYLD_EXPORTS_TRIE
                     0x80000034):        # LC_DYLD_CHAINED_FIXUPS
            _shift_linkedit_data_cmd(out, cmd_offset, file_delta)
        cmd_offset += size

    return bytes(out)

def _shift_u32_at(buf, off, delta):
    """Add delta to the little-endian uint32 at buf[off]."""
    v = read_le32(buf, off)
    struct.pack_into("<I", buf, off, v + delta)

def _shift_u64_at(buf, off, delta):
    v = read_le64(buf, off)
    struct.pack_into("<Q", buf, off, v + delta)

def _patch_segment64(buf, seg, new_filesize, new_vmsize, new_section_size):
    """Overwrite filesize and vmsize in an LC_SEGMENT_64 command.

    segment_command_64 layout (offsets from start of load command):
      0:  cmd       (uint32)
      4:  cmdsize   (uint32)
      8:  segname   (char[16])
      24: vmaddr    (uint64)  <- do NOT change
      32: vmsize    (uint64)  <- update
      40: fileoff   (uint64)  <- do NOT change
      48: filesize  (uint64)  <- update
      56: maxprot   (uint32)
      60: initprot  (uint32)
      64: nsects    (uint32)
      68: flags     (uint32)
    """
    base = seg.offset
    struct.pack_into("<Q", buf, base + 32, new_vmsize)    # vmsize
    struct.pack_into("<Q", buf, base + 48, new_filesize)  # filesize
    # Update the single section's size field.
    # section_64 layout from start of section:
    #   0:  sectname (char[16])
    #   16: segname  (char[16])
    #   32: addr     (uint64)
    #   40: size     (uint64)  <- update
    #   48: offset   (uint32)
    if seg.sections:
        sec_base = seg.sections[0]
        struct.pack_into("<Q", buf, sec_base + 40, new_section_size)

def _shift_segment64(buf, cmd_off, delta):
    """Shift fileoff and vmaddr of a segment and all its sections."""
    # segment_command_64:
    #   24: vmaddr  (uint64)
    #   40: fileoff (uint64)
    #   64: nsects  (uint32)
    _shift_u64_at(buf, cmd_off + 24, delta)   # vmaddr (at +24)
    _shift_u64_at(buf, cmd_off + 40, delta)   # fileoff (at +40)
    nsects = read_le32(buf, cmd_off + 64)     # nsects (at +64 inside segment_command_64)
    # section_64 structs start at cmd_off + sizeof(segment_command_64) = cmd_off + 72
    sec_off = cmd_off + 72
    for _ in range(nsects):
        # section_64:
        #   32: addr   (uint64)
        #   40: size   (uint64)
        #   48: offset (uint32)
        _shift_u64_at(buf, sec_off + 32, delta)   # addr
        _shift_u32_at(buf, sec_off + 48, delta)   # offset
        sec_off += 80

def _shift_linkedit_cmd(buf, cmd_off, delta, field_off):
    """Shift a single uint32 offset field within a load command."""
    v = read_le32(buf, cmd_off + field_off)
    if v:
        struct.pack_into("<I", buf, cmd_off + field_off, v + delta)

def _shift_dysymtab(buf, cmd_off, delta):
    """Shift file-offset fields in LC_DYSYMTAB (skip count fields at 60, 68).
    dysymtab_command file offsets: tocoff(32), modtaboff(40), extrefsymoff(48),
    indirectsymoff(56), extreloff(64), locreloff(72).
    Fields at 60 (nindirectsyms) and 68 (nextrel) are counts, not offsets."""
    for field in (32, 40, 48, 56, 64, 72):
        _shift_linkedit_cmd(buf, cmd_off, delta, field)

def _shift_dyld_info(buf, cmd_off, delta):
    """Shift file-offset fields in LC_DYLD_INFO / LC_DYLD_INFO_ONLY."""
    for field in (8, 16, 24, 32, 40):
        _shift_linkedit_cmd(buf, cmd_off, delta, field)

def _shift_linkedit_data_cmd(buf, cmd_off, delta):
    """Shift dataoff in LC_FUNCTION_STARTS, LC_DATA_IN_CODE, etc."""
    _shift_linkedit_cmd(buf, cmd_off, delta, 8)

# ---------------------------------------------------------------------------
# Build the Info.plist for the installer .app
# ---------------------------------------------------------------------------
def make_info_plist():
    # Prefer builder/mac_info.plist: has LSBackgroundOnly=true, correct bundle ID, NSPrincipalClass, etc.
    builder_plist = os.path.join(REPO, "builder", "mac_info.plist")
    if os.path.exists(builder_plist):
        with open(builder_plist, "rb") as f:
            return f.read()
    # Fallback: use the engine's own Info.plist
    engine_plist = os.path.join(ENGINE_APP, "Contents", "Info.plist")
    if os.path.exists(engine_plist):
        with open(engine_plist, "rb") as f:
            return f.read()
    raise FileNotFoundError("No Info.plist source found")

# ---------------------------------------------------------------------------
# Build a real payload zip from locally available sources
# ---------------------------------------------------------------------------
def create_payload(payload_path):
    """
    Creates the installer payload zip from mac-bin (pre-built engine binaries)
    and ide/ide-support (IDE scripts).

    The payload is a standard zip with:
      - manifest.txt  — tab-delimited install map (type\\tdest\\tzip_name\\t)
      - file000000 … fileNNNNNN — the actual files

    Manifest entry types:
      folder     \\t dest
      file       \\t dest \\t zip_name \\t
      executable \\t dest \\t zip_name \\t

    Variables in dest paths (replaced at install time by the installer):
      [[installFolder]]           — user-chosen install root (.app bundle)
      [[installFolder]]/Contents/Tools — IDE tools/resources
    """
    import zipfile

    print("Building payload zip …")

    MAC_BIN      = f"{REPO}/mac-bin"
    IDE_DIR      = f"{REPO}/ide"
    IDE_SUPPORT  = f"{REPO}/ide-support"

    if not os.path.isdir(MAC_BIN):
        print("  ERROR: mac-bin directory missing")
        return
    if not os.path.isdir(IDE_DIR):
        print("  ERROR: ide directory missing")
        return

    manifest_lines = []
    counter = [0]

    def next_name():
        n = f"file{counter[0]:06d}"
        counter[0] += 1
        return n

    def add_folder_entry(dest):
        manifest_lines.append(f"folder\t{dest}")

    def add_file_entry(src, dest, zf, is_exec=False):
        if not os.path.exists(src):
            return False
        zname = next_name()
        zf.write(src, zname)
        typ = "executable" if is_exec else "file"
        manifest_lines.append(f"{typ}\t{dest}\t{zname}\t")
        return True

    def is_executable(path):
        return os.access(path, os.X_OK)

    def add_tree(src_dir, dest_prefix, zf, force_exec=False, exclude_dirs=None):
        """
        Recursively add all files from src_dir → dest_prefix in the zip.
        Generates folder + file manifest entries.

        exclude_dirs: optional set/list of directory names to skip entirely
                      (matched against the bare directory name, not the full path).
                      E.g. exclude_dirs={'_CodeSignature'} avoids bundling a stale
                      Apple code-signature that will be invalidated once the installer
                      adds Contents/Tools/* into the same bundle.  A fresh ad-hoc
                      signature is applied by the installer after all files land.
        """
        if not os.path.isdir(src_dir):
            print(f"  WARNING: source dir missing: {src_dir}")
            return
        add_folder_entry(dest_prefix)
        for root, dirs, files in os.walk(src_dir, followlinks=True):
            dirs.sort()
            if exclude_dirs:
                dirs[:] = [d for d in dirs if d not in exclude_dirs]
            rel = os.path.relpath(root, src_dir)
            if rel != ".":
                add_folder_entry(f"{dest_prefix}/{rel}")
            for fname in sorted(files):
                src = os.path.join(root, fname)
                rel_file = os.path.relpath(src, src_dir)
                dest = f"{dest_prefix}/{rel_file}"
                exec_flag = force_exec or is_executable(src)
                add_file_entry(src, dest, zf, is_exec=exec_flag)

    def add_single(src, dest, zf, is_exec=None):
        if is_exec is None:
            is_exec = is_executable(src)
        if os.path.isdir(src):
            add_tree(src, dest, zf, force_exec=is_exec)
        else:
            add_file_entry(src, dest, zf, is_exec=is_exec)

    TF  = "[[installFolder]]"          # TargetFolder  = install root (.app)
    SF  = f"{TF}/Contents/Tools"       # SupportFolder = ToolsFolder

    with zipfile.ZipFile(payload_path, "w", zipfile.ZIP_DEFLATED,
                         compresslevel=6) as zf:

        # ----------------------------------------------------------------
        # 1. Engine.MacOSX
        #    Recursively install mac-bin/HyperXTalk.app → TF
        #    Skip _CodeSignature: the signature only covers the original
        #    bundle files.  Once the installer adds Contents/Tools/** the
        #    old CodeResources becomes stale/invalid, confusing macOS.
        #    The installer re-signs the bundle with codesign after all
        #    files have been extracted (see installer_utilities.livecodescript).
        # ----------------------------------------------------------------
        print("  Adding Engine …")
        lc_app = f"{MAC_BIN}/HyperXTalk.app"
        add_tree(lc_app, TF, zf, force_exec=True, exclude_dirs={'_CodeSignature'})

        # Extra files that go alongside the engine binary
        for fname in ["revpdfprinter.bundle", "revsecurity.dylib"]:
            src = f"{MAC_BIN}/{fname}"
            add_single(src, f"{TF}/Contents/MacOS/{fname}", zf, is_exec=True)

        # ----------------------------------------------------------------
        # 2. Toolset:  ide/Toolset/** → SF/Toolset
        # ----------------------------------------------------------------
        print("  Adding Toolset …")
        add_tree(f"{IDE_DIR}/Toolset", f"{SF}/Toolset", zf)

        # edition.txt
        zname = next_name()
        zf.writestr(zname, "Community")
        manifest_lines.append(f"file\t{SF}/edition.txt\t{zname}\t")

        # IDE-support library scripts → SF/Toolset/libraries
        add_folder_entry(f"{SF}/Toolset/libraries")
        for fname in [
            "revdeploylibraryandroid.livecodescript",
            "revdeploylibraryios.livecodescript",
            "revdeploylibraryemscripten.livecodescript",
            "revliburl.livecodescript",
            "revsaveasandroidstandalone.livecodescript",
            "revsaveasemscriptenstandalone.livecodescript",
            "revsaveasiosstandalone.livecodescript",
            "revsaveasstandalone.livecodescript",
            "revsblibrary.livecodescript",
            "revhtml5urllibrary.livecodescript",
            "revdocsparser.livecodescript",
        ]:
            src = f"{IDE_SUPPORT}/{fname}"
            if os.path.exists(src):
                add_file_entry(src, f"{SF}/Toolset/libraries/{fname}", zf)

        # ----------------------------------------------------------------
        # 3. Toolchain.MacOSX: lc-compile, lc-run, modules → SF/Toolchain
        # ----------------------------------------------------------------
        print("  Adding Toolchain …")
        add_folder_entry(f"{SF}/Toolchain")
        for fname in ["lc-compile", "lc-run", "lc-compile-ffi-java"]:
            src = f"{MAC_BIN}/{fname}"
            if os.path.exists(src):
                add_file_entry(src, f"{SF}/Toolchain/{fname}", zf, is_exec=True)
        modules_dir = f"{MAC_BIN}/modules"
        if os.path.isdir(modules_dir):
            add_tree(modules_dir, f"{SF}/Toolchain/modules", zf)

        # ----------------------------------------------------------------
        # 4. Externals.MacOSX → SF/Externals
        # ----------------------------------------------------------------
        print("  Adding Externals …")
        ext_dir = f"{SF}/Externals"
        add_folder_entry(ext_dir)
        for bundle in ["revspeech.bundle", "revxml.bundle",
                       "revbrowser.bundle", "revzip.bundle"]:
            src = f"{MAC_BIN}/{bundle}"
            add_single(src, f"{ext_dir}/{bundle}", zf, is_exec=True)

        # ----------------------------------------------------------------
        # 5. Databases.MacOSX → SF/Externals  +  SF/Externals/Database Drivers
        # ----------------------------------------------------------------
        for bundle in ["revdb.bundle"]:
            src = f"{MAC_BIN}/{bundle}"
            add_single(src, f"{ext_dir}/{bundle}", zf, is_exec=True)

        db_dir = f"{ext_dir}/Database Drivers"
        add_folder_entry(db_dir)
        for bundle in ["dbmysql.bundle", "dbodbc.bundle",
                       "dbpostgresql.bundle", "dbsqlite.bundle"]:
            src = f"{MAC_BIN}/{bundle}"
            add_single(src, f"{db_dir}/{bundle}", zf, is_exec=True)

        # ----------------------------------------------------------------
        # 6. Mobile.MacOSX: reviphone.bundle, revandroid.bundle → SF/Externals
        # ----------------------------------------------------------------
        for bundle in ["reviphone.bundle", "revandroid.bundle"]:
            src = f"{MAC_BIN}/{bundle}"
            add_single(src, f"{ext_dir}/{bundle}", zf, is_exec=True)

        # ----------------------------------------------------------------
        # 7. Runtime.MacOSX: HyperXTalk-Standalone.app → Runtime/Mac OS X/arm64
        # ----------------------------------------------------------------
        print("  Adding Runtimes …")
        rt_mac = f"{SF}/Runtime/Mac OS X/arm64"
        if os.path.isdir(f"{MAC_BIN}/HyperXTalk-Standalone.app"):
            add_tree(f"{MAC_BIN}/HyperXTalk-Standalone.app",
                     f"{rt_mac}/Standalone.app", zf, force_exec=True)
            add_folder_entry(f"{rt_mac}/Support")
            for fname in ["revpdfprinter.bundle", "revsecurity.dylib"]:
                src = f"{MAC_BIN}/{fname}"
                add_single(src, f"{rt_mac}/Support/{fname}", zf, is_exec=True)

        # ----------------------------------------------------------------
        # 8. Misc: License/about/Open-Source-Licenses → SF
        # ----------------------------------------------------------------
        print("  Adding Misc …")
        for fname in ["License Agreement.txt", "about.txt",
                      "Open Source Licenses.txt"]:
            src = f"{IDE_DIR}/{fname}"
            if os.path.exists(src):
                add_file_entry(src, f"{SF}/{fname}", zf)

        # ----------------------------------------------------------------
        # 9. Plugins → SF/Plugins
        # ----------------------------------------------------------------
        plugins_dir = f"{IDE_DIR}/Plugins"
        if os.path.isdir(plugins_dir):
            add_tree(plugins_dir, f"{SF}/Plugins", zf)

        # ----------------------------------------------------------------
        # 10. Resources → SF/Resources
        # ----------------------------------------------------------------
        resources_dir = f"{IDE_DIR}/Resources"
        if os.path.isdir(resources_dir):
            add_tree(resources_dir, f"{SF}/Resources", zf)

        # ----------------------------------------------------------------
        # 11. Extensions (packaged_extensions) → SF/Extensions
        # ----------------------------------------------------------------
        print("  Adding Extensions …")
        ext_pkg = f"{MAC_BIN}/packaged_extensions"
        if os.path.isdir(ext_pkg):
            add_tree(ext_pkg, f"{SF}/Extensions", zf)

        # ----------------------------------------------------------------
        # 12. Uninstaller → SF/LiveCode Setup.app
        #     Note: must live inside Contents/Tools, NOT at the bundle root,
        #     so it does not break the app bundle's code signature seal.
        # ----------------------------------------------------------------
        stub = f"{MAC_BIN}/installer-stub"
        if os.path.exists(stub):
            add_file_entry(stub, f"{SF}/LiveCode Setup.app", zf, is_exec=True)

        # ----------------------------------------------------------------
        # Write manifest.txt
        # ----------------------------------------------------------------
        manifest_content = "\n".join(manifest_lines) + "\n"
        zf.writestr("manifest.txt", manifest_content)
        print(f"  manifest.txt: {len(manifest_lines)} entries")

    size_mb = os.path.getsize(payload_path) / (1024 * 1024)
    print(f"  Payload: {payload_path} ({size_mb:.1f} MB)")

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(WORK_DIR, exist_ok=True)

    # ------------------------------------------------------------------
    # 1. Read source files
    # ------------------------------------------------------------------
    print("Reading source files …")
    with open(INSTALLER_STACK, "rb") as f:
        main_stack = f.read()
    assert main_stack[:4] == b"REVO", "installer.livecode is not a binary stack"

    with open(REVLIBURL_STACK, "rb") as f:
        revliburl = f.read()
    # Strip UTF-8 BOM if present
    if revliburl[:3] == b'\xef\xbb\xbf':
        revliburl = revliburl[3:]

    with open(INSTALLER_UTILS, "rb") as f:
        utils = f.read()
    if utils[:3] == b'\xef\xbb\xbf':
        utils = utils[3:]

    with open(ENGINE_BIN, "rb") as f:
        engine_data = f.read()

    # ------------------------------------------------------------------
    # 2. Build the capsule
    # ------------------------------------------------------------------
    print("Building capsule …")
    startup_script = (
        'send "extensionInitialize" to stack "revLibUrl"\n'
        'insert script of stack "InstallerUtilities" into back'
    )
    raw_capsule = make_capsule_stream(main_stack, [revliburl, utils], startup_script)
    print(f"  Uncompressed capsule: {len(raw_capsule):,} bytes")

    compressed = compress_capsule(raw_capsule)
    print(f"  Compressed capsule:   {len(compressed):,} bytes")

    # Project = uint32_le(project_size) + compressed capsule
    project_size = 4 + len(compressed)
    project_data = struct.pack("<I", project_size) + compressed
    print(f"  Project block:        {len(project_data):,} bytes")

    # ------------------------------------------------------------------
    # 3. Patch the Mach-O binary
    # ------------------------------------------------------------------
    print("Patching Mach-O binary …")
    patched_binary = patch_macho(engine_data, project_data)
    print(f"  Patched binary size: {len(patched_binary):,} bytes")

    # ------------------------------------------------------------------
    # 4. Create the .app bundle
    # ------------------------------------------------------------------
    app_path = os.path.join(OUT_DIR, f"{APP_NAME}.app")
    if os.path.exists(app_path):
        shutil.rmtree(app_path)

    print(f"Creating bundle: {app_path}")
    macos_dir     = os.path.join(app_path, "Contents", "MacOS")
    resources_dir = os.path.join(app_path, "Contents", "Resources")
    os.makedirs(macos_dir,     exist_ok=True)
    os.makedirs(resources_dir, exist_ok=True)

    # Info.plist
    with open(os.path.join(app_path, "Contents", "Info.plist"), "wb") as f:
        f.write(make_info_plist())

    # PkgInfo
    with open(os.path.join(app_path, "Contents", "PkgInfo"), "wb") as f:
        f.write(b"APPLREVO")

    # Engine binary
    bin_out = os.path.join(macos_dir, "Installer")
    with open(bin_out, "wb") as f:
        f.write(patched_binary)
    os.chmod(bin_out, 0o755)

    # Copy resource files from source engine .app
    for src_item in ["Installer.icns", "HyperXTalk.rsrc", "LiveCode-Community.rsrc"]:
        src = os.path.join(ENGINE_APP, "Contents", "Resources", src_item)
        if os.path.exists(src):
            shutil.copy2(src, resources_dir)
    # Copy language localisation .lproj dirs
    for item in os.listdir(os.path.join(ENGINE_APP, "Contents", "Resources")):
        if item.endswith(".lproj"):
            src = os.path.join(ENGINE_APP, "Contents", "Resources", item)
            dst = os.path.join(resources_dir, item)
            shutil.copytree(src, dst)

    # Payload
    payload_path = os.path.join(resources_dir, "payload")
    create_payload(payload_path)

    # ------------------------------------------------------------------
    # 5. Remove .setup.txt from bundle root before signing.
    #    The installer writes this file at install time to track the
    #    installation for the uninstaller. If present at signing time it
    #    breaks the bundle signature seal ("unsealed contents present in
    #    the bundle root"). It is safe to remove here — it will be
    #    written fresh by the installer when the user installs.
    # ------------------------------------------------------------------
    setup_txt = os.path.join(app_path, ".setup.txt")
    if os.path.exists(setup_txt):
        os.remove(setup_txt)
        print("Removed .setup.txt from bundle root before signing.")

    # ------------------------------------------------------------------
    # 6. Ad-hoc code-sign
    # ------------------------------------------------------------------
    identity_label = "ad-hoc" if CODESIGN_IDENTITY == "-" else CODESIGN_IDENTITY
    print(f"Code-signing ({identity_label}) …")
    subprocess.run(["xattr", "-cr", app_path], capture_output=True)
    result = subprocess.run(
        ["codesign", "--force", "--deep", "--sign", CODESIGN_IDENTITY, app_path],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"  ERROR: codesign failed (rc={result.returncode}):")
        print(f"    stdout: {result.stdout.strip()}")
        print(f"    stderr: {result.stderr.strip()}")
        sys.exit(1)
    print("  Signed successfully.")
    # Verify the signature
    verify = subprocess.run(
        ["codesign", "-v", "--verbose=1", app_path],
        capture_output=True, text=True
    )
    if verify.returncode != 0:
        print(f"  WARNING: codesign verify failed: {verify.stderr.strip()}")

    print(f"\n✓ Installer: {app_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
