// Worker runtime secrets, set via the deploy workflow's `wrangler secret bulk`
// (or `wrangler secret put` locally).
//
// `DISCORD_BOT_TOKEN` is used ONLY for thread management (create a thread, read the
// thread transcript, echo the user's input) — never to call any AI/Linear API.
// There is intentionally NO ANTHROPIC_API_KEY / LINEAR_API_KEY here: this Worker only
// calls the Claude Code Routine /fire endpoint, never the Anthropic Messages API or
// Linear directly. Do not add other secrets.
export interface Env {
  DISCORD_PUBLIC_KEY: string;
  CLAUDE_ROUTINE_FIRE_URL: string;
  CLAUDE_ROUTINE_BEARER_TOKEN: string;
  DISCORD_BOT_TOKEN: string;
}
