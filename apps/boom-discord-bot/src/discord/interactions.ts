// Discord interaction type/response enums and small response builders.
// https://discord.com/developers/docs/interactions/receiving-and-responding

export const InteractionType = {
  PING: 1,
  APPLICATION_COMMAND: 2,
} as const;

export const InteractionResponseType = {
  PONG: 1,
  CHANNEL_MESSAGE_WITH_SOURCE: 4,
  DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE: 5,
} as const;

// Discord message flag: EPHEMERAL (only the invoking user sees the message).
const EPHEMERAL = 64;

const JSON_HEADERS = { "Content-Type": "application/json" };

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: JSON_HEADERS });
}

export function pong(): Response {
  return jsonResponse({ type: InteractionResponseType.PONG });
}

// Public deferred ack: shows "thinking…" to the whole channel, edited later.
export function deferredPublic(): Response {
  return jsonResponse({ type: InteractionResponseType.DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE });
}

// Immediate ephemeral reply, used for user-input errors (unknown command, empty text).
export function ephemeralMessage(content: string): Response {
  return jsonResponse({
    type: InteractionResponseType.CHANNEL_MESSAGE_WITH_SOURCE,
    data: { content, flags: EPHEMERAL },
  });
}

// Edit the original (deferred) interaction response. The interaction token is the
// only auth needed — no bot token. Tokens are valid for 15 minutes.
export async function editOriginalResponse(
  applicationId: string,
  interactionToken: string,
  content: string,
): Promise<void> {
  const url = `https://discord.com/api/v10/webhooks/${applicationId}/${interactionToken}/messages/@original`;
  await fetch(url, {
    method: "PATCH",
    headers: JSON_HEADERS,
    body: JSON.stringify({ content }),
  });
}
