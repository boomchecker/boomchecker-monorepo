import type { Env } from "./env";
import {
  COMMAND_NAME,
  TEXT_OPTION,
  MAX_TEXT_LEN,
  CALLBACK_PATH,
  CLAUDE_PREFIX,
  MAX_DISCORD_MESSAGE,
} from "./constants";
import { SafeError } from "./errors";
import { verifyDiscordRequest } from "./discord/verify";
import {
  InteractionType,
  pong,
  deferredPublic,
  ephemeralMessage,
  editOriginalResponse,
} from "./discord/interactions";
import {
  buildThreadRoutineText,
  buildThreadName,
  getOptionValue,
  getInvokerUsername,
  isThreadChannel,
  type Interaction,
} from "./discord/payload";
import { createThread, postUserTurn, postToThread, fetchTranscript } from "./discord/rest";
import { fireRoutine } from "./routine/fire";

export default {
  async fetch(request: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    if (request.method !== "POST") {
      return new Response("Method Not Allowed", { status: 405 });
    }

    const url = new URL(request.url);

    // Callback from the routine: deliver its result into the Discord thread.
    if (url.pathname === CALLBACK_PATH) {
      return handleCallback(request, env);
    }

    // Otherwise this is a Discord interaction. Verify the Ed25519 signature first.
    const signature = request.headers.get("x-signature-ed25519");
    const timestamp = request.headers.get("x-signature-timestamp");
    const rawBody = await request.text();

    const isValid = await verifyDiscordRequest(
      rawBody,
      signature,
      timestamp,
      env.DISCORD_PUBLIC_KEY,
    );
    if (!isValid) {
      return new Response("Bad request signature", { status: 401 });
    }

    let interaction: Interaction;
    try {
      interaction = JSON.parse(rawBody) as Interaction;
    } catch {
      return new Response("Invalid JSON", { status: 400 });
    }

    if (interaction.type === InteractionType.PING) {
      return pong();
    }

    if (interaction.type === InteractionType.APPLICATION_COMMAND) {
      if (interaction.data?.name !== COMMAND_NAME) {
        return ephemeralMessage("Unknown command.");
      }

      const userText = (getOptionValue(interaction, TEXT_OPTION) ?? "").trim();
      if (!userText) {
        return ephemeralMessage("Please provide a task description.");
      }

      // Ack within Discord's 3s window, then do the slow work in the background.
      ctx.waitUntil(handleCommand(env, interaction, userText, url.origin));
      return deferredPublic();
    }

    return new Response("Unsupported interaction type", { status: 400 });
  },
} satisfies ExportedHandler<Env>;

// Constant-time comparison for the callback bearer token.
function timingSafeEqual(a: string, b: string): boolean {
  if (a.length !== b.length) {
    return false;
  }
  let mismatch = 0;
  for (let i = 0; i < a.length; i++) {
    mismatch |= a.charCodeAt(i) ^ b.charCodeAt(i);
  }
  return mismatch === 0;
}

// Routine -> Worker callback: authenticate the narrow callback token, then post the
// result into the thread using the bot token. The routine never holds the bot token.
async function handleCallback(request: Request, env: Env): Promise<Response> {
  const auth = request.headers.get("authorization") ?? "";
  if (
    !env.ROUTINE_CALLBACK_TOKEN ||
    !timingSafeEqual(auth, `Bearer ${env.ROUTINE_CALLBACK_TOKEN}`)
  ) {
    return new Response("Unauthorized", { status: 401 });
  }

  let body: { thread_id?: string; content?: string };
  try {
    body = (await request.json()) as { thread_id?: string; content?: string };
  } catch {
    return new Response("Invalid JSON", { status: 400 });
  }
  if (!body.thread_id || !body.content) {
    return new Response("Missing thread_id or content", { status: 400 });
  }

  try {
    await postToThread(
      env,
      body.thread_id,
      `${CLAUDE_PREFIX}\n${body.content}`.slice(0, MAX_DISCORD_MESSAGE),
    );
  } catch {
    return new Response("Failed to post to Discord", { status: 502 });
  }
  return new Response("ok", { status: 200 });
}

// Resolve the thread (create one if in a normal channel), fire the routine with the
// thread transcript as context plus a callback URL/token, and edit the deferred
// message. The routine delivers its result by calling back to CALLBACK_PATH.
async function handleCommand(
  env: Env,
  interaction: Interaction,
  userText: string,
  origin: string,
): Promise<void> {
  const username = getInvokerUsername(interaction);
  const channelId = interaction.channel_id ?? interaction.channel?.id;
  const channelType = interaction.channel?.type;

  let content: string;
  try {
    if (!channelId) {
      throw new SafeError("missing channel context");
    }

    let threadId: string;
    let createdThread = false;
    if (isThreadChannel(channelType)) {
      threadId = channelId;
    } else {
      threadId = await createThread(env, channelId, buildThreadName(userText));
      createdThread = true;
    }

    // Read prior context, then echo the new request so it persists for next time.
    const transcript = await fetchTranscript(env, threadId);
    await postUserTurn(env, threadId, username, userText);

    const base = (env.PUBLIC_WORKER_BASE_URL || origin).replace(/\/+$/, "");
    const callbackUrl = `${base}${CALLBACK_PATH}`;

    const routineText = buildThreadRoutineText({
      userText,
      transcript,
      threadId,
      callbackUrl,
      callbackToken: env.ROUTINE_CALLBACK_TOKEN,
    }).slice(0, MAX_TEXT_LEN);
    const sessionUrl = await fireRoutine(env, routineText);

    content = createdThread
      ? `Opened thread <#${threadId}> — the result will appear there. (run: ${sessionUrl})`
      : `Working on it — the result will appear in this thread. (run: ${sessionUrl})`;
  } catch (error) {
    const reason = error instanceof SafeError ? error.message : "unexpected error";
    content = `Failed to start the task. Reason: ${reason}`;
  }

  try {
    await editOriginalResponse(interaction.application_id, interaction.token, content);
  } catch {
    // The follow-up edit failed; nothing safe to do. Never log secrets/tokens.
  }
}
