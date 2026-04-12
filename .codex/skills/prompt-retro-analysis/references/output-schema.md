# Output Schema

## Normalized CSV

- `prompt_prefix`
- `prompt_raw`
- `timestamp_utc`
- `timestamp_kst`
- `kst_date`
- `kst_hour`
- `session_id`
- `turn_id`
- `model`
- `hook_event_name`
- `prompt`
- `prompt_char_count`
- `prompt_line_count`
- `is_multiline`
- `contains_question`
- `has_file_reference`
- `has_command_reference`
- `repair_signal`
- `context_dependency_signal`

## Session Summary CSV

- `session_id`
- `prompt_count`
- `session_start_kst`
- `session_end_kst`
- `session_duration_minutes`
- `models_used`
- `prompt_prefixes`
- `matching_commit_authors`
- `nonmatching_prompt_prefixes`
- `author_match_rate`
- `repair_signal_count`
- `context_dependency_count`
- `context_management_rating`
- `avg_prompt_chars`
