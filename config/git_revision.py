#!/usr/bin/env python3
# Emit the current git HEAD revision hash for use in GYP variable expansion.
# If HEAD cannot be resolved (unborn branch, corrupted .git/HEAD, detached
# HEAD with no commits, etc.) emit a placeholder so that configure does not
# abort — a missing revision string is non-fatal for local development builds.

import subprocess
import sys
import os

def main():
    depth = sys.argv[1] if len(sys.argv) > 1 else '.'
    try:
        rev = subprocess.check_output(
            ['git', '-C', depth, 'rev-parse', 'HEAD'],
            stderr=open(os.devnull, 'w')
        ).decode().strip()
        if rev:
            print(rev)
            return
    except Exception:
        pass
    print('0000000')

if __name__ == '__main__':
    main()
