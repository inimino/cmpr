# cmpr System Documentation

This directory contains documentation and reports providing visibility into the cmpr project.

## README Files

- **[../README.md](../README.md)** - Human-written concise README (main project README)
- **[README_AI.md](README_AI.md)** - AI-generated comprehensive README with detailed examples and explanations

## Value and Presentation

- **[VALUE_PROPOSITION.md](VALUE_PROPOSITION.md)** - Comprehensive analysis of cmpr's value proposition, key benefits, and use cases
- **[ONE_PAGE_PITCH.md](ONE_PAGE_PITCH.md)** - One-page pitch document for presentations
- **[claude_exploration.md](claude_exploration.md)** - An AI agent's first-person account of exploring the cmpr codebase

## System Reports

Auto-generated markdown reports providing visibility into the cmpr project state:

- **[reachability.md](reachability.md)** - Block navigation structure and reachability analysis
- **[wants_dashboard.md](wants_dashboard.md)** - All wants in the system and their automation states (TBD)
- **[event_activity.md](event_activity.md)** - Recent event system activity and agent executions (TBD)

## Graph Visualizations

The [../graphs/](../graphs/) directory contains Graphviz DOT files for visualizing the codebase structure:

- **reachability_full.dot** - Complete root → hub navigation structure (44 hubs)
- **hub_expansion.dot** - Detailed expansion of selected hubs showing contained blocks

To generate visualizations:

```bash
# Install graphviz if needed
sudo apt-get install graphviz  # Ubuntu/Debian
brew install graphviz          # macOS

# Generate SVG (recommended for web)
dot -Tsvg graphs/reachability_full.dot -o graphs/reachability_full.svg
dot -Tsvg graphs/hub_expansion.dot -o graphs/hub_expansion.svg

# Or PNG
dot -Tpng graphs/reachability_full.dot -o graphs/reachability_full.png
dot -Tpng graphs/hub_expansion.dot -o graphs/hub_expansion.png
```

## Generating Reports

Reports can be regenerated using:

```bash
# Reachability report
dist/cmpr --print-code '#root_agent_check_impl' | bash

# Full export (when implemented)
dist/cmpr --export-docs
```

## Privacy and Safety

These reports contain only project metadata:
- Block navigation structure
- Wants (goals and constraints)
- Automation state tracking
- Agent execution patterns
- Event system activity

No secrets, credentials, or sensitive data are included.

## Additional Resources

For more information about cmpr:
- [CLAUDE.md](../CLAUDE.md) - How AI agents work with cmpr
- [AGENTS.md](../AGENTS.md) - Agent integration guide
