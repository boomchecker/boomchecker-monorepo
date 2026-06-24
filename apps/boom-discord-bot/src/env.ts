// Worker runtime secrets — EXACTLY these three, set via `wrangler secret put`.
//
// There is intentionally NO ANTHROPIC_API_KEY here: this Worker only calls the
// Claude Code Routine /fire endpoint (authenticated with a per-routine bearer
// token), never the Anthropic Messages API. Do not add other secrets.
export interface Env {
  DISCORD_PUBLIC_KEY: string;
  CLAUDE_ROUTINE_FIRE_URL: string;
  CLAUDE_ROUTINE_BEARER_TOKEN: string;
}
