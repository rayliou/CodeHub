from __future__ import annotations

import base64
import re
from dataclasses import dataclass, field
from pathlib import Path
from shlex import quote
from typing import Any, Sequence
from uuid import uuid4

from device_case_framework.result import CommandResult
from device_case_framework.transports.base import BaseTransport, build_script_command


@dataclass(slots=True)
class SerialTransport(BaseTransport):
    port: str
    baudrate: int = 115200
    username: str | None = None
    password: str | None = None
    prompt_regex: str = r"[#>$]\s*$"
    timeout: float = 20
    login_timeout: float = 30
    encoding: str = "utf-8"
    remote_tmp_dir: str = "/tmp/pytest-cases"
    _serial: Any | None = field(init=False, default=None, repr=False)
    _session: Any | None = field(init=False, default=None, repr=False)

    name = "serial"

    def connect(self) -> None:
        try:
            import serial
            from pexpect import fdpexpect
        except ModuleNotFoundError as exc:
            raise RuntimeError(
                "pyserial and pexpect are required for serial transport."
            ) from exc

        self._serial = serial.Serial(self.port, self.baudrate, timeout=0.2)
        self._session = fdpexpect.fdspawn(
            self._serial.fileno(),
            timeout=self.timeout,
            encoding=self.encoding,
        )
        self._session.delaybeforesend = 0.02
        self._login()

    def close(self) -> None:
        if self._serial is not None:
            self._serial.close()
            self._serial = None
        self._session = None

    def run(self, command: str, timeout: float | None = None) -> CommandResult:
        session = self._require_session()
        sentinel = f"__PYTEST_EXIT_{uuid4().hex}__"
        wrapped = f"{command}; rc=$?; echo {sentinel}$rc"

        session.sendline(wrapped)
        session.expect(self.prompt_regex, timeout=timeout or self.timeout)
        raw_output = session.before or ""

        match = re.search(rf"{re.escape(sentinel)}(\d+)", raw_output)
        if not match:
            raise RuntimeError(
                f"Could not parse exit code for serial command: {command}\n"
                f"raw output:\n{raw_output}"
            )

        exit_code = int(match.group(1))
        stdout = _clean_serial_output(raw_output, wrapped, sentinel)
        return CommandResult(command=command, exit_code=exit_code, stdout=stdout, stderr="")

    def run_script(
        self,
        script_path: str | Path,
        args: Sequence[str] = (),
        timeout: float | None = None,
        cleanup: bool = True,
    ) -> CommandResult:
        local_path = Path(script_path)
        if not local_path.is_file():
            raise FileNotFoundError(f"Script not found: {local_path}")

        encoded = base64.b64encode(local_path.read_bytes()).decode("ascii")
        remote_path = (
            f"{self.remote_tmp_dir.rstrip('/')}/"
            f"{local_path.stem}_{uuid4().hex}{local_path.suffix}"
        )

        self.run(f"mkdir -p {quote(self.remote_tmp_dir)}")
        self.run(f"echo {quote(encoded)} | base64 -d > {quote(remote_path)}")
        self.run(f"chmod +x {quote(remote_path)}")

        command = build_script_command(remote_path, local_path.suffix, args)
        try:
            return self.run(command, timeout=timeout)
        finally:
            if cleanup:
                self.run(f"rm -f {quote(remote_path)}")

    def _login(self) -> None:
        session = self._require_session()
        session.sendline("")
        idx = session.expect(
            [self.prompt_regex, r"[Ll]ogin:\s*$", r"[Pp]assword:\s*$"],
            timeout=self.login_timeout,
        )

        if idx == 0:
            return

        if idx == 1:
            if not self.username:
                raise RuntimeError("Serial username is required when login prompt appears.")
            session.sendline(self.username)
            next_idx = session.expect(
                [self.prompt_regex, r"[Pp]assword:\s*$"],
                timeout=self.login_timeout,
            )
            if next_idx == 0:
                return
            if self.password is None:
                raise RuntimeError(
                    "Serial password is required when password prompt appears."
                )
            session.sendline(self.password)
            session.expect(self.prompt_regex, timeout=self.login_timeout)
            return

        if self.password is None:
            raise RuntimeError("Serial password is required when password prompt appears.")
        session.sendline(self.password)
        session.expect(self.prompt_regex, timeout=self.login_timeout)

    def _require_session(self) -> Any:
        if self._session is None:
            raise RuntimeError("Serial session is not connected.")
        return self._session


def _clean_serial_output(raw_output: str, wrapped_command: str, sentinel: str) -> str:
    lines = raw_output.replace("\r", "").splitlines()

    if lines and lines[0].strip() == wrapped_command.strip():
        lines = lines[1:]

    cleaned: list[str] = []
    for line in lines:
        if sentinel in line:
            continue
        cleaned.append(line)
    return "\n".join(cleaned).strip()
