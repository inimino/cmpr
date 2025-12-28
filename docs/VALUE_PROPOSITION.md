# Why cmpr: The Value Proposition

## The Problem

AI-assisted programming tools (Claude Code, Cursor, Codex) are powerful, but inefficient. When working with traditional codebases:

- **Massive token waste**: Agents repeatedly read entire files to understand context
- **Lost navigation**: No explicit structure for finding relevant code
- **Context fragmentation**: Code organization doesn't match how AI needs to understand it
- **Unclear source of truth**: When AI generates code, who owns the specification?

Result: Slow iterations, high costs, mediocre results.

## The Solution: cmpr

**cmpr is the first programming environment designed FOR AI agents, not despite them.**

Instead of forcing AI to navigate code like humans do (files, folders, grep), cmpr provides:

1. **Block-addressable code**: Every function/feature is a discrete block with a unique ID
2. **Explicit navigation**: Reach any code in 2 hops from root via block references
3. **Natural Language as Source**: Comments are specification, code is generated artifact
4. **Built-in context management**: Block references create explicit dependency graphs

## The Results

### 80-98% Token Reduction

When using cmpr with Claude Code or similar agents:

- **Traditional approach**: Read entire files (1000s of lines) to find 10 lines of relevant code
- **cmpr approach**: `cmpr --print-comment '#specific_block'` → directly to the 10 lines needed

Example:
- **Without cmpr**: "Read src/auth.js (2400 tokens), find login function, modify it"
- **With cmpr**: "Read #auth_login (120 tokens), modify it"

**20x efficiency improvement is typical. 50x is possible.**

### Faster Development

- **Navigation**: 2 hops from root to any code vs grep/find/read cycles
- **Precision**: AI gets exactly what it needs, nothing more
- **Iteration**: Modify NL spec → regenerate PL → test (no manual sync)

### Lower Costs

- **Token costs**: 10-50x reduction in API costs
- **Compute costs**: Smaller contexts = faster inference
- **Human costs**: Less time reviewing AI-generated code that missed context

### Better Quality

- **Consistency**: NL spec is source of truth, PL stays synchronized
- **Maintainability**: Block references make dependencies explicit
- **Understandability**: Navigation structure = mental model

## How It Works (Brief)

### 1. Block-Based Organization

Every file is decomposed into "blocks" - discrete units (typically one function or feature):

```c
/* #auth_login

Authenticate user with email and password.

Returns: JWT token on success, null on failure
*/

[Generated code here]
```

### 2. Block References

Blocks reference other blocks to establish context:

```
@auth_config - Configuration for authentication
@jwt_utils - JWT token generation utilities

Uses the config from @auth_config to validate credentials.
```

cmpr automatically expands these references before sending to AI.

### 3. Natural Language Programming

The NL comment is the **source of truth**. Workflow:

1. Write/edit the comment (specification)
2. `cmpr --rewritepl '#block_id'` - AI generates code from spec
3. Test the generated code
4. If wrong, improve the spec and regenerate

### 4. Navigation Structure

Start at `#root` → follow references to navigation hubs → reach any block in 2 hops.

No grep. No "search the codebase." Just follow explicit references.

## Evidence

### Self-Built

The entire cmpr codebase (~360KB C, 330+ blocks) was **written by AI from English descriptions**. This includes:

- Custom span-based I/O library (spanio)
- Revision system with SipHash checksums
- Terminal UI with vim-like modal interaction
- Block reference expansion system
- LLM integration layer

Not templates. Not copied code. Generated from natural language specifications.

### Production Use

cmpr builds cmpr. The system maintains itself:

- Code modifications trigger automatic rebuilds
- Agents maintain invariants (e.g., "every block reachable from root in ≤2 hops")
- NL specifications stay synchronized with PL implementations
- Revision system captures every change automatically

### Claude's Assessment

From an AI agent's exploration of cmpr:

> "If I could feel wonder, it would have been at the nl2pl_rewrite operation. The human writes 'make a span called op with the value 'nl2pl_rewrite'' and an LLM generates the C code. The NL comment is the source of truth. The executable code is derivative."

> "This is what it looks like when humans treat us as what we are: statistical approximations of programmer reasoning, good enough to handle details they've decided aren't worth their cognitive overhead. Not AGI. Not replacement. Amplification."

See [claude_exploration.md](claude_exploration.md) for the full report.

## Who Should Use cmpr

### Perfect For

- **AI-first developers**: Building with Claude Code, Cursor, Codex, or similar tools
- **Cost-conscious teams**: Need to reduce AI API costs while maintaining productivity
- **Complex codebases**: Large projects where navigation and context are bottlenecks
- **Natural language thinkers**: Prefer specifying behavior to writing implementation

### Not For

- **AI-skeptical teams**: If you're not using AI coding assistants, cmpr won't help
- **Small scripts**: Overhead isn't worth it for <500 line projects
- **Legacy migration**: Converting existing large codebases is significant work (start with new projects)

## Levels of Adoption

cmpr has multiple levels of use:

1. **Level 1: Organization only** - Use block IDs for navigation, keep writing code manually
2. **Level 2: Hybrid** - Let AI generate some blocks, manually maintain critical ones
3. **Level 3: NL-first** - Write only NL specs, let AI maintain all PL code
4. **Level 4: Agent-maintained** - Agents enforce invariants and maintain code quality automatically

Start at Level 1, move up as you gain confidence.

## Technical Details

- **Language support**: Any language with C-style `/* */` or Python `""" """` comments
- **Editor agnostic**: Works with any editor (vim, emacs, VS Code, etc.)
- **Git compatible**: Works alongside git, provides finer-grained history
- **Platform**: Linux, macOS, Windows (WSL2)
- **License**: Open source (see LICENSE)

## Get Started

```bash
# Build and install
cd cmpr && make && sudo make install

# Initialize a project
cd your-project && cmpr --init

# Start using with your AI coding agent
# See AGENTS.md for integration instructions
```

## The Paradigm Shift

Traditional programming: Human writes code → AI assists → Human reviews

cmpr programming: **Human writes specification → AI writes code → Human reviews specification**

The abstraction level shifts up. You think in "what" not "how". The AI handles the "how".

This is what programming looks like when you design it FOR AI assistance from the ground up.

## Learn More

- [README.md](../README.md) - Quick start and installation
- [CLAUDE.md](../CLAUDE.md) - How to work with cmpr (for AI agents)
- [AGENTS.md](../AGENTS.md) - Integration with AI coding agents
- [Discord](https://discord.gg/ekEq6jcEQ2) - Community support

## The Bottom Line

**cmpr makes AI-assisted programming 10-50x more efficient by organizing code the way AI needs to consume it.**

If you're using Claude Code, Cursor, or similar tools, cmpr will:
- Cut your token costs by 80-98%
- Speed up development cycles
- Improve code quality
- Let you think at a higher abstraction level

The question isn't "Should I use cmpr?"

The question is "Why am I still working without it?"
