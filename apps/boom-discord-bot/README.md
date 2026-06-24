# boom-discord-bot

A Discord slash-command **transport** running on Cloudflare Workers. It does not
create Linear issues itself — it forwards your request to a **Claude Code Routine**,
which owns the repo context, MCP connectors, and the Linear integration. Each task
lives in its own Discord **thread**, and re-running the command inside a thread
replays the whole thread as context.

```
/boom-linear text:"..."        ─▶ Worker: verify, defer, (create/find thread),
                                   read transcript, fire routine with a callback
Claude Code Routine /fire      ─▶ runs in the cloud, creates the Linear issue
routine ─▶ POST /routine-callback (bearer CALLBACK_TOKEN)
Worker  ─▶ posts the result into the thread with the bot token
```

## Behaviour

- **In a normal channel:** the Worker opens a **thread**, echoes your request into it,
  and fires the routine. The result is posted back into that thread.
- **Inside an existing thread:** the Worker reads the **whole thread transcript** (prior
  requests + Claude's prior results), sends it to the routine as context with your new
  request, and the answer lands in the same thread.

Context = the thread transcript, replayed each turn. The fire endpoint creates a **new
session every call** (no session-resume), so "memory" is the thread itself.

The Worker makes **no** Anthropic Messages API call, **no** Linear API call, and **no**
MCP call. `DISCORD_BOT_TOKEN` is used only for thread management + posting results.

## How Claude's result gets back (callback, not MCP/webhook)

The fire endpoint is asynchronous — it returns a session URL immediately, before the
work is done, and the Worker can't read the result back. So the **routine calls the
Worker back** when finished:

1. The Worker passes `THREAD_ID`, `CALLBACK_URL`, and a narrow `CALLBACK_TOKEN` in the
   fire payload.
2. The routine `POST`s `{thread_id, content}` to `CALLBACK_URL` with
   `Authorization: Bearer <CALLBACK_TOKEN>`.
3. The Worker verifies the token and posts the message into the thread using the bot
   token.

The routine never holds the Discord bot token and needs no Discord MCP or per-channel
webhook — just a one-purpose callback token. This works for any thread/channel where the
bot has permissions.

## Why Interactions Endpoint URL (not the Gateway)

Discord delivers slash commands to a public HTTPS **Interactions Endpoint URL** as
signed POST requests — perfect for a serverless Worker. We do **not** use the Gateway
or the `MESSAGE_CONTENT` privileged intent: the trigger is always `/boom-linear`, and
the transcript is rebuilt only from messages the bot owns (echoed requests + posted
results), readable via REST with normal channel permissions.

## Secrets

> All deployment and runtime secrets are stored in GitHub Environment `staging`.
> GitHub Actions uploads runtime secrets to Cloudflare during deployment.
> Manual `wrangler secret put` is not required for normal deployment.

Stored in the **`staging` GitHub Environment** (Settings → Environments → `staging`):

| Name | Role |
| --- | --- |
| `CLOUDFLARE_API_TOKEN` | Deploy-only — GitHub Actions auth to Cloudflare |
| `CLOUDFLARE_ACCOUNT_ID` | Deploy-only — Cloudflare account |
| `DISCORD_PUBLIC_KEY` | Runtime — verify request signatures |
| `CLAUDE_ROUTINE_FIRE_URL` | Runtime — routine fire endpoint |
| `CLAUDE_ROUTINE_BEARER_TOKEN` | Runtime — routine bearer token |
| `DISCORD_BOT_TOKEN` | Runtime — thread create/read/echo + post results |
| `ROUTINE_CALLBACK_TOKEN` | Runtime — shared secret the routine presents to `/routine-callback` |
| `PUBLIC_WORKER_BASE_URL` | Runtime — *optional*; callback base URL (defaults to the request origin if empty) |

The deploy workflow (`environment: staging`) builds a `0600` temp JSON from the runtime
secrets, runs `wrangler secret bulk`, then deploys. There is intentionally **no
`ANTHROPIC_API_KEY`** and **no `LINEAR_API_KEY`** — this Worker only calls the routine
fire endpoint. `ROUTINE_CALLBACK_TOKEN` is **not** set on the routine; the routine
receives it at runtime in the fire payload.

Used **locally** by the register script (not at runtime): `DISCORD_APPLICATION_ID`,
`DISCORD_GUILD_ID` (plain IDs) + `DISCORD_BOT_TOKEN`.

## Setup

All commands run from the monorepo root unless noted. This app uses **pnpm**.

### 1. Install

```bash
pnpm install
```

### 2. Create and configure the Claude Code Routine

At [claude.ai/code/routines](https://claude.ai/code/routines): create a routine with
your repo + the Linear connector, add an **API** trigger, copy the fire **URL** +
**Generate token** → `CLAUDE_ROUTINE_FIRE_URL` / `CLAUDE_ROUTINE_BEARER_TOKEN`.

- In the routine's environment, **allow network to your Worker host** (e.g.
  `boom-discord-bot.<sub>.workers.dev`) — the default "Trusted" policy blocks it, so the
  callback `POST` would fail.
- Routine **prompt** must call the Worker back (do **not** use Discord MCP). The request
  text contains `THREAD_ID`, `CALLBACK_URL`, `CALLBACK_TOKEN`, `USER_REQUEST`:
  > After creating/updating the Linear issue, post the result back. Use the literal
  > `CALLBACK_URL`, `CALLBACK_TOKEN`, and `THREAD_ID` values from the request:
  > ```
  > curl -sS -X POST "<CALLBACK_URL>" \
  >   -H "Authorization: Bearer <CALLBACK_TOKEN>" \
  >   -H "Content-Type: application/json" \
  >   -d "$(jq -nc --arg t "<THREAD_ID>" --arg c "Done. Created <issue title and URL>" \
  >        '{thread_id:$t, content:$c}')"
  > ```
  > Keep content under 2000 chars. Do not use Discord MCP; do not post to Discord directly.

### 3. Invite the bot to your server with thread permissions

Developer Portal → **OAuth2 → URL Generator** → scopes `bot` + `applications.commands`,
permissions: **View Channels, Create Public Threads, Send Messages in Threads, Read
Message History**. Open the URL and add the bot to your server.

### 4. Configure the `staging` GitHub Environment

**Settings → Environments → `staging`**, add these **environment secrets**:

```text
CLOUDFLARE_API_TOKEN
CLOUDFLARE_ACCOUNT_ID
DISCORD_PUBLIC_KEY
CLAUDE_ROUTINE_FIRE_URL
CLAUDE_ROUTINE_BEARER_TOKEN
DISCORD_BOT_TOKEN
ROUTINE_CALLBACK_TOKEN      # generate: openssl rand -hex 32
PUBLIC_WORKER_BASE_URL      # optional; the Worker URL, or leave empty
```

`DISCORD_PUBLIC_KEY` = app **Public Key**; `DISCORD_BOT_TOKEN` = **Bot → Reset Token**.

### 5. Deploy (via GitHub Actions)

Push to `main` (touching `apps/boom-discord-bot/**`) or run **Deploy Discord Bot** via
*workflow_dispatch*. It uploads runtime secrets (`wrangler secret bulk`) then deploys —
no manual `wrangler secret put`. Copy the Worker URL from the logs.

> First-ever deploy: `wrangler secret bulk` attaches to an existing Worker. If it has
> never been created, the upload step can fail with "Worker not found". One-time fix:
> `pnpm wrangler deploy` once locally (`wrangler login` first), or temporarily use
> `wrangler deploy --secrets-file <file>`.

### 6. Set the Discord Interactions Endpoint URL

Developer Portal → app → **General Information → Interactions Endpoint URL** = the Worker
URL. Save (Discord PINGs it; needs a correct `DISCORD_PUBLIC_KEY`).

### 7. Register the slash command

```bash
export DISCORD_BOT_TOKEN="..."
export DISCORD_APPLICATION_ID="..."
export DISCORD_GUILD_ID="..."
pnpm --filter boom-discord-bot register:commands
```

### 8. Try it

```
/boom-linear text:"Validate CubeMX pinout against the schematic"
```

A thread opens; the result lands in it. Re-run `/boom-linear` **inside that thread** with
a follow-up — the whole thread is sent as context.

## Local development

```bash
cd apps/boom-discord-bot
cp .dev.vars.example .dev.vars   # fill in the runtime secrets
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
  Threads / View Channels** (step 3).
- **Thread opens but no result appears** — the routine's callback failed: check the
  routine prompt does the `POST` (not Discord MCP), that the routine environment allows
  your Worker host, and that `CALLBACK_TOKEN` from the request matches
  `ROUTINE_CALLBACK_TOKEN`.
- **Callback `401`** — `ROUTINE_CALLBACK_TOKEN` mismatch between the Worker and the value
  the routine sent.
- **Follow-up "authentication failed" / "routine not found"** — wrong fire URL/token or
  the routine is paused.
- **"rate limited"** — daily routine run allowance hit (see claude.ai/code/routines).
- **Command not visible** — re-run the register script; confirm `DISCORD_GUILD_ID`.
