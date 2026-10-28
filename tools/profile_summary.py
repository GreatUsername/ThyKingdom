#!/usr/bin/env python3
"""Сводка по трассе easy_profiler: сколько времени стоит каждая подсистема.

Зачем: GUI профайлера показывает таймлайн, но не даёт числа, которое можно сравнить
до и после правки. Скрипт читает выгрузку и печатает таблицу «шагов, всего, на шаг».

Каждый шаг подсистемы записан отдельным блоком с именем из System::name(), поэтому
таблица отвечает не «шаг стоит столько», а «вот сколько стоит каждая подсистема».

Запуск:

    python tools/profile_summary.py trace.json
    python tools/profile_summary.py trace.prof --converter <path>/profiler_converter.exe

Первый вид требует готового JSON от profiler_converter; второй конвертирует сам.
"""

from __future__ import annotations

import argparse
import collections
import json
import subprocess
import sys
from pathlib import Path


def load(path: Path, converter: str | None) -> dict:
    if path.suffix.lower() == ".json":
        return json.loads(path.read_text(encoding="utf-8", errors="replace"))

    if not converter:
        raise SystemExit(
            "ошибка: нужен путь к profiler_converter.exe (--converter), "
            "либо готовый .json вместо .prof"
        )
    # The JSON is written next to the trace and removed afterwards. A temporary
    # directory is the obvious choice and the wrong one: on Windows its cleanup
    # changes permissions on the directory, which fails under a restricted sandbox.
    out = path.with_suffix(".json")
    try:
        subprocess.run([converter, str(path), str(out)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        return json.loads(out.read_text(encoding="utf-8", errors="replace"))
    finally:
        out.unlink(missing_ok=True)


def collect(data: dict) -> tuple[collections.Counter, collections.Counter]:
    """Суммирует длительности по имени блока.

    Обход нарочно не зависит от формы дерева: блоки лежат в threads, но вложенность
    может меняться от версии к версии, поэтому ищем любой объект с парой start/stop.
    """
    totals: collections.Counter = collections.Counter()
    counts: collections.Counter = collections.Counter()

    def walk(node: object) -> None:
        if isinstance(node, dict):
            start, stop, name = node.get("start"), node.get("stop"), node.get("name")
            if isinstance(start, int) and isinstance(stop, int) and isinstance(name, str):
                totals[name] += stop - start
                counts[name] += 1
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(data.get("threads", []))
    return totals, counts


def main(argv: list[str] | None = None) -> int:
    try:
        sys.stdout.reconfigure(errors="replace")
    except (AttributeError, ValueError):
        pass

    parser = argparse.ArgumentParser(
        description="Сводка по трассе easy_profiler: время по подсистемам.")
    parser.add_argument("trace", help="файл .json или .prof")
    parser.add_argument("--converter", help="путь к profiler_converter для .prof")
    args = parser.parse_args(argv)

    path = Path(args.trace)
    if not path.is_file():
        raise SystemExit(f"ошибка: файл не найден: {path}")

    totals, counts = collect(load(path, args.converter))
    if not totals:
        print("в трассе нет блоков: профайлер не был включён во время прогона?")
        return 1

    grand = sum(totals.values())
    print(f"  {'подсистема':<16}{'шагов':>10}{'всего, мс':>13}{'мкс/шаг':>11}{'доля':>9}")
    for name, total_ns in totals.most_common():
        steps = counts[name]
        print(f"  {name:<16}{steps:>10}{total_ns / 1e6:>13.1f}"
              f"{total_ns / steps / 1e3:>11.3f}{100.0 * total_ns / grand:>8.1f}%")
    print(f"  {'(всего)':<16}{sum(counts.values()):>10}{grand / 1e6:>13.1f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
