# boom-discord-bot

A Discord slash-command **transport** running on Cloudflare Workers. It does not
create Linear issues itself — it forwards your request to a **Claude Code Routine**,
which owns the repo context, MCP connectors, and the Linear integration. Each task
lives in its own Discord **thread**, and re-running the command inside a thread
replays the whole thread as context.

```
/boom-linear text:"..."         ─▶ Worker: verify, defer, (create/find thread),
                                    read thread transcript, fire routine
Claude Code Routine /fire       ─▶ runs in the cloud, creates the Linear issue
routine ─▶ Discord webhook      ─▶ posts the result back into the thread
```

## Behaviour

- **In a normal channel:** the Worker opens a **thread**, echoes your request into
  it, and fires the routine. The routine posts its result back into that thread.
- **Inside an existing thread:** the Worker reads the **whole thread transcript**
  (the prior requests + Claude's prior results), sends it to the routine as context
  along with your new request, and the routine replies in the same thread.

Context = the thread transcript, replayed on each turn. The fire endpoint creates a
**new session every call** (it has no session-resume), so "memory" is the thread
itself, not a live Claude session.

The Worker contains **no** Anthropic Messages API call, **no** Linear API call, and
**no** MCP logic. `DISCORD_BOT_TOKEN` is used only for thread management (create a
thread, read the transcript, echo your input).

## Why Interactions Endpoint URL (not the Gateway)

Discord delivers slash commands to a public HTTPS **Interactions Endpoint URL** as
signed POST requests — perfect for a serverless Worker. We do **not** use the Gateway
and do **not** need the `MESSAGE_CONTENT` privileged intent: the trigger is always the
`/boom-linear` command, and the transcript is rebuilt only from messages the bot owns
(your echoed requests + the webhook results), which are readable via REST with normal
channel permissions. We do not use Webhook Events; the Worker URL must be set as the
**Interactions Endpoint URL** or Discord won't send commands or the verification PING.

## How Claude's result gets back into the thread

The fire endpoint is asynchronous — it returns a session URL immediately, before the
work is done. So the **routine** posts the final result into the thread itself, via a
Discord channel webhook, using the `THREAD_ID` the Worker passes in the request text:

```
POST $DISCORD_WEBHOOK_URL?thread_id=<THREAD_ID>
{ "content": "<short result + Linear issue URL>" }
```

The Worker's deferred ack just points at the thread / says "working"; the real answer
arrives as a follow-up message when the routine finishes.

## Secrets

> All deployment and runtime secrets are stored in GitHub Environment `staging`.
> GitHub Actions uploads runtime secrets to Cloudflare during deployment.
> Manual `wrangler secret put` is not required for normal deployment.

Stored in the **`staging` GitHub Environment** (Settings → Environments → `staging`):

| Name | Role |
| --- | --- |
| `CLOUDFLARE_API_TOKEN` | Deploy-only — GitHub Actions auth to Cloudflare |
| `CLOUDFLARE_ACCOUNT_ID` | Deploy-only — Cloudflare account |
| `DISCORD_PUBLIC_KEY` | Worker runtime — verify request signatures |
| `CLAUDE_ROUTINE_FIRE_URL` | Worker runtime — routine fire endpoint |
| `CLAUDE_ROUTINE_BEARER_TOKEN` | Worker runtime — routine bearer token |
| `DISCORD_BOT_TOKEN` | Worker runtime — thread create/read/echo (also used locally by the register script) |

The deploy workflow (`environment: staging`) builds a `0600` temp JSON from the four
runtime secrets and runs `wrangler secret bulk` to upload them, then deploys. There is
intentionally **no `ANTHROPIC_API_KEY`** and **no `LINEAR_API_KEY`** — this Worker only
calls the Claude Code Routine fire endpoint.

Configured **on the routine** (claude.ai, not the Worker): `DISCORD_WEBHOOK_URL` — the
channel webhook the routine posts results to. Used **locally** by the register script:
`DISCORD_APPLICATION_ID`, `DISCORD_GUILD_ID` (plain IDs, not secret) + `DISCORD_BOT_TOKEN`.

## Setup

All commands run from the monorepo root unless noted. This app uses **pnpm**.

### 1. Install

```bash
pnpm install
```

### 2. Create a Discord channel webhook

Discord: target channel → **Edit Channel → Integrations → Webhooks → New Webhook** →
copy the URL. This is `DISCORD_WEBHOOK_URL` for the routine.

### 3. Create and configure the Claude Code Routine

At [claude.ai/code/routines](https://claude.ai/code/routines): create a routine with
your repo + the Linear connector, add an **API** trigger, and copy the fire **URL** +
**Generate token** → `CLAUDE_ROUTINE_FIRE_URL` / `CLAUDE_ROUTINE_BEARER_TOKEN`.

- Set the routine **environment variable** `DISCORD_WEBHOOK_URL` to the webhook from
  step 2.
- In the routine's environment, **allow network to `discord.com`** (the default
  "Trusted" policy blocks it — add it to Allowed domains, or use Full).
- Routine **prompt** must include something like:
  > The request includes a line `THREAD_ID: <id>`. Create the Linear issue, then post a
  > concise summary plus the issue URL back to Discord by sending `POST
  > $DISCORD_WEBHOOK_URL?thread_id=<id>` with JSON body `{"content":"..."}` (≤2000 chars).

### 4. Invite the bot to your server with thread permissions

Developer Portal → **OAuth2 → URL Generator** → scopes `bot` + `applications.commands`,
bot permissions: **View Channels, Create Public Threads, Send Messages in Threads, Read
Message History**. Open the generated URL and add the bot to your server.

### 5. Configure the `staging` GitHub Environment

**Settings → Environments → New environment → `staging`**, add these six **environment
secrets** (exact names):

```text
CLOUDFLARE_API_TOKEN
CLOUDFLARE_ACCOUNT_ID
DISCORD_PUBLIC_KEY
CLAUDE_ROUTINE_FIRE_URL
CLAUDE_ROUTINE_BEARER_TOKEN
DISCORD_BOT_TOKEN
```

`DISCORD_PUBLIC_KEY` = app **Public Key** (General Information). `DISCORD_BOT_TOKEN` =
**Bot → Reset Token**.

### 6. Deploy (via GitHub Actions)

Push to `main` (touching `apps/boom-discord-bot/**`) or run the **Deploy Discord Bot**
workflow via *workflow_dispatch*. It uploads the runtime secrets (`wrangler secret
bulk`) then deploys — no manual `wrangler secret put`. Copy the Worker URL from the
logs.

> First-ever deploy: `wrangler secret bulk` attaches to an existing Worker. If the
> Worker has never been created, the upload step can fail with "Worker not found".
> One-time fix: `pnpm wrangler deploy` once locally (`wrangler login` first), or
> temporarily switch the deploy step to `wrangler deploy --secrets-file <file>`.

### 7. Set the Discord Interactions Endpoint URL

Developer Portal → app → **General Information → Interactions Endpoint URL** = the
Worker URL. Save (Discord PINGs it; needs a correct `DISCORD_PUBLIC_KEY`).

### 8. Register the slash command

```bash
export DISCORD_BOT_TOKEN="..."
export DISCORD_APPLICATION_ID="..."
export DISCORD_GUILD_ID="..."
pnpm --filter boom-discord-bot register:commands
```

### 9. Try it

```
/boom-linear text:"Validate CubeMX pinout against the schematic"
```

A thread opens; the result lands in it. Re-run `/boom-linear` **inside that thread**
with a follow-up — the whole thread is sent as context.

## Local development

```bash
cd apps/boom-discord-bot
cp .dev.vars.example .dev.vars   # fill in the four runtime secrets
pnpm wrangler dev
```

## Test & typecheck

```bash
pnpm --filter boom-discord-bot typecheck
pnpm --filter boom-discord-bot test
# or via go-task from the root: task bot:typecheck / task bot:test
```

## Troubleshooting

- **Discord won't save the endpoint URL** — `PING`/signature failed. Check
  `DISCORD_PUBLIC_KEY` matches the app Public Key and is set on the Worker.
- **`401 Bad request signature`** — wrong public key, or a proxy altered the body.
- **"could not create thread"** — the bot isn't in the server or lacks **Create Public
  Threads / View Channels** (step 4).
- **Thread opens but no result appears** — the routine couldn't post back: check
  `DISCORD_WEBHOOK_URL`, that `discord.com` is allowed in the routine environment, and
  that the prompt uses `?thread_id=<id>`.
- **Follow-up "authentication failed" / "routine not found"** — wrong fire URL/token or
  the routine is paused.
- **"rate limited"** — daily routine run allowance hit (see claude.ai/code/routines).
- **Command not visible** — re-run the register script; confirm `DISCORD_GUILD_ID`.
