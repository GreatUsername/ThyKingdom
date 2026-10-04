#!/usr/bin/env python3
"""Recompute the summary table in 03 Mechanics register.md from its own rows.

The summary then cannot disagree with the register: every count is derived by
parsing the numbered rows, not typed by hand. Statuses present in the rows but
missing from the summary are reported so the table can be extended.

Paths are resolved from this file's location, so the script works on any
machine and from any working directory.

Usage:
    python tools/update_register_summary.py [--check] [register.md]
"""
from __future__ import annotations

import io
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTER = os.path.join(REPO_ROOT, "Notes", "Thy Kingdom", "Codex", "03 Mechanics register.md")

# The summary script rewrites cell values, which leaves the summary table's
# padding stale. Re-pad afterwards so the two scripts can be run in either
# order. pad_tables.py sits next to this file.
sys.dont_write_bytecode = True  # importing it must not litter __pycache__
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import pad_tables
except Exception:  # pragma: no cover - padding is a convenience, not a requirement
    pad_tables = None

ROW = re.compile(r"^\|\s*(\d+)\s*\|")
SUMMARY_ROW = re.compile(r"^\|\s*`([a-z0-9]+)`\s*\|")
UNSTATUSED = re.compile(r"^\|\s*без статуса")
TOTAL_ROW = re.compile(r"^\|\s*\*\*всего\*\*")

ORDER = ["m1", "m2", "demo", "next", "maybe", "parked", "cut"]


def main(argv: list) -> int:
    check_only = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    path = argv[0] if argv else REGISTER
    if not os.path.isfile(path):
        sys.exit(f"no such file: {path}")

    text = io.open(path, encoding="utf-8").read()
    lines = text.split("\n")

    counts: dict = {}
    total = 0
    highest = 0
    for ln in lines:
        m = ROW.match(ln)
        if not m:
            continue
        cells = [c.strip() for c in ln.strip()[1:-1].split("|")]
        if len(cells) < 4:
            continue
        total += 1
        highest = max(highest, int(m.group(1)))
        key = cells[3].strip("`")
        counts[key] = counts.get(key, 0) + 1

    unknown = sorted(set(counts) - set(ORDER) - {"—"})

    out: list = []
    for ln in lines:
        m = SUMMARY_ROW.match(ln)
        if m and m.group(1) in counts:
            cells = ln.strip()[1:-1].split("|")
            cells[-1] = f" {counts[m.group(1)]} "
            out.append("|" + "|".join(cells) + "|")
            continue
        if UNSTATUSED.match(ln):
            cells = ln.strip()[1:-1].split("|")
            cells[-1] = f" {counts.get('—', 0)} "
            out.append("|" + "|".join(cells) + "|")
            continue
        if TOTAL_ROW.match(ln):
            cells = ln.strip()[1:-1].split("|")
            cells[-1] = f" **{total}** "
            out.append("|" + "|".join(cells) + "|")
            continue
        out.append(ln)

    after = "\n".join(out)
    changed = after != text
    if changed and not check_only:
        io.open(path, "w", encoding="utf-8", newline="").write(after)
        print(f"summary updated in {path}")
        if pad_tables is not None and pad_tables.process(path, False):
            print(f"summary table re-padded in {path}")
    elif changed:
        print(f"summary would change in {path} (--check)")
    if check_only and pad_tables is not None and pad_tables.process(path, True):
        print(f"table padding would also change in {path} (--check)")

    for k in ORDER:
        print(f"{k:<8} {counts.get(k, 0)}")
    print(f"{'—':<8} {counts.get('—', 0)}")
    print(f"total    {total}")
    print(f"highest id {highest}")
    if unknown:
        print(f"WARNING statuses missing from summary table: {unknown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
