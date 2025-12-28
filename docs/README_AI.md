# cmpr: AI-Native Programming Environment

> **Note**: This is an AI-generated comprehensive README. For the original human-written version, see [../README.md](../README.md).

## Accelerating AI-Assisted Programming

**cmpr makes AI-assisted programming 10-50x more efficient.**

If you're using Claude Code, Cursor, Codex, or similar AI coding tools, cmpr will:
- **Cut token costs by 80-98%** through precise block-based navigation
- **Speed up development** by eliminating context-finding overhead
- **Improve code quality** with natural language specifications as source of truth
- **Enable higher abstraction thinking** - focus on "what", let AI handle "how"

## What is cmpr?

cmpr is a block-based code organization system designed from the ground up for AI-assisted programming.

Instead of forcing AI to navigate code like humans (files, folders, grep), cmpr provides:
- **Block-addressable code** - every function/feature is a discrete, uniquely-identified block
- **Explicit navigation** - reach any code in 2 hops from root via block references
- **Natural language programming** - NL comments are specification, PL code is generated
- **Built-in context management** - block references create explicit dependency graphs

**Result**: AI agents get exactly the context they need, nothing more. No wasted tokens, no lost context.

## Quick Start

### Installation

```bash
# Build and install (requires gcc)
git clone https://github.com/yourusername/cmpr
cd cmpr
make
sudo make install

# Initialize a project
cd your-project
cmpr --init
```

### For AI Agent Users

Copy our `AGENTS.md` into your project directory and let your AI coding agent (Claude Code, Cursor, etc.) onboard your codebase:

```bash
cp path/to/cmpr/AGENTS.md your-project/
# Then ask your AI agent: "Please onboard this project to cmpr using AGENTS.md"
```

Your agent will:
1. Add block IDs to your code using comments
2. Set up the `.cmpr/` directory
3. Configure the project for cmpr workflow

**No code changes** - just structural annotations via comments.

### Basic Usage

```bash
# Navigate from root to any block
cmpr --print-comment '#root'        # See navigation hubs
cmpr --print-comment '#specific_block'  # Read a specific block

# Search across blocks
cmpr --grep 'pattern'

# Edit natural language spec
cmpr --replace-comment '#block_id' < new_spec.txt

# Regenerate code from spec
cmpr --rewritepl '#block_id'

# See all blocks in project
cmpr --files-blocks
```

### Example Block

```c
/* #example_add

Add two integers and return the sum.

Parameters:
  a: first integer
  b: second integer

Returns: sum of a and b
*/

int add(int a, int b) {
    return a + b;
}
```

The comment is the **source of truth**. The code can be regenerated from it.

## Core Concepts

### Blocks

The block is the fundamental unit:
- One block = one function/feature (typically)
- Size determined by "what AI can write correctly in one go"
- Each block has a Natural Language (NL) part and optional Programming Language (PL) part
- Blocks have unique IDs like `#block_name`

### Navigation

Start at `#root` block → follow references → reach anywhere in 2 hops:

```bash
cmpr --print-comment '#root'           # See navigation hubs
cmpr --print-comment '#auth_hub'       # Navigate to auth subsystem
cmpr --print-comment '#auth_login'     # Read specific function
```

No grep needed. Navigation is explicit.

### Block References

Blocks reference other blocks to establish context:

```
@auth_config - Authentication configuration
@jwt_utils - JWT token utilities

Uses @auth_config to validate credentials and @jwt_utils to generate tokens.
```

cmpr automatically expands references before sending to AI.

### Natural Language Programming

The NL comment is source of truth. Standard workflow:

1. **Edit NL spec**: `cmpr --replace-comment '#block_id' < spec.txt`
2. **Generate PL code**: `cmpr --rewritepl '#block_id'`
3. **Test**: Run your tests
4. **Iterate**: If code wrong, improve spec and regenerate

Focus on specification, not implementation.

## Why Use cmpr?

### Token Efficiency

**Traditional approach:**
```bash
# AI reads entire file to find one function
Read src/auth.js (2400 tokens) → Find login → Modify
```

