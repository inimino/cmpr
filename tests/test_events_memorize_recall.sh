#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CMPR_BIN="$ROOT_DIR/dist/cmpr"

TESTDIR=$(mktemp -d)
trap 'rm -rf "$TESTDIR"' EXIT
cd "$TESTDIR"

"$CMPR_BIN" --init
"$CMPR_BIN" --T0
"$CMPR_BIN" --event "state A event" --strength 255
"$CMPR_BIN" --memorize
first_snapshot=$(ls .cmpr/events)

"$CMPR_BIN" --event "state B event" --strength 255
"$CMPR_BIN" --memorize
second_snapshot=$(ls .cmpr/events | sort | tail -n 1)

if [[ "$first_snapshot" == "$second_snapshot" ]]; then
  echo "FAIL: second memorize did not create new snapshot"
  exit 1
fi

"$CMPR_BIN" --T0

"$CMPR_BIN" --recall
recalled=$("$CMPR_BIN" --T)
expected=$'"state A event" 255.\n"state B event" 255.'
if [[ "$recalled" != "$expected" ]]; then
  echo "FAIL: recall did not load latest snapshot"
  exit 1
fi

echo "PASS: test_events_memorize_recall"
