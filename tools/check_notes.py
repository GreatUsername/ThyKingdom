"""Структурные проверки заметок Кодекса.

Ловят ошибки, которые глазами не видны и которые уже случались: сдвоенный
заголовок и повторное определение термина. Проверяется:

- сдвоенные заголовки вида `## Что-то## Что-то`;
- повтор первого столбца в таблицах словаря;
- повтор номеров механик в реестре;
- повтор номеров решений и расхождение между строкой статуса и разделом.

Запуск:  python tools/check_notes.py
Код возврата 1, если что-то не так.
"""

from __future__ import annotations

import io
import re
import sys
from collections import Counter
from pathlib import Path

CODEX = Path(__file__).resolve().parent.parent / "Notes" / "Thy Kingdom" / "Codex"

errors: list[str] = []


def read(name: str) -> str:
    return io.open(CODEX / name, encoding="utf-8").read()


def tables(text: str):
    """Каждая markdown-таблица как список строк, где строка — список ячеек."""
    block: list[list[str]] = []
    for line in text.splitlines():
        s = line.strip()
        if len(s) > 1 and s.startswith("|") and s.endswith("|"):
            block.append([c.strip() for c in s[1:-1].split("|")])
            continue
        if block:
            yield block
            block = []
    if block:
        yield block


def is_separator(cells: list[str]) -> bool:
    return bool(cells) and all(re.fullmatch(r":?-{2,}:?", c) for c in cells)


def data_rows(text: str):
    """Строки таблиц, идущие после строки-разделителя, без шапок."""
    for block in tables(text):
        sep = next((i for i, c in enumerate(block) if is_separator(c)), None)
        if sep is None:
            continue
        for cells in block[sep + 1 :]:
            if not is_separator(cells):
                yield cells


# --- 1. сдвоенные заголовки ------------------------------------------------
for path in sorted(CODEX.glob("*.md")):
    for n, line in enumerate(io.open(path, encoding="utf-8").read().splitlines(), 1):
        if re.match(r"^#+ .*#+ ", line):
            errors.append(f"{path.name}:{n}: сдвоенный заголовок — {line.strip()}")

# --- 2. повтор первого столбца в словаре -----------------------------------
terms = [r[0].lower() for r in data_rows(read("04 Glossary.md")) if r and r[0]]
for term, count in Counter(terms).items():
    if count > 1:
        errors.append(f"04 Glossary.md: термин «{term}» определён {count} раза")

# --- 3. повтор номеров механик ---------------------------------------------
register = read("03 Mechanics register.md")
ids = [int(m.group(1)) for m in re.finditer(r"^\|\s*(\d+)\s*\|", register, re.M)]
for mech, count in Counter(ids).items():
    if count > 1:
        errors.append(f"03 Mechanics register.md: механика {mech} встречается {count} раза")
if ids and sorted(ids) != list(range(1, max(ids) + 1)):
    missing = sorted(set(range(1, max(ids) + 1)) - set(ids))
    if missing:
        errors.append(f"03 Mechanics register.md: пропущены номера {missing}")

# --- 4. решения: повторы и согласованность строки статуса с разделом -------
decisions = read("05 Open decisions.md")
rows = [int(m.group(1)) for m in re.finditer(r"^\|\s*ОД-(\d+)\s*\|", decisions, re.M)]
sections = [int(m.group(1)) for m in re.finditer(r"^## ОД-(\d+)\.", decisions, re.M)]
for oid, count in Counter(rows).items():
    if count > 1:
        errors.append(f"05 Open decisions.md: ОД-{oid} в таблице статусов {count} раза")
for oid, count in Counter(sections).items():
    if count > 1:
        errors.append(f"05 Open decisions.md: раздел ОД-{oid} встречается {count} раза")
for oid in sorted(set(rows) ^ set(sections)):
    where = "нет раздела" if oid in rows else "нет строки статуса"
    errors.append(f"05 Open decisions.md: ОД-{oid} — {where}")

# --- итог -------------------------------------------------------------------
if errors:
    print(f"найдено проблем: {len(errors)}")
    for e in errors:
        print(f"  - {e}")
    sys.exit(1)

print("структура в порядке")
