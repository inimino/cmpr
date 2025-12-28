# cmpr System Reports

This directory contains auto-generated markdown reports providing visibility into the cmpr project state.

## Reports

- **wants_dashboard.md** - All wants in the system and their automation states
- **event_activity.md** - Recent event system activity and agent executions

## Generation

Reports are generated using:
```bash
dist/cmpr --print-code '#generate_export_docs' | sh
```

Or via the CLI command:
```bash
dist/cmpr --export-docs
```

## Safety

These reports contain only project metadata:
- Wants (goals and constraints)
- Automation state tracking
- Agent execution patterns
- Event system activity

No secrets, credentials, or sensitive data are included.

## Freshness

Reports are snapshots and may be outdated. Check generation timestamps in each report.
