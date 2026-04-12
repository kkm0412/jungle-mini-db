#!/usr/bin/env python3
from __future__ import annotations

import csv
import subprocess
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path


def parse_bool(value: str) -> bool:
    return value == "True"


def parse_dt(value: str) -> datetime:
    return datetime.fromisoformat(value)


def load_git_authors(repo_path: Path | None) -> set[str]:
    if repo_path is None:
        return set()

    try:
        result = subprocess.run(
            ["git", "-C", str(repo_path), "log", "--format=%an"],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError:
        return set()

    return {line.strip() for line in result.stdout.splitlines() if line.strip()}


def rate_context_management(prompt_count: int, context_dependency_count: int) -> str:
    if prompt_count == 0:
        return "unknown"
    ratio = context_dependency_count / prompt_count
    if ratio == 0:
        return "explicit"
    if ratio <= 0.25:
        return "mostly_explicit"
    if ratio <= 0.5:
        return "mixed"
    return "context_fragile"


def main() -> int:
    if len(sys.argv) not in {3, 4}:
        print(
            "Usage: build_prompt_retro_report.py <normalized_csv> <session_summary_csv> [repo_path]",
            file=sys.stderr,
        )
        return 1

    input_path = Path(sys.argv[1]).resolve()
    output_path = Path(sys.argv[2]).resolve()
    repo_path = Path(sys.argv[3]).resolve() if len(sys.argv) == 4 else None
    output_path.parent.mkdir(parents=True, exist_ok=True)
    git_authors = load_git_authors(repo_path)

    sessions: dict[str, list[dict[str, str]]] = defaultdict(list)
    with input_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            sessions[row["session_id"]].append(row)

    fieldnames = [
        "session_id",
        "prompt_count",
        "session_start_kst",
        "session_end_kst",
        "session_duration_minutes",
        "models_used",
        "prompt_prefixes",
        "matching_commit_authors",
        "nonmatching_prompt_prefixes",
        "author_match_rate",
        "repair_signal_count",
        "context_dependency_count",
        "context_management_rating",
        "avg_prompt_chars",
    ]

    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        for session_id, rows in sorted(sessions.items(), key=lambda item: item[0]):
            rows.sort(key=lambda row: parse_dt(row["timestamp_kst"]))
            start = parse_dt(rows[0]["timestamp_kst"])
            end = parse_dt(rows[-1]["timestamp_kst"])
            duration_minutes = (end - start).total_seconds() / 60
            models_used = "|".join(sorted({row["model"] for row in rows if row["model"]}))
            repair_signal_count = sum(1 for row in rows if parse_bool(row["repair_signal"]))
            context_dependency_count = sum(
                1 for row in rows if parse_bool(row["context_dependency_signal"])
            )
            prompt_prefixes = sorted({row["prompt_prefix"] for row in rows if row["prompt_prefix"]})
            matching_authors = sorted(prefix for prefix in prompt_prefixes if prefix in git_authors)
            nonmatching_prefixes = sorted(prefix for prefix in prompt_prefixes if prefix not in git_authors)
            prefixed_rows = [row for row in rows if row["prompt_prefix"]]
            matched_rows = [row for row in prefixed_rows if row["prompt_prefix"] in git_authors]
            author_match_rate = (
                f"{len(matched_rows) / len(prefixed_rows):.2f}" if prefixed_rows else ""
            )
            avg_prompt_chars = sum(int(row["prompt_char_count"]) for row in rows) / len(rows)
            writer.writerow(
                {
                    "session_id": session_id,
                    "prompt_count": len(rows),
                    "session_start_kst": start.isoformat(),
                    "session_end_kst": end.isoformat(),
                    "session_duration_minutes": f"{duration_minutes:.1f}",
                    "models_used": models_used,
                    "prompt_prefixes": "|".join(prompt_prefixes),
                    "matching_commit_authors": "|".join(matching_authors),
                    "nonmatching_prompt_prefixes": "|".join(nonmatching_prefixes),
                    "author_match_rate": author_match_rate,
                    "repair_signal_count": repair_signal_count,
                    "context_dependency_count": context_dependency_count,
                    "context_management_rating": rate_context_management(
                        len(rows), context_dependency_count
                    ),
                    "avg_prompt_chars": f"{avg_prompt_chars:.1f}",
                }
            )

    print(f"Wrote session summary CSV: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
