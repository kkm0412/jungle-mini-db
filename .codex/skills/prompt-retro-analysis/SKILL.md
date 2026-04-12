---
name: prompt-retro-analysis
description: Use when analyzing Codex prompt logs such as logs/{username}-prompt-log.jsonl from a target repository into cleaned CSV and markdown retrospective outputs, including git user name prefix evaluation against commit authors, timezone normalization, exclusion filtering, session grouping, and session-based context management assessment.
---

# Prompt Retro Analysis

Use this skill when the user wants to turn raw Codex prompt logs into a retrospective dataset or report for a selected log source and its repository context.

## Workflow

1. Inspect the log schema and sample rows first.
2. Confirm the source timezone and the reporting timezone.
3. Normalize the log into a tabular file with derived columns:
   - UTC timestamp
   - local timestamp
   - session metadata
   - prompt shape flags
4. Exclude noise deliberately:
   - title-generation wrapper prompts
   - obvious test noise
   - context-only followups if the report calls for primary prompts only
5. Compare prompt authorship signals:
   - extract `prompt_prefix`
   - compare it against Git commit author names in the analyzed repository
   - flag aligned, partially aligned, or unmatched prefixes
6. Group by `session_id` and summarize:
   - session duration
   - dominant topics or task types
   - prompt-prefix and commit-author alignment
   - repair-heavy sessions
   - session-based context management quality
   - plan-to-implementation or implementation-to-debug transitions if the data supports it
7. Produce compact outputs the team can reuse:
   - normalized CSV
   - session summary CSV
   - a short markdown lessons document

## Working Rules

- Prefer deterministic transformations over ad hoc manual edits.
- Be explicit about exclusion rules in the report.
- If the user mentions Korean time or KST, normalize to `Asia/Seoul`.
- Keep the lessons document opinionated but evidence-based.
- When a log source is unusual, read the schema first rather than assuming field names.
- Treat session-based context management as a first-class evaluation dimension.
- When commit author names and prompt prefixes disagree, report that mismatch explicitly.

## Resources

- Run `scripts/normalize_prompt_log.py` to derive row-level fields from JSONL input.
- Run `scripts/build_prompt_retro_report.py` to create a session summary CSV from the normalized CSV and optionally compare prompt prefixes to Git author names.
- Read `references/analysis-rules.md` when deciding exclusion rules or report sections.
- Read `references/output-schema.md` when you need to know the expected output columns.
