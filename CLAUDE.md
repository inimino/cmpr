# CLAUDE.md

Guidance to Claude Code.

## Overview

**cmpr1 vs cmpr2**: This is the cmpr1 codebase (open source). A nearby directory contains cmpr2 (SaaS product). To access cmpr2 blocks: `cd ../cmpr; cmpr --print-comment '#foo'` etc.

## Overview

cmpr provides code block database features.

**CLAUDE.md Principle**: This file teaches HOW to work with cmpr (commands, workflow, principles), not WHAT the codebase contains (structure, subsystems, features). Code navigability belongs in the navigable block structure itself. Almost no specific block IDs should appear in CLAUDE.md - navigation information lives in the code, accessed by reading the root block and following references.

## Building and Testing

**Standard workflow for source changes**:

```bash
# Regenerate cmpr.c from blocks, then build
scripts/cmpr-c-from-cmpr-src-c
make
dist/cmpr --version  # Verify it built

# Test your changes
dist/cmpr [commands to test]

# When satisfied, install
sudo make install
```

**Key insight**: The mistake to avoid is running `make` without eventually running `sudo make install`, then continuing to use the old installed `cmpr`.

**Build commands**:
```bash
# Regenerate cmpr.c from blocks, then build
scripts/cmpr-c-from-cmpr-src-c
make

# Test the build
dist/cmpr --version
dist/cmpr [test commands]

# Install once satisfied
sudo make install

# Check what's installed
cmpr --version
```

**When to use which binary**:
- `dist/cmpr` - For testing changes immediately after `make`
- `cmpr` - For normal operations with the installed version
- After you're happy with `dist/cmpr` testing, install it with `sudo make install`

**Adding new blocks to cmpr.c**:
- New blocks must be created IN cmpr.c (not INBOX.c) for compilation
- Add the block ID to `cmpr-c-build` to include it in the build
- The build script reads blocks from cmpr.c and assembles them

**Getting Help**:
- `cmpr --help` - Print usage summary
- `cmpr --help topics` - List all available help topics
- `cmpr --help <topic>` - Get detailed help on a specific topic (e.g., `cmpr --help events`, `cmpr --help editing`)

## Code Updates

**MANDATORY FIRST STEP FOR EVERY TASK**:

When the user asks you to work on ANY task related to this codebase, you MUST:

1. **START by reading the root block**: `cmpr --print-comment` with the root block ID
2. **NAVIGATE using block references**: Follow block IDs mentioned in the output (2-3 hops to reach any part of codebase)
3. **NEVER use the Task tool with Explore subagent** - it uses traditional tools and defeats the entire cmpr workflow
4. **NEVER start with grep/find/Read/Glob** - these are fallbacks for when cmpr navigation fails

**Example of CORRECT workflow**:
```
User: "I want to work on the agent system"
Assistant: [Immediately runs] cmpr --print-comment on the root block
Assistant: [Sees reference to agent hub, follows it] cmpr --print-comment on that hub
Assistant: [Now understands the structure and can navigate to specific blocks]
```

**Example of WRONG workflow**:
```
User: "I want to work on the agent system"
Assistant: [Uses Task tool with Explore subagent] ❌ WRONG
Assistant: [Uses grep to search for "agent"] ❌ WRONG
Assistant: [Uses Glob to find agent files] ❌ WRONG
```

**Always prefer cmpr commands over traditional text tools** (grep, sed, cat, etc.) when working with the cmpr codebase.

**Why use cmpr commands**:
- They understand the block structure
- They are way more efficient, because you can get directly from the root block to any other block in the codebase in ~ log n steps

**Navigation Commands**:
- `cmpr --print-block '#id'` - Show entire block (NL + PL)
- `cmpr --print-comment '#id'` - Show only NL comment
- `cmpr --print-code '#id'` - Show only PL code
- `cmpr --grep 'pattern'` - Search for pattern across blocks (PREFER THIS for searching)
- `cmpr --find-block 'search_term'` - Find blocks containing text (deprecated, use --grep instead)
- `cmpr --files-blocks` - Overview of all blocks in the project

