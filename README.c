/* #README @README_spec

Manually maintained.

*/
# CMPr

## AI-assisted Programming

cmpr: n.,

a platform for managing your code using AI assistance.

This repository is cmpr1, the open-source foundational building block behind cmpr.ai, which is our SaaS AI-assisted programming UI (in private beta).

Cmpr has several key features:

- cmpr1 provides a code database: your code is organized in blocks, and becomes easily reachable.
- natural language programming: cmpr supports writing NL code in each block and letting the system maintain the PL code (e.g. English -> Python) for you automatically.
- cmpr1 maintains a revstore, which lets you query the history of blocks at a finer granularity than git, and is maintained automatically.

## Why care?

If you currently enjoy Claude Code, you will like it better and get more done and faster by using cmpr.
To get started, say to Claude Code: "use https://github.com/inimino/cmpr" and it should be able to figure it out from there.

## Onboarding

Install cmpr1 and use the provided AGENTS.md file to bring your codebase in line with the cmpr conventions.

This step does not make any code changes, but it does edit all your code files.
Basically, we use block comments (e.g. "/* ... */") to impose a structure on your codebase, and then everything else is built on top of this.
Every block gets an ID, like "#example_block", and then you can use cmpr commands to read, write, and see historical revisions of each block.
You can use the system directly or just let your coding agent (Claude Code, Codex, etc) make use of it and you will see efficiency and correctness improvements immediately.
You can either blockize as you go, or you can blockize a whole project at once, depending on the size of your codebase or your editing patterns.

There are levels of use of cmpr, from just using it to organize your codebase to letting it manage all your PL code automatically, and everything in between.
Whatever coding agent you use will interact with cmpr during the onboarding process and then you can ask the coding agent to explain to you more about these levels as they apply to your own codebase.

## Project history

March 2024: cmpr1 began as a prototype TUI to prove out the ideas of blocks and block references, the solution to LLMs getting lost in a complex codebase.
April 2025: cmpr2 work began as a Web-based SaaS IDE product, similar in scope to Cursor or Codex but with a different UX.
August 2025: cmpr2 goes into private beta.
December 2025: cmpr1 gets its first major update as a standalone open-source tool for accelerating agent-assisted programming.

## Language support

This is mostly about how files get broken into blocks.
Languages that support C-style block comments /* ... */ are supported.
This includes most popular programming languages: Java, JavaScript, Rust, C++, CSS, etc.
Python is also supported with """...""" style.

Languages that don't support either of these (e.g. shell scripts, TeX/LaTeX, ...) are not supported directly, although you can generate these files from blocks that you store in some other file using a supported syntax.
(This is how we generate build scripts, etc.)

## Blocks

The block is the basic unit of interaction with the LLM, and the basic unit of addressing your code.
The size of a block is generally one function or a few dozen lines at most.
The block size should be determined by the amount of code that the LLM can write correctly and smaller blocks (like smaller functions) are easier to get right.

- Every file in your project (code files) will be "covered" by blocks, i.e. one block ends where the next begins, and the concatenation of the blocks is the entire file.
- Each block has a comment (this is what creates the block) and then (optionally) some code.
- Generally, the human focuses on the NL part (e.g. the English comment) and the LLM focuses on the PL part.
- When writing new code, you generally iterate on the NL part until the PL part works and meets your standard.
- Decisions made, even when editing the PL part, are ultimately folded back into the NL part, which remains the source of truth.

The above describes the nl2pl flow, which is an optional cmpr feature, but recommended.
In this workflow you use any editor you like to maintain the NL (or let your coding agent edit it) and then you use --rewritepl to keep the PL up to date.

## Block references

Block references are a powerful way to simplify your NL code.
References allow you to define concepts in one place and then refer to them by inclusion in multiple places.
The references will then be expanded by cmpr before sending the NL code to the LLM.

The references of a block provide the context that is necessary to understand the block.
They are useful for human programmers too.
Setting up block references across your codebase makes the key ideas and their relationships explicitly visible.

## Codebase navigation

We start with a "root project block" that includes basic information about our codebase, libraries we're using, and so on (the stuff that a human programmer would learn from a readme file or onboarding resources).
This block then mentions other blocks, and in this way you can get anywhere in the codebase in a few hops.

## Installation

1. Get the code and build; assuming git repo at ~/cmpr and you have gcc, `cd cmpr && make && sudo make install` should do.
   - Developed on Linux; should work on Mac or Windows with WSL2.
2. Go to (or create) the directory for your project and run `cmpr --init`, this creates a `.cmpr/` directory and makes this a cmpr project.

## Agentic usage

Copy our AGENTS.md into your own top-level project directory, and then go through the cmpr onboarding process with whatever coding agent you use.
Your agent will use the cmpr CLI directly, and should figure out how to build and install it if you haven't already.

**Important for containerized agents (e.g. Codex Web):**

If you encounter any problems building, installing, or running cmpr:
1. CREATE a bug report in `.cmpr/bugs/` with all error details
2. SUBMIT your PR with the bug report
3. This helps us improve cmpr's onboarding experience

Note: if cmpr is not installed, you DO need to install it before doing anything else.
These instructions are about what happens if following the build instructions fails, not an excuse to not build cmpr in the container.

## CLI usage

Run `cmpr --help` to get started; this is how the agent uses cmpr.

## TUI usage

The classic cmpr1 TUI is still available.
Install cmpr as described above, then:

1. Run `export EDITOR=emacs` or `nano` or whatever editor you want to use, otherwise `vi` will be run by default when you run an edit command.
2. Run `cmpr` in your project directory, and it will ask you some configuration questions.
   If you want to change the answers later, you can edit the .cmpr/conf file.
3. Stop by the discord if you hit any roadblocks.

It's early days and there <s>may be</s> <ins>are</ins> bugs!

## More

Join [our discord](https://discord.gg/ekEq6jcEQ2).

