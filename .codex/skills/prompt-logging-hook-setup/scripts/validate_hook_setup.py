#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise SystemExit(f"Failed to read {path}: {exc}")


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: validate_hook_setup.py <repo_path>", file=sys.stderr)
        return 1

    repo = Path(sys.argv[1]).resolve()
    config_path = repo / ".codex" / "config.toml"
    hooks_path = repo / ".codex" / "hooks.json"
    log_paths = sorted((repo / "logs").glob("*-prompt-log.jsonl"))
    issues: list[str] = []
    notes: list[str] = []

    if not config_path.exists():
        issues.append(f"Missing config file: {config_path}")
    else:
        config_text = read_text(config_path)
        if "codex_hooks = true" not in config_text:
            issues.append("codex_hooks is not enabled in .codex/config.toml")
        else:
            notes.append("codex_hooks is enabled")

    hook_script_path: Path | None = None
    if not hooks_path.exists():
        issues.append(f"Missing hooks file: {hooks_path}")
    else:
        try:
            hooks_data = json.loads(read_text(hooks_path))
        except json.JSONDecodeError as exc:
            issues.append(f".codex/hooks.json is invalid JSON: {exc}")
            hooks_data = {}

        submit_hooks = hooks_data.get("hooks", {}).get("UserPromptSubmit", [])
        if not submit_hooks:
            issues.append("UserPromptSubmit hook is not configured")
        else:
            notes.append("UserPromptSubmit hook exists")
            for group in submit_hooks:
                for item in group.get("hooks", []):
                    command = item.get("command", "")
                    if ".codex/hooks/" in command and command.endswith(".py\""):
                        script_fragment = command.split(".codex/hooks/", 1)[1].rsplit('"', 1)[0]
                        hook_script_path = repo / ".codex" / "hooks" / script_fragment
                        break
                if hook_script_path:
                    break

    if hook_script_path is None:
        issues.append("Could not infer a Python hook script path from .codex/hooks.json")
    elif not hook_script_path.exists():
        issues.append(f"Referenced hook script does not exist: {hook_script_path}")
    else:
        script_text = read_text(hook_script_path)
        notes.append(f"Hook script found: {hook_script_path}")
        if "json.load(sys.stdin)" not in script_text:
            issues.append("Hook script does not appear to read payload JSON from stdin")
        if ".mkdir(" not in script_text:
            issues.append("Hook script does not appear to create the log directory")
        if "user.name" not in script_text:
            issues.append("Hook script does not appear to read git config user.name")
        if '"prompt_prefix"' not in script_text:
            issues.append("Hook script does not appear to write prompt_prefix")
        if '"prompt_raw"' not in script_text:
            issues.append("Hook script does not appear to preserve prompt_raw")
        if "-prompt-log.jsonl" in script_text and "build_log_path" in script_text:
            notes.append("Hook script appears to target per-user prompt logs")
        elif "user-prompts.jsonl" in script_text:
            issues.append("Hook script still targets the legacy shared user-prompts JSONL log")

    if log_paths:
        log_path = log_paths[0]
        try:
            first_line = log_path.read_text(encoding="utf-8").splitlines()[0]
            sample = json.loads(first_line)
        except (IndexError, json.JSONDecodeError, OSError):
            notes.append(f"Existing log file could not be sampled safely: {log_path}")
        else:
            if "prompt_prefix" in sample:
                notes.append(f"Existing log rows include prompt_prefix: {log_path}")
            else:
                notes.append(f"Existing log rows do not include prompt_prefix yet: {log_path}")

    print(f"Repository: {repo}")
    print("")
    if notes:
        print("Notes:")
        for note in notes:
            print(f"- {note}")
        print("")

    if issues:
        print("Issues:")
        for issue in issues:
            print(f"- {issue}")
        return 2

    print("No structural issues found.")
    print("If hooks still do not fire in Codex, verify that the repository is opened in trusted mode.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