**List blocks in a specific file**:
```bash
cmpr --files-blocks | grep -A 1000 'file: rvs_lib.py' | grep -B 1000 -m 1 '^file:' | head -n -1
```

This grep pipeline extracts just the blocks for a specific file from the `--files-blocks` output.

**Editing Commands**:
- `cmpr --replace '#id'` - Replace entire block (NL + PL) from stdin; for blocks with no PL part, this effectively replaces just the NL
- `cmpr --replace-comment '#id'` - Replace only NL part
- `cmpr --replace-code '#id'` - Replace only PL part
- `cmpr --rewritepl '#id'` - Regenerate PL from NL using nl2pl
- `cmpr --after '#id'` - Add a new block after the given block ID, reading contents from stdin

There should be --before but there isn't.
This is annoying when you want to make a new block be the first one in a file.
The workaround is: cat the new block then the current first block of the file separated by a newline, into --replace <id of first block>.

**INBOX Pattern for Staging New Blocks**:

The #INBOX block serves as a staging area for new blocks during development:

```bash
# Add a new block after INBOX
<something> | cmpr --after '#INBOX'
# Check the INBOX
cmpr --files-blocks | grep -A 1000 'file: INBOX.c' | grep -B 1000 -m 1 '^file:' | head -n -1
```

This pattern:
- Provides a known location for rapid iteration without deciding final placement
- Keeps new work (experience reports, experiments) organized
- Blocks in INBOX should be moved to appropriate locations during review sessions
- Use the block moving pattern (save, delete, insert) to relocate staged blocks

When to use INBOX:
- Experience reports documenting work sessions
- Experimental or exploratory blocks
- New documentation blocks whose final home is unclear
- Any block where you want to defer the navigation structure decision

## Reordering/Moving Blocks

**Adding a block at the START of a file** (no --before exists yet):
```bash
# Create new content with overview block, then concat existing first block, then replace
cat new_block.txt <(cmpr --print-block '#first_block_id') | cmpr --replace '#first_block_id'
```

To move a block to a different position in a file:

1. Save the block to a temp file: `cmpr --print-block '#block_id' > /tmp/block.txt`
2. Delete the block from its current position: `echo "" | cmpr --replace '#block_id'`
3. Insert it at the new position: `cat /tmp/block.txt | cmpr --after '#target_block_id'`

**IMPORTANT**: Never create temporary duplicates of blocks (adding before deleting) because having duplicate block IDs results in undefined behavior. Always delete first, then add at the new location.

**Example - Moving a block**:
```bash
# Step 1: Save block
cmpr --print-block '#block_to_move' > /tmp/block.txt

# Step 2: Delete from current location
echo "" | cmpr --replace '#block_to_move'

# Step 3: Insert at new location (after the block that should precede it)
cat /tmp/block.txt | cmpr --after '#preceding_block'
```

**Navigating the Codebase**:

All navigation MUST start from the root block and follow block references:

1. **Start at the root block**: Read it to see the main navigation hubs
2. **Use 2-3 hops**: You should be able to reach any area of the codebase in 2-3 `--print-comment` calls by following block references
3. **The root block is the map**: It provides access to all major subsystem hubs (implementation details, agent framework, event system, libraries, etc.)

**Navigation Principle**: If you cannot reach the blocks you need from the root block by following direct references, that is a PROBLEM. DO NOT work around it by using search commands. Instead:
1. STOP and inform the user that navigation is broken
2. Help fix the navigation structure by adding appropriate overview blocks or references
3. Only proceed with the original task after navigation is fixed
4. If the only thing you do is fix the navigation, so that you can find what you need to in 2-3 hops from root, and then you end the session and the programmer commits your change, that was a good session.

### NL/PL Synchronization

**Standard Workflow**:
1. Edit ONLY the NL using `cmpr --replace-comment '#blockid'` which takes new contents on stdin.
   - you should always have the previous NL in scope, otherwise do a --print-comment first, then make your changes
2. Run `cmpr --rewritepl '#block_id'` to regenerate PL from NL
3. Run `cmpr --print-code '#block_id'` to verify the generated PL looks correct
4. Test the changes with `dist/cmpr`

