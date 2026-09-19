# Apps

Application services live under `apps/`. They run in the **`sw-devcontainer`**.

| App | Stack | Purpose |
| --- | ----- | ------- |
| [`api-backend`](api-backend.md) | Go 1.24 + Gin | REST API: device registration & management, JWT auth, OpenAPI docs |
| [`boom-discord-bot`](boom-discord-bot.md) | TypeScript + Cloudflare Workers | Discord slash-command transport that fires a Claude Code Routine to create Linear issues |
| [`device-web`](device-web.md) | Node.js + Express | Minimal local UI, also embedded into `bom-node` firmware |

The repo is a pnpm workspace; `apps/**` packages are versioned with Changesets. Run
`task -l` to see app tasks (e.g. `bot:dev`, `bot:test`, `bot:deploy`).

Detailed docs for each app are included from the app's own `README.md` on the pages
linked above.
