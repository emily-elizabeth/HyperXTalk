#!/usr/bin/env python3
"""Python replacement for encode_errors.pl - generates C string arrays from error enum headers"""
"""Usage: python3 encode_errors.py infolder outfile"""

import sys
import os
import re

def generate_errors_list(source_file, name):
    with open(source_file, 'r') as f:
        lines = f.readlines()

    errorStrings = 'const char * %s = \n\n\"' % name

    found = False
    for line in lines:
        # If the first word of the line is "enum" we have found the error list
        if re.match(r'^{', line):
            found = True
            continue

        # Continue reading lines until we get to the enum
        if not found:
            continue

        # End of the enum
        if '};' in line:
            break

        # only use lines with quoted strings
        # extract the string, add it to the result
        line_match = re.search('\".*\"', line)
        if line_match:
           noquotes = line_match.group().replace('"', '')
           noslash = noquotes.replace('\\', '\\\\')
           errorStrings += '%s\\n' % noslash

    errorStrings += '\";\n'
    return errorStrings

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
