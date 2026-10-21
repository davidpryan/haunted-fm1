#!/bin/sh
# Fetch the JieLi AC79 SDK (Apache-2.0) at the revision fm1-base was tested with: felucca-bsp's script, trying
# JieLi's GitHub mirror and then their gitee repository (one of them is usually reachable).
#   tools/get_sdk.sh [DEST]       (default: ~/.jieli/ac79-sdk)
set -e
cd "$(dirname "$0")/.."
DEST="${1:-$HOME/.jieli/ac79-sdk}"
if [ -f "$DEST/cpu/wl82/tools/uboot.boot" ]; then
    echo "AC79_SDK=$DEST (already there)"
    exit 0
fi
for url in https://github.com/Jieli-Tech/fw-AC79_AIoT_SDK https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git; do
    rm -rf "$DEST"
    if SDK_URL="$url" sh base/bsp/tools/get_sdk.sh "$DEST"; then
        exit 0
    fi
    echo "get_sdk: $url failed, trying the next"
done
echo "get_sdk: could not fetch the SDK" >&2
exit 1
