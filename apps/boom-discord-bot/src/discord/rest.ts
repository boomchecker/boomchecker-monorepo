// Discord REST helpers that act as the bot (need DISCORD_BOT_TOKEN). Used only for
// thread management: create a thread, echo the user's input, and rebuild the thread
// transcript. No AI/Linear calls happen here.
import type { Env } from "../env";
import { SafeError } from "../errors";
import {
  DISCORD_API,
  ECHO_PREFIX,
  CLAUDE_LABEL,
  TRANSCRIPT_FETCH_LIMIT,
  MAX_THREAD_NAME,
  MAX_CONTEXT_CHARS,
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

// Echo the user's request into the thread so it becomes part of the transcript for
// later turns. Best-effort: a failure here must not abort the command.
export async function postUserTurn(
  env: Env,
  threadId: string,
  username: string,
  text: string,
): Promise<void> {
  try {
    await fetch(`${DISCORD_API}/channels/${threadId}/messages`, {
      method: "POST",
      headers: botHeaders(env),
      body: JSON.stringify({
        content: `${ECHO_PREFIX} **${username}:** ${text}`.slice(0, 2000),
      }),
    });
  } catch {
    // Ignore — echo is non-critical.
  }
}

interface DiscordMessage {
  content?: string;
  webhook_id?: string;
}

// Rebuild a chronological transcript from messages the bot owns: user-turn echoes
// (start with ECHO_PREFIX) and Claude results (posted by a webhook). Other users'
// plain messages are ignored, so the MESSAGE_CONTENT privileged intent is not needed.
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
    if (content.startsWith(ECHO_PREFIX)) {
      lines.push(content);
    } else if (message.webhook_id) {
      lines.push(`${CLAUDE_LABEL} ${content}`);
    }
  }

  const transcript = lines.join("\n\n");
  return transcript.length > MAX_CONTEXT_CHARS ? transcript.slice(-MAX_CONTEXT_CHARS) : transcript;
}
