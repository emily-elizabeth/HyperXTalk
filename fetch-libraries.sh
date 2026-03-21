#!/bin/sh
# Libraries are provided via the thirdparty git submodule.
# Nothing to fetch.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "note: prebuilt libraries already present via submodule (root: $SCRIPT_DIR)"
exit 0