**When to Manually Maintain PL**:
- Only mark a block as "Manually maintained" if you MUST write PL directly
- Add "Manually maintained." as the last line of the NL comment
- This is rare and should be avoided when possible - prefer letting the system generate code

**CRITICAL: NL Precision for nl2pl**:
- The nl2pl system can generate correct code, but ONLY when the NL is unambiguous
- Vague specifications lead to incorrect implementations
- When performance or correctness matter, be VERY explicit about:
  - Exact algorithms (e.g., "BFS traversal calling cmpr --print-comment for each block")
  - Data structures (e.g., "block-scoped graph, not file-scoped")
  - What NOT to do (e.g., "don't scan entire files, only individual NL comments")
- If the generated PL is wrong, the NL was probably ambiguous - fix the NL, not the PL

**Function Declarations in NL**:
- Every C function block must have a function declaration in the NL comment (e.g., `void handle_foo(span arg)`)
- This removes ambiguity for nl2pl and serves as documentation for callers
- fdecls.h is a GENERATED file - NEVER edit it manually
- fdecls.h is generated from cmpr2's cmpr.c (`../cmpr/cmpr.c`) by `scripts/cmpr-c-from-cmpr-src-c`
- ALL new blocks that need to be in the build must be added to cmpr2's cmpr.c using `cmpr --after '#existing_block'`
- INBOX.c is for staging/experimentation only - blocks there don't participate in fdecls.h scripting
- new blocks that go in the build also need to be added to cmpr-c-build which is the blocks in the order, and we ship the PL only
- Order of C functions in cmpr.c doesn't matter; forward declarations are never needed (fdecls.h provides them)

**Using @C for Idiomatic Code**:
- Always add `@C` blockref when creating C function blocks
- #C references #libraryintro which documents idiomatic C patterns using our spanio library
- If nl2pl consistently makes the same mistake, add notes or refs to #C so it always sees them when generating code

**Important Notes**:
- `cmpr` (at `/usr/local/bin/cmpr`) is the installed system version
- `dist/cmpr` is the locally built binary from `make` - use for testing changes
- After modifying source code, run `make` to rebuild, then test with `dist/cmpr`
- When satisfied with changes, run `sudo make install` to update the installed binary
- The interactive `r` command in the TUI does the same as `--rewritepl`

### Testing Changes

**Build System**:
- Run `make` to build `dist/cmpr`
- Check build timestamp: `dist/cmpr --version`
- The Makefile will compile changed source files and link the binary

**Testing Workflow**:
1. Modify source code using cmpr commands
2. Run `make` to rebuild
3. Test with `dist/cmpr` to verify changes work
4. Once satisfied, run `sudo make install`
5. Now the installed `cmpr` has your changes

### Code Conventions

It should be possible to reach any block by following "Justifies: " lines, or explicit blockid mentions, starting from the root block.

**Duplicate Block References**: It is perfectly fine for a block ID to be mentioned multiple times in a parent block. Duplicate references do not cause any problems and are sometimes useful for documentation clarity.

**Important Note**:
We are still building up the graph from the root block to all the other blocks.
If you cannot reach the blocks that you need from the root block by following direct references, that's a problem.
DO NOT work around it but always stop and make some edits.

The basic idea is this:
The root block should give a high-level overview of the parts of the project.
If you know what you're trying to do (e.g. add feature X) then you should be able to determine where the relevant code is by just following blockids and using --print-comment 2-3 times, which makes things very efficient.
When that's not the case, you should probably ask the programmer about what needs to be improved in the structure, because we're still building this system out.

Similarly, if a cmpr command doesn't work or doesn't do what you expect, don't fall back to using other tools, but always let the programmer know and we'll fix it together.
We're using cmpr to build cmpr itself here, so if cmpr doesn't work right, then we always fix that before continuing with whatever we were doing before.

### Project Configuration
- Configuration is stored in `.cmpr/conf`
- Default model and build commands are configurable per project

## Core Architecture

### Single-Tier C System
This is a pure C application with no web frontend or HTTP server:
- **`cmpr.c`** - Main application: block database, CLI commands, and terminal UI (TUI). Includes hardcoded prompt templates for nl2pl code generation.
- **`spanio.c`** - Custom I/O library using span-based string handling
- **`Makefile`** - Build system (details documented in blocks)

