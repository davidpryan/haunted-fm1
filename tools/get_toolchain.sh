#!/bin/sh
# Fetch JieLi's Linux toolchain (clang for pi32v2) with fm1-base's script.
#   tools/get_toolchain.sh [DEST]       (default: ~/.jieli; the toolchain is DEST/toolchain)
set -e
cd "$(dirname "$0")/.."
DEST="${1:-$HOME/.jieli}"
if [ -x "$DEST/toolchain/pi32v2/bin/clang" ]; then
    echo "JIELI_TOOLCHAIN=$DEST/toolchain (already there)"
    exit 0
fi
exec sh base/tools/get_toolchain.sh "$DEST"
