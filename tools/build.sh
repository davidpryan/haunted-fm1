#!/bin/sh
# Build one of the haunted firmwares with fm1-base (base/, a git submodule).
#   tools/build.sh NAME [--release X.Y] [--host | --web]      NAME: a directory under firmware/
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain; tools/get_toolchain.sh fetches it)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/.jieli/ac79-sdk; tools/get_sdk.sh fetches it)
set -e
cd "$(dirname "$0")/.."
FW="${1:?usage: tools/build.sh NAME [--release X.Y] [--host | --web]}"
shift
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/.jieli/ac79-sdk}"
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
[ -f "firmware/$FW/fm1.toml" ] || { echo "build.sh: no firmware/$FW/fm1.toml"; ls firmware; exit 1; }
exec "${PYTHON:-python3}" base/tools/build.py "firmware/$FW" "$@"
