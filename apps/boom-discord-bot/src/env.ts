// Worker runtime secrets, set via the deploy workflow's `wrangler secret bulk`
// (or `wrangler secret put` locally).
//
// Result delivery uses a callback: the routine never gets the Discord bot token. It
// receives a narrow `ROUTINE_CALLBACK_TOKEN` and calls the Worker's /routine-callback
// endpoint; the Worker then posts to the thread with `DISCORD_BOT_TOKEN`.
//
// There is intentionally NO ANTHROPIC_API_KEY / LINEAR_API_KEY here: this Worker only
// calls the Claude Code Routine /fire endpoint, never the Anthropic Messages API or
// Linear directly. Do not add other secrets.
export interface Env {
  DISCORD_PUBLIC_KEY: string;
  CLAUDE_ROUTINE_FIRE_URL: string;
  CLAUDE_ROUTINE_BEARER_TOKEN: string;
  // Used only for thread management + posting results (never for any AI/Linear call).
  DISCORD_BOT_TOKEN: string;
  // Shared secret the routine must present to the /routine-callback endpoint.
  ROUTINE_CALLBACK_TOKEN: string;
  // Public base URL of this Worker, used to build the callback URL. Optional — if
  // empty, the Worker falls back to the origin of the incoming request.
  PUBLIC_WORKER_BASE_URL: string;
}
