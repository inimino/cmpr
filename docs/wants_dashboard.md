# All Wants Dashboard
Generated: 2025-12-28 10:00:37

## Overview

- **Total wants**: 23
- **Automation state distribution**:
  - Tracked: 23 (100%)
  - Checked: 0 (0%)
  - Assisted: 0 (0%)
  - Owned: 0 (0%)

## Automation State Definitions

| State | Definition | Capabilities |
|---|---|---|
| **Tracked** | Want is documented | Can read want statement |
| **Checked** | Can verify if met | Can run CHECK agent |
| **Assisted** | Can help fix violations | Can run FIX agent |
| **Owned** | Automatically maintained | System enforces want |

## All Wants Detail

| # | Want (truncated) | Block | Agent | State |
|---|---|---|---|---|
| 1 | We want this block to contain a list of blocks, such that each block contains... | `#root` | none | Tracked |
| 2 | We want to be able to pipe at least up to $2^{30}$ bytes into cmpr --checksum... | `#cmpr_checksum` | none | Tracked |
| 3 | We want to specify the contents of cmpr.c here as a list of block references. | `#cmpr1_build_manifest` | none | Tracked |
| 4 | We want blocks that appear after #INBOX to be moved to appropriate locations ... | `#want_maturation_agent_check` | none | Tracked |
| 5 | We want public_html/wants_dashboard.html generated and kept current, showing ... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 6 | We want public_html/nl2pl_health.html generated and kept current, showing whi... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 7 | We want public_html/navigation_graph.html generated and kept current, showing... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 8 | We want public_html/inbox_flow.html generated and kept current, showing curre... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 9 | We want public_html/event_activity.html generated and kept current, showing t... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 10 | We want public_html/block_size.html generated and kept current, showing block... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 11 | We want public_html/test_coverage.html generated and kept current, showing te... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 12 | We want public_html/revision_activity.html generated and kept current, showin... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 13 | We want public_html/agent_ecosystem.html generated and kept current, showing ... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 14 | We want public_html/dependency_map.html generated and kept current, showing b... | `#claude_experience_report_block_quality_agents_20251227` | none | Tracked |
| 15 | We want this block to contain a list of blocks... | `#claude_experience_report_root_agent_per_block_plan_20251227` | none | Tracked |
| 16 | We want to be able to pipe at least up to $2^{30}$ bytes... | `#claude_experience_report_root_agent_per_block_plan_20251227` | none | Tracked |
| 17 | We want this block to contain a list of blocks... | `#claude_experience_report_root_agent_per_block_plan_20251227` | none | Tracked |
| 18 | We want to be able to pipe at least up to... | `#root_agent_per_block_tracking_plan` | none | Tracked |
| 19 | We want to specify the contents of cmpr.c here... | `#root_agent_per_block_tracking_plan` | none | Tracked |
| 20 | We want this block to contain a list of blocks... | `#claude_experience_report_root_agent_per_block_plan_20251227` | none | Tracked |
| 21 | We want cmpr1 to have the essential blocks... | `#root_agent_per_block_tracking_plan` | none | Tracked |
| 22 | We want this block to contain a list of blocks, such that each block contains... | `#root` | none | Tracked |
| 23 | We want cmpr1 to have the essential blocks from cmpr2 that are referenced by ... | `#cmpr_events` | none | Tracked |

## Maturation Path Analysis

### Current State: All Tracked

All 23 wants are currently in **tracked** state. None have CHECK or FIX agents.

### Maturation Priorities

Based on impact and feasibility, suggested order for maturation (tracked → checked → assisted):

1. **Navigation Graph (Block #root)** - Already has CHECK agent (#root_agent_check_impl)
   - Move to: **checked** (add agent integration)
   - Impact: Core infrastructure health

2. **Reports (#report_wants)** - All 10 report wants
   - Move to: **checked** (implement report generators)
   - Impact: System visibility

3. **INBOX Organization** - Block relocation tracking
   - Move to: **checked** (scan INBOX blocks)
   - Impact: Codebase organization

4. **nl2pl Health** - Code generation tracking
   - Move to: **checked** (parse 'Manually maintained.' markers)
   - Impact: Development workflow

## Event Spaces for Want Tracking

Each want can define event spaces. Current examples:

| Want | Event Space | Example Events |
|---|---|---|
| Navigation (#root) | BR (Block Reachability) | `"The block id is: #foo"` + `"The block is reachable"` |
| All Wants Dashboard | RW (Report: Wants) | `"Report type: wants_dashboard"` + `"Total wants: 23"` |
| Navigation Graph Report | RNG (Report: Nav Graph) | `"Blocks at 2 hops: 342"` + `"Blocks at >2 hops: 5"` |

## Next Steps

1. **Implement want maturation agent** (#want_maturation_agent_check)
   - Parse --agents-wants output
   - Detect CHECK/FIX agent blocks
   - Determine automation state
   - Emit events to T

2. **Add automation metadata to wants**
   - Which wants have agents?
   - Which agents are tested?
   - Which wants have event spaces defined?

3. **Generate maturation roadmap**
   - Dependency analysis (which wants enable others?)
   - Effort estimation (lines of code needed)
   - Priority scoring (impact × feasibility)

## References

- Want maturation framework: `cmpr --print-comment '#want_maturation_overview'`
- Event system guide: `cmpr --print-comment '#event_system_guide'`
- Root agent (example): `cmpr --print-comment '#root_agent_check_impl'`
- All report wants: `cmpr --print-comment '#report_wants'`
