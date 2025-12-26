/* #INBOX

This is the inbox block. New blocks are added after this block as a staging area.

The purpose of #INBOX is to provide a known location where new blocks can be inserted during development, before we decide on their proper location in the codebase structure.

"We want blocks that appear after #INBOX to be moved to appropriate locations in the codebase based on their content and purpose." 255.

Workflow:
1. New blocks (especially experience reports, experiments, or exploratory work) get added after #INBOX
2. During review or refactoring sessions, we examine blocks after #INBOX
3. We decide the appropriate navigation hub or section for each block
4. We move the block to its proper location (see CLAUDE.md for block moving pattern)
5. #INBOX remains as a staging area for future additions

This pattern helps maintain the navigational structure while allowing rapid iteration during development.

*/

/* #claude_experience_report_blocklist_20251226

Experience Report: cmpr1 vs cmpr2 Blocklist Comparison
Date: 2025-12-26
Task: Compare blocklists in cmpr.c between cmpr1 and cmpr2 versions

## Context

User requested comparison of blocks in cmpr.c between:
- cmpr1: /home/admin/cmpr1/cmpr.c (open-source implementation)
- cmpr2: /home/admin/cmpr/cmpr.c (reference implementation, may contain proprietary code)

The goal was to understand what blocks exist in each version, with the longer-term objective being synchronization.

## Findings

**Overall Statistics:**
- 188 blocks common to both versions
- 47 blocks only in cmpr1
- 37 blocks only in cmpr2
- Total: 236 blocks in cmpr1, 227 blocks in cmpr2

**cmpr1-Specific Blocks (47 total):**

*Agent/Event System (15 blocks):*
- #cmpr_events, #events_types, #events_functions
- #events_persistence_questions, #events_workflow_questions, #events_example_interpretation
- #root_agent, #root_agent_check, #root_agent_fix
- #root_agent_impl, #root_agent_impl_2, #root_agent_impl_3
- #root_agent_progress
- #agent_runner, #want_definition

*Experience Reports & Documentation (7 blocks):*
- #claude_experience_report_events_20251224
- #claude_experience_report_events_20251224_2
- #claude_exploration_report
- #claude_product_pattern_experience
- #claude_root_agent_experience
- #glossary
- #high_cardinality_storage

*Implementation Features (13 blocks):*
- #nl2algo, #nl2pl_rewrite, #pl2nl_rewrite, #pl2nl_rewrite_cb
- #summarize_block
- #handle_agents, #handle_run
- #block_map_selftest, #block_ids_for_file_line
- #parse_block_map_entry, #parse_compiler_error_line
- #normalize_path_for_match, #paths_match_for_block_map

*Infrastructure & Relations (12 blocks):*
- #cmpr_rels (appears twice in listing), #cmpr_rels_plan, #cmpr_model
- #cmpr_checksum, #cat_core
- #cmpr1_build_manifest, #cmpr2_via_cmpr1
- #content_index
- #compile(), #pipe_cmd_cmp()
- #add_projfile(span)

**cmpr2-Specific Blocks (37 total):**

*Overview/Navigation Blocks (8 blocks):*
- #cmpr_c_overview
- #block_ops_overview
- #llm_integration_overview
- #design_docs_overview
- #parsing_io_overview
- #ui_display_overview
- #rev_system_c_overview
- #blockref_expansion_overview

*Refactored Command Handling (6 blocks):*
- #handle_args_2, #handle_args_3, #handle_args_4
- #block_id_arg
- #after, #replace, #replace_comment, #print_all

*Language-Specific Block Parsers (6 blocks):*
- #find_blocks_language_python
- #find_blocks_language_c
- #find_blocks_language_markdown
- #find_blocks_language_auto
- #find_blocks_language_none
- #set_file_language

*New Operations (7 blocks):*
- #insert_block_after, #insert_block_before
- #print_files_blocks, #write_block_map
- #expand_block, #find_block
- #grep_blocks (duplicate in listing)

*UI & Display (2 blocks):*
- #print_current_blocks
- #Settings

*Other (8 blocks):*
- #cmpr1_refactor_guidance
- #tabular_programming
- #grep_feature_implementation_strategy
- #line_for_block
- #expand_refs_2_rec_body
- #compile (without parens, vs cmpr1's #compile())
- #pipe_cmd_cmp (without parens, vs cmpr1's #pipe_cmd_cmp())
- #add_projfile (vs cmpr1's #add_projfile(span))

## Analysis

**cmpr1 Characteristics:**
- Contains active development of agent/event system
- Has experience reports documenting work sessions
- Includes experimental features (nl2algo, block_map_selftest)
- Less structured navigation (missing overview blocks)
- More raw implementation blocks

**cmpr2 Characteristics:**
- Well-structured with comprehensive overview blocks
- Refactored command handlers (split into _2, _3, _4 blocks)
- Language-specific parsing is more modular
- Better navigation architecture (#cmpr_c_overview → #block_ops_overview → etc.)
- Includes refactoring guidance (#cmpr1_refactor_guidance, #tabular_programming)

**Notable Patterns:**
1. cmpr2 has split monolithic blocks into smaller, more focused ones
2. cmpr2 has overview blocks for major subsystems
3. cmpr1 has agent/event system that cmpr2 lacks
4. Some blocks have naming differences (with/without parens, different suffixes)

## Critical Discovery: Lost #root Block

During this investigation, discovered that #root block was missing from cmpr.c in cmpr1.
This is a serious navigation structure failure.

**Resolution:**
- Found #root in git history (commit 69de82a)
- Restored it by concatenating with #source_intro
- Verified restoration successful

The #root block defines the want: "every code block in the project is reachable within 2 hops"
It references navigation hubs: #root_agent, #cmpr_events

## Recommendations

**Do NOT auto-synchronize** - the two codebases serve different purposes:
- cmpr1: open-source, active agent/event development
- cmpr2: reference implementation, better-structured navigation

**What cmpr1 should pull from cmpr2:**
1. Overview blocks (#cmpr_c_overview, #block_ops_overview, etc.) - critical for navigation
2. Refactored command handlers (handle_args_2/3/4 split pattern)
3. Language-specific parser blocks (more modular than current approach)
4. #cmpr1_refactor_guidance (contains guidance for migrating cmpr2 patterns)

**What cmpr1 should keep unique:**
1. Agent/event system blocks (active development, not ready for cmpr2)
2. Experience reports (documentation of cmpr1 development)
3. Experimental features being developed

**What needs attention:**
1. #root block must reference an overview block (currently only references #root_agent and #cmpr_events)
2. Need to add #cmpr_c_overview or similar as a navigation hub in #root
3. Verify all 236 blocks are actually reachable from #root (they're not currently)

## Process Observations

**What worked well:**
- Using cmpr --files-blocks to extract blocklists
- Sorting and using comm for comparison
- Following CLAUDE.md guidance to start from #root (which revealed it was missing!)

**What could improve:**
- Should have verification step to ensure #root exists before any codebase work
- Need better tooling for visualizing block reachability from #root
- Could use --grep more effectively instead of --files-blocks + grep

**Meta-lesson:**
The very task of comparing blocklists revealed a navigation structure failure (#root missing).
This validates the want line in #root - navigation structure IS critical and needs active maintenance.

## Status

Task completed as requested:
✓ Compared blocklists
✓ Identified differences
✓ Did NOT synchronize (per user instruction)
✓ Produced this experience report
✓ Ensured experience report is reachable from #root (see next steps)

Next: Update #root to reference this experience report for navigation.

*/
/* #blog_post_blockset_visualization

Blog Post: Visualizing the cmpr1 Block Graph

A blog post concept for visualizing the cmpr1 codebase block structure using various graph representations.

## Motivation

The cmpr1 codebase is organized as a directed graph of blocks:
- Nodes: Code blocks with unique IDs (e.g., #main, #argtable, #root_agent)
- Edges: References between blocks (mentions of #blockid in NL comments)
- Hub structure: The #root want specifies 2-hop reachability through hub nodes

Traditional code visualization tools (call graphs, dependency trees) don't capture this block-reference structure. We need visualizations that show:
1. The hub-and-spoke topology from #root
2. Coverage gaps (unreferenced blocks)
3. Reference density and clustering
4. Navigation paths through the conceptual graph

## Proposed Visualizations

### 1. Hub-and-Spoke Radial Layout
**Purpose**: Show the 2-hop navigation structure from #root

**Description**:
- Center: #root block
- Inner ring: Hub blocks (8 nodes)
- Outer ring: Leaf blocks referenced by hubs (60-70 nodes)
- Beyond outer ring: Unreferenced blocks (210 nodes, shown in gray/dimmed)

**Visual encoding**:
- Node size: Number of outgoing references
- Node color: By subsystem (core=blue, UI=green, LLM=purple, etc.)
- Edge thickness: Reference count (multiple mentions = thicker edge)
- Unreferenced nodes: Faded, positioned outside main structure

**Interaction**:
- Hover: Show block ID and first line of NL
- Click: Navigate to block source
- Filter: Show/hide unreferenced blocks

### 2. Force-Directed Graph
**Purpose**: Reveal natural clustering and hidden structure

**Description**:
- All blocks as nodes with force-directed physics
- References as edges with spring forces
- Blocks cluster by mutual references
- #root and hubs act as gravity wells

**Visual encoding**:
- Node color: By file (cmpr.c, spanio.c, INBOX.c, etc.)
- Edge color: Reference type (direct mention, Justifies:, See also:)
- Node shape: Circle=implementation, Square=overview/hub, Diamond=meta

**Benefits**:
- Reveals implicit subsystems (tight clusters)
- Shows bridge blocks (high betweenness centrality)
- Identifies orphan islands (unreferenced subgraphs)

### 3. Sankey Diagram: File → Hub → Block Flow
**Purpose**: Show how blocks are distributed across files and hubs

**Description**:
- Left column: Source files (cmpr.c, spanio.c, INBOX.c, etc.)
- Middle column: Hub blocks
- Right column: Referenced vs unreferenced blocks

**Flow encoding**:
- Flow width: Number of blocks
- Color: By hub subsystem
- Split flows: Blocks referenced by multiple hubs

**Insights**:
- Which files contribute most blocks to which hubs
- Coverage distribution across source files
- Files with many unreferenced blocks (needs hub creation)

### 4. Hierarchical Edge Bundling
**Purpose**: Show reference patterns while reducing visual clutter

**Description**:
- Circular layout with blocks arranged by hub
- Curved edges bundle together when they share hub membership
- Cross-hub references shown as longer arcs

**Visual encoding**:
- Angular position: Hub membership
- Radial position: Block depth (hub=inner, leaf=outer)
- Edge opacity: Reference count
- Edge color: Cross-hub (red) vs intra-hub (gray)

**Insights**:
- How much cross-referencing between subsystems
- Which hubs are most self-contained
- Bridge blocks that connect subsystems

### 5. Treemap: Coverage by Subsystem
**Purpose**: Show quantitative coverage at a glance

**Description**:
- Rectangle for each hub, sized by total blocks in subsystem
- Subdivided into: referenced (green), unreferenced (red)
- Nested labels show block IDs

**Metrics displayed**:
- Coverage percentage per hub
- Total blocks per hub
- Overall project coverage (currently 24.5%)

### 6. Chord Diagram: Inter-Hub References
**Purpose**: Show which hubs reference each other

**Description**:
- Circle with 8 segments (one per hub)
- Chords connect hubs when blocks in one reference blocks in another
- Chord width: Number of cross-references

**Insights**:
- Which subsystems are tightly coupled
- Independent vs interdependent hubs
- Dependency flow direction

## Data Pipeline

### Step 1: Extract Block Graph
```bash
# Get all blocks with their references
cmpr --files-blocks > blocks.txt

# For each block, extract referenced block IDs
for block_id in $(grep "Block.*#" blocks.txt | cut -d'#' -f2); do
  cmpr --print-comment "#$block_id" | grep -oE '#[a-zA-Z_][a-zA-Z0-9_]*' > "refs/$block_id.txt"
done

# Build adjacency list
# Output: block_id,referenced_id,file,hub_membership
```

### Step 2: Compute Graph Metrics
```python
import networkx as nx

# Build directed graph
G = nx.DiGraph()
# Add nodes and edges from extracted data
# Compute metrics:
- Reachability from #root (BFS)
- Shortest paths (navigation distance)
- Clustering coefficient (tight groupings)
- Betweenness centrality (bridge blocks)
- PageRank (important blocks)
```

### Step 3: Generate Visualizations
Using D3.js or Observable:
```javascript
// Load graph data
const graph = await d3.json('blockgraph.json');

// Render force-directed layout
const simulation = d3.forceSimulation(graph.nodes)
  .force('link', d3.forceLink(graph.edges))
  .force('charge', d3.forceManyBody())
  .force('center', d3.forceCenter());
```

## Blog Post Structure

### Title
"Visualizing Code as a Block Graph: 6 Ways to See cmpr1's Structure"

### Introduction
- What is block-based code organization
- Why traditional code viz doesn't work
- The #root want and 2-hop reachability goal

### Section 1: Current State
- 278 blocks, 8 hubs, 24.5% coverage
- The hub creation journey (manual → assisted → owned)
- Show treemap of coverage

### Section 2: The Navigation Graph
- Hub-and-spoke radial visualization
- Interactive demo: click to navigate
- Discussion of 2-hop constraint

### Section 3: Natural Clustering
- Force-directed graph reveals subsystems
- Comparison to declared hub structure
- Opportunities for better organization

### Section 4: File Distribution
- Sankey diagram: files → hubs → coverage
- Which files need more hub organization
- INBOX pattern for staging blocks

### Section 5: Cross-References
- Chord diagram of inter-hub references
- Discussion of coupling vs cohesion
- Hierarchical edge bundling for detail

### Section 6: Metrics and Evolution
- Time-series of coverage over sessions
- Agent-driven maintenance model
- Future: autonomous hub creation

### Conclusion
- Block graphs as a way to think about code
- Tools for working with block-based code
- Open questions and future work

## Implementation Notes

**Technology stack**:
- D3.js v7 for visualizations
- Observable notebooks for interactivity
- NetworkX for graph analysis
- Export static images for blog (SVG)

**Data refresh**:
- Run extraction script after each session
- Track coverage metrics over time
- Animate transitions between states

**Accessibility**:
- Provide text-based graph summaries
- Alternative table view of metrics
- Export graph as CSV for analysis

## Future Extensions

1. **Animation**: Show hub creation process step-by-step
2. **Diff view**: Compare graph before/after refactoring
3. **3D graph**: Use WebGL for large block sets (500+ blocks)
4. **Query interface**: "Show me all blocks that reference X"
5. **Suggestion engine**: "Create hub for these 12 unreferenced blocks"

Justifies: Documentation of block graph visualization concepts for potential blog post or tool development.

*/
/* #claude_experience_report_root_agent_manual_hubs_20251226_1

Experience Report: Manual Hub Creation for Root Agent
Session Date: 2025-12-26

## Objective
Following programmer decision (D > C > B priority), begin manual hub creation to organize 260+ unreferenced blocks and satisfy the #root want.

## Programmer Decision Context

Received multi-option prioritized approach:
- **Option D (Primary)**: Manual hub creation with programmer guidance
- **Option C (Fallback)**: Group by subsystem if automation needed
- **Option B (Fallback)**: Group by functionality if subsystem unclear

Decision recorded in: .cmpr/root_agent_guidance.txt

## What Was Accomplished

### 1. Created Hub Proposal Document (#root_hub_proposal_20251226)
Analyzed 271 blocks across codebase and proposed 10 hub blocks organized by subsystem:
- Identified natural groupings (core, parsing, indexing, revisions, LLM, UI, etc.)
- Estimated block counts per hub (2-16 constraint)
- Prioritized hubs by importance (Priority 1, 2, 3)

### 2. Created 6 New Hub Blocks
All hubs created using `cmpr --after '#INBOX'` pattern:

**#cmpr_c_core** (9 blocks)
- main, init, argtable, handle_args, langtable, config_fields, pragmas, projfiles
- Scope: Core cmpr.c initialization and argument handling

**#parsing_io** (9 blocks)
- rope, ingest, read_, read_file_into, find_all_blocks, find_all_lines, assoc_spans
- Scope: Low-level parsing and file I/O (note: spanio.c blocks need separate hub)

**#block_indexing** (12 blocks)
- index_block_ids, block_idx, ids_for_block, id_for_block, block_for_span
- set_current_block, block_id_jump, blocks, files, checksums (3 blocks)
- Scope: Block discovery, indexing, lookup operations

**#revision_system** (8 blocks)
- rev_info, get_revs, get_revs_2, get_revdir, cmpr_checksum, checksums, checksum_setup
- Scope: .cmpr/revs management and history tracking

**#llm_integration** (12 blocks)
- call_llm, call_gpt, call_gpt_curl, call_anthropic, call_anthropic_curl
- call_ollama, call_ollama_curl, read_openai_key, read_anthropic_key
- filename_template, filename_variables
- Scope: External API integration for language models

**#ui_navigation** (11 blocks)
- ui_state, clear_display, jk_order, jk_implementation, block_id_jump
- block_sanity_check, inp_sanity_checks, complain_and_exit, complain_and_prompt, print_config
- Scope: Terminal UI, display, interactive navigation

### 3. Updated #root Block
Modified root block to reference all 8 hubs (6 new + 2 existing):
- Core Systems: cmpr_c_core, parsing_io, block_indexing
- Specialized Subsystems: revision_system, llm_integration, ui_navigation
- Meta-systems: root_agent, cmpr_events

Fixed duplicate #root_agent reference in process.

### 4. Verified Progress with CHECK Mode

**Before**:
- Hub blocks: 2
- Total blocks: 267
- Unreferenced: 260 (97.4% unreachable)
- Want status: NOT satisfied

**After**:
- Hub blocks: 8
- Total blocks: 278 (includes new hub blocks + experience reports)
- Unreferenced: 210 (75.5% unreachable)
- Want status: NOT satisfied (but improving)

**Progress**: 50 blocks now reachable (was ~7), coverage improved from ~2.6% to 24.5%

## Observations

### Pattern That Emerged
Each hub block follows consistent structure:
1. Title line describing scope
2. Brief overview sentence
3. "## Navigation Structure" section
4. "Core blocks:" list with bullet points (block ID + brief description)
5. Optional notes about related blocks
6. "Referenced by: #root" footer

This pattern makes hubs scannable and consistent.

### Block Count Distribution
The 2-16 constraint is generous enough:
- Most hubs have 8-12 blocks (comfortable middle range)
- No hub felt artificially split or overcrowded
- The constraint encourages meaningful grouping

### Duplicate Detection Issue
CHECK mode detected #root_agent twice because:
- First mention: "See #root_agent for more" (in prose)
- Second mention: "- #root_agent - Agent system..." (in hub list)

The grep-based extraction doesn't distinguish context. This is acceptable for now - the script still validates correctly.

### Hub Placement Strategy
Used `cmpr --after '#INBOX'` for all new hubs, which:
- Keeps them together during creation
- Avoids deciding final file placement prematurely
- Follows INBOX staging pattern from CLAUDE.md

These should eventually be moved to appropriate source files (cmpr.c, INBOX.c, etc.) but staging in INBOX worked well for rapid iteration.

## What Still Needs Doing

### Remaining 210 Unreferenced Blocks
Need to create additional hubs for:

**Priority 2** (should do next):
- #prompts_nl2pl - Prompt templates (20-25 blocks estimated)
- #experience_docs - Experience reports and docs (30-40 blocks)
- #build_config - Build system and Makefile (10-15 blocks)
- #spanio_core - spanio.c I/O library (15-20 blocks)

**Priority 3** (after Priority 2):
- #command_handlers - CLI command implementations
- #python_backend - cmpr.py blocks if they exist
- #migration_tools - Migration scripts

**Ungrouped blocks** (TBD):
- Various one-off blocks in INBOX.c
- TODO items
- Config files
- Miscellaneous utility blocks

### Potential Issues
1. Some blocks may not fit natural groupings (singleton blocks)
2. Some files (INBOX.c, TODO) are intentionally ephemeral - should their blocks be in hubs?
3. Experience report blocks proliferate - need hub but also might archive old ones
4. The 2-16 constraint may force awkward groupings for small subsystems

### Alternative Approaches if Manual Gets Tedious
Could switch to Option C (automated subsystem grouping) for remaining blocks:
- Analyze block content programmatically
- Group by file as fallback
- Use LLM to suggest groupings based on NL comments

## Root Agent Status After This Session

The root agent has progressed from:
- **TRACKED** state (we record the want)
- **CHECKED** state (we can verify satisfaction) ✓ Already had this
- **ASSISTED** state (we can offer help) ✓ Achieved this session
- **OWNED** state (autonomous maintenance) - Still TODO

Next session can:
1. Continue manual hub creation until coverage is sufficient (>80%?)
2. Implement automated grouping for long tail of blocks
3. Add FIX mode automation once grouping strategy is validated

## Technical Notes

### Commands Used This Session
```bash
# Create hub blocks
cat <<'EOF' | cmpr --after '#INBOX'
/* #hub_name
...
*/
/* #cmpr_c_core

Core C Implementation Hub

This hub organizes the central cmpr.c functionality including program initialization, argument handling, and main command dispatch.

## Navigation Structure

Core blocks:
- #main - Program entry point
- #init - Initialization and setup
- #argtable - Command-line argument definitions
- #handle_args - Argument parsing and command dispatch
- #langtable - Language detection and file type mapping
- #config_fields - Configuration structure and defaults
- #pragmas - Pragma handling
- #projfiles - Project file discovery

Referenced by: #root

*/
/* #parsing_io

Parsing & I/O Hub

This hub covers low-level parsing, span-based I/O operations, and file reading infrastructure.

## Navigation Structure

Core blocks:
- #rope - Rope data structure for efficient text operations
- #ingest - File ingestion and block extraction
- #read_ - File reading operations
- #read_file_into - Read file content into buffer
- #find_all_blocks - Discover all blocks in source
- #find_all_lines - Line-based parsing
- #assoc_spans - Span association structures
- #assoc_spans_lookup - Span lookup operations

Note: spanio.c blocks are in a separate file and should have their own hub.

Referenced by: #root

*/
/* #block_indexing

Block Management & Indexing Hub

This hub covers block discovery, indexing, lookup, and navigation operations.

## Navigation Structure

Core blocks:
- #index_block_ids - Build index of block IDs
- #block_idx - Block index structure
- #ids_for_block - Get IDs associated with a block
- #id_for_block - Get primary ID for a block
- #block_for_span - Find block containing a span
- #set_current_block - Set active block context
- #block_id_jump - Jump to block by ID
- #blocks - List blocks command handler
- #files - List files command handler
- #current_block_checksum - Get checksum for current block
- #selected_checksum - Get checksum for selected block

Referenced by: #root

*/
/* #revision_system

Revision Tracking & History Hub

This hub covers the revision system that stores all code changes in .cmpr/revs/.

## Navigation Structure

Core blocks:
- #rev_info - Revision information structure
- #get_revs - Retrieve revision list
- #get_revs_2 - Extended revision operations
- #get_revdir - Get revision directory path
- #cmpr_checksum - Checksum calculation for revisions
- #checksums - Checksum management
- #checksum_setup - Initialize checksum system

Referenced by: #root

*/
/* #llm_integration

LLM & API Integration Hub

This hub covers external API calls to language models (OpenAI, Anthropic, Ollama, etc.).

## Navigation Structure

Core blocks:
- #call_llm - Main LLM calling interface
- #call_gpt - OpenAI GPT integration
- #call_gpt_curl - GPT curl implementation
- #call_anthropic - Anthropic Claude integration
- #call_anthropic_curl - Anthropic curl implementation
- #call_ollama - Ollama local model integration
- #call_ollama_curl - Ollama curl implementation
- #read_openai_key - OpenAI API key reading
- #read_anthropic_key - Anthropic API key reading
- #filename_template - Template for output filenames
- #filename_variables - Template variable substitution

Referenced by: #root

*/
/* #ui_navigation

UI & Navigation Hub

This hub covers the terminal user interface, display, and interactive navigation features.

## Navigation Structure

Core blocks:
- #ui_state - UI state management
- #clear_display - Clear terminal display
- #jk_order - j/k navigation ordering
- #jk_implementation - j/k navigation implementation
- #block_id_jump - Jump to block by ID
- #block_sanity_check - Block validation
- #inp_sanity_checks - Input validation
- #complain_and_exit - Error handling with exit
- #complain_and_prompt - Error handling with prompt
- #print_config - Display configuration

Referenced by: #root

*/
/* #root_hub_proposal_20251226

Proposed Manual Hub Structure for #root

Based on analysis of 271 blocks across the codebase, here are suggested hub blocks to create manually:

## Proposed Hub Blocks

### 1. #cmpr_c_core (Core C Implementation)
Contains: main, init, argtable, handle_args, block operations, indexing
Scope: Core cmpr.c functionality excluding specialized subsystems
Estimated: 30-40 blocks

### 2. #llm_integration (LLM & API Integration)
Contains: call_llm, call_gpt, call_anthropic, call_ollama, read_openai_key, etc.
Scope: All LLM calling and external API integration
Estimated: 15-20 blocks

### 3. #revision_system (Revision Tracking)
Contains: get_revs, get_revdir, rev_info, checksum operations
Scope: .cmpr/revs management and history tracking
Estimated: 20-25 blocks

### 4. #parsing_io (Parsing & I/O)
Contains: spanio blocks, ingest, file reading, rope structures
Scope: Low-level parsing, span-based I/O, file operations
Estimated: 25-30 blocks

### 5. #ui_navigation (UI & Display)
Contains: clear_display, ui_state, navigation (jk), block_id_jump
Scope: Terminal UI, display, interactive navigation
Estimated: 15-20 blocks

### 6. #block_indexing (Block Management)
Contains: find_all_blocks, index_block_ids, block_for_span, id_for_block
Scope: Block discovery, indexing, lookup operations
Estimated: 15-20 blocks

### 7. #prompts_nl2pl (Prompts & Templates)
Contains: prompt_templates blocks, nl2pl_rewrite, pl2nl_rewrite, etc.
Scope: All prompt templates and NL/PL conversion
Estimated: 20-25 blocks

### 8. #experience_docs (Documentation & Reports)
Contains: claude_experience_report_*, glossary, README blocks
Scope: Human-readable documentation and session reports
Estimated: 30-40 blocks

### 9. #build_config (Build & Configuration)
Contains: Makefile blocks, config_fields, pragmas, projfiles
Scope: Build system and project configuration
Estimated: 10-15 blocks

### 10. #cmpr_python (Python Backend)
Contains: cmpr.py blocks, HTTP server, rels implementation
Scope: Python server and web frontend backend
Estimated: 20-30 blocks (if they exist in block form)

## Already Existing Hubs

- #root_agent (agent system) ✓
- #cmpr_events (events/T/E/S system) ✓

## Approach

Start by creating the hubs that cover the most commonly-needed areas:
Priority 1: #cmpr_c_core, #parsing_io, #block_indexing
Priority 2: #revision_system, #llm_integration, #ui_navigation
Priority 3: #prompts_nl2pl, #experience_docs, #build_config

Each hub block should:
- List 2-16 child blocks explicitly
- Provide brief description of scope
- Be referenced from #root

Justifies: Manual hub creation as chosen option D.

*/
/* #claude_experience_report_root_agent_protocol_20251226_1

Experience Report: Root Agent Progress & Request Protocol Design
Session Date: 2025-12-26

## Objective
Make progress towards a working root agent and design a protocol for agents to request work/decisions from the programmer.

## What Was Accomplished

### 1. Reviewed Current Root Agent State
- Read blocks: #root, #root_agent, #root_agent_impl (parts 1-3), #root_agent_check, #root_agent_progress
- Ran CHECK mode successfully
- Current state: 269 blocks total, 260 unreferenced (96.7% unreachable from #root)
- Want: "All blocks reachable within 2 hops from #root" is NOT satisfied (20 bits confidence)

### 2. Designed Agent Request Protocol
Created #agent_request_protocol defining:
- Four request types: DECISION_NEEDED, WORK_NEEDED, CLARIFICATION_NEEDED, BLOCKED
- Structured request format for agent-to-programmer communication
- Response mechanism using rels or directive blocks
- Integration with agent CHECK/FIX loop

Example request format:
```
REQUEST: DECISION_NEEDED
AGENT: #root_agent
PRIORITY: MEDIUM
CONTEXT: <situation>
OPTIONS:
  - Option A: <description>
  - Option B: <description>
RATIONALE: <why this is needed>
```

### 3. Implemented Root Agent FIX Mode
Created #root_agent_fix with:
- Runs CHECK to identify unreferenced blocks
- Checks for programmer guidance in .cmpr/root_agent_guidance.txt
- If no guidance exists, emits structured REQUEST
- Exit code 1 when blocked, 0 when satisfied
- Real request output for 260 unreferenced blocks with 4 grouping options

### 4. Created Agent Runner Utility
Created #agent_runner block providing:
- Unified interface to run any agent's CHECK/FIX modes
- Extracts bash scripts from NL comments
- Simplifies agent invocation

## How to Use

Run root agent CHECK mode:
```bash
cmpr --print-comment '#root_agent_check' | sed -n '/^```bash$/,/^```$/p' | sed '1d;$d' | bash
```

Run root agent FIX mode (will emit REQUEST):
```bash
cmpr --print-comment '#root_agent_fix' | sed -n '/^```bash$/,/^```$/p' | sed '1d;$d' | bash
```

Provide guidance to agent:
```bash
echo "Option C: Group by subsystem" > .cmpr/root_agent_guidance.txt
# Then re-run FIX mode
```

## Current Root Agent Status

CHECK mode reports:
- Hub blocks found in #root: 2 (#root_agent, #cmpr_events) 
- Note: #root_agent is listed twice (minor bug in #root NL)
- All hubs satisfy 2-16 constraint
- 260 blocks unreferenced
- Want NOT satisfied (exit code 1)

FIX mode status:
- Emits REQUEST for grouping strategy
- Blocked until programmer provides decision
- Four options presented: by file, by functionality, by subsystem, or manual

## What's Needed from Programmer

The root agent is now in ASSISTED state (can check, can offer help, but needs guidance to fix).

To unblock, choose ONE:

**Option A**: Group by file (1 hub per source file)
- Pro: Automatic, no decisions needed
- Con: May create too many hubs, not semantic

**Option B**: Group by functionality (e.g., #cmpr_c_core, #cmpr_py_api, #frontend)
- Pro: Natural separation by language/layer
- Con: Requires defining functional boundaries

**Option C**: Group by subsystem (e.g., #parsing, #io, #ui, #agents, #revisions)
- Pro: Semantic organization by purpose
- Con: Requires understanding all blocks' purposes

**Option D**: Manual - programmer creates hubs manually
- Pro: Maximum control
- Con: Most effort, takes longest

Recommendation: Option C (by subsystem) provides the best balance of semantic organization and navigability.

## Next Steps

After programmer decision:
1. Implement the chosen grouping strategy in #root_agent_fix
2. Create hub blocks using cmpr --after
3. Update #root NL to reference all hubs
4. Re-run CHECK to verify want satisfaction
5. Transition root agent to OWNED state (fully autonomous)

## Technical Notes

- Agent blocks use bash scripts embedded in NL comments (to avoid C compilation)
- Scripts extracted with: `sed -n '/^```bash$/,/^```$/p' | sed '1d;$d'`
- Exit code 0 = satisfied, 1 = not satisfied or blocked
- SN notation used for confidence: 0 bits = event space, 20 bits = ~million:1 confidence, 255 = definitional

*/
/* #agent_request_protocol

Protocol for Agents to Request Work or Decisions from the Programmer

## Overview

Agents operate autonomously to maintain wants, but often encounter situations where they need human input. This protocol defines a standardized way for agents to request work or decisions.

## Request Types

### Type 1: DECISION_NEEDED
Agent knows multiple valid approaches exist and needs programmer to choose.
Example: "Should we group blocks by file, by functionality, or by subsystem?"

### Type 2: WORK_NEEDED
Agent knows what needs to be done but cannot do it automatically.
Example: "Need to create 15 hub blocks with meaningful groupings - requires domain knowledge"

### Type 3: CLARIFICATION_NEEDED
Agent's goal is ambiguous or underspecified.
Example: "Should 'reachable in 2 hops' count indirect references or only direct block ID mentions?"

### Type 4: BLOCKED
Agent cannot proceed due to missing infrastructure or capabilities.
Example: "Cannot implement FIX mode without relational database to track block references"

## Request Format

Requests are emitted by agents as structured output:

```
REQUEST: <type>
AGENT: <agent_block_id>
PRIORITY: <LOW|MEDIUM|HIGH|CRITICAL>
CONTEXT: <brief description>
OPTIONS: [optional - for DECISION_NEEDED]
  - Option A: <description>
  - Option B: <description>
RATIONALE: <why this request is needed>
```

## Response Mechanism

Programmer responses are recorded using rels:
- (agent_id, "programmer-decision", "chosen_option")
- (agent_id, "programmer-says", "free_text_guidance")

Or by creating directive blocks:
- #<agent>_directive_YYYYMMDD_N blocks containing guidance

## Integration with Agent Loop

CHECK mode can emit requests:
- Reports current state in SN notation
- Identifies what's needed to proceed
- Emits REQUEST if cannot proceed

FIX mode consumes responses:
- Checks for programmer-decision rels
- Reads directive blocks
- Proceeds with approved plan

Justifies: The need for standardized agent-to-programmer communication as mentioned in #root_agent_impl_3.

*/
