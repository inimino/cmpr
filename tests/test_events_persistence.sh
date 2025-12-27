#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CMPR_BIN="$ROOT_DIR/dist/cmpr"

TESTDIR=$(mktemp -d)
trap 'rm -rf "$TESTDIR"' EXIT
cd "$TESTDIR"

"$CMPR_BIN" --init
"$CMPR_BIN" --T0
"$CMPR_BIN" --event "persistence test" --strength 255

first_run=$("$CMPR_BIN" --T)
if [[ "$first_run" != '"persistence test" 255.' ]]; then
  echo "FAIL: event missing after first add"
  exit 1
fi

"$CMPR_BIN" --event "second event" --strength 255
second_run=$("$CMPR_BIN" --T)
expected_second=$'"persistence test" 255.\n"second event" 255.'
if [[ "$second_run" != "$expected_second" ]]; then
  echo "FAIL: events missing after second add"
  exit 1
fi

"$CMPR_BIN" --T0
third_run=$("$CMPR_BIN" --T)
if [[ -n "$third_run" ]]; then
  echo "FAIL: T0 did not clear events"
  exit 1
fi

fourth_run=$("$CMPR_BIN" --T)
if [[ -n "$fourth_run" ]]; then
  echo "FAIL: cleared state did not persist"
  exit 1
fi

echo "PASS: test_events_persistence"
