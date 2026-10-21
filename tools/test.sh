#!/bin/sh
# Run one firmware's host-simulator scenarios (tools/build.sh NAME --host first, or this builds it).
#   tools/test.sh NAME
set -u
cd "$(dirname "$0")/.."
FW="${1:?usage: tools/test.sh NAME}"
NAME="$(sed -n 's/^name *= *"\([^"]*\)".*/\1/p' "firmware/$FW/fm1.toml" | tr 'A-Z' 'a-z')"
HOST="firmware/$FW/build/host/${NAME}_host"
[ -x "$HOST" ] || sh tools/build.sh "$FW" --host || exit 1
FAIL=0
for s in firmware/$FW/tests/*.txt; do
    n=$(basename "$s" .txt)
    out="firmware/$FW/build/scenarios/$n"
    mkdir -p "$out"
    if "$HOST" "$s" "$out" > "$out/log.txt" 2>&1; then
        echo "  ok   $FW/$n"
    else
        echo "  FAIL $FW/$n (see $out/log.txt)"
        tail -12 "$out/log.txt" | sed 's/^/       /'
        FAIL=1
    fi
done
exit $FAIL
