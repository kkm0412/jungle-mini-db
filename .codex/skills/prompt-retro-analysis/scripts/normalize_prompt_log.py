#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import re
import sys
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo


KST = ZoneInfo("Asia/Seoul")
PREFIX_RE = re.compile(r"^\[(.+?)\]\s*(.*)$", re.DOTALL)
FILE_REF_RE = re.compile(r"(?:[A-Za-z0-9_.-]+/)+[A-Za-z0-9_.-]+")
CMD_RE = re.compile(r"\b(?:git|cmake|make|pytest|python3?|npm|node|cargo)\b")
REPAIR_RE = re.compile(r"(안 돼|안돼|에러|오류|실패|failed|error|broken)", re.IGNORECASE)
CONTEXT_RE = re.compile(r"(이거|그거|이대로|좋아|지금은|했음|봐줘|고쳐줘|다시 봐줘|그 문제)", re.IGNORECASE)


def parse_timestamp(value: str) -> datetime:
    return datetime.fromisoformat(value)


def derive_prefix_and_raw(payload: dict[str, object]) -> tuple[str, str, str]:
    prompt = str(payload.get("prompt", "") or "")
    prompt_raw = payload.get("prompt_raw")
    prompt_prefix = payload.get("prompt_prefix") or payload.get("git_user_name") or ""

    if prompt_raw is not None:
        return str(prompt_prefix), str(prompt_raw), prompt

    match = PREFIX_RE.match(prompt)
    if match:
        return match.group(1), match.group(2), prompt

    return str(prompt_prefix), prompt, prompt


def has_context_dependency(prompt_raw: str, has_file_reference: bool, is_multiline: bool) -> bool:
    if has_file_reference or is_multiline:
        return False
    if len(prompt_raw.strip()) <= 20:
        return True
    return bool(CONTEXT_RE.search(prompt_raw))


def iter_rows(input_path: Path):
    with input_path.open("r", encoding="utf-8") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError as exc:
                raise SystemExit(f"Invalid JSON at line {line_number}: {exc}")
            yield payload


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: normalize_prompt_log.py <input_jsonl> <output_csv>", file=sys.stderr)
        return 1

    input_path = Path(sys.argv[1]).resolve()
    output_path = Path(sys.argv[2]).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "prompt_prefix",
        "prompt_raw",
        "timestamp_utc",
        "timestamp_kst",
        "kst_date",
        "kst_hour",
        "session_id",
        "turn_id",
        "model",
        "hook_event_name",
        "prompt",
        "prompt_char_count",
        "prompt_line_count",
        "is_multiline",
        "contains_question",
        "has_file_reference",
        "has_command_reference",
        "repair_signal",
        "context_dependency_signal",
    ]

    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        for payload in iter_rows(input_path):
            prompt_prefix, prompt_raw, prompt = derive_prefix_and_raw(payload)
            timestamp_utc = parse_timestamp(payload["timestamp"])
            timestamp_kst = timestamp_utc.astimezone(KST)
            is_multiline = "\n" in prompt_raw
            has_file_reference = bool(FILE_REF_RE.search(prompt_raw))
            writer.writerow(
                {
                    "prompt_prefix": prompt_prefix,
                    "prompt_raw": prompt_raw,
                    "timestamp_utc": timestamp_utc.isoformat(),
                    "timestamp_kst": timestamp_kst.isoformat(),
                    "kst_date": timestamp_kst.date().isoformat(),
                    "kst_hour": timestamp_kst.hour,
                    "session_id": payload.get("session_id", ""),
                    "turn_id": payload.get("turn_id", ""),
                    "model": payload.get("model", ""),
                    "hook_event_name": payload.get("hook_event_name", ""),
                    "prompt": prompt,
                    "prompt_char_count": len(prompt_raw),
                    "prompt_line_count": prompt_raw.count("\n") + 1 if prompt_raw else 0,
                    "is_multiline": str(is_multiline),
                    "contains_question": str("?" in prompt_raw or "왜" in prompt_raw or "어떻게" in prompt_raw),
                    "has_file_reference": str(has_file_reference),
                    "has_command_reference": str(bool(CMD_RE.search(prompt_raw))),
                    "repair_signal": str(bool(REPAIR_RE.search(prompt_raw))),
                    "context_dependency_signal": str(
                        has_context_dependency(prompt_raw, has_file_reference, is_multiline)
                    ),
                }
            )

    print(f"Wrote normalized CSV: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
