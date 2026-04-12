#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path


CONFIG_TEXT = """[features]
codex_hooks = true
"""

HOOKS_JSON = {
    "hooks": {
        "UserPromptSubmit": [
            {
                "hooks": [
                    {
                        "type": "command",
                        "command": '/usr/bin/python3 "$(git rev-parse --show-toplevel)/.codex/hooks/user_prompt_submit_log.py"',
                        "statusMessage": "Logging prompt",
                    }
                ]
            }
        ]
    }
}

HOOK_SCRIPT = """#!/usr/bin/env python3
\"\"\"Append every UserPromptSubmit payload to a project-local JSONL log.\"\"\"

from __future__ import annotations

import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SAFE_FILENAME_RE = re.compile(r"[^\\w.-]+", re.UNICODE)


def get_git_user_name() -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "config", "user.name"],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError:
        return "unknown-user"

    user_name = result.stdout.strip()
    return user_name or "unknown-user"


def build_prefixed_prompt(prefix: str, prompt: str) -> str:
    return f"[{prefix}] {prompt}" if prompt else f"[{prefix}]"


def build_log_path(git_user_name: str) -> Path:
    safe_user_name = SAFE_FILENAME_RE.sub("-", git_user_name).strip("-._")
    safe_user_name = safe_user_name or "unknown-user"
    return REPO_ROOT / "logs" / f"{safe_user_name}-prompt-log.jsonl"


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError as exc:
        print(f"UserPromptSubmit hook received invalid JSON: {exc}", file=sys.stderr)
        return 0

    git_user_name = get_git_user_name()
    raw_prompt = payload.get("prompt", "")
    log_entry = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "session_id": payload.get("session_id"),
        "turn_id": payload.get("turn_id"),
        "cwd": payload.get("cwd"),
        "model": payload.get("model"),
        "hook_event_name": payload.get("hook_event_name"),
        "git_user_name": git_user_name,
        "prompt_prefix": git_user_name,
        "prompt_raw": raw_prompt,
        "prompt": build_prefixed_prompt(git_user_name, raw_prompt),
    }
    log_path = build_log_path(git_user_name)

    try:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        with log_path.open("a", encoding="utf-8") as log_file:
            log_file.write(json.dumps(log_entry, ensure_ascii=False))
            log_file.write("\\n")
    except OSError as exc:
        print(f"Failed to append prompt log: {exc}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
"""


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: install_or_repair_hook.py <repo_path>", file=sys.stderr)
        return 1

    repo = Path(sys.argv[1]).resolve()
    codex_dir = repo / ".codex"
    hooks_dir = codex_dir / "hooks"
    config_path = codex_dir / "config.toml"
    hooks_path = codex_dir / "hooks.json"
    script_path = hooks_dir / "user_prompt_submit_log.py"

    hooks_dir.mkdir(parents=True, exist_ok=True)
    config_path.write_text(CONFIG_TEXT, encoding="utf-8")
    hooks_path.write_text(json.dumps(HOOKS_JSON, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    script_path.write_text(HOOK_SCRIPT, encoding="utf-8")
    script_path.chmod(0o755)

    print(f"Updated {config_path}")
    print(f"Updated {hooks_path}")
    print(f"Updated {script_path}")
    print("If Codex still does not load hooks, reopen the repository in trusted mode.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