**cmpr approach:**
```bash
# AI reads exactly what it needs
cmpr --print-comment '#auth_login' (120 tokens) → Modify
```

**Result: 20x fewer tokens** for typical operations. 50x is possible for large files.

### Built By AI, For AI

The entire cmpr codebase (~360KB C, 330+ blocks) was **written by AI from English descriptions**:
- Custom span-based I/O library
- Revision system with cryptographic checksums
- Terminal UI with vim-like interaction
- Block expansion and LLM integration

Not templates. Not copied. **Generated from natural language.**

cmpr proves its own value proposition: it was built using itself.

### Self-Maintaining

cmpr includes an "agent system" - agents maintain wants (invariants):
- "Every block reachable from root in ≤2 hops"
- "NL and PL parts stay synchronized"
- Auto-rebuild on source changes

The system maintains itself.

## Levels of Adoption

You can adopt cmpr incrementally:

1. **Level 1: Organization** - Just use block IDs for navigation
2. **Level 2: Hybrid** - Let AI generate some blocks, manually maintain others
3. **Level 3: NL-first** - AI generates all PL from NL specs
4. **Level 4: Agent-maintained** - Agents enforce invariants automatically

Start where you're comfortable.

## Language Support

**Supported:**
- Any language with `/* */` block comments (C, C++, Java, JavaScript, Rust, CSS, etc.)
- Python (using `"""..."""` docstrings)

**Workarounds for unsupported languages:**
- Store scripts in blocks, export to files during build
- Use cmpr revision tracking on files (but not block-addressable)

## Additional Features

### Revision System

Fine-grained automatic versioning:
- Every block change saved to `.cmpr/revs/`
- SipHash checksums for integrity
- Complements git with block-level history

### TUI (Terminal UI)

Classic interactive mode:
```bash
export EDITOR=vim  # or emacs, nano, etc.
cmpr  # Interactive mode
```

Vim-like navigation: `j/k` move, `/` search, `r` regenerate, `B` build.

### Event System

Track what's happening in your codebase:
- Agent executions
- Want satisfaction tracking
- Temporal reasoning about code state

## Project Status

- **March 2024**: cmpr1 prototype - blocks, references, context management
- **April 2025**: cmpr2 web-based IDE begins development
- **August 2025**: cmpr2 private beta
- **December 2025**: cmpr1 major update - standalone open-source tool

This is **cmpr1** - the open-source foundation. The cmpr2 SaaS product builds on these concepts.

## Resources

- **[Human-written README](../README.md)** - Original concise version
- **[VALUE_PROPOSITION.md](VALUE_PROPOSITION.md)** - Detailed value analysis
- **[ONE_PAGE_PITCH.md](ONE_PAGE_PITCH.md)** - Quick pitch
- **[claude_exploration.md](claude_exploration.md)** - AI perspective
- **[CLAUDE.md](../CLAUDE.md)** - How to work with cmpr (for AI agents)
- **[AGENTS.md](../AGENTS.md)** - Agent integration guide
- **[Discord](https://discord.gg/ekEq6jcEQ2)** - Community support
- **CLI Help**: `cmpr --help`

## The Paradigm Shift

**Traditional programming:**
Human writes code → AI assists → Human reviews code

**cmpr programming:**
Human writes specification → AI writes code → Human reviews specification

You think at a higher abstraction level. AI handles the details.

This is what programming looks like when you design FOR AI, not despite it.

## Contributing

cmpr is open source and under active development. We welcome:
- Bug reports
- Feature requests
- Documentation improvements
- Code contributions

**For containerized agents (Codex Web, etc.):**
If you encounter build/install issues:
1. Create bug report in `.cmpr/bugs/`
2. Submit PR with bug report
3. This helps us improve onboarding

## License

See [LICENSE](../LICENSE) file.

## Get Help

- **Discord**: [Join our community](https://discord.gg/ekEq6jcEQ2)
- **GitHub Issues**: Report bugs and request features
- **Documentation**: Start with `cmpr --help`

---

**The bottom line**: If you're using AI coding assistants, cmpr makes you 10-50x more efficient.

Try it. You won't go back.
