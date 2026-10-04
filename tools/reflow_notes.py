#!/usr/bin/env python3
"""Join hard-wrapped lines so each markdown block is a single line.

Enforces rule 7 of the Codex: a paragraph is one line, with no break inside a
sentence. Blocks deliberately kept on their own lines: frontmatter, fenced
code, tables, headings, horizontal rules, blockquotes, list items, and bold
field labels such as ``**Решение:** —`` so a decision's fields never merge.

The rewrite is whitespace-only and all-or-nothing: every target file's
whitespace-normalised text must be unchanged, otherwise nothing is written.
Blockquote markers are stripped before comparison, because joining a
blockquote's continuation lines legitimately removes the per-line '>'.

Paths are resolved from this file's location, so the script works on any
machine and from any working directory.

Usage:
    python tools/reflow_notes.py [--check] [path ...]
    python tools/reflow_notes.py              # defaults to the Codex directory
"""
from __future__ import annotations

import glob
import io
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODEX_DIR = os.path.join(REPO_ROOT, "Notes", "Thy Kingdom", "Codex")

LIST = re.compile(r"^(\s*)([-*+]|\d+\.)\s+(.*)$")
FIELD = re.compile(r"^\*\*[^*]{1,40}[:.]\*\*")
HR = re.compile(r"^(-{3,}|\*{3,}|_{3,})$")
BQ = re.compile(r"(?m)^\s*>+\s?")


def norm(s: str) -> str:
    """Whitespace- and blockquote-marker-insensitive comparison key."""
    return re.sub(r"\s+", " ", BQ.sub("", s)).strip()


def reflow(text: str) -> str:
    lines = text.split("\n")
    out: list = []
    buf = None

    def flush() -> None:
        nonlocal buf
        if buf is not None:
            out.append(buf)
            buf = None

    i, n = 0, len(lines)
    in_fm = False
    in_code = False
    while i < n:
        line = lines[i]
        s = line.strip()

        if i == 0 and s == "---":
            in_fm = True
            out.append(line)
            i += 1
            continue
        if in_fm:
            out.append(line)
            if s == "---":
                in_fm = False
            i += 1
            continue
        if s.startswith("```"):
            flush()
            out.append(line)
            in_code = not in_code
            i += 1
            continue
        if in_code:
            out.append(line)
            i += 1
            continue
        if s == "":
            flush()
            out.append("")
            i += 1
            continue
        if s.startswith("|"):
            flush()
            out.append(line)
            i += 1
            continue
        if s.startswith("#") or HR.match(s):
            flush()
            out.append(s)
            i += 1
            continue
        if s.startswith(">"):
            flush()
            segs = []
            while i < n and lines[i].strip().startswith(">"):
                segs.append(lines[i].strip()[1:].strip())
                i += 1
            para: list = []
            for p in segs:
                if p == "":
                    if para:
                        out.append("> " + " ".join(para))
                        para = []
                    out.append(">")
                else:
                    para.append(p)
            if para:
                out.append("> " + " ".join(para))
            continue

        m = LIST.match(line)
        if m:
            flush()
            buf = f"{m.group(1)}{m.group(2)} {m.group(3).strip()}"
            i += 1
            continue
        if FIELD.match(s):
            flush()
            buf = s
            i += 1
            continue
        buf = s if buf is None else buf.rstrip() + " " + s
        i += 1

    flush()
    joined = "\n".join(out)
    result = []
    in_code = False
    for ln in joined.split("\n"):
        if ln.strip().startswith("```"):
            in_code = not in_code
            result.append(ln)
            continue
        if in_code or ln.lstrip().startswith("|") or ln.lstrip().startswith(">"):
            result.append(ln)
            continue
        result.append(re.sub(r"(?<=\S) {2,}(?=\S)", " ", ln).rstrip())
    return "\n".join(result)


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

    plan = []
    failed = False
    for path in targets:
        before = io.open(path, encoding="utf-8").read()
        after = reflow(before)
        nb, na = norm(before), norm(after)
        if nb != na:
            failed = True
            k = 0
            while k < min(len(nb), len(na)) and nb[k] == na[k]:
                k += 1
            print(f"CONTENT WOULD CHANGE: {path} at {k}")
            print(f"  before: {nb[max(0, k - 90): k + 90]!r}")
            print(f"  after : {na[max(0, k - 90): k + 90]!r}")
            continue
        if before != after:
            plan.append((path, after))

    if failed:
        print("refusing to write anything")
        return 1
    if check_only:
        print(f"would reflow {len(plan)} file(s); nothing written (--check)")
        return 0
    for path, after in plan:
        io.open(path, "w", encoding="utf-8", newline="").write(after)
        print(f"reflowed {path}")
    print(f"files rewritten: {len(plan)} (all content-identical)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
