---
name: conventional-commit
description: Write git commit messages following the Conventional Commits v1.0.0 specification. Use whenever committing or drafting/suggesting a commit message.
---

# Conventional Commit

Commit messages follow [Conventional Commits v1.0.0](https://www.conventionalcommits.org/en/v1.0.0/):

```
<type>[optional scope][!]: <description>

[optional body]

[optional footer(s)]
```

## Types

| Type | Use for | SemVer |
|---|---|---|
| `feat` | New functionality | MINOR |
| `fix` | Bug fix | PATCH |
| `refactor` | Code change that neither fixes a bug nor adds a feature | — |
| `docs` | Documentation only (docs/, README, comments) | — |
| `build` | Build system, CMakeLists.txt, dependencies, toolchain | — |
| `perf` | Performance improvement | — |
| `style` | Formatting only, no behavior change | — |
| `test` | Tests only | — |
| `chore` | Maintenance not covered above (.gitignore, assets, tooling) | — |
| `ci` | CI configuration | — |

## Rules

- **Description**: imperative mood, lowercase start, no trailing period, concise one-liner (`feat: add playback status API`, not `Added playback status API.`).
- **Scope** (optional): a noun for the codebase area in parentheses — `fix(player): join worker before closing device`. Useful scopes here: `player`, `gui`, `filesystem`, `settings`, `switch`.
- **Breaking changes**: append `!` before the colon (`feat!: ...`) and/or add a `BREAKING CHANGE: <description>` footer. Rare in this project (no public API), so only when it genuinely breaks something a user relies on (e.g. settings-file format).
- **Body** (optional): one blank line after the description; explain *why* when the diff alone doesn't. Skip it for self-explanatory changes.
- **Footers**: one blank line after the body; tokens use hyphens (`Reviewed-by:`). Keep the `Co-Authored-By: Claude ...` footer required by the harness as the last footer.
- One logical change per commit — if the message wants to say "and", consider splitting.

## Picking the type

Choose by the *primary intent* of the change, matching the branch type when on a Conventional Branch (`feature/` → `feat`, `bugfix/` → `fix`, `chore/` → `chore`, ...; see the conventional-branch skill). A feature that also updates docs/ (mandatory in this repo) is still one `feat` commit — docs ride along with the change they document.

## Examples

```
feat(player): add PlaybackStatus snapshot API
fix(gui): balance Begin/End when file browser is empty
refactor: extract Application layer from main.cpp
docs: update audio classDiagram with metadata types
build(switch): link libopenmpt via pkg-config
chore: add conventional-branch skill
```
