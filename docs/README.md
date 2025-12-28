# cmpr System Reports

This directory contains auto-generated markdown reports providing visibility into the cmpr project state.

## Reports

- **[reachability.md](reachability.md)** - Block navigation structure and reachability analysis
- **wants_dashboard.md** - All wants in the system and their automation states (TBD)
- **event_activity.md** - Recent event system activity and agent executions (TBD)

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

## Generation

Reports can be regenerated using:
```bash
# Reachability report
dist/cmpr --print-code '#root_agent_check_impl' | bash

# Full export (when implemented)
dist/cmpr --export-docs
```

## Safety

These reports contain only project metadata:
- Block navigation structure
- Wants (goals and constraints)
- Automation state tracking
- Agent execution patterns
- Event system activity

No secrets, credentials, or sensitive data are included.