### Block-Based Code Organization
- Code is organized into discrete "blocks" with IDs like `#block_name`
- Each block contains:
  - **NL Part**: Natural language comment (source of truth)
  - **PL Part**: Programming language code (generated from NL)
- Block references (`@other_block`) provide context dependencies
- Transitive references create dependency graphs

### Revision System
- Every code change is automatically versioned in `.cmpr/revs/`
- Uses SipHash checksums for content integrity
- Complete history tracking with timestamps
- There is an 'rvs' command which lets us interact with the revisions; we'll expand this section later; it's not very useful yet.

## Key Components

### cmpr.c (Main Application)
- **CLI Mode**: Command-line interface with flags like `--grep`, `--print-block`, `--replace`, etc.
- **Terminal UI (TUI)**: Interactive mode with single-keystroke commands (`j/k/g/G` for navigation, `r` for rewrite, `B` for build)
- **Block Database**: Parses and manages blocks across all source files
- **Operations**: Block navigation, code generation (nl2pl), building, search, history
- **LLM Integration**: Calls external LLM APIs for nl2pl code generation

### spanio.c (I/O Library)
- Custom I/O library using span-based string handling with `.buf` and `.end` pointers
- Arena allocation avoiding malloc overhead
- Efficient string operations without null-terminator dependencies

### Prompt System
- LLM prompt templates for nl2pl (natural language to programming language) conversion are hardcoded in cmpr.c
- Prompt functions (pt_nl2pl_rewrite, pt_agreement, etc.) return template strings
- Simplified from previous generation-based system to avoid circular build dependencies

## Development Patterns

### Natural Language Programming Workflow
1. Write English descriptions in block comments
2. AI converts to working code in target language
3. System maintains consistency between documentation and code
4. Focus on higher-level architectural decisions

### Agent System and Decision Tracking

**AGENT COMMUNICATION CONTRACT**:

ALL agent results are communicated via T (transient memory):
- Agents use `cmpr --event "..." --strength N` for ALL output
- NO stdout/stderr for communicating results (infrastructure files in .cmpr/agents/ are fine)
- Callers read results via `cmpr --T`
- This makes all agent activity queryable and persistent

Required event patterns:
```
"Agent: <name>" 255.
"Mode: CHECK|FIX" 255.
"Status: <result>" 255.
"<domain-specific facts>" N.
```

**Running Agents**:

To execute an agent:
```bash
cmpr --T0  # Clear T first
cmpr --print-code '#agent_block_id' | bash
cmpr --T   # Read results
```

To list all available agents:
```bash
dist/cmpr --agents
```

Example - run an agent and read results:
```bash
cmpr --T0
cmpr --print-code '#agent_block_id' | bash
cmpr --T
```

Navigation to agents:
1. Start at root block → follow references to agent hub
2. The agent hub lists all agent blocks and explains how to run them

**Agent Architecture**:
- An agent = **Predicate (Want)** + **Step Function (CHECK/FIX)**
- Wants establish event spaces: {desired state, complement}
- Agents verify and maintain wants through CHECK and FIX modes

**Four Decision States** (tracked → checked → assisted → owned):
1. **Tracked**: We record the want but don't verify it
2. **Checked**: We can determine if criteria is met
3. **Assisted**: We can offer help with fixing it
4. **Owned**: We automatically maintain the want

**Implementing Agents**:
- Create two blocks: predicate block + step function block
- Both executable via `cmpr --print-code '#blockid' | sh` or `bash`
- ALL results communicated via T using `cmpr --event` (no stdout for results)
- Example pattern: predicate block + CHECK mode block + FIX mode block
- See `cmpr --help agents` for current protocol details

**SN Notation** for confidence levels:
- 255 bits = definitional (statement is defined to be true)
- 20 bits = ~1 million to 1 confidence (virtually certain)
- 0 bits = describes possible event with no support

### Event System (T/E/S)

The event system provides temporal reasoning capabilities through tracking events in "transient memory" (T).

