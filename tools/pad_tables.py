#!/usr/bin/env python3
"""Normalise markdown tables to the padded style Obsidian's table formatter produces.

Cells are padded to the widest cell in their column (minimum 3, so the
separator row stays valid markdown), and separator rows are rewritten as
dashes matching that width. Line endings are preserved.

Paths are resolved from this file's location, so the script works on any
machine and from any working directory.

Usage:
    python tools/pad_tables.py [--check] [path ...]
    python tools/pad_tables.py                 # defaults to the Codex directory
"""
from __future__ import annotations

import glob
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODEX_DIR = os.path.join(REPO_ROOT, "Notes", "Thy Kingdom", "Codex")

SEP = re.compile(r"^:?-+:?$")


def is_row(line: str) -> bool:
    s = line.strip()
    return len(s) > 1 and s.startswith("|") and s.endswith("|")


def split_row(line: str) -> list:
    inner = line.strip()[1:-1]
    return [c.strip() for c in re.split(r"(?<!\\)\|", inner)]


def is_separator(cells: list) -> bool:
    return bool(cells) and all(SEP.match(c) for c in cells if c != "") and any(
        SEP.match(c) for c in cells
    )


def pad_block(rows: list) -> list:
    parsed = [split_row(r) for r in rows]
    ncol = max(len(p) for p in parsed)
    for p in parsed:
        p.extend([""] * (ncol - len(p)))
    widths = [max(max(len(p[i]) for p in parsed), 3) for i in range(ncol)]

    out: list = []
    for p in parsed:
        if is_separator(p):
            out.append("| " + " | ".join("-" * w for w in widths) + " |")
        else:
            out.append("| " + " | ".join(c.ljust(widths[i]) for i, c in enumerate(p)) + " |")
    return out


def process(path: str, check_only: bool) -> bool:
    """Return True if the file's tables would change."""
    with open(path, "rb") as f:
        raw = f.read()
    newline = "\r\n" if b"\r\n" in raw else "\n"
    lines = raw.decode("utf-8").replace("\r\n", "\n").split("\n")

    result: list = []
    block: list = []
    for line in lines:
        if is_row(line):
            block.append(line)
            continue
        if block:
            result.extend(pad_block(block))
            block = []
        result.append(line)
    if block:
        result.extend(pad_block(block))

    after = newline.join(result).encode("utf-8")
    if after == raw:
        return False
    if not check_only:
        with open(path, "wb") as f:
            f.write(after)
    return True


def collect(args: list) -> list:
    targets: list = []
    for a in args:
        if os.path.isdir(a):
            targets.extend(sorted(glob.glob(os.path.join(a, "*.md"))))
        elif os.path.isfile(a):
            targets.append(a)
        else:
            sys.exit(f"no such path: {a}")
    return targets


def main(argv: list) -> int:
    check_only = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    targets = collect(argv) if argv else sorted(glob.glob(os.path.join(CODEX_DIR, "*.md")))
    if not targets:
        sys.exit("no markdown files found")

    changed = 0
    for path in targets:
        if process(path, check_only):
            changed += 1
            print(f"{'would pad' if check_only else 'padded'} {path}")
    print(f"files {'needing padding' if check_only else 'rewritten'}: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
