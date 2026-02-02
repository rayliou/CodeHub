#!/usr/bin/env bash
set -euo pipefail

org_url="${1:?org url required}"
project="${2:?project required}"
mirror_root="${3:?mirror_root required}"
config_json="${4:?config json required}"
push_mirrors="${PUSH_MIRRORS:-0}"

az devops configure --defaults organization="$org_url" project="$project" >/dev/null

mirror_root_from_config="$(python3 - <<'PY' "$config_json" "$project"
import json, sys
cfg = json.load(open(sys.argv[1], "r", encoding="utf-8"))
project = sys.argv[2]
if "projects" not in cfg:
    print("")
    sys.exit(0)
for p in cfg.get("projects", []):
    if p.get("name") == project:
        print(p.get("mirrorRoot", ""))
        sys.exit(0)
print(f"project_not_found: {project}", file=sys.stderr)
sys.exit(1)
PY
)"
if [[ -n "${mirror_root_from_config}" ]]; then
  mirror_root="${mirror_root_from_config}"
fi

python3 - <<'PY' "$config_json" "$project" | while IFS=$'\t' read -r name skip; do
import json, sys
cfg = json.load(open(sys.argv[1], "r", encoding="utf-8"))
project = sys.argv[2]
repos = None
if "projects" in cfg:
    for p in cfg.get("projects", []):
        if p.get("name") == project:
            repos = p.get("repos", [])
            break
    if repos is None:
        print(f"project_not_found: {project}", file=sys.stderr)
        sys.exit(1)
else:
    repos = cfg.get("repos", [])
for r in repos:
    print(r.get("name", ""), "1" if r.get("skip") else "0", sep="\t")
PY
  if [[ -z "${name}" ]]; then
    continue
  fi
  if [[ "${skip}" == "1" ]]; then
    python3 - <<PY "$name"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "skip_push"}, ensure_ascii=True))
PY
    continue
  fi

  local_repo="${mirror_root}/${name}.git"
  if [[ ! -d "${local_repo}" ]]; then
    python3 - <<PY "$name" "$local_repo" "$mirror_root"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "missing_local_mirror", "path": sys.argv[2], "mirrorRoot": sys.argv[3]}, ensure_ascii=True))
PY
    continue
  fi

  ssh_url="$(az repos show --repository "${name}" --output json --query "sshUrl")"
  ssh_url="${ssh_url%\"}"
  ssh_url="${ssh_url#\"}"
  if [[ -z "${ssh_url}" || "${ssh_url}" == "null" ]]; then
    python3 - <<PY "$name"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "missing_remote_repo"}, ensure_ascii=True))
PY
    continue
  fi

  if [[ "${push_mirrors}" == "1" ]]; then
    cmd=(git -C "${local_repo}" push --prune "${ssh_url}" "+refs/heads/*:refs/heads/*" "+refs/tags/*:refs/tags/*")
    cmd_str="git -C \"${local_repo}\" push --prune \"${ssh_url}\" \"+refs/heads/*:refs/heads/*\" \"+refs/tags/*:refs/tags/*\""
  else
    cmd=(git -C "${local_repo}" push --prune --dry-run "${ssh_url}" "+refs/heads/*:refs/heads/*" "+refs/tags/*:refs/tags/*")
    cmd_str="git -C \"${local_repo}\" push --prune --dry-run \"${ssh_url}\" \"+refs/heads/*:refs/heads/*\" \"+refs/tags/*:refs/tags/*\""
  fi

  set +e
  "${cmd[@]}"
  rc=$?
  set -e

  if [[ $rc -eq 0 ]]; then
    if [[ "${push_mirrors}" == "1" ]]; then
      python3 - <<PY "$name" "$ssh_url" "$cmd_str"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "pushed", "sshUrl": sys.argv[2], "cmd": sys.argv[3]}, ensure_ascii=True))
PY
    else
      python3 - <<PY "$name" "$ssh_url" "$cmd_str"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "dry_run", "sshUrl": sys.argv[2], "cmd": sys.argv[3]}, ensure_ascii=True))
PY
    fi
  else
    python3 - <<PY "$name" "$ssh_url" "$rc"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "push_failed", "sshUrl": sys.argv[2], "rc": int(sys.argv[3])}, ensure_ascii=True))
PY
  fi
done
