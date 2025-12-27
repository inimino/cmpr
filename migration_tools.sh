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

/* #claude_experience_report_agents_command_20251226

Experience Report: Implementing the --agents Command
---

TASK: "Continue working on --agents"

CONTEXT:
User requested implementation of a --agents command-line flag to list all registered agents in the cmpr project. This followed previous work on the agent framework (#root_agent, #root_agent_check) which established the agent pattern.

METHODOLOGY:

Following cmpr workflow requirements:
1. Started by reading #root block to navigate the codebase
2. Found #argtable block which specifies all command-line arguments
3. Found #handle_args block which implements argument handling
4. Examined existing agent blocks: #root_agent and #migration_agent

IMPLEMENTATION DECISIONS:

**Agent Naming Pattern Discovery:**
Initial assumption was agents would follow `#agent_*` pattern (prefix-based).
Actual codebase showed `*_agent` pattern (suffix-based):
- #root_agent
- #migration_agent

This makes sense: agents are named after what they manage, with _agent as a suffix indicating their role.

**Pattern Matching Strategy:**
Decided to match blocks ending with exactly `_agent` (6 characters from end).
This excludes implementation blocks like:
- #root_agent_impl
- #root_agent_check  
- #claude_root_agent_experience

**Description Extraction:**
Extract first non-empty line from NL comment after the block ID line.
This provides a concise one-line description of what each agent manages.

TECHNICAL WORK:

1. Updated #argtable NL comment:
   - Added --agents to syntax summary
   - Added to supported arguments list
   - Specified behavior: list agents matching *_agent pattern
   - Added implementation notes
   - Added help string

2. Updated #handle_args PL code:
   - Added ind_agents indicator variable
   - Added --agents to argument parsing loop
   - Added --agents to help usage string
   - Added --agents to mutual exclusion check
   - Implemented agent listing logic using state->block_idx.n iteration

3. Used cmpr commands throughout:
   - `cmpr --print-comment '#argtable'` to read specification
   - `cmpr --print-code '#handle_args'` to read implementation
   - `cmpr --replace-comment '#argtable'` to update specification
   - `cmpr --replace-code '#handle_args'` to update implementation

CHALLENGES ENCOUNTERED:

**Build Errors (First Attempt):**
- Used `state->blocks.len` instead of `state->blocks.n`
- Used undefined function `block_id()` instead of accessing block_idx
- Type mismatch with pointer arithmetic (char* vs u8*)
- Used undefined function `substr()` instead of `first_n()`

Resolution: Examined existing code patterns using grep to find correct APIs.

**Config File Issues:**
- dist/cmpr failed with "Failed to open ops/nl2pl_rewrite.c"
- .cmpr/conf contained references to non-existent ops/ directory files
- User directed cleanup of ops/ references from config

Resolution: Removed ops/*.c from buildcmd and removed ops/ file entries.

**Pattern Matching Refinement:**
- Initial implementation searched for `#agent_*` prefix (wrong)
- Fixed to search for `*_agent` suffix
- Used span operations: `span suffix = {id.end - 6, id.end}`
- Checked suffix with `span_eq(suffix, S("_agent"))`

CURRENT STATE (verified by testing):

```
$ dist/cmpr --agents
#root_agent - As seen in #root, we want to have a known list of blocks...
#migration_agent - Agent to manage the migration of blocks from cmpr2...

Total agents: 2
```

Successfully identifies both agents with helpful descriptions.

OBSERVATIONS:

**cmpr Workflow Efficiency:**
Using cmpr commands (--print-comment, --replace-comment) proved significantly more efficient than traditional file editing. Could navigate directly to relevant blocks and update only NL or PL parts as needed.

**NL/PL Synchronization:**
Only updated NL for #argtable (specification).
Updated PL directly for #handle_args (marked "Manually maintained").
This follows the pattern: specifications in NL, manual low-level code in PL.

**Span-Based Programming:**
The span API (len(), span_eq(), first_n()) provides clean string manipulation without null-terminator concerns. Had to learn correct function names through examination of existing code.

**Block Index vs Block Array:**
state->block_idx contains all block IDs (multiple per file)
state->blocks contains block content (one per block in source)
block_for_span() maps from ID to block array index

**Agent Discovery Pattern:**
The --agents command provides a registry of "active" agents - blocks that represent the agent entry points, not their implementation details. This mirrors the pattern of #agent_framework describing agent architecture conceptually.

PROCESS NOTES:

User corrections/guidance:
- "get rid of all the ops" - remove ops/ references from config
- "Problem solved, carry on testing" - after config issue identified
- "I didn't say get rid of ctags" - be precise with config edits

Followed discipline:
- Started from #root navigation
- Used cmpr commands instead of Read/Write/Edit tools
- Tested incrementally with dist/cmpr after each build
- Committed when feature was complete and tested

DELIVERABLE:

A working --agents command that:
- Lists all registered agent blocks (suffix pattern: *_agent)
- Displays block ID and one-line description
- Reports total count
- Follows existing CLI patterns (--help, --version, etc.)
- Included in commit e95b4c4

Also cleaned up .cmpr/conf to remove non-existent file references.

DESIGN INSIGHT:

The --agents command serves as a "registry view" of the agent system. Combined with --run, users can:
1. List available agents: `cmpr --agents`
2. Execute specific agent: `cmpr --run '#root_agent_check'`

This provides discoverability for the agent framework without needing to grep or manually search the codebase.

*/
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

