from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

import pytest

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from device_case_framework.transports import SerialTransport, SSHTransport


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("device")
    group.addoption(
        "--device-config",
        action="store",
        default=None,
        help="Path to TOML config. If omitted, ./device.toml is used when it exists.",
    )
    group.addoption("--default-transport", action="store", default="ssh", choices=["ssh", "serial"])

    group.addoption("--ssh-host", action="store", default=None)
    group.addoption("--ssh-port", action="store", default=None, type=int)
    group.addoption("--ssh-user", action="store", default=None)
    group.addoption("--ssh-password", action="store", default=None)
    group.addoption("--ssh-key", action="store", default=None)
    group.addoption("--ssh-timeout", action="store", default=None, type=float)
    group.addoption("--ssh-remote-tmp", action="store", default=None)
    group.addoption("--ssh-sync-local-dir", action="store", default=None)
    group.addoption("--ssh-sync-remote-dir", action="store", default=None)
    group.addoption("--ssh-sync", action="store_true", dest="ssh_sync", default=None)
    group.addoption("--no-ssh-sync", action="store_false", dest="ssh_sync")
    group.addoption("--ssh-sync-delete", action="store_true", dest="ssh_sync_delete", default=None)
    group.addoption("--no-ssh-sync-delete", action="store_false", dest="ssh_sync_delete")

    group.addoption("--serial-port", action="store", default=None)
    group.addoption("--serial-baudrate", action="store", default=None, type=int)
    group.addoption("--serial-user", action="store", default=None)
    group.addoption("--serial-password", action="store", default=None)
    group.addoption("--serial-prompt", action="store", default=None)
    group.addoption("--serial-timeout", action="store", default=None, type=float)
    group.addoption("--serial-login-timeout", action="store", default=None, type=float)
    group.addoption("--serial-remote-tmp", action="store", default=None)


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers",
        'transport(name): Route test to "ssh" or "serial" session fixture.',
    )


def _load_device_config(pytestconfig: pytest.Config) -> dict[str, Any]:
    config_path_opt = pytestconfig.getoption("--device-config")
    if config_path_opt:
        config_path = Path(config_path_opt)
        if not config_path.exists():
            raise pytest.UsageError(f"--device-config file not found: {config_path}")
    else:
        config_path = ROOT / "device.toml"
        if not config_path.exists():
            return {}

    with config_path.open("rb") as f:
        loaded = tomllib.load(f)
    if not isinstance(loaded, dict):
        raise pytest.UsageError(f"Invalid TOML structure in config: {config_path}")
    return loaded


def _pick(pytestconfig: pytest.Config, cli_option: str, value_from_file: Any, default: Any) -> Any:
    cli_value = pytestconfig.getoption(cli_option)
    if cli_value is not None:
        return cli_value
    if value_from_file is not None:
        return value_from_file
    return default


def _as_bool(value: Any, default: bool) -> bool:
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        lowered = value.strip().lower()
        if lowered in {"1", "true", "yes", "on"}:
            return True
        if lowered in {"0", "false", "no", "off"}:
            return False
    raise pytest.UsageError(f"Invalid boolean value: {value!r}")


