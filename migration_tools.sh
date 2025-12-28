/* #cmpr2_to_cmpr1_migration

"We want cmpr1 to have the essential blocks from cmpr2 that are referenced by cmpr1's wants and overview blocks." 20.

Context:
- cmpr1 (C implementation) is the open-source version
- cmpr2 (Python implementation) is more feature-complete and private
- cmpr1 should have blocks it references but not necessarily ALL cmpr2 blocks
- See #cmpr2_via_cmpr1 for how to access cmpr2 blocks

Event space: Block Migration Status

The want above defines an event space for each block referenced by cmpr1:
  "The block id is: {blockid}"
  "The block exists in cmpr1"
or
  "The block id is: {blockid}"
  "The block is missing from cmpr1"

The migration_agent maintains this want by:
- CHECK: Identifying blocks referenced by cmpr1 wants/overviews but missing from cmpr1
- FIX: Migrating approved blocks from cmpr2 using #extract_block_from_cmpr2

Current state (2025-12-27):
- 374 blocks in cmpr1
- 1033 blocks in cmpr2  
- 747 blocks in cmpr2 but not in cmpr1

The migration agent (#migration_agent) helps identify which blocks should be migrated by:
1. Listing blocks only in cmpr2
2. Checking if they're referenced by cmpr1 wants/overviews
3. Presenting candidates for human approval
4. Using #extract_block_from_cmpr2 for approved migrations

Progress tracking:
- 2025-12-27: Agent fixed to use --files-blocks, now functional
- Previous: Migrated 5 core blocks + helpers (see #claude_experience_report_cmpr2_parity_20251227)

Next actions:
1. Run migration agent to identify candidates
2. Review cmpr1 overview blocks for broken references
3. Migrate needed blocks one by one
4. Update confidence level as progress is made

*/
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
  cmpr --print-code '#migration_agent' | bash

*/
#!/bin/bash
# Migration agent implementation

set -e

echo "=== cmpr2 to cmpr1 Migration Agent ===" >&2
echo "" >&2

# Get block lists using --files-blocks and extract block IDs
# Filter to only include lines with actual block IDs (starting with #)
echo "Scanning blocks in both repositories..." >&2
CMPR1_BLOCKS=$(dist/cmpr --files-blocks | grep '^Block' | awk '{print $3}' | grep '^#')
CMPR2_BLOCKS=$(cd ../cmpr && cmpr --files-blocks | grep '^Block' | awk '{print $3}' | grep '^#')

# Find blocks only in cmpr2
CMPR2_ONLY=$(comm -13 <(echo "$CMPR1_BLOCKS" | sort) <(echo "$CMPR2_BLOCKS" | sort))

echo "" >&2
echo "Blocks in cmpr2 but not in cmpr1:" >&2
echo "$CMPR2_ONLY"
echo "" >&2
echo "Total: $(echo "$CMPR2_ONLY" | wc -l) blocks" >&2

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
/* #root_agent_check_impl

Executable agent that checks if the #root want is satisfied.

Run with: cmpr --print-code '#root_agent_check_impl' | bash

Returns exit code 0 if satisfied, 1 if not.

Reports event space state using SN notation.

Integrates with T (transient memory):
- Clears T at the start (--T0) to start fresh
- Writes agent metadata to T (agent name, mode, timestamp)
- Writes CHECK results to T (hub count, violations, unreferenced count, status)
- Memorizes snapshot after check (for historical tracking)

The script should:
1. Clear T with --T0 (fresh start for this check run)
2. Add event: "Agent: root_agent" to T
3. Add event: "Mode: CHECK" to T
4. Add event: "Timestamp: <ISO8601>" to T
5. Run the check logic (count hubs, check coverage, etc.)
6. Add event: "Hub blocks: N" to T
7. Add event: "Hub violations: N" to T
8. Add event: "Unreferenced blocks: N" to T
9. Add event: "Status: constraint satisfied" or "Status: constraint not satisfied" to T
10. Memorize the snapshot
11. Output the main SN line (for backward compatibility)
12. Return appropriate exit code

T is designed to hold summary state for ONE run, not accumulate hundreds of block IDs.
For per-block tracking, use a separate loop pattern with --T0/--memorize per block.

Manually maintained.

*/
#!/bin/bash
set -euo pipefail

# Clear T to start fresh
dist/cmpr --T0 2>/dev/null || true

# Integrate with T: Write agent metadata
dist/cmpr --event "Agent: root_agent" --strength 255 2>/dev/null || true
dist/cmpr --event "Mode: CHECK" --strength 255 2>/dev/null || true
dist/cmpr --event "Timestamp: $(date -Iseconds)" --strength 255 2>/dev/null || true

echo "=== Root Agent CHECK ===" >&2
echo >&2

