from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from shlex import join as shell_join
from shlex import quote
from typing import Any, Sequence
from uuid import uuid4

from device_case_framework.result import CommandResult
from device_case_framework.transports.base import BaseTransport, build_script_command


@dataclass(slots=True)
class SSHTransport(BaseTransport):
    host: str
    username: str
    password: str | None = None
    port: int = 22
    key_filename: str | None = None
    timeout: float = 15
    remote_tmp_dir: str = "/tmp/pytest-cases"
    _client: Any | None = field(init=False, default=None, repr=False)
    _synced_roots: list[tuple[Path, str]] = field(init=False, default_factory=list, repr=False)

    name = "ssh"

    def connect(self) -> None:
        try:
            import paramiko
        except ModuleNotFoundError as exc:
            raise RuntimeError("paramiko is required for SSH transport.") from exc

        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        client.connect(
            hostname=self.host,
            port=self.port,
            username=self.username,
            password=self.password,
            key_filename=self.key_filename,
            timeout=self.timeout,
        )
        self._client = client

    def close(self) -> None:
        if self._client is not None:
            self._client.close()
            self._client = None

    def run(self, command: str, timeout: float | None = None) -> CommandResult:
        client = self._require_client()
        _, stdout, stderr = client.exec_command(command, timeout=timeout)
        exit_code = stdout.channel.recv_exit_status()
        out_text = stdout.read().decode("utf-8", errors="replace")
        err_text = stderr.read().decode("utf-8", errors="replace")
        return CommandResult(
            command=command,
            exit_code=exit_code,
            stdout=out_text.strip(),
            stderr=err_text.strip(),
        )

    def sync_directory(
        self,
        local_dir: str | Path,
        remote_dir: str | None = None,
        delete: bool = True,
    ) -> None:
        local_root = Path(local_dir).expanduser().resolve()
        if not local_root.is_dir():
            raise FileNotFoundError(f"SSH sync local directory not found: {local_root}")

        remote_root = (remote_dir or self.remote_tmp_dir).rstrip("/")
        self.run(f"mkdir -p {quote(remote_root)}")

        rsync_cmd: list[str] = ["rsync", "-az"]
        if delete:
            rsync_cmd.append("--delete")
        rsync_cmd.extend(["-e", self._build_rsync_ssh_command()])
        rsync_cmd.extend(
            [
                f"{str(local_root)}/",
                f"{self.username}@{self.host}:{remote_root}/",
            ]
        )

        cmd = self._maybe_prefix_with_sshpass(rsync_cmd)
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
            timeout=max(float(self.timeout), 300),
        )
        if proc.returncode != 0:
            raise RuntimeError(
                "rsync failed for SSH pre-sync.\n"
                f"command: {' '.join(cmd)}\n"
                f"stdout:\n{proc.stdout}\n"
                f"stderr:\n{proc.stderr}"
            )

        chmod_cmd = (
            f"find {quote(remote_root)} -type f "
            "\\( -name '*.sh' -o -name '*.py' -o -name '[0-9]*_*' \\) -exec chmod +x {} \\;"
        )
        self.run(chmod_cmd)
        self._register_synced_root(local_root, remote_root)

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

        resolved_local = local_path.expanduser().resolve()
        synced_remote = self._lookup_synced_remote_path(resolved_local)
        if synced_remote:
            command = build_script_command(synced_remote, resolved_local.suffix, args)
            return self.run(command, timeout=timeout)

        remote_path = (
            f"{self.remote_tmp_dir.rstrip('/')}/"
            f"{local_path.stem}_{uuid4().hex}{local_path.suffix}"
        )

        self.run(f"mkdir -p {quote(self.remote_tmp_dir)}")
        client = self._require_client()
        with client.open_sftp() as sftp:
            sftp.put(str(local_path), remote_path)
        self.run(f"chmod +x {quote(remote_path)}")

        command = build_script_command(remote_path, local_path.suffix, args)
        try:
            return self.run(command, timeout=timeout)
        finally:
            if cleanup:
                self.run(f"rm -f {quote(remote_path)}")

    def _register_synced_root(self, local_root: Path, remote_root: str) -> None:
        self._synced_roots = [pair for pair in self._synced_roots if pair[0] != local_root]
        self._synced_roots.append((local_root, remote_root))

    def _lookup_synced_remote_path(self, local_path: Path) -> str | None:
        best_match: tuple[Path, str] | None = None
        for local_root, remote_root in self._synced_roots:
            try:
                local_path.relative_to(local_root)
            except ValueError:
                continue
            if best_match is None or len(str(local_root)) > len(str(best_match[0])):
                best_match = (local_root, remote_root)

        if best_match is None:
            return None

        local_root, remote_root = best_match
        relative = local_path.relative_to(local_root).as_posix()
        return f"{remote_root.rstrip('/')}/{relative}"

    def _build_rsync_ssh_command(self) -> str:
        ssh_parts = [
            "ssh",
            "-p",
            str(self.port),
            "-o",
            "StrictHostKeyChecking=no",
            "-o",
            "UserKnownHostsFile=/dev/null",
        ]
        if self.key_filename:
            ssh_parts.extend(["-i", self.key_filename])
        return shell_join(ssh_parts)

    def _maybe_prefix_with_sshpass(self, cmd: list[str]) -> list[str]:
        if self.password and not self.key_filename:
            sshpass = shutil.which("sshpass")
            if not sshpass:
                raise RuntimeError(
                    "Password-based rsync requires `sshpass` or SSH key auth. "
                    "Install sshpass or configure [ssh].key_filename."
                )
            return [sshpass, "-p", self.password, *cmd]
        return cmd

    def _require_client(self) -> Any:
        if self._client is None:
            raise RuntimeError("SSH client is not connected.")
        return self._client
