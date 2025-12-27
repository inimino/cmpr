#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CMPR_BIN="$ROOT_DIR/dist/cmpr"

TESTDIR=$(mktemp -d)
trap 'rm -rf "$TESTDIR"' EXIT
cd "$TESTDIR"

"$CMPR_BIN" --init
"$CMPR_BIN" --T0
"$CMPR_BIN" --event "The block id is: #root" --strength 255
"$CMPR_BIN" --event "The block id is: #cmpr_events" --strength 255

output=$("$CMPR_BIN" --T)
expected=$'"The block id is: #root" 255.\n"The block id is: #cmpr_events" 255.'

if [[ "$output" != "$expected" ]]; then
  echo "FAIL: unexpected --T output"
  echo "expected:"$'\n'"$expected"
  echo "actual:"$'\n'"$output"
  exit 1
fi

if [[ ! -f .cmpr/T ]]; then
  echo "FAIL: .cmpr/T was not created"
  exit 1
fi

printf "%s\n" "$expected" > expected_T
if ! cmp -s expected_T .cmpr/T; then
  echo "FAIL: .cmpr/T contents mismatch"
  echo "contents:"$'\n'"$(cat .cmpr/T)"
  exit 1
fi

echo "PASS: test_events_basic"
