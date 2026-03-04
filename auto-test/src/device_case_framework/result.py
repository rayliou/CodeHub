from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class CommandResult:
    command: str
    exit_code: int
    stdout: str
    stderr: str = ""

    def failure_message(self) -> str:
        return (
            f"command: {self.command}\n"
            f"exit_code: {self.exit_code}\n"
            f"stdout:\n{self.stdout}\n"
            f"stderr:\n{self.stderr}"
        )
