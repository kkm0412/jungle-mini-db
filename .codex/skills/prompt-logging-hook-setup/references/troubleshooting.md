# Hook Troubleshooting Checklist

## Read First

- Check `.codex/config.toml` and confirm `codex_hooks = true`.
- Check `.codex/hooks.json` and confirm the expected event is present.
- Check the referenced hook script exists and is readable.
- Check the hook script records `git_user_name`, `prompt_prefix`, and `prompt_raw`.
- Check whether the repository being inspected must be opened in trusted mode for `.codex` to load.

## Common Failure Modes

### Hook never fires

- `.codex` was not loaded because the project is not trusted.
- `codex_hooks` is disabled.
- the wrong event name was configured.

### Hook fires but no log file appears

- the command path in `hooks.json` is broken
- the script writes to a path whose parent directory does not exist
- the script exits early on JSON parsing or filesystem error

### Log file appears but contents are poor

- the script is not preserving the payload fields the user cares about
- the script is not prefixing the prompt with the Git `user.name`
- timestamps are missing timezone information
- the script does not distinguish missing keys from empty strings
- the repository Git identity is unset, so the prefix falls back to an unknown placeholder

### Structure looks correct but nothing fires

- the repository may not be opened in trusted mode
- tell the user to reopen the repository as trusted because files inside the repository cannot toggle Codex trust state themselves

## Recommended Minimal Output Fields

- timestamp
- session_id
- turn_id
- cwd
- model
- hook_event_name
- git_user_name
- prompt_prefix
- prompt_raw
- prompt