**Key Concepts**:
- **T (transient memory)**: Current event state, automatically persisted to `.cmpr/T`
- **E (events)**: Individual event strings with associated strength values
- **S (strength)**: Binary log odds representing bits of support for a proposition
- **SN lines**: Format is `"event_string" <strength>.` where interior quotes are NOT escaped

**CLI Commands**:
- `cmpr --T0` - Reset T to empty state
- `cmpr --event "string" --strength 255` - Add event to T (currently only strength 255 supported)
- `cmpr --T` - Print current T state as SN lines
- `cmpr --memorize` - Save timestamped snapshot of T to `.cmpr/events/`
- `cmpr --recall` - Search snapshots using current T as query, load matching snapshot with full context

**Implementation Details**:
- T persists automatically to `.cmpr/T` on every change
- Events are loaded on startup and saved after modifications
- Memorize creates timestamped snapshots (YYYYMMDD-HHMMSS-nanos format)
- Recall searches snapshots (newest first) for ones containing any query event from current T, then loads all events from the matching snapshot
- Event strings can contain any characters including quotes (per SN spec)
- Duplicate events update strength rather than creating duplicates

**SN Format Specification**:
Per the SN notation convention:
- SN lines begin with `"` and end with `" <digits>.`
- Interior double quotes are NOT escaped
- Parse by finding `" <digits>.` pattern at end of line
- Everything between opening `"` and final `" <digits>.` is the event string

**Event System Workflow and Design Intent**:

NOTE: This section describes intended design patterns that are still being validated through actual use.

CRITICAL UNDERSTANDING: T is "transient memory" - the name and the existence of `--T0` (clear T) reveal the design intent.

T is meant to be CLEARED between work sessions. It holds CURRENT context, not ALL historical state.

Typical workflow:
```bash
# Clear T for new work
cmpr --T0

# Set context (e.g., which block we're examining)
cmpr --event "The block id is: #foo" --strength 255

# Add facts about current context
cmpr --event "The block author is: Alice" --strength 255
cmpr --event "The block needs refactoring" --strength 255

# Save snapshot for historical record
cmpr --memorize

# Repeat for next block/context
```

Event Pattern Usage:

The **variable pattern** is the correct approach:
```
"The block id is: #foo" 255.
"The block is reachable" 255.
"The block author is: Alice" 255.
```

T holds context for ONE entity at a time. To track multiple entities, LOOP:
```bash
for block in $all_blocks; do
  dist/cmpr --T0
  dist/cmpr --event "The block id is: $block" --strength 255
  dist/cmpr --event "The block is reachable" --strength 255
  dist/cmpr --memorize
done
```

WRONG APPROACHES (do not use):

1. **"Embedded pattern"** - trying to avoid deduplication:
   ```
   "Block #foo is reachable" 255.  # WRONG
   "Block #bar is unreachable" 255.  # WRONG
   ```
   This tries to load all entities into one T state, violating T's transient design.

2. **Loading hundreds of events into one T state**:
   Fights the design. T is not a database for all historical state.

3. **"Alternative mechanisms"** (files, databases, etc.):
   The intended pattern is to use the T workflow correctly:
   Loop with --T0, set context, add events, --memorize.

When designing solutions:
- If you find yourself fighting `--T0` or avoiding `--memorize`, reconsider the approach
- T is for CURRENT work context, snapshots (via --memorize) are for HISTORICAL queries
- Pay attention to what system commands exist - they reveal design intent
- The existence of --T0 means T is MEANT to be cleared regularly

**Event System Visualizations**:

The event system includes visualization tools for analyzing temporal data:

**Timeline Visualization**:
```bash
# Generate interactive timeline of all event snapshots
cmpr --print-code '#generate_timeline_html' | bash > public_html/event_timeline.html
```
Creates scatter plot showing agent runs, work sessions, and metrics over time.

**Metric Plotting**:
```bash
# Plot specific metrics over time
cmpr --print-code '#generate_metric_plot' | bash -s unreferenced > public_html/metric_unreferenced.html
cmpr --print-code '#generate_metric_plot' | bash -s hubs > public_html/metric_hubs.html
cmpr --print-code '#generate_metric_plot' | bash -s events > public_html/metric_events.html
```
Generates line charts tracking metrics extracted from event snapshots.