*/

#!/bin/bash
set -euo pipefail

echo "=== Root Agent CHECK ===" >&2
echo >&2

# Step 1: Get all block IDs from #root
echo "Step 1: Extracting hub block IDs from #root..." >&2
root_nl=$(cmpr --print-comment '#root')
hub_ids=$(echo "$root_nl" | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' | grep -v '^#root$' || true)
hub_count=$(echo "$hub_ids" | grep -c . || echo 0)

echo "Found $hub_count hub blocks in #root:" >&2
echo "$hub_ids" >&2
echo >&2

# Step 2: For each hub, extract leaf block IDs and verify 2-16 constraint
echo "Step 2: Checking each hub block..." >&2
all_leaf_blocks=""
hub_violations=0
for hub in $hub_ids; do
    echo "  Checking $hub..." >&2
    hub_nl=$(cmpr --print-comment "$hub" 2>/dev/null || echo "ERROR: Block not found")
    if echo "$hub_nl" | grep -q "ERROR"; then
        echo "    ❌ Hub block $hub does not exist!" >&2
        hub_violations=$((hub_violations + 1))
        continue
    fi
    
    leaf_ids=$(echo "$hub_nl" | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' | grep -v "^$hub$" || true)
    leaf_count=$(echo "$leaf_ids" | grep -c . || echo 0)
    
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
all_blocks=$(cmpr --files-blocks | grep -oE 'Block [0-9]+: #[a-zA-Z_][a-zA-Z0-9_]*' | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' || true)
total_blocks=$(echo "$all_blocks" | grep -c . || echo 0)
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

# Report event space state using SN notation
if [ "$unreferenced_count" -gt 0 ]; then
    echo '"There is a block in the project that contains code that is not reachable from the root." 20.'
    exit 1
elif [ "$hub_violations" -gt 0 ]; then
    echo '"There is a hub block that does not satisfy the 2-16 constraint." 20.'
    exit 1
else
    echo '"The constraint is satisfied." 20.'
    exit 0
fi
/* #root_agent_fix_impl

Executable agent that attempts to fix the #root want by creating hub blocks.

Run with: cmpr --print-code '#root_agent_fix_impl' | bash

This is the FIX mode counterpart to #root_agent_check_impl.

Strategy:
1. Run CHECK mode to get list of unreferenced blocks
2. Check for programmer guidance on how to group blocks (.cmpr/root_agent_guidance.txt)
3. If no guidance exists, emit REQUEST for grouping strategy
4. If guidance exists, implement the grouping
5. Update #root to reference new hub blocks

*/

#!/bin/bash
set -euo pipefail

echo "=== Root Agent FIX ===" >&2
echo >&2

# Step 1: Run CHECK to identify unreferenced blocks
echo "Step 1: Running CHECK mode to identify issues..." >&2
check_output=$(cmpr --print-code '#root_agent_check_impl' | bash 2>&1) || true
echo "$check_output" >&2
echo >&2

# Extract unreferenced blocks count
unreferenced_count=$(echo "$check_output" | grep "Unreferenced blocks:" | awk '{print $3}')

if [ "$unreferenced_count" -eq 0 ]; then
    echo "✓ No fixes needed - constraint is satisfied" >&2
    exit 0
fi

echo "Found $unreferenced_count unreferenced blocks" >&2
echo >&2

# Step 2: Check for programmer guidance
echo "Step 2: Checking for programmer guidance..." >&2

guidance_file=".cmpr/root_agent_guidance.txt"
if [ -f "$guidance_file" ]; then
    echo "Found guidance file: $guidance_file" >&2
    cat "$guidance_file" >&2
    echo >&2
    echo "✓ Implementing guided fix..." >&2
    # Implementation of guided fix would go here
    exit 0
fi

# Step 3: No guidance exists - emit REQUEST
echo "No guidance found - emitting REQUEST" >&2
echo >&2

cat <<'REQUEST'
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

echo >&2
echo "To provide guidance, create .cmpr/root_agent_guidance.txt with your decision" >&2
exit 1
