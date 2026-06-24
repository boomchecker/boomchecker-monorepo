# Monorepo rules

A monorepo only stays usable if everyone follows the same conventions. These are not
optional — PRs that ignore them get sent back.

## Repository layout

```text
apps/      backend + services (api-backend, boom-discord-bot, device-web)
fw/        firmware (bom-node ESP32, bom-stm32node STM32)
scripts/   DSP / analysis tools
hw/        hardware (Altium libs, templates, node schematics)
templates/ document templates
docs/      this documentation
```

The repo is a **pnpm workspace** (`apps/**`, `fw/**`) and tasks are run with
[Taskfile](https://taskfile.dev) — `task -l` lists everything.

## Conventional Commits

Every commit message follows
[Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>(<scope>): <short summary>
```

- **type** — `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `build`, `ci`, …
- **scope** — the **workspace/area you touched**, e.g. `bom-node`, `api-backend`,
  `boom-discord-bot`, `scripts`, `docs`. Scopes align with the directory names.
- **summary** — imperative, lower-case, no trailing period.

Examples (taken from real history):

```text
feat: add discord bot
feat(scripts): add FxLMS algo
fix: remove sphinx-multiproject and pin Sphinx
feat(stmnode:schema): add schematics for stmnode-v0.1
```

## Changesets — versioning

Versioned packages (only those under `apps/**` and `fw/**`) use
[Changesets](https://github.com/changesets/changesets). If your change affects a
versioned package, add a changeset describing it:

```bash
task changeset          # interactive: pick packages + bump level + summary
```

This writes a small markdown file under `.changeset/`. **Commit it with your PR.**
Maintainers later run `task changeset:version` to roll the version bumps and
changelogs. You normally don't run that yourself.

!!! note "When do I need a changeset?"
    Only when you change the behaviour of a versioned package (`apps/**`, `fw/**`).
    Docs-only, script, or hardware changes don't need one. Access is `restricted` and
    the base branch is `main` (`.changeset/config.json`).

## Branches

Use `yourname/short-description` (e.g. `maxamart/improve-documentation`). Details and
the full PR flow are in [Contributing](contributing.md).

## Discovering tasks

When in doubt, list what the repo can do:

```bash
task -l
```

Next: [Contributing (theses) →](contributing.md)