**Snapshot Statistics**:
```bash
# Generate statistical analysis of all snapshots
cmpr --print-code '#generate_snapshot_stats' | bash > public_html/snapshot_stats.html
```
Shows event pattern distributions, agent activity, and snapshot analysis.

**Visualization Index**:
```bash
# Generate navigation page for all visualizations
cmpr --print-code '#generate_visualization_index' | bash > public_html/index.html
```

All visualizations are self-contained HTML files using Chart.js, viewable in any browser.
Navigate from root → #cmpr_events → #event_visualization_overview to find these blocks.

**Practical Event System Examples**:

Example 1 - Recording agent execution:
```bash
cmpr --T0
cmpr --event "Agent: root_agent" --strength 255
cmpr --event "Mode: CHECK" --strength 255
cmpr --event "Unreferenced blocks: 52" --strength 255
cmpr --event "Status: constraint satisfied" --strength 255
cmpr --memorize
```

Example 2 - Finding previous work on a specific block:
```bash
cmpr --T0
cmpr --event "The block id is: #foo" --strength 255
cmpr --recall  # Loads all events from last time we worked on #foo
cmpr --T      # View the restored context
```

Example 3 - Session end workflow:
```bash
# After completing work, record experience report
cmpr --T0
cmpr --event "The experience report is: #claude_experience_report_topic_20251228" --strength 255
cmpr --event "Implemented feature X" --strength 255
cmpr --event "Created 5 new blocks" --strength 255
cmpr --memorize
```

## File Structure

- **Core Application**: `cmpr.c` (main application with CLI and TUI, includes hardcoded prompt templates)
- **I/O Library**: `spanio.c` (span-based string handling)
- **Staging Area**: `INBOX.c` (staging area for new blocks)
- **Configuration**: `.cmpr/conf` (project configuration)
- **Build System**: `Makefile` (build system details in blocks)
- **Revisions**: `.cmpr/revs/` (automatic versioning)
- **Events**: `.cmpr/events/` (event system snapshots), `.cmpr/T` (current transient memory)
- **Reports**: `public_html/` (generated HTML reports for system visibility)
- **Build Output**: `dist/cmpr` (compiled binary)

## Common Pitfalls and Process Reminders

**CRITICAL: Always Use cmpr Commands**

After exiting planning mode or when context-switching, it's easy to forget cmpr commands exist and fall back to traditional file editing (Write, Edit tools). This makes you 10x slower and less token-efficient.

**Before touching ANY file**:
1. Check if it's block-managed: `cmpr --files-blocks | grep filename`
2. If yes, use ONLY cmpr commands: `--print-comment`, `--replace`, `--after`
3. NEVER use Write/Edit tools on block-managed files

**Common mistakes**:
- ❌ Testing with old installed `cmpr` after making changes → ✅ Use `dist/cmpr` to test, then `sudo make install`
- ❌ Using Task tool with Explore subagent to "explore the codebase" → ✅ Start at root block and navigate
- ❌ Using `Write` to create new blocks → ✅ Use `cmpr --after <block_id>`
- ❌ Using `Edit` to modify existing blocks → ✅ Use `cmpr --replace '#block_id'`
- ❌ Using `Read` + manual parsing → ✅ Use `cmpr --print-comment '#block_id'`
- ❌ Using `grep`/`find` to locate code → ✅ Use `cmpr --grep` or navigate from root
- ❌ Manually reading .cmpr/revs files → ✅ Use existing rvs indices and helpers
- ❌ Starting ANY task without reading root block first → ✅ Always start by reading the root block to see navigation hubs
- ❌ Piping commands into `--replace-code` without testing → ✅ Test with `wc -l`, then `grep`, THEN replace
- ❌ Grepping or filtering `make` output → ✅ Read it directly - it's a serious build system, not npm
- ❌ Creating blocks without knowing final location → ✅ Use `cmpr --after '#INBOX'` and move later

**Recovering a Deleted Block from Revisions**:

If you accidentally delete or corrupt a block, find it in recent revisions:
```bash
b='#block_id'
find .cmpr/revs -type f | sort | tail -n32 | while read rev; do
  cmpr --print-block "$b" "$rev" >/dev/null 2>&1 && printf "%s contains %s\n" "$rev" "$b"
done
```

Once you find a revision containing the block, extract and restore it:
```bash
cmpr --print-block '#block_id' .cmpr/revs/YYYYMMDD-HHMMSS > /tmp/block.txt
cat /tmp/block.txt | cmpr --after '#some_existing_block'
```

**CRITICAL: Navigation Structure**

Every block MUST be reachable from #root in ≤2 hops. This is a core want maintained by the root agent.

When creating new blocks:
1. ❌ **WRONG**: Create block in INBOX, leave it there permanently
2. ✅ **CORRECT**: Create block AND immediately integrate it into navigation:
   - Add reference to relevant hub block
   - OR create new hub if starting a new subsystem
   - OR use INBOX only for temporary/experimental blocks

The navigation structure IS the codebase organization. Breaking navigation means:
- The block is effectively lost (not discoverable)
- It won't appear in anyone's mental model of the system
- The #root want is violated

Fix navigation BEFORE implementing anything else. If you cannot reach the blocks you need in 2 hops from #root by following references, that is a PROBLEM that must be fixed first, not worked around.

**Block structure patterns**:
- ❌ One block containing multiple function implementations → ✅ Overview block listing child blocks
- Each block should either be:
  - An overview/index block (NL only, listing other blocks)
  - A single implementation block (NL + PL for one function/feature)
- When you see a block with many functions, refactor it into an overview + individual blocks
- Don't be afraid to refactor a block into two new blocks.
  When you do this: make the first block the right size and the second block contain everything else.
  If it can't be divided up that way, don't refactor it.
  Use the _2 prefix for the second block unless there's clearly something better to call it (like draw_the_rest_of_the_fucking_owl).
  When you do that, don't change anything else, and wrap up the session and commit the change soon if you can.

**When working with existing infrastructure**:
- DON'T reimplement helpers that already exist (like checksum functions)
- DO navigate from root block to find existing functionality
- DO check existing index catalogs before designing new indices
- DO look at similar command blocks for patterns

Never be afraid to go back to the root block and look for something else.

**Plan mode amnesia**:
- Planning mode can last multiple turns - easy to forget the cmpr workflow
- When exiting plan mode, IMMEDIATELY verify: "Am I working with block-managed files?"
- Refresh memory of cmpr commands before starting implementation

## Session Workflow

**Starting a session (MANDATORY - do this before ANY task):**

1. **Read the root block first** - understand current navigation structure:
```bash
cmpr --print-comment '#root'
```

2. **Check T state for recent work context**:
```bash
cmpr --T
```

3. **Find and read recent experience reports** - check INBOX for recent reports:
```bash
cmpr --files-blocks | grep -A8 '#INBOX'
```
This shows recent blocks after INBOX including experience reports. Read any relevant ones with `cmpr --print-comment '#block_id'`.

T contains events from recent work sessions. Check for:
- Experience report events: `"The experience report is: #blockid"` - READ THESE BLOCKS
- Agent execution results: `"Agent: "`, `"Status: "`, etc.
- Want tracking: `"The want is: "`, `"Automation state: "`

**Why this matters**: Experience reports contain detailed context about what was attempted, what worked, what failed, and next steps. Starting a task without reading relevant experience reports means repeating mistakes or missing important context.

To load context from a previous session:
```bash
cmpr --T0  # Clear current T
cmpr --event "The experience report is: #blockid" --strength 255
cmpr --recall  # Loads all events from that session
cmpr --T  # View loaded context
```

**Ending a session:**

1. Write experience report (see Experience Reports section below)
2. Add experience report to INBOX: `cat report.txt | cmpr --after <inbox_block_id>`
3. Add event to T with block ID:
   ```bash
   cmpr --event "The experience report is: #experience_report_block_id" --strength 255
   ```
4. Save snapshot:
   ```bash
   cmpr --memorize
   ```

