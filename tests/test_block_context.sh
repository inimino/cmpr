#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CMPR_BIN="$ROOT_DIR/dist/cmpr"

TESTDIR=$(mktemp -d)
trap 'rm -rf "$TESTDIR"' EXIT
cd "$TESTDIR"

"$CMPR_BIN" --init

# Create synthetic block context data
# We don't need cmpr to scan files - we're testing the event system
# so we manually construct all 5 event spaces for a synthetic block

BLOCK_ID="#synthetic_test_block"
BLOCK_IDX="42"
BLOCK_CONTENT="/* #synthetic_test_block

This is a synthetic test block for validating block context workflow.

It has multiple lines to test that BC can handle multi-line content.
*/
void synthetic_function() {
    printf(\"Hello from synthetic block\\n\");
}"
BLOCK_SUMMARY="This is a synthetic test block for validating block context workflow."
BLOCK_REVTIME="2025-12-27T10:00:00+00:00"

# Step 1: Reset T and load all 5 event spaces (BID, BIX, BC, BS, BTS)
"$CMPR_BIN" --T0
"$CMPR_BIN" --event "The block id is: $BLOCK_ID" --strength 255
"$CMPR_BIN" --event "The block idx is: $BLOCK_IDX" --strength 255
"$CMPR_BIN" --event "The block content is: $BLOCK_CONTENT" --strength 255
"$CMPR_BIN" --event "The block summary is: $BLOCK_SUMMARY" --strength 255
"$CMPR_BIN" --event "The block revtime is: $BLOCK_REVTIME" --strength 255

# Step 2: Verify all 5 events are in T
output=$("$CMPR_BIN" --T)
event_count=$(echo "$output" | wc -l)

if [[ $event_count -ne 5 ]]; then
  echo "FAIL: Expected 5 events in T, got $event_count"
  echo "Output:"
  echo "$output"
  exit 1
fi

# Step 3: Verify each event space is present
for event_space in "The block id is:" "The block idx is:" "The block content is:" "The block summary is:" "The block revtime is:"; do
  if ! echo "$output" | grep -q "^\"$event_space"; then
    echo "FAIL: Missing event space: $event_space"
    echo "T output:"
    echo "$output"
    exit 1
  fi
done

# Step 4: Memorize the complete block context
"$CMPR_BIN" --memorize

# Step 5: Verify snapshot was created
if [[ ! -d .cmpr/events ]]; then
  echo "FAIL: .cmpr/events directory not created"
  exit 1
fi

snapshot_count=$(ls -1 .cmpr/events/ | wc -l)
if [[ $snapshot_count -ne 1 ]]; then
  echo "FAIL: Expected 1 snapshot, found $snapshot_count"
  exit 1
fi

# Step 6: Reset T and test recall with just BID (should restore all 5)
"$CMPR_BIN" --T0
"$CMPR_BIN" --event "The block id is: $BLOCK_ID" --strength 255

# Verify only 1 event in T before recall
before_recall=$("$CMPR_BIN" --T)
before_count=$(echo "$before_recall" | wc -l)
if [[ $before_count -ne 1 ]]; then
  echo "FAIL: Expected 1 event before recall, got $before_count"
  exit 1
fi

# Step 7: Recall should load all 5 event spaces
"$CMPR_BIN" --recall

# Step 8: Verify all 5 events were recalled
recalled_output=$("$CMPR_BIN" --T)
recalled_count=$(echo "$recalled_output" | wc -l)

if [[ $recalled_count -ne 5 ]]; then
  echo "FAIL: Expected 5 events after recall, got $recalled_count"
  echo "Recalled output:"
  echo "$recalled_output"
  exit 1
fi

# Step 9: Verify each event space is present after recall
for event_space in "The block id is:" "The block idx is:" "The block content is:" "The block summary is:" "The block revtime is:"; do
  if ! echo "$recalled_output" | grep -q "^\"$event_space"; then
    echo "FAIL: Missing event space after recall: $event_space"
    echo "Recalled output:"
    echo "$recalled_output"
    exit 1
  fi
done

# Step 10: Test recall with different event space (BIX instead of BID)
"$CMPR_BIN" --T0
"$CMPR_BIN" --event "The block idx is: $BLOCK_IDX" --strength 255
"$CMPR_BIN" --recall

recalled_via_bix=$("$CMPR_BIN" --T)
recalled_via_bix_count=$(echo "$recalled_via_bix" | wc -l)

if [[ $recalled_via_bix_count -ne 5 ]]; then
  echo "FAIL: Expected 5 events after recall via BIX, got $recalled_via_bix_count"
  echo "Recalled output:"
  echo "$recalled_via_bix"
  exit 1
fi

echo "PASS: test_block_context"
