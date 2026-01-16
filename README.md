# CMPr

## Accelerating AI-assisted Programming

Cmpr a platform for managing your code using AI assistance.

## Features

- a block database for your code
- natural language programming: cmpr supports writing NL code in each block and letting the system maintain the PL code.
- cmpr1 maintains a revstore, which gives you finer resolution in the time axis than git with no extra effort except adding files to the cmpr project and running a process to watch the files and automatically store revs.

## Why use it?

If you currently use Claude Code, you can perform the same tasks with 80% to 98% fewer tokens.
This means it is cheaper, faster, and you will get a better result.

## Quick Start

Install cmpr and tell Claude Code to use cmpr for reading and writing code instead of it's built-in file read and update tools, or ad-hoc bash scripts, and to set up your existing code files in blocks and add them to .cmpr/conf, either all at once or as you go.

Recommended: `cmpr --help claude-setup >> CLAUDE.md`

Create a root block that describes your project at a high level and will link to all your other blocks.
Claude (based on the recommended CLAUDE.md) will print this block first in each session and then navigate to the rest of your code from there.

## Project history

March 2024: cmpr1 began as a prototype TUI to prove out the ideas of blocks, block references, and context management.
April 2025: cmpr2 work began as a Web-based SaaS IDE product, similar in scope to Cursor or Codex but with a different UX.
August 2025: cmpr2 goes into private beta.
December 2025: cmpr1 gets its first major update as a standalone open-source tool for accelerating agent-assisted programming.

## Installation

1. Get the code and build; assuming git repo at ~/cmpr and you have gcc, `cd cmpr && make && sudo make install` should do.
   - Developed on Linux; should work on Mac or Windows with WSL2.
2. Go to (or create) the directory for your project and run `cmpr --init`, this creates a `.cmpr/` directory and .cmpr/conf where your files will be listed.

## Agentic usage

As of January 2026 cmpr is recommended for use with Claude Code, but can be used with Codex or any other coding agent with similar results.

**Important for containerized agents (e.g. Codex Web):**

If you encounter any problems building, installing, or running cmpr:
1. DO NOT try to fix the problem yourself
2. CREATE a bug report in `.cmpr/bugs/` with all error details
3. SUBMIT your PR with the bug report
4. This helps us improve cmpr's onboarding experience

Note: if cmpr is not installed, you DO need to install it before doing anything else.
These instructions are about what happens if following the build instructions fails, not an excuse to not build cmpr in the container.

## CLI usage

Run `cmpr --help` to get started; this is how the agent uses cmpr.

## TUI usage

The classic cmpr1 TUI is still available.
Install cmpr as described above, then run `cmpr` in your project directory, use j/k to scroll through your blocks, q to quit and ? to see all other keyboard shortcuts.

## More

Join [our discord](https://discord.gg/ekEq6jcEQ2).
