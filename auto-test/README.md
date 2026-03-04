# Pytest Device Case Framework

Lightweight test-case framework for embedded/Linux devices with these goals:

- One connection per test session (`scope="session"`), not one per test.
- Mix SSH and serial cases in the same pytest run.
- Treat local `bash` / `python` scripts as test cases.
- Run all cases, a subset, or a single case with standard pytest selectors.

## Install

```bash
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

## Configure Device Access

Copy the sample config and edit credentials/device paths:

```bash
cp device.example.toml device.toml
```

The framework auto-loads `device.toml` from project root. You can also pass a path:

```bash
pytest --device-config /path/to/device.toml
```

## Add Script Cases

Put all case assets under `cases/`. Any file whose basename starts with `数字_` is treated as a test case (for example `001_boot.sh`, `020_network.py`, `300_uart.expect`).

- Use `cases/ssh/` for SSH-routed cases.
- Use `cases/serial/` for serial-routed cases.
- Files without numeric prefix are treated as support files (synced for SSH, not executed as tests).

- SSH cases are synced once at session start via `rsync`, then executed in place.
- Serial cases are transferred per case via base64 over the serial shell.

## Run

- Run all discovered cases:
```bash
pytest -s -v
```

- Run only SSH cases:
```bash
pytest -s -v -k ssh
```

- Run one case:
```bash
pytest -s -v tests/test_script_cases.py::test_script_cases[ssh/001_health_check.sh]
```

- Run only serial transport explicitly:
```bash
pytest -s -v --default-transport serial -k serial
```

## Override Config in CLI

Example SSH:

```bash
pytest -s -v --ssh-host 192.168.1.100 --ssh-user root --ssh-password password
```

Disable SSH pre-sync (if needed):

```bash
pytest -s -v --no-ssh-sync
```

Example serial:

```bash
pytest -s -v --serial-port /dev/ttyUSB0 --serial-user root --serial-password password
```

## Notes

- Missing transport configuration causes only relevant tests to be skipped.
- SSH and serial each maintain one session per pytest invocation.
- SSH pre-sync requires `rsync` on host and target. Password auth also needs `sshpass` unless key auth is used.
- Serial login supports prompt detection and optional username/password flow.
- Transport route is inferred from path: cases under `cases/ssh/` use SSH, under `cases/serial/` use serial.
- `.py` uses `python3`, `.sh/.bash` uses `bash`; other suffixes are executed directly (ensure shebang + execute permission).
- SSH pre-sync auto-`chmod +x` for `.sh`, `.py`, and `数字_` files.
