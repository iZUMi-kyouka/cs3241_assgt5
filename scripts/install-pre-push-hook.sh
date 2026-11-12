#!/usr/bin/env bash
set -euo pipefail

repo_root=$(git rev-parse --show-toplevel)
hook_path="$repo_root/.githooks/pre-push"
existing_hooks_path=$(git -C "$repo_root" config --local --get core.hooksPath || true)

if [[ ! -f "$hook_path" ]]; then
    echo "Expected hook not found: $hook_path" >&2
    exit 1
fi

if [[ -n "$existing_hooks_path" && "$existing_hooks_path" != "$repo_root/.githooks" ]]; then
    echo "This repository already uses core.hooksPath=$existing_hooks_path." >&2
    echo "Review that setting before replacing it with $repo_root/.githooks." >&2
    exit 1
fi

command -v gh >/dev/null 2>&1 || {
    echo "Install GitHub CLI (gh) before enabling the pre-push hook." >&2
    exit 1
}
gh auth status >/dev/null
gh auth setup-git --hostname github.com

chmod +x "$hook_path"
git -C "$repo_root" config --local core.hooksPath "$repo_root/.githooks"

echo "Installed the repository pre-push hook."
