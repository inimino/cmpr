# Event System Report
Generated: 2025-12-28 10:00:37

## Overview

- **Total events in T**: 7
- **Source**: Mixed (domain agent + meta-level events)

## Event Spaces Identified

| Event Space Prefix | Example Event | Source Layer |
|---|---|---|
| `"Agent: "` | `"Agent: root_agent"` | Domain (execution metadata) |
| `"Mode: "` | `"Mode: CHECK"` | Domain (execution metadata) |
| `"Status: "` | `"Status: constraint not satisfied"` | Domain (verification result) |
| `"Hub blocks: "` | `"Hub blocks: 8"` | Domain (measured values) |
| `"The want is: "` | `"The want is: all blocks reachable..."` | Meta (want identifier) |
| `"Automation state: "` | `"Automation state: assisted"` | Meta (maturity tracking) |
| `"CHECK implementation: "` | `"CHECK implementation: #root_agent_check_impl"` | Meta (infrastructure detection) |

## All Events in Current T

| Event String | Strength | Event Space | Layer |
|---|---|---|---|
| `"Agent: root_agent"` | 255 | "Agent: " | Domain |
| `"Mode: CHECK"` | 255 | "Mode: " | Domain |
| `"Timestamp: 2025-12-28T09:34:44+00:00"` | 255 | "Timestamp: " | Domain |
| `"Hub blocks: 19"` | 255 | "Hub blocks: " | Domain |
| `"Hub violations: 0"` | 255 | "Hub violations: " | Domain |
| `"Unreferenced blocks: 220"` | 255 | "Unreferenced blocks: " | Domain |
| `"Status: constraint not satisfied"` | 255 | "Status: " | Domain |

## Event Composition Pattern

This snapshot demonstrates the **composition pattern**:

- **Domain events** (Layer: Domain): Emitted by `#root_agent_check_impl`
  - Execution metadata: Agent, Mode, Timestamp
  - Measured values: Hub blocks, Hub violations, Unreferenced blocks
  - Verification result: Status

- **Meta-level events** (Layer: Meta): Emitted by want maturation agent
  - Want identifier: Links to specific want text
  - Automation state: tracked/checked/assisted/owned
  - Infrastructure detection: Which blocks exist

Both sets of events coexist in the same T state because the want maturation
agent **calls** the domain agent rather than replacing it.

## Agent → Event Mapping

| Agent Block | Events Emitted | Event Spaces Used |
|---|---|---|
| `#root_agent_check_impl` | Agent, Mode, Timestamp, Hub blocks, Hub violations, Unreferenced blocks, Status | Domain execution/measurement spaces |
| `#want_maturation_agent` (simulated) | The want is, Automation state, CHECK implementation | Meta-level tracking spaces |

## How to Query These Events

```bash
# Current snapshot (before memorize)
dist/cmpr --T

# After memorize, recall by want text
dist/cmpr --T0
dist/cmpr --event "The want is: all blocks reachable from #root in ≤2 hops" --strength 255
dist/cmpr --recall
dist/cmpr --T
```

---

*This report demonstrates the event system's ability to layer meta-level reasoning*
*on top of domain-specific verification without replacing existing agents.*
