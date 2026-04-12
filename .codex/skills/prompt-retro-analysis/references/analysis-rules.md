# Analysis Rules

## Suggested Exclusions

- automatic title-generation wrapper prompts
- hook smoke tests
- obvious keyboard noise
- bare acknowledgements with no standalone task meaning

## Useful Derived Fields

- `prompt_prefix`
- `prompt_raw`
- `timestamp_kst`
- `kst_date`
- `kst_hour`
- `prompt_char_count`
- `prompt_line_count`
- `is_multiline`
- `contains_question`
- `has_file_reference`
- `has_command_reference`
- `repair_signal`
- `context_dependency_signal`
- `prefix_matches_commit_author`

## Session-Level Questions

- Does the prompt prefix align with a real Git commit author in the analyzed repository?
- Are multiple prompt prefixes mixed inside one session?
- Did the session move from planning to implementation?
- Did it later move into debugging?
- Were the good prompts explicit about file, function, or output format?
- Were weak prompts overly context-dependent?
- Did the session manage context explicitly, or rely on short follow-up prompts that only make sense from prior turns?

## Markdown Report Shape

- scope and exclusions
- key observations
- author alignment findings
- strong prompt patterns
- weak prompt patterns
- session-based context management findings
- reusable rules
