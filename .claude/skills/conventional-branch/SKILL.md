---
name: conventional-branch
description: Name git branches following the Conventional Branch specification (conventionalbranch.org). Use whenever creating, renaming, or suggesting a git branch name.
---

# Conventional Branch

Branch names follow [Conventional Branch](https://conventionalbranch.org/): `<type>/<description>`. Trunk branches (`main`, `master`, `develop`) take no prefix.

## Types

Match the type to the same intent you'd use for the Conventional Commit the branch will produce:

| Type | Use for |
|---|---|
| `feature/` | New functionality (commit type `feat:`) |
| `bugfix/` | Bug corrections (commit type `fix:`) |
| `hotfix/` | Urgent production/release fixes |
| `release/` | Release preparation (e.g. `release/v1.2.0`) |
| `chore/` | Non-code tasks: dependencies, docs, build, CI |

The spec also defines AI-agent source types (`claude/`, `ai/`, ...). Default to the purpose-driven types above; use `claude/<description>` only if the user asks to mark the branch as AI-authored.

## Description rules

- Lowercase letters (`a-z`), digits (`0-9`), hyphens between words — nothing else (no spaces, underscores, uppercase, or special characters).
- No consecutive hyphens, no leading/trailing hyphens.
- Dots are allowed only in version numbers on `release/` branches.
- Short and imperative-ish: describe the work, not the whole sentence.
- A ticket/issue number may lead the description: `feature/issue-123-login`.

Validation regex: `^(feature|bugfix|hotfix|release|chore|claude)/[a-z0-9]+(-[a-z0-9]+)*$` (plus dots for release versions, e.g. `release/v1.2.0`).

## Examples

Valid: `feature/add-metadata-tab`, `bugfix/fix-audio-underrun`, `chore/update-imgui-submodule`, `release/v1.2.0`, `feature/issue-42-theme-toggle`

Invalid: `Feature/Add-Login` (uppercase), `feature/new--login` (consecutive hyphens), `fix/header_bug` (underscore — and `fix/` isn't a type here, use `bugfix/`)

## Workflow

1. Derive the type from the task (same judgment as picking the Conventional Commit type).
2. Build the description from the task's key words, kebab-cased; include the issue number if one is in play.
3. Check the name against the rules above, then create it: `git checkout -b <type>/<description>`.
