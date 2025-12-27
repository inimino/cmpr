Start by reading CLAUDE.md and try to be at least half as good as Claude Code.

- Use dist/cmpr for navigation and edits: begin with `dist/cmpr --print-comment '#root'`, follow block links, and prefer `--after`/`--replace` over manual edits for block-managed files.
- Avoid sed/awk/grep-first workflows; reserve traditional tools only when cmpr navigation truly fails.
- Run relevant tests before and after changes; keep TDD mindset by adding or updating tests alongside fixes.
- When you finish a session, add an experience report after #INBOX (name it like #codex_experience_report_TOPIC_YYYYMMDD_N) summarizing what changed and what to do next.
- Keep CLI behavior consistent with `dist/cmpr --help`, and rerun help output when modifying command-line flows to verify docs stay accurate.
