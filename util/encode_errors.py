#!/usr/bin/env python3
"""Python replacement for encode_errors.pl - generates C string arrays from error enum headers"""
"""Usage: python3 encode_errors.py infolder outfile"""

import sys
import os
import re

def generate_errors_list(source_file, name):
    with open(source_file, 'r') as f:
        lines = f.readlines()

    # Collect all error strings first
    entries = []
    found = False
    for line in lines:
        if re.match(r'^{', line):
            found = True
            continue
        if not found:
            continue
        if '};' in line:
            break
        line_match = re.search('\".*\"', line)
        if line_match:
            noquotes = line_match.group().replace('"', '')
            noslash = noquotes.replace('\\', '\\\\')
            entries.append('%s\\n' % noslash)

    # MSVC limits a single string literal to 16380 characters.  Split the
    # output into adjacent string literals so the compiler concatenates them
    # at compile time without hitting that limit.
    CHUNK = 200  # entries per literal chunk — well within the size limit
    chunks = []
    for i in range(0, max(len(entries), 1), CHUNK):
        chunk_entries = entries[i:i + CHUNK]
        chunks.append('\"' + ''.join(chunk_entries) + '\"')

    result = 'const char * %s =\n' % name
    result += '\n'.join(chunks)
    result += ';\n'
    return result

# Need to generate the error lists for both the parse and execution errors
path = sys.argv[1]
output_file = sys.argv[2]

output = ""
output += generate_errors_list(os.path.join(path, "executionerrors.h"), "MCexecutionerrors")
output += "\n"
output += generate_errors_list(os.path.join(path, "parseerrors.h"), "MCparsingerrors")

# Write out the error lists
os.makedirs(os.path.dirname(output_file), exist_ok=True)
with open(output_file, 'w') as f:
    f.write(output)
