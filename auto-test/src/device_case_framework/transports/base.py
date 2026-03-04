from __future__ import annotations

from abc import ABC, abstractmethod
from pathlib import Path
from shlex import quote
from typing import Sequence

from device_case_framework.result import CommandResult


def interpreter_for_suffix(suffix: str) -> str:
    normalized = suffix.lower()
    if normalized == ".py":
        return "python3"
    if normalized in {".sh", ".bash"}:
        return "bash"
    return ""


def build_script_command(remote_path: str, suffix: str, args: Sequence[str]) -> str:
    interpreter = interpreter_for_suffix(suffix)
    joined_args = " ".join(quote(str(arg)) for arg in args)
    target = quote(remote_path)
    if interpreter:
        base = f"{interpreter} {target}"
    else:
        base = target
    if joined_args:
        return f"{base} {joined_args}"
    return base


class BaseTransport(ABC):
    name = "base"

    @abstractmethod
    def connect(self) -> None:
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        raise NotImplementedError

    @abstractmethod
    def run(self, command: str, timeout: float | None = None) -> CommandResult:
        raise NotImplementedError

    @abstractmethod
    def run_script(
        self,
        script_path: str | Path,
        args: Sequence[str] = (),
        timeout: float | None = None,
        cleanup: bool = True,
    ) -> CommandResult:
        raise NotImplementedError
