# Azure DevOps Migration Helpers

Small scripts + Just targets to manage Azure DevOps Git repo creation and mirror pushes.

## Config

`config/projects.json` is the source of truth. It supports multiple projects; each project has its own repo list.

Example:

```json
{
  "projects": [
    {
      "name": "Edge Solution Prod",
      "mirrorRoot": "/home/lxr/W_urgent/done.edge solution Prod",
      "repos": [
        { "name": "buspas_base_imgs" },
        { "name": "edge-experiments" },
        { "name": "ESP32_interface" },
        { "name": "StrawHats" }
      ]
    }
  ]
}
```

## Targets

All default values are in `Justfile`.

- `az-check`: validate Azure CLI + DevOps extension.
- `list-local`: list local mirror repos under `MIRROR_ROOT` and emit `projects.json` format for `PROJECT2`.
- `local-sync-mirrors`: run `git remote update --prune` for every `*.git` under `MIRROR_ROOT_SYNC`.
- `dst-create-repos`: create missing repos in the destination org/project using `config/projects.json`.
- `dst-push-mirrors`: push local mirrors to destination repos.
- `dst-sync`: run create then push.

## Usage

Update values in `Justfile` as needed (org URLs, project name, mirror roots).

Generate a `projects.json` from local mirrors:

```bash
just list-local > config/projects.json
```

Create missing repos:

```bash
just dst-create-repos
```

Sync local mirror remotes:

```bash
just local-sync-mirrors
```

Push mirrors:

```bash
just dst-push-mirrors
```

Live push (requires explicit opt-in):

```bash
just dst-push-mirrors-live
```

## Notes

- Azure DevOps auth: set `AZURE_DEVOPS_EXT_PAT` in your environment.
- The create/push scripts operate on a single project at a time; manual review of `config/projects.json` is expected.
- `dst-push-mirrors` runs in dry-run mode by default. Set `PUSH_MIRRORS=1` (or use `dst-push-mirrors-live`) to perform a real push.
- If `mirrorRoot` is set for the project in `config/projects.json`, it overrides `MIRROR_ROOT` for push operations.
- Mirror pushes use the repository `sshUrl` (no HTTPS).
- Pushes include only `refs/heads/*` and `refs/tags/*` (PR refs like `refs/pull/*` are excluded).
