#!/usr/bin/env python3
"""
Fallback generator for icudata-minimal.cpp when icupkg.exe is not available.

Usage: python3 generate_icudata_cpp.py <icudt58l.dat> <out_dir>

Copies <icudt58l.dat> to <out_dir>/data/icudata-minimal.dat and encodes it
as <out_dir>/src/icudata-minimal.cpp using the same format as encode_data.pl.
"""

import sys
import os

def encode_dat_to_cpp(dat_path, cpp_path, var_name='s_icudata'):
    with open(dat_path, 'rb') as f:
        data = f.read()

    length = len(data)
    rows = []
    for i in range(0, length, 16):
        chunk = data[i:i+16]
        rows.append(', '.join('0x{:02x}'.format(b) for b in chunk))

    array_body = ',\n\t'.join(rows)

    cpp = (
        'alignas(16) unsigned char {}[] = \n{{\n\t{}\n}};\n'
        'unsigned int {}_length = {};\n'
    ).format(var_name, array_body, var_name, length)

    os.makedirs(os.path.dirname(cpp_path), exist_ok=True)
    with open(cpp_path, 'w') as f:
        f.write(cpp)

    print('Generated {} ({} bytes)'.format(cpp_path, length))


def main():
    if len(sys.argv) < 3:
        print('Usage: generate_icudata_cpp.py <icudt58l.dat> <out_dir>')
        sys.exit(1)

    dat_src  = sys.argv[1]
    out_dir  = sys.argv[2]

    dat_dst = os.path.join(out_dir, 'data', 'icudata-minimal.dat')
    cpp_dst = os.path.join(out_dir, 'src',  'icudata-minimal.cpp')

    os.makedirs(os.path.dirname(dat_dst), exist_ok=True)

    import shutil
    shutil.copy2(dat_src, dat_dst)
    print('Copied {} -> {}'.format(dat_src, dat_dst))

    encode_dat_to_cpp(dat_dst, cpp_dst)


if __name__ == '__main__':
    main()
