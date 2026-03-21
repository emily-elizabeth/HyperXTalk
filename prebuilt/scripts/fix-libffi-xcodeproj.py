#!/usr/bin/env python3
"""
Fix duplicate/bad libffi references in Xcode project files.

The problem: each project.pbxproj has Frameworks build phases that contain 2-3 libffi.a references:
1. A PBXReferenceProxy (isa = PBXReferenceProxy) - BAD, resolves to _build/mac/Debug/libffi.a
2. A PBXFileReference pointing to /opt/homebrew/Cellar/libffi/.../libffi.a - BAD, Homebrew path
3. A PBXFileReference pointing to a relative path containing "prebuilt/lib/mac/libffi.a" - GOOD, keep this

This script removes the bad references from Frameworks build phases.
"""

import re
import sys
from pathlib import Path
from collections import defaultdict

def find_all_pbxproj_files(build_mac_dir):
    """Find all project.pbxproj files under build-mac/"""
    pbxproj_files = list(Path(build_mac_dir).rglob("project.pbxproj"))
    return sorted(pbxproj_files)

def extract_bad_file_references(content):
    """
    Find IDs of bad libffi file references:
    - PBXReferenceProxy entries for libffi.a
    - PBXFileReference entries with /opt/homebrew or Cellar/libffi path

    Returns a set of bad file reference IDs.
    """
    bad_refs = set()

    # Pattern 1: PBXReferenceProxy for libffi.a
    # Example: D40323692CD907D662BEA069 /* libffi.a */ = {
    #             isa = PBXReferenceProxy;
    #             fileType = archive.ar;
    #             path = libffi.a;
    proxy_pattern = r'([A-F0-9]{24})\s*/\*\s*libffi\.a\s*\*/\s*=\s*\{\s*isa\s*=\s*PBXReferenceProxy;'
    for match in re.finditer(proxy_pattern, content):
        bad_refs.add(match.group(1))

    # Pattern 2: PBXFileReference for libffi.a with /opt/homebrew or Cellar/libffi path
    # Example: 5F0901C3FFA1C4D1F810BA09 /* libffi.a */ = {isa = PBXFileReference; lastKnownFileType = archive.ar; name = libffi.a; path = /opt/homebrew/Cellar/libffi/3.5.2/lib/libffi.a; sourceTree = "<group>"; };
    homebrew_pattern = r'([A-F0-9]{24})\s*/\*\s*libffi\.a\s*\*/\s*=\s*\{isa\s*=\s*PBXFileReference;[^}]*path\s*=\s*[^}]*(?:/opt/homebrew|Cellar/libffi)[^}]*\};'
    for match in re.finditer(homebrew_pattern, content):
        bad_refs.add(match.group(1))

    return bad_refs

def extract_bad_build_file_ids(content, bad_file_refs):
    """
    Find PBXBuildFile IDs that reference bad file references.

    Pattern: XXXXXXXX /* libffi.a in Frameworks */ = {isa = PBXBuildFile; fileRef = <bad_file_ref>;

    Returns a set of bad PBXBuildFile IDs.
    """
    bad_build_file_ids = set()

    # For each bad file reference, find PBXBuildFile entries that reference it
    for file_ref in bad_file_refs:
        pattern = rf'([A-F0-9]{{24}})\s*/\*\s*libffi\.a\s*in\s*Frameworks\s*\*/\s*=\s*\{{isa\s*=\s*PBXBuildFile;\s*fileRef\s*=\s*{file_ref}'
        for match in re.finditer(pattern, content):
            bad_build_file_ids.add(match.group(1))

    return bad_build_file_ids