# Step 1: Get all block IDs from #root
echo "Step 1: Extracting hub block IDs from #root..." >&2
root_nl=$(dist/cmpr --print-comment '#root')
hub_ids=$(echo "$root_nl" | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' | grep -v '^#root$' || true)
hub_count=$(echo "$hub_ids" | wc -w)

echo "Found $hub_count hub blocks in #root:" >&2
echo "$hub_ids" >&2
echo >&2

# Step 2: For each hub, extract leaf block IDs and verify 2-16 constraint
echo "Step 2: Checking each hub block..." >&2
all_leaf_blocks=""
hub_violations=0
for hub in $hub_ids; do
    echo "  Checking $hub..." >&2
    hub_nl=$(dist/cmpr --print-comment "$hub" 2>/dev/null || echo "ERROR: Block not found")
    if echo "$hub_nl" | grep -q "ERROR"; then
        echo "    ❌ Hub block $hub does not exist!" >&2
        hub_violations=$((hub_violations + 1))
        continue
    fi
    
    leaf_ids=$(echo "$hub_nl" | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' | grep -v "^$hub$" || true)
    leaf_count=$(echo "$leaf_ids" | wc -w)
    
    if [ "$leaf_count" -lt 2 ] || [ "$leaf_count" -gt 16 ]; then
        echo "    ❌ Has $leaf_count blocks (should be 2-16)" >&2
        hub_violations=$((hub_violations + 1))
    else
        echo "    ✓ Has $leaf_count blocks" >&2
    fi
    
    all_leaf_blocks="$all_leaf_blocks"$'\n'"$leaf_ids"
done
echo >&2

# Step 3: Get all blocks in project
echo "Step 3: Getting all blocks in project..." >&2
all_blocks=$(dist/cmpr --files-blocks | grep -oE 'Block [0-9]+: #[a-zA-Z_][a-zA-Z0-9_]*' | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' || true)
total_blocks=$(echo "$all_blocks" | wc -w)
echo "Total named blocks in project: $total_blocks" >&2
echo >&2

# Step 4: Check coverage
echo "Step 4: Checking coverage..." >&2
unreferenced=""
for block in $all_blocks; do
    # Skip #root and hubs
    if [ "$block" = "#root" ]; then
        continue
    fi
    if echo "$hub_ids" | grep -qF "$block"; then
        continue
    fi
    
    # Check if in leaf blocks
    if ! echo "$all_leaf_blocks" | grep -qF "$block"; then
        unreferenced="$unreferenced $block"
    fi
done

unreferenced_count=$(echo "$unreferenced" | wc -w)
if [ "$unreferenced_count" -gt 0 ]; then
    echo "❌ Found $unreferenced_count unreferenced blocks" >&2
else
    echo "✓ All blocks are referenced" >&2
fi
echo >&2

echo "=== Summary ===" >&2
echo "Hub blocks: $hub_count" >&2
echo "Hub violations: $hub_violations" >&2
echo "Unreferenced blocks: $unreferenced_count" >&2
echo >&2

# Write results to T
dist/cmpr --event "Hub blocks: $hub_count" --strength 255 2>/dev/null || true
dist/cmpr --event "Hub violations: $hub_violations" --strength 255 2>/dev/null || true
dist/cmpr --event "Unreferenced blocks: $unreferenced_count" --strength 255 2>/dev/null || true

# Report event space state using SN notation
if [ "$unreferenced_count" -gt 0 ]; then
    dist/cmpr --event "Status: constraint not satisfied" --strength 255 2>/dev/null || true
    echo '"There is a block in the project that contains code that is not reachable from the root." 20.'
    
    # Memorize this check
    dist/cmpr --memorize 2>/dev/null || true
    exit 1
elif [ "$hub_violations" -gt 0 ]; then
    dist/cmpr --event "Status: constraint not satisfied (hub violations)" --strength 255 2>/dev/null || true
    echo '"There is a hub block that does not satisfy the 2-16 constraint." 20.'
    
    # Memorize this check
    dist/cmpr --memorize 2>/dev/null || true
    exit 1
else
    dist/cmpr --event "Status: constraint satisfied" --strength 255 2>/dev/null || true
    echo '"The constraint is satisfied." 20.'
    
    # Memorize this check
    dist/cmpr --memorize 2>/dev/null || true
    exit 0
fi
/* #root_agent_fix_impl

Executable agent that attempts to fix the #root want by creating hub blocks.

Run with: cmpr --print-code '#root_agent_fix_impl' | bash

This is the FIX mode counterpart to #root_agent_check_impl.

Strategy:
1. Run CHECK mode to get list of unreferenced blocks
2. Check for programmer guidance on how to group blocks (.cmpr/root_agent_guidance.txt)
3. If no guidance exists, write REQUEST to .cmpr/requests/ and emit to stdout
4. If guidance exists, implement the grouping
5. Update #root to reference new hub blocks

Requests are saved to: .cmpr/requests/YYYYMMDD-HHMMSS_root_agent_DECISION_NEEDED.txt

Integrates with T (transient memory):
- Clears T at the start (--T0) to start fresh
- Runs CHECK as subprocess (CHECK writes to T but we discard that)
- Writes FIX agent metadata to T AFTER CHECK completes
- Records each action taken (guidance found, request emitted, etc.)
- Memorizes snapshot after FIX attempt

The script should:
1. Clear T with --T0 (fresh start for this FIX run)
2. Run CHECK to identify issues (CHECK will write to T and memorize its own snapshot)
3. After CHECK completes, write FIX metadata to T:
   - "Agent: root_agent"
   - "Mode: FIX"
   - "Timestamp: <ISO8601>"
4. Run the fix logic
5. For each hub created: Add event: "Created hub: #hub_name with N blocks" to T
6. If request emitted: Add event: "Request emitted: DECISION_NEEDED" to T
7. Add event: "Status: fix completed" or "Status: awaiting guidance" to T
8. Memorize the snapshot
9. Return appropriate exit code

Note: FIX gets its own clean T state separate from CHECK. Both memorize independently.

Manually maintained.

*/
#!/bin/bash
set -euo pipefail

# Clear T to start fresh
dist/cmpr --T0 2>/dev/null || true

echo "=== Root Agent FIX ===" >&2
echo >&2

# Step 1: Run CHECK to identify unreferenced blocks
echo "Step 1: Running CHECK mode to identify issues..." >&2
check_output=$(dist/cmpr --print-code '#root_agent_check_impl' | bash 2>&1) || true
echo "$check_output" >&2
echo >&2

# Now write FIX metadata to T (after CHECK has completed and used T)
dist/cmpr --event "Agent: root_agent" --strength 255 2>/dev/null || true
dist/cmpr --event "Mode: FIX" --strength 255 2>/dev/null || true
dist/cmpr --event "Timestamp: $(date -Iseconds)" --strength 255 2>/dev/null || true

# Extract unreferenced blocks count
unreferenced_count=$(echo "$check_output" | grep "Unreferenced blocks:" | awk '{print $3}')

if [ "$unreferenced_count" -eq 0 ]; then
    echo "✓ No fixes needed - constraint is satisfied" >&2
    dist/cmpr --event "Status: no fix needed (constraint satisfied)" --strength 255 2>/dev/null || true
    dist/cmpr --memorize 2>/dev/null || true
    exit 0
fi

echo "Found $unreferenced_count unreferenced blocks" >&2
echo >&2

dist/cmpr --event "Issue found: $unreferenced_count unreferenced blocks" --strength 255 2>/dev/null || true

# Step 2: Check for programmer guidance
echo "Step 2: Checking for programmer guidance..." >&2

guidance_file=".cmpr/root_agent_guidance.txt"
if [ -f "$guidance_file" ]; then
    echo "Found guidance file: $guidance_file" >&2
    cat "$guidance_file" >&2
    echo >&2
    echo "✓ Implementing guided fix..." >&2
    
    dist/cmpr --event "Guidance found: following programmer instructions" --strength 255 2>/dev/null || true
    dist/cmpr --event "Status: implementing guided fix" --strength 255 2>/dev/null || true
    dist/cmpr --memorize 2>/dev/null || true
    
    # Implementation of guided fix would go here
    exit 0
fi

# Step 3: No guidance exists - write REQUEST to persistent storage
echo "No guidance found - submitting REQUEST to programmer" >&2
echo >&2

# Create requests directory if it doesn't exist
mkdir -p .cmpr/requests

# Generate timestamp in format: YYYYMMDD-HHMMSS
timestamp=$(date +%Y%m%d-%H%M%S)
request_file=".cmpr/requests/${timestamp}_root_agent_DECISION_NEEDED.txt"

# Write REQUEST to file
cat > "$request_file" <<'REQUEST'
REQUEST: DECISION_NEEDED
AGENT: #root_agent
PRIORITY: MEDIUM
CONTEXT: 221+ blocks are unreferenced from #root. Need to create hub blocks to organize them.
OPTIONS:
  - Option A: Group by file (create hub per source file)
  - Option B: Group by functionality (create hubs like #cmpr_c_core, #cmpr_py_api, #frontend)
  - Option C: Group by subsystem (create hubs like #parsing, #io, #ui, #agents, #revisions)
  - Option D: Manual - programmer will create hubs manually
RATIONALE: Organizing 221+ blocks requires understanding the codebase architecture and intended structure. This is a one-time architectural decision that will shape future navigation.
REQUEST

# Also emit to stdout
cat "$request_file"

echo >&2
echo "✓ REQUEST saved to: $request_file" >&2
echo "To provide guidance, create .cmpr/root_agent_guidance.txt with your decision" >&2

# Write to T that we emitted a request
dist/cmpr --event "Request emitted: DECISION_NEEDED" --strength 255 2>/dev/null || true
dist/cmpr --event "Request file: $request_file" --strength 255 2>/dev/null || true
dist/cmpr --event "Status: awaiting programmer guidance" --strength 255 2>/dev/null || true
dist/cmpr --memorize 2>/dev/null || true

exit 1
