# Agent workflow notes (conflict-safe)

## Context
These notes relocate the earlier README/INBOX guidance into a dedicated file to avoid merge conflicts while keeping the instructions available.

## Build and list agents
- Build the binary: `make`
- List available agents: `dist/cmpr --agents`

## Inspect and run agents
- Inspect an agent definition: `dist/cmpr --agent <name>`
- Dry-run an agent in CHECK mode against a workspace: `dist/cmpr --agent-run <name> CHECK <path>`
- Apply automated edits in FIX mode: `dist/cmpr --agent-run <name> FIX <path>`

## Visualization reference
For a visual explanation of the cmpr workflow, see the proposed block-graph diagram and blog-style walkthrough referenced in the documentation plan. The goal is to illustrate how blocks connect and how agents traverse them.

## Merge-conflict note
Previous attempts to update README.md and INBOX.c conflicted with upstream changes. Future edits should land in standalone files like this one until the branch can rebase cleanly.
