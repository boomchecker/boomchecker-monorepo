# Boomchecker Monorepo

Monorepo for research and development of acoustic detection units. Primary stack is WSL2 + VS Code with pnpm workspaces and Docker devcontainers for consistent environments.

## Workflow and rules
### Issues
Tasks are tracked in repository issues. Use labels to flag priority and area, and capture acceptance criteria in the description.

### Commits
Commits follow Conventional Commits (cheat sheet: https://gist.github.com/qoomon/5dfcdf8eec66a051ecd85625518cfd13). Keep scopes aligned with workspace names (e.g., `feat(api-backend): ...`), and keep PRs small and focused.

### Versioning
We use Changesets; each package has its own version. Only projects in `apps/` and `fw/` are versioned. Add a changeset for user-facing changes, run `task changeset:version` before release branches, and let CI/publish pipelines consume the generated versions.

## Repository structure
- `apps/` - backend (`api-backend`) and future front-end services.
- `fw/` - firmware for the units and related tools.
- `scripts/` - helper scripts for development (experiments, utilities).
- `.devcontainer/` - Docker devcontainers: `fw-devcontainer` (ESP-IDF) and `sw-devcontainer` (Go/Node) for VS Code.

## Taskfile
- Root `Taskfile.yml` contains shared tasks and includes Taskfiles in `fw/` and `apps/api-backend/` when present.
- Examples:
  - `task -l` - list available tasks.
  - `task changeset` - create a new changeset.
  - `task changeset:version` - apply pending changesets to package versions.

## Docker and development environment
Devcontainer is a prebuilt image plus VS Code configuration that pins toolchains, CLIs and dependencies so everyone develops in the same environment (no host drift, works the same on CI). We use two devcontainers at the moment.
- `fw-devcontainer`: ESP-IDF-based image prepared for firmware work
- `sw-devcontainer`: Go 1.23 + Node 20 for backend/web services.

- VS Code usage: open in WSL2, run `Ctrl + Shift + P` and type `Remote-Containers: Reopen in Container`, and pick the service you need. The correct toolchains/extensions are baked in.

## Quick start
1) Install Docker Desktop with the WSL2 backend.
2) Open the repo in VS Code and launch the appropriate devcontainer (fw or sw).
3) Run `pnpm install` inside that container if needed.
4) Run `task setup` inside that container and start coding.