@pytest.fixture(scope="session")
def device_settings(pytestconfig: pytest.Config) -> dict[str, dict[str, Any]]:
    loaded = _load_device_config(pytestconfig)
    ssh_loaded = loaded.get("ssh", {}) if isinstance(loaded.get("ssh", {}), dict) else {}
    serial_loaded = loaded.get("serial", {}) if isinstance(loaded.get("serial", {}), dict) else {}
    ssh_remote_tmp_dir = _pick(
        pytestconfig,
        "--ssh-remote-tmp",
        ssh_loaded.get("remote_tmp_dir"),
        "/tmp/pytest-cases",
    )

    ssh = {
        "host": _pick(pytestconfig, "--ssh-host", ssh_loaded.get("host"), None),
        "port": int(_pick(pytestconfig, "--ssh-port", ssh_loaded.get("port"), 22)),
        "username": _pick(pytestconfig, "--ssh-user", ssh_loaded.get("username"), "root"),
        "password": _pick(pytestconfig, "--ssh-password", ssh_loaded.get("password"), None),
        "key_filename": _pick(pytestconfig, "--ssh-key", ssh_loaded.get("key_filename"), None),
        "timeout": float(_pick(pytestconfig, "--ssh-timeout", ssh_loaded.get("timeout"), 15)),
        "remote_tmp_dir": ssh_remote_tmp_dir,
        "sync_enabled": _as_bool(
            _pick(pytestconfig, "ssh_sync", ssh_loaded.get("sync_enabled"), True),
            True,
        ),
        "sync_local_dir": _pick(
            pytestconfig,
            "--ssh-sync-local-dir",
            ssh_loaded.get("sync_local_dir"),
            "cases",
        ),
        "sync_remote_dir": _pick(
            pytestconfig,
            "--ssh-sync-remote-dir",
            ssh_loaded.get("sync_remote_dir"),
            f"{str(ssh_remote_tmp_dir).rstrip('/')}/cases",
        ),
        "sync_delete": _as_bool(
            _pick(pytestconfig, "ssh_sync_delete", ssh_loaded.get("sync_delete"), True),
            True,
        ),
    }

    serial = {
        "port": _pick(pytestconfig, "--serial-port", serial_loaded.get("port"), None),
        "baudrate": int(_pick(pytestconfig, "--serial-baudrate", serial_loaded.get("baudrate"), 115200)),
        "username": _pick(pytestconfig, "--serial-user", serial_loaded.get("username"), None),
        "password": _pick(pytestconfig, "--serial-password", serial_loaded.get("password"), None),
        "prompt_regex": _pick(
            pytestconfig,
            "--serial-prompt",
            serial_loaded.get("prompt_regex"),
            r"[#>$]\s*$",
        ),
        "timeout": float(_pick(pytestconfig, "--serial-timeout", serial_loaded.get("timeout"), 20)),
        "login_timeout": float(
            _pick(pytestconfig, "--serial-login-timeout", serial_loaded.get("login_timeout"), 30)
        ),
        "remote_tmp_dir": _pick(
            pytestconfig,
            "--serial-remote-tmp",
            serial_loaded.get("remote_tmp_dir"),
            "/tmp/pytest-cases",
        ),
    }
    return {"ssh": ssh, "serial": serial}


@pytest.fixture(scope="session")
def ssh_device(device_settings: dict[str, dict[str, Any]]) -> SSHTransport:
    cfg = device_settings["ssh"]
    if not cfg["host"]:
        pytest.skip("SSH not configured. Set [ssh].host in device.toml or pass --ssh-host.")

    conn = SSHTransport(
        host=cfg["host"],
        port=cfg["port"],
        username=cfg["username"],
        password=cfg["password"],
        key_filename=cfg["key_filename"],
        timeout=cfg["timeout"],
        remote_tmp_dir=cfg["remote_tmp_dir"],
    )
    conn.connect()
    if cfg["sync_enabled"]:
        local_sync_dir = Path(cfg["sync_local_dir"])
        if not local_sync_dir.is_absolute():
            local_sync_dir = (ROOT / local_sync_dir).resolve()
        conn.sync_directory(
            local_dir=local_sync_dir,
            remote_dir=str(cfg["sync_remote_dir"]),
            delete=bool(cfg["sync_delete"]),
        )
    yield conn
    conn.close()


@pytest.fixture(scope="session")
def serial_device(device_settings: dict[str, dict[str, Any]]) -> SerialTransport:
    cfg = device_settings["serial"]
    if not cfg["port"]:
        pytest.skip("Serial not configured. Set [serial].port in device.toml or pass --serial-port.")

    conn = SerialTransport(
        port=cfg["port"],
        baudrate=cfg["baudrate"],
        username=cfg["username"],
        password=cfg["password"],
        prompt_regex=cfg["prompt_regex"],
        timeout=cfg["timeout"],
        login_timeout=cfg["login_timeout"],
        remote_tmp_dir=cfg["remote_tmp_dir"],
    )
    conn.connect()
    yield conn
    conn.close()


@pytest.fixture
def device(request: pytest.FixtureRequest) -> SSHTransport | SerialTransport:
    marker = request.node.get_closest_marker("transport")
    if marker and marker.args:
        transport = str(marker.args[0]).lower()
    else:
        transport = request.config.getoption("--default-transport")

    if transport == "ssh":
        return request.getfixturevalue("ssh_device")
    if transport == "serial":
        return request.getfixturevalue("serial_device")

    raise pytest.UsageError(f'Unsupported transport "{transport}". Use "ssh" or "serial".')


@pytest.fixture
def run_script(device: SSHTransport | SerialTransport):
    def _run(script_path: str | Path, *args: str, timeout: float | None = None, cleanup: bool = True):
        normalized_args = tuple(str(arg) for arg in args)
        return device.run_script(
            script_path=script_path,
            args=normalized_args,
            timeout=timeout,
            cleanup=cleanup,
        )

    return _run
