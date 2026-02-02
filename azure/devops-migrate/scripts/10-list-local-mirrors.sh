#!/usr/bin/env bash
set -euo pipefail

root="${1:?mirror root required}"
project="${2:?project name required}"

python3 - <<'PY' "$root" "$project"
import json, os, sys
root = sys.argv[1]
project = sys.argv[2]
repos = []
if os.path.isdir(root):
    for name in sorted(os.listdir(root)):
        p = os.path.join(root, name)
        if os.path.isdir(p) and name.endswith(".git"):
            repos.append({"name": name[:-4]})
print(json.dumps({"projects": [{"name": project, "mirrorRoot": root, "repos": repos}]}, ensure_ascii=True, indent=2))
PY
