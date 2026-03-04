from __future__ import annotations

import re
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
CASE_ROOT = ROOT / "cases"
CASE_NAME_PATTERN = re.compile(r"^(\d+)_")


def _sort_key(path: Path) -> tuple[int, str]:
    match = CASE_NAME_PATTERN.match(path.name)
    order = int(match.group(1)) if match else 10**9
    return (order, path.as_posix())


def _detect_transport(path: Path) -> str | None:
    parts = set(path.relative_to(CASE_ROOT).parts)
    if "serial" in parts:
        return "serial"
    if "ssh" in parts:
        return "ssh"
    return None


def discover_cases():
    if not CASE_ROOT.exists():
        return [
            pytest.param(
                None,
                marks=pytest.mark.skip(reason=f"Missing case directory: {CASE_ROOT}"),
                id="cases-missing",
            )
        ]

    scripts = [
        path
        for path in CASE_ROOT.rglob("*")
        if path.is_file() and CASE_NAME_PATTERN.match(path.name)
    ]

    if not scripts:
        return [
            pytest.param(
                None,
                marks=pytest.mark.skip(reason=f"No numeric-prefixed script cases in: {CASE_ROOT}"),
                id="cases-empty",
            )
        ]

    params: list[object] = []
    for path in sorted(scripts, key=_sort_key):
        transport = _detect_transport(path)
        marks = [pytest.mark.transport(transport)] if transport else []
        case_id = path.relative_to(CASE_ROOT).as_posix()
        params.append(pytest.param(path, marks=marks, id=case_id))
    return params


CASES = discover_cases()

@pytest.mark.parametrize("script_path", CASES)
def test_script_cases(device, script_path):
    if script_path is None:
        pytest.skip("No script cases.")

    result = device.run_script(script_path)
    assert result.exit_code == 0, result.failure_message()
