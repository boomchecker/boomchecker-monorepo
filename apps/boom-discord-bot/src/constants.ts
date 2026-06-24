// Discord slash command name and its single text option.
export const COMMAND_NAME = "boom-linear";
export const TEXT_OPTION = "text";

// Claude Code Routine fire endpoint is in research preview and gated behind a
// dated beta header. Bump this when migrating to a newer dated version.
export const ANTHROPIC_BETA = "experimental-cc-routine-2026-04-01";
export const ANTHROPIC_VERSION = "2023-06-01";

// Max length of the freeform `text` field accepted by the fire endpoint.
export const MAX_TEXT_LEN = 65536;

// Discord REST base.
export const DISCORD_API = "https://discord.com/api/v10";

// Marker prefixing a user-turn echo the bot posts into the thread. Used to rebuild
// the transcript from bot-owned messages (so we never need the MESSAGE_CONTENT intent).
export const ECHO_PREFIX = "📥";

// Label prepended to Claude's results (posted by the routine via webhook) when
// assembling the transcript.
export const CLAUDE_LABEL = "🤖 Claude:";

// How many recent thread messages to pull when rebuilding context.
export const TRANSCRIPT_FETCH_LIMIT = 100;

// Discord thread name limit, and how much transcript context to keep (leaves room
// under MAX_TEXT_LEN for the new request and framing).
export const MAX_THREAD_NAME = 100;
export const MAX_CONTEXT_CHARS = 50000;
