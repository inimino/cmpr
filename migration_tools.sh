/* #migration_agent

Agent to manage the migration of blocks from cmpr2 to cmpr1 with human approval.

This agent helps satisfy the want in #cmpr2_to_cmpr1_migration.

Process:
1. Identify blocks that exist in cmpr2 but not in cmpr1
2. For each block, determine if it should be migrated based on:
   - Is it referenced by wants or overview blocks in cmpr1?
   - Does it provide functionality needed for cmpr1?
3. Present candidates to human for approval
4. Extract approved blocks using #extract_block_from_cmpr2
5. Update references in overview blocks

Justifies: #cmpr2_to_cmpr1_migration

Usage:
  cmpr --run '#migration_agent'

*/
#!/bin/bash
# Migration agent implementation

set -e

echo "=== cmpr2 to cmpr1 Migration Agent ==="
echo ""

# Get block lists
echo "Scanning blocks in both repositories..."
CMPR1_BLOCKS=$(dist/cmpr cmpr.c --list-blocks)
CMPR2_BLOCKS=$(dist/cmpr ../cmpr/cmpr.c --list-blocks)

# Find blocks only in cmpr2
CMPR2_ONLY=$(comm -13 <(echo "$CMPR1_BLOCKS" | sort) <(echo "$CMPR2_BLOCKS" | sort))

echo ""
echo "Blocks in cmpr2 but not in cmpr1:"
echo "$CMPR2_ONLY" | head -20
echo ""
echo "Total: $(echo "$CMPR2_ONLY" | wc -l) blocks"
echo ""

# Show migration status
echo "Migration status from #cmpr2_to_cmpr1_migration:"
cmpr --print-comment '#cmpr2_to_cmpr1_migration' | grep -A 10 "Progress tracking"

echo ""
echo "Next steps:"
echo "1. Review overview blocks to identify needed implementation blocks"
echo "2. For each needed block, use: cmpr --print-code '#extract_block_from_cmpr2' | sh -s -- BLOCKID AFTER_BLOCKID"
echo "3. Update #cmpr2_to_cmpr1_migration progress tracking"

/* #extract_block_from_cmpr2

Script to extract a block from cmpr2 (/home/admin/cmpr/cmpr.c) and insert it into cmpr1.

Usage:
  cmpr --print-code '#extract_block_from_cmpr2' | sh -s -- BLOCKID AFTER_BLOCKID

Example:
  cmpr --print-code '#extract_block_from_cmpr2' | sh -s -- '#design_docs_overview' '#llm_integration_overview'

This will:
1. Extract the block with ID BLOCKID from /home/admin/cmpr/cmpr.c
2. Insert it into the current project after the block AFTER_BLOCKID

*/
#!/bin/bash
# Extract a block from cmpr2 and add it to cmpr1

BLOCKID="$1"
AFTER="$2"
CMPR2="/home/admin/cmpr/cmpr.c"

if [ -z "$BLOCKID" ] || [ -z "$AFTER" ]; then
    echo "Usage: $0 BLOCKID AFTER_BLOCKID" >&2
    echo "Example: $0 '#design_docs_overview' '#llm_integration_overview'" >&2
    exit 1
fi

# Extract block from cmpr2
# Find the block starting with /* BLOCKID and ending with the closing */
# Then pipe it to cmpr --after to insert it
awk -v blockid="$BLOCKID" '
    $0 == "/* " blockid {flag=1}
    flag {print}
    flag && $0 == "*/" {flag=0; exit}
' "$CMPR2" | cmpr --after "$AFTER"