**Before git commit:**

Always `cmpr --memorize` before committing. This preserves temporal context.

For major changes to cmpr.c, consider committing one event file to git:
```bash
cmpr --memorize
# Pick the most recent snapshot
git add .cmpr/events/YYYYMMDD-HHMMSS-*
git commit -m "your message"
```

This creates git-tracked checkpoints of system state at significant moments.

This creates a queryable checkpoint. Future sessions can use `--recall` to load all events from this session, providing full context about what was done, which agents ran, what state was observed, etc.

**Pattern:**
- T is transient memory for CURRENT work
- Snapshots (via --memorize) preserve historical context
- Experience report events enable finding session work later
- Each memorized snapshot contains: agent events + want tracking + experience report reference

## Experience Reports

**When to Write**:
- At the end of every work session
- When completing significant tasks (planning, implementation, debugging)
- When stopping work on something that's not finished

**What to Include**:
- Session goal
- What was accomplished (detailed)
- What works
- Known issues/blockers
- Next steps
- Full context for resuming work

**Naming Pattern**:
- Format: `#<agent>_experience_report_<topic>_YYYYMMDD_N`
- Agent names: claude, codex, or other agents
- Example: `#claude_experience_report_<descriptive_topic>_YYYYMMDD`

**Response Format**:
- Chat responses should be ONE LINE referencing the experience report
- Example: "See #experience_report_block_id"
- ALL details, summaries, and context go in the experience report block
- Keep conversation clean and searchable - detail lives in blocks

**Storage**:
- Experience reports go in INBOX initially: `cat report.txt | cmpr --after '#INBOX'`
- Can be moved to permanent locations later during review
- Or left in INBOX as temporal documentation

topics
basic
blocks
editing
search
nl2pl
events
agents
reports
wants
agent-qa

CLAUDE.md Prologue for cmpr Projects
=====================================

Add this section to your project's CLAUDE.md to enable Claude to work
effectively with cmpr-organized codebases.

---BEGIN CLAUDE.MD CONTENT---

## cmpr Block System

This codebase uses cmpr for block-based code organization. Each file contains
discrete "blocks" with unique IDs (e.g., #block_name).

**IMPORTANT**: Always use cmpr commands instead of traditional file tools.

### Navigation Commands

cmpr --print-block '#id'    Show entire block (NL + PL)
cmpr --print-comment '#id'  Show only NL comment (documentation)
cmpr --print-code '#id'     Show only PL code (implementation)
cmpr --grep 'pattern'       Search across all blocks
cmpr --files-blocks         List all blocks in the project

### Editing Commands

cmpr --replace '#id'        Replace entire block from stdin
cmpr --replace-comment '#id' Replace only NL part from stdin
cmpr --replace-code '#id'   Replace only PL part from stdin
cmpr --after '#id'          Add new block after given block from stdin

### Navigation Pattern

Always start from the root block and follow references:

1. cmpr --print-comment '#root'     Read root block
2. Follow block references (2-3 hops max to reach any code)
3. Use --grep only when navigation fails

If you cannot reach needed blocks from #root in 2-3 hops, that indicates
a navigation structure problem that should be fixed.

### Block Structure

Each block has two parts:
- NL (Natural Language): Documentation/specification in comments
- PL (Programming Language): Implementation code

The NL is the source of truth. Use cmpr --rewritepl '#id' to regenerate
PL from NL when specifications change.

### Event System (Agent State)

Agents communicate via T (transient memory):

cmpr --T0                              Clear T
cmpr --event "message" --strength 255  Add event to T
cmpr --T                               Print current T
cmpr --memorize                        Save T snapshot
cmpr --recall                          Load matching snapshot

Agent pattern for recalling previous state:
  cmpr --T0
  cmpr --event "Agent: myagent" --strength 255
  cmpr --recall
  cmpr --T  # now contains events from last run

---END CLAUDE.MD CONTENT---

Usage:
  cmpr --help claude-setup >> CLAUDE.md

Then customize the CLAUDE.md with project-specific details:
- Build commands
- Test commands
- Project structure overview
- Any project-specific conventions

*/

# trigger 1768103400
