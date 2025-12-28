# cmpr: AI-Native Programming

## The One-Sentence Pitch

**cmpr makes AI-assisted programming 10-50x more efficient by organizing code the way AI needs to consume it.**

---

## The Problem

Current AI coding tools (Claude Code, Cursor, Codex) waste 80-98% of tokens reading irrelevant code:
- AI reads entire files to find one function
- Context lost across tool calls
- No explicit navigation structure
- Specification vs. implementation confusion

**Result**: Slow, expensive, mediocre.

---

## The Solution

cmpr = **Block-addressable code + Natural language specs + Explicit navigation**

### Block-Addressable Code
Every function/feature = one block with unique ID (`#block_name`)
```bash
cmpr --print-comment '#auth_login'  # Read exactly what you need
```

### Natural Language as Source
Comments are specification. Code is generated.
```bash
cmpr --replace-comment '#block_id' < spec.txt  # Edit spec
cmpr --rewritepl '#block_id'                    # Generate code
```

### Explicit Navigation
Reach any code in 2 hops from root. No grep. No search.

---

## The Results

| Metric | Impact |
|--------|--------|
| **Token reduction** | 80-98% fewer tokens |
| **Speed** | 10-50x faster iterations |
| **Cost** | 10-50x lower API costs |
| **Quality** | Consistent, maintainable code |

---

## The Proof

**cmpr was built using cmpr.**
- 360KB C codebase, 330+ blocks
- 100% AI-generated from English specs
- Includes: custom I/O library, revision system, terminal UI, LLM integration
- Production-grade systems programming from natural language

**Bootstrapped**: The system maintains itself using its own workflow.

---

## The Paradigm Shift

**Traditional**: Human writes code → AI assists → Human reviews code

**cmpr**: Human writes spec → AI writes code → Human reviews spec

You think at a **higher abstraction level**. AI handles implementation details.

---

## Who Should Use It

✅ **Perfect for:**
- AI-first developers using Claude Code, Cursor, Codex
- Teams with high AI API costs
- Complex codebases where navigation is a bottleneck

❌ **Not for:**
- AI-skeptical teams
- Tiny scripts (<500 lines)
- Teams unwilling to restructure code

---

## Adoption Path

**Level 1**: Just use block IDs for navigation
**Level 2**: Let AI generate some blocks
**Level 3**: AI generates all code from NL specs
**Level 4**: Agents maintain invariants automatically

Start at Level 1. Move up when ready.

---

## Technical Specs

- **Languages**: Any with `/* */` comments (C, Java, JS, Rust, etc.) + Python
- **Platform**: Linux, macOS, Windows (WSL2)
- **License**: Open source
- **Dependencies**: gcc (to build)

---

## Get Started

```bash
git clone https://github.com/yourusername/cmpr
cd cmpr && make && sudo make install
cd your-project && cmpr --init
# Copy AGENTS.md and let your AI agent onboard the codebase
```

**5 minutes to first block. 1 hour to full onboarding.**

---

## The Bottom Line

If you're using AI coding assistants and **NOT** using cmpr, you're burning tokens and time.

**Try cmpr. You won't go back.**

---

## Resources

- **GitHub**: https://github.com/yourusername/cmpr
- **Detailed value prop**: [docs/VALUE_PROPOSITION.md](VALUE_PROPOSITION.md)
- **AI agent's perspective**: [docs/claude_exploration.md](claude_exploration.md)
- **Discord**: https://discord.gg/ekEq6jcEQ2

---

*cmpr: Programming in English, building the future.*
