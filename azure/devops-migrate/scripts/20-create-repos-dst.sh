#!/usr/bin/env bash
set -euo pipefail

org_url="${1:?org url required}"
project="${2:?project required}"
config_json="${3:?config json required}"

az devops configure --defaults organization="$org_url" project="$project" >/dev/null

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
print(json.dumps({"repo": sys.argv[1], "action": "skip_create"}, ensure_ascii=True))
PY
    continue
  fi

  existing_id="$(az repos list --output json --query "[?name=='${name}'].id | [0]")"
  existing_id="${existing_id%\"}"
  existing_id="${existing_id#\"}"

  if [[ -n "${existing_id}" && "${existing_id}" != "null" ]]; then
    python3 - <<PY "$name" "$existing_id"
import json, sys
print(json.dumps({"repo": sys.argv[1], "action": "exists", "id": sys.argv[2]}, ensure_ascii=True))
PY
    continue
  fi

  created="$(az repos create --name "${name}" --output json)"
  python3 - <<PY "$created"
import json, sys
obj = json.loads(sys.argv[1])
print(json.dumps({
  "repo": obj.get("name"),
  "action": "created",
  "id": obj.get("id"),
  "remoteUrl": obj.get("remoteUrl"),
  "webUrl": obj.get("webUrl"),
}, ensure_ascii=True))
PY
done