def remove_from_frameworks_build_phases(content, bad_build_file_ids):
    """
    Remove bad build file IDs from PBXFrameworksBuildPhase "files = (...)" sections.

    Handles both cases:
    - Entries on their own line: "\t\t\t\tAAA /* libffi.a in Frameworks */,\n"
    - Multiple entries on same line: "AAA /* libffi.a in Frameworks */,\t\tBBB /* libffi.a in Frameworks */,"

    Returns the modified content and the count of removals.
    """
    if not bad_build_file_ids:
        return content, 0

    removal_count = 0

    # For each bad build file ID, remove it from the files list
    for build_file_id in bad_build_file_ids:
        # Pattern 1: Entry on its own line (with optional trailing comma and newline)
        # Matches: 				CC6C5A2760ABC4D1844FFF6C /* libffi.a in Frameworks */,
        pattern_own_line = rf'\s*{build_file_id}\s*/\*\s*libffi\.a\s*in\s*Frameworks\s*\*/,?\n'

        new_content = re.sub(pattern_own_line, '', content)
        if new_content != content:
            removal_count += 1
            content = new_content
            continue

        # Pattern 2: Entry on same line with others, preceded by comma and/or whitespace
        # Matches: ",\t\t\t\tCC6C5A2760ABC4D1844FFF6C /* libffi.a in Frameworks */,"
        # This handles the case where multiple IDs are on the same line
        pattern_same_line_after = rf',\s*{build_file_id}\s*/\*\s*libffi\.a\s*in\s*Frameworks\s*\*/(?=,)'

        new_content = re.sub(pattern_same_line_after, '', content)
        if new_content != content:
            removal_count += 1
            content = new_content
            continue

        # Pattern 3: Entry at beginning of same-line list (followed by comma and more entries)
        # Matches: "CC6C5A2760ABC4D1844FFF6C /* libffi.a in Frameworks */,\t\t\t\t"
        pattern_same_line_start = rf'{build_file_id}\s*/\*\s*libffi\.a\s*in\s*Frameworks\s*\*/,\s*'

        new_content = re.sub(pattern_same_line_start, '', content)
        if new_content != content:
            removal_count += 1
            content = new_content

    return content, removal_count

def fix_pbxproj_file(pbxproj_path):
    """
    Fix a single project.pbxproj file.

    Returns a tuple of (was_modified, summary_dict)
    """
    # Read the file
    with open(pbxproj_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    original_content = content

    # Step 1: Find bad file references
    bad_file_refs = extract_bad_file_references(content)

    if not bad_file_refs:
        return False, {"file": str(pbxproj_path), "bad_file_refs": 0, "bad_build_files": 0, "removals": 0}

    # Step 2: Find bad PBXBuildFile IDs that reference bad files
    bad_build_file_ids = extract_bad_build_file_ids(content, bad_file_refs)

    # Step 3: Remove from Frameworks build phases
    content, removal_count = remove_from_frameworks_build_phases(content, bad_build_file_ids)

    # Write back if modified
    was_modified = (content != original_content)
    if was_modified:
        with open(pbxproj_path, 'w', encoding='utf-8') as f:
            f.write(content)

    return was_modified, {
        "file": str(pbxproj_path),
        "bad_file_refs": len(bad_file_refs),
        "bad_build_files": len(bad_build_file_ids),
        "removals": removal_count,
        "modified": was_modified
    }

def main():
    build_mac_dir = "/sessions/hopeful-stoic-knuth/mnt/HyperXTalk/build-mac"

    pbxproj_files = find_all_pbxproj_files(build_mac_dir)

    if not pbxproj_files:
        print(f"Error: No project.pbxproj files found under {build_mac_dir}")
        sys.exit(1)

    print(f"Found {len(pbxproj_files)} project.pbxproj files")
    print()

    total_modified = 0
    results = []

    for pbxproj_path in pbxproj_files:
        was_modified, summary = fix_pbxproj_file(pbxproj_path)
        results.append(summary)

        if was_modified:
            total_modified += 1
            relative_path = pbxproj_path.relative_to(build_mac_dir)
            print(f"✓ {relative_path}")
            print(f"  Bad file references found: {summary['bad_file_refs']}")
            print(f"  Bad build file IDs found: {summary['bad_build_files']}")
            print(f"  Removals from Frameworks: {summary['removals']}")
            print()

    print("=" * 80)
    print(f"Summary: Modified {total_modified} out of {len(pbxproj_files)} files")

    if total_modified == 0:
        print("No files needed modification.")

    return 0 if total_modified >= 0 else 1

if __name__ == "__main__":
    sys.exit(main())
