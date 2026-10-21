#!/usr/bin/env python3
"""Прогоняет все сценарии и сверяет хеши с эталонными.

Зачем: эталонный хеш проверяет только тот, кто не забыл запустить прогонщик руками.
Скрипт делает это за него и возвращает ненулевой код, если хоть один сценарий
разошёлся, поэтому его можно поставить в общий прогон и в сборочный шаг.

Сценарий без эталона (`hash = 0`) считается провалом, а не пропуском: сценарий,
который ничего не сверяет, выглядит как зелёная проверка, но ею не является.

Запуск:

    python tools/run_scenarios.py
    python tools/run_scenarios.py --runner Project/build-tools/runner/runner.exe
    python tools/run_scenarios.py --scenarios Project/scenarios

Коды возврата: 0 — все сошлись, 1 — есть расхождения или ошибки, 2 — не найден
прогонщик или каталог со сценариями.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Путь по умолчанию — обычная сборка проекта. Прогонщик без расширения тоже
# принимается: так выглядит сборка не под Windows.
DEFAULT_RUNNER = REPO / "Project" / "build" / "runner" / "runner.exe"
DEFAULT_SCENARIOS = REPO / "Project" / "scenarios"

BUILD_HINT = (
    'cmake -S Project -B Project/build -G "MinGW Makefiles"\n'
    "cmake --build Project/build"
)

STATUS_MATCH = "сошёлся"
STATUS_MISMATCH = "РАСХОЖДЕНИЕ"
STATUS_NO_GOLDEN = "НЕТ ЭТАЛОНА"
STATUS_ERROR = "ОШИБКА"


def die(message: str, code: int = 2, hint: bool = False) -> None:
    """Печатает ошибку и выходит. hint добавляет команду сборки прогонщика."""
    print(f"ошибка: {message}", file=sys.stderr)
    if hint:
        print("\nСоберите прогонщик:\n\n" + BUILD_HINT, file=sys.stderr)
    raise SystemExit(code)


def locate_runner(given: str | None) -> Path:
    """Возвращает путь к прогонщику или выходит с подсказкой, как его собрать."""
    if given:
        path = Path(given).expanduser()
        if not path.is_file():
            die(f"прогонщик не найден: {path}", hint=True)
        return path

    for candidate in (DEFAULT_RUNNER, DEFAULT_RUNNER.with_suffix("")):
        if candidate.is_file():
            return candidate

    die(
        f"прогонщик не найден ни по одному пути, включая {DEFAULT_RUNNER}. "
        "Укажите его через --runner.",
        hint=True,
    )


def locate_scenarios(given: str | None) -> tuple[Path, list[Path]]:
    """Возвращает каталог и отсортированный список сценариев."""
    directory = Path(given).expanduser() if given else DEFAULT_SCENARIOS
    if not directory.is_dir():
        die(f"каталог со сценариями не найден: {directory}")

    files = sorted(directory.glob("*.scn"))
    if not files:
        die(f"в каталоге нет ни одного сценария: {directory}")
    return directory, files


def parse_json_line(stdout: str) -> dict | None:
    """Ищет в выводе строку JSON. Прогонщик печатает ровно одну, но берём последнюю."""
    for line in reversed(stdout.splitlines()):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(payload, dict):
            return payload
    return None


def run_scenario(runner: Path, scenario: Path) -> dict:
    """Прогоняет один сценарий и возвращает запись результата."""
    started = time.perf_counter()
    try:
        proc = subprocess.run(
            [str(runner), str(scenario), "--json"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        return {"name": scenario.name, "status": STATUS_ERROR, "detail": f"не запустился: {exc}"}

    elapsed_ms = (time.perf_counter() - started) * 1000.0
    payload = parse_json_line(proc.stdout or "")
    if payload is None:
        # Прогонщик объясняет отказ в stderr и не печатает JSON: битый сценарий,
        # неизвестный ключ, не найденный файл.
        lines = [line for line in (proc.stderr or "").splitlines() if line.strip()]
        detail = lines[0].strip() if lines else f"прогонщик вернул код {proc.returncode} без JSON"
        return {"name": scenario.name, "status": STATUS_ERROR, "detail": detail, "ms": elapsed_ms}

    got = payload.get("hash")
    expect = payload.get("expect")
    record = {"name": scenario.name, "hash": got, "expect": expect, "ms": elapsed_ms}

    if expect == 0:
        record["status"] = STATUS_NO_GOLDEN
        record["detail"] = "в сценарии hash = 0: сверять не с чем"
    elif payload.get("match"):
        record["status"] = STATUS_MATCH
    else:
        record["status"] = STATUS_MISMATCH
        record["detail"] = f"ожидался {expect}, получен {got}"
    return record


def format_hash(value: object) -> str:
    if isinstance(value, int) and value >= 0:
        return f"{value:016x}"
    return "-"


def print_table(results: list[dict]) -> None:
    print(f"  {'сценарий':<24}{'хеш':<20}{'мс':>9}  статус")
    for record in results:
        ms = record.get("ms", 0.0)
        print(
            f"  {record['name']:<24}{format_hash(record.get('hash')):<20}"
            f"{ms:>9.1f}  {record['status']}"
        )


def main(argv: list[str] | None = None) -> int:
    # В консоли с однобайтовой кодировкой часть символов не представима, а падать
    # из-за оформления отчёта инструмент не должен.
    try:
        sys.stdout.reconfigure(errors="replace")
        sys.stderr.reconfigure(errors="replace")
    except (AttributeError, ValueError):
        pass

    parser = argparse.ArgumentParser(
        description="Прогоняет все сценарии и сверяет хеши с эталонными.",
        epilog="Коды возврата: 0 — все сошлись, 1 — есть расхождения, 2 — нет прогонщика.",
    )
    parser.add_argument(
        "--runner",
        help="путь к прогонщику; по умолчанию Project/build/runner/runner.exe",
    )
    parser.add_argument(
        "--scenarios",
        help="каталог со сценариями; по умолчанию Project/scenarios",
    )
    args = parser.parse_args(argv)

    runner = locate_runner(args.runner)
    directory, files = locate_scenarios(args.scenarios)

    print(f"прогонщик: {runner}")
    print(f"сценарии:  {directory} ({len(files)} шт.)")
    print()

    results = [run_scenario(runner, scenario) for scenario in files]
    print_table(results)
    print()

    failed = [record for record in results if record["status"] != STATUS_MATCH]
    if not failed:
        print(f"сошлись все сценарии: {len(results)}")
        return 0

    print(f"не сошлось: {len(failed)} из {len(results)}", file=sys.stderr)
    for record in failed:
        print(f"  {record['name']}: {record['status']} — {record.get('detail', '')}", file=sys.stderr)
    print(
        "\nЭталон меняют осознанно: сначала объясняют, что изменилось в мире, "
        "потом вписывают новый хеш в [expect].",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
