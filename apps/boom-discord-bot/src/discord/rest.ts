// Discord REST helpers that act as the bot (need DISCORD_BOT_TOKEN). Used only for
// thread management: create a thread, echo the user's input, post results, and
// rebuild the thread transcript. No AI/Linear calls happen here.
import type { Env } from "../env";
import { SafeError } from "../errors";
import {
  DISCORD_API,
  ECHO_PREFIX,
  CLAUDE_PREFIX,
  TRANSCRIPT_FETCH_LIMIT,
  MAX_THREAD_NAME,
  MAX_CONTEXT_CHARS,
  MAX_DISCORD_MESSAGE,
} from "../constants";
import { ChannelType } from "./payload";

function botHeaders(env: Env): Record<string, string> {
  return {
    Authorization: `Bot ${env.DISCORD_BOT_TOKEN}`,
    "Content-Type": "application/json",
  };
}

// Create a public thread (not attached to a message) in the given channel.
export async function createThread(env: Env, channelId: string, name: string): Promise<string> {
  const response = await fetch(`${DISCORD_API}/channels/${channelId}/threads`, {
    method: "POST",
    headers: botHeaders(env),
    body: JSON.stringify({
      name: name.slice(0, MAX_THREAD_NAME) || "boom-linear task",
      type: ChannelType.PUBLIC_THREAD,
      auto_archive_duration: 1440,
    }),
  });
  if (!response.ok) {
    throw new SafeError("could not create thread (check bot permissions)");
  }
  const data = (await response.json()) as { id: string };
  return data.id;
}

// Post a message into a thread/channel as the bot.
export async function postToThread(env: Env, channelId: string, content: string): Promise<void> {
  const response = await fetch(`${DISCORD_API}/channels/${channelId}/messages`, {
    method: "POST",
    headers: botHeaders(env),
    body: JSON.stringify({ content: content.slice(0, MAX_DISCORD_MESSAGE) }),
  });
  if (!response.ok) {
    throw new SafeError(`could not post to thread (HTTP ${response.status})`);
  }
}

// Echo the user's request into the thread so it becomes part of the transcript for
// later turns. Best-effort: a failure here must not abort the command.
export async function postUserTurn(
  env: Env,
  threadId: string,
  username: string,
  text: string,
): Promise<void> {
  try {
    await postToThread(env, threadId, `${ECHO_PREFIX} ${username}: ${text}`);
  } catch {
    // Ignore — echo is non-critical.
  }
}

interface DiscordMessage {
  content?: string;
}

// Rebuild a chronological transcript from messages the bot owns: user-turn echoes
// (start with ECHO_PREFIX) and Claude results (start with CLAUDE_PREFIX, posted by the
// Worker via the routine callback). Everything else (plain user chatter, the deferred
// ack) is ignored, so the MESSAGE_CONTENT privileged intent is not needed.
// Best-effort: returns "" rather than failing the command.
export async function fetchTranscript(env: Env, threadId: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(
      `${DISCORD_API}/channels/${threadId}/messages?limit=${TRANSCRIPT_FETCH_LIMIT}`,
      { headers: botHeaders(env) },
    );
  } catch {
    return "";
  }
  if (!response.ok) {
    return "";
  }

  const messages = (await response.json()) as DiscordMessage[];
  const lines: string[] = [];
  // Discord returns newest-first; reverse to chronological order.
  for (const message of messages.reverse()) {
    const content = message.content ?? "";
    if (content.startsWith(ECHO_PREFIX) || content.startsWith(CLAUDE_PREFIX)) {
      lines.push(content);
    }
  }

  const transcript = lines.join("\n\n");
  return transcript.length > MAX_CONTEXT_CHARS ? transcript.slice(-MAX_CONTEXT_CHARS) : transcript;
}
