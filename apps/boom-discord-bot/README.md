# boom-discord-bot

A thin Discord slash-command **transport** running on Cloudflare Workers. It does
not create Linear issues itself — it forwards the command text to a **Claude Code
Routine**, which owns the repo context, MCP connectors, and the Linear integration.

```
/boom-linear text:"..."  ──▶  Cloudflare Worker  ──▶  Claude Code Routine /fire  ──▶  Linear issue
                              (verify, defer, fire)     (creates the issue)
        ◀── public follow-up with the Claude Code session URL ──┘
```

## What it does

1. Receives the Discord interaction (HTTP POST only).
2. Verifies the Ed25519 request signature (native Web Crypto, no deps).
3. Answers the Discord `PING` handshake with `PONG`.
4. On `/boom-linear`, returns a **deferred** ack immediately (within Discord's 3s
   limit), then in the background fires the routine and edits the message with the
   returned **session URL**.

The Worker contains **no** Anthropic Messages API call, **no** Linear API call, and
**no** MCP logic. All of that lives in the routine you configure at
[claude.ai/code/routines](https://claude.ai/code/routines).

## Why Interactions Endpoint URL (not the Gateway / Webhook Events)

Discord delivers slash commands to a public HTTPS **Interactions Endpoint URL** as
signed POST requests. This fits a serverless Worker perfectly: no long-running
process, no Gateway connection, no privileged intents, no message-history access.
We do **not** use Webhook Events (a separate, unrelated delivery channel) — the
endpoint must be set as the Interactions Endpoint URL or Discord will not send
slash commands or the verification `PING`.

## The follow-up shows a session URL, not the issue

The fire endpoint is asynchronous: it returns a Claude Code **session URL** as soon
as the run starts, before the issue exists. The follow-up therefore links to the
run. The Linear issue appears once the routine finishes.

> The session URL only opens for the **routine owner's** claude.ai account. If a
> teammate triggers the command, they still see the link but cannot open the session.

## Secrets

> All deployment and runtime secrets are stored in GitHub Environment `staging`.
> GitHub Actions uploads runtime secrets to Cloudflare during deployment.
> Manual `wrangler secret put` is not required for normal deployment.

Stored in the **`staging` GitHub Environment** (Settings → Environments → `staging`):

| Name | Role |
| --- | --- |
| `CLOUDFLARE_API_TOKEN` | Deploy-only — used by GitHub Actions to authenticate to Cloudflare |
| `CLOUDFLARE_ACCOUNT_ID` | Deploy-only — Cloudflare account for the deploy |
| `DISCORD_PUBLIC_KEY` | Worker runtime — uploaded to Cloudflare before deploy |
| `CLAUDE_ROUTINE_FIRE_URL` | Worker runtime — uploaded to Cloudflare before deploy |
| `CLAUDE_ROUTINE_BEARER_TOKEN` | Worker runtime — uploaded to Cloudflare before deploy |

The deploy workflow (`environment: staging`) builds a temporary JSON file from the
three runtime secrets and runs `wrangler secret bulk` to upload them to the Worker,
then deploys. The temp file is created with `0600` permissions and deleted via a
shell trap even if the upload fails. There is intentionally **no `ANTHROPIC_API_KEY`**
(and no `LINEAR_API_KEY`) — this Worker only calls the Claude Code Routine fire endpoint.

Used **locally only** by the register script (never at runtime, never on Cloudflare):
`DISCORD_BOT_TOKEN`, `DISCORD_APPLICATION_ID`, `DISCORD_GUILD_ID`.

## Setup

All commands run from the monorepo root unless noted. This app uses **pnpm**.

### 1. Install

```bash
pnpm install
```

### 2. Create and configure the Claude Code Routine

At [claude.ai/code/routines](https://claude.ai/code/routines): create a routine with
your repo + the Linear connector, write a prompt that creates a Linear issue from the
incoming `text`, add an **API** trigger, then copy the fire **URL** and **Generate
token**. These become `CLAUDE_ROUTINE_FIRE_URL` and `CLAUDE_ROUTINE_BEARER_TOKEN`.

### 3. Configure the `staging` GitHub Environment

In the GitHub repo: **Settings → Environments → New environment → `staging`**, then add
these five **environment secrets** (exact names):

```text
CLOUDFLARE_API_TOKEN
CLOUDFLARE_ACCOUNT_ID
DISCORD_PUBLIC_KEY
CLAUDE_ROUTINE_FIRE_URL
CLAUDE_ROUTINE_BEARER_TOKEN
```

`DISCORD_PUBLIC_KEY` is the application **Public Key** from the Discord Developer
Portal (General Information). `CLAUDE_ROUTINE_FIRE_URL` / `CLAUDE_ROUTINE_BEARER_TOKEN`
come from the routine's API trigger (step 2).

### 4. Deploy (via GitHub Actions)

Push to `main` (changing anything under `apps/boom-discord-bot/**`) or run the
**Deploy Discord Bot** workflow via *workflow_dispatch*. The workflow uploads the
runtime secrets to Cloudflare (`wrangler secret bulk`) and then deploys — no manual
`wrangler secret put`. Copy the Worker URL from the deploy logs (or the Cloudflare
dashboard).

> First-ever deploy: `wrangler secret bulk` attaches to an existing Worker. If the
> Worker has never been created, the secret-upload step can fail with "Worker not
> found". One-time fix: run `pnpm wrangler deploy` once locally (`wrangler login`
> first), or temporarily switch the deploy step to
> `wrangler deploy --secrets-file <file>` (creates the Worker and sets secrets
> atomically). After the Worker exists, the normal flow works unattended.

### 5. Set the Discord Interactions Endpoint URL

Discord Developer Portal → your app → **General Information** → **Interactions
Endpoint URL** = the Worker URL. Save. Discord sends a `PING`; the endpoint must
answer or the URL will not save.

### 6. Register the slash command

```bash
export DISCORD_BOT_TOKEN="..."
export DISCORD_APPLICATION_ID="..."
export DISCORD_GUILD_ID="..."
pnpm --filter boom-discord-bot register:commands
```

### 7. Try it

In the guild:

```
/boom-linear text:"Validate CubeMX pinout against the schematic"
```

You should see a deferred ack, then a public follow-up with the session URL; the
Linear issue appears once the routine finishes.

## Local development

```bash
cd apps/boom-discord-bot
cp .dev.vars.example .dev.vars   # fill in the three secrets
pnpm wrangler dev
```

## Test & typecheck

```bash
pnpm --filter boom-discord-bot typecheck
pnpm --filter boom-discord-bot test
# or via go-task from the root:
task bot:typecheck
task bot:test
```

## Troubleshooting

- **Discord won't save the endpoint URL** — the `PING`/signature check failed. Verify
  `DISCORD_PUBLIC_KEY` matches the app's Public Key and is set as a Worker secret.
- **`401 Bad request signature`** — wrong public key, or a proxy altered the body.
- **Follow-up says "authentication failed" / "routine not found"** — the fire URL or
  token is wrong, or the routine is paused. Regenerate the token in the routines UI.
- **"rate limited"** — daily routine run allowance hit; see usage at
  [claude.ai/code/routines](https://claude.ai/code/routines).
- **Command not visible** — re-run the register script; confirm `DISCORD_GUILD_ID`.
