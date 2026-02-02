#!/usr/bin/env bash
set -euo pipefail

mirror_root="${1:?mirror root required}"

if [[ ! -d "${mirror_root}" ]]; then
  echo "missing_root: ${mirror_root}"
  exit 1
fi

shopt -s nullglob
found=0
for repo in "${mirror_root}"/*.git; do
  if [[ ! -d "${repo}" ]]; then
    continue
  fi
  found=1
  name="$(basename "${repo}")"
  name="${name%.git}"

  set +e
  git -C "${repo}" remote update --prune
  rc=$?
  set -e

  if [[ $rc -eq 0 ]]; then
    echo "updated: ${name} (${repo})"
  else
    echo "update_failed: ${name} (${repo}) rc=${rc}"
  fi
done

if [[ $found -eq 0 ]]; then
  echo "no_mirrors_found: ${mirror_root}"
fi
