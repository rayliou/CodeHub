# Repository Guidelines

## Project Structure & Module Organization
- Source code lives in `src/device_case_framework/`.
- Transport implementations are in `src/device_case_framework/transports/` (`ssh.py`, `serial.py`, shared logic in `base.py`).
- Pytest wiring and session fixtures are in `tests/conftest.py`.
- Case discovery/execution is in `tests/test_script_cases.py`.
- Device-side scripts and assets live under `cases/`.
- Files named `^\d+_` are executable test cases.
- Non-prefixed files are support assets (synced for SSH, not treated as tests).
- Example runtime config: `device.example.toml` (copy to `device.toml` locally).

## Build, Test, and Development Commands
- `python -m venv .venv && source .venv/bin/activate`: create and activate virtualenv.
- `pip install -e .`: install framework in editable mode.
- `pytest --collect-only -q`: verify case discovery and IDs.
- `pytest -s -v`: run all discovered cases.
- `pytest -s -v -k ssh` / `pytest -s -v -k serial`: run one transport subset.
- `pytest -s -v tests/test_script_cases.py::test_script_cases[ssh/001_health_check.sh]`: run a single case.

## Coding Style & Naming Conventions
- Python: 4-space indentation, PEP 8 style, type hints for public functions.
- Files/modules: `snake_case.py`; classes: `CamelCase`; constants: `UPPER_CASE`.
- Keep transport behavior explicit and side effects minimal.
- Case file naming: numeric prefix plus underscore, e.g. `001_boot.sh`, `120_uart.expect`.

## Testing Guidelines
- Framework uses `pytest` with session-scoped fixtures to avoid reconnect overhead.
- Transport route is path-based: `cases/ssh/` => SSH, `cases/serial/` => serial.
- `.py` runs with `python3`, `.sh/.bash` with `bash`, other suffixes execute directly (needs shebang + exec bit).
- Validate changes with `pytest --collect-only -q` and a focused run (`-k` or single case node).

## Commit & Pull Request Guidelines
- No Git history is available in this workspace, so no established convention can be inferred.
- Use clear, scoped commit messages (recommended: Conventional Commits, e.g. `feat(ssh): add rsync pre-sync`).
- PRs should include what changed and why.
- PRs should include test evidence (commands run and key results).
- PRs should include config/security impact (especially device access settings).

## Security & Configuration Tips
- Do not commit `device.toml` or real credentials.
- Prefer SSH key auth over password auth.
- SSH pre-sync depends on `rsync` (and `sshpass` when using password auth without keys).
