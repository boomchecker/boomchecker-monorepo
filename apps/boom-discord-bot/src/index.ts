import type { Env } from "./env";
import { COMMAND_NAME, TEXT_OPTION, MAX_TEXT_LEN } from "./constants";
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
  getOptionValue,
  getInvokerUsername,
  isThreadChannel,
  type Interaction,
} from "./discord/payload";
import { createThread, postUserTurn, fetchTranscript } from "./discord/rest";
import { fireRoutine } from "./routine/fire";

export default {
  async fetch(request: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    if (request.method !== "POST") {
      return new Response("Method Not Allowed", { status: 405 });
    }

    // Verify the Ed25519 signature over (timestamp + rawBody) before trusting anything.
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
      ctx.waitUntil(handleCommand(env, interaction, userText));
      return deferredPublic();
    }

    return new Response("Unsupported interaction type", { status: 400 });
  },
} satisfies ExportedHandler<Env>;

// Resolve the thread to work in (create one if the command was run in a normal
// channel), fire the routine with the thread transcript as context, and edit the
// deferred message. Claude's actual result is posted into the thread later by the
// routine (via its Discord webhook) — the fire endpoint is asynchronous.
async function handleCommand(
  env: Env,
  interaction: Interaction,
  userText: string,
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
      threadId = await createThread(env, channelId, userText);
      createdThread = true;
    }

    // Read prior context, then echo the new request so it persists for next time.
    const transcript = await fetchTranscript(env, threadId);
    await postUserTurn(env, threadId, username, userText);

    const routineText = buildThreadRoutineText({
      username,
      userText,
      transcript,
      threadId,
    }).slice(0, MAX_TEXT_LEN);
    const sessionUrl = await fireRoutine(env, routineText);

    content = createdThread
      ? `🧵 Opened thread <#${threadId}> — the result will appear there. (run: ${sessionUrl})`
      : `🚀 Working — the result will appear in this thread. (run: ${sessionUrl})`;
  } catch (error) {
    const reason = error instanceof SafeError ? error.message : "unexpected error";
    content = `❌ Failed to start the task. Reason: ${reason}`;
  }

  try {
    await editOriginalResponse(interaction.application_id, interaction.token, content);
  } catch {
    // The follow-up edit failed; nothing safe to do. Never log secrets/tokens.
  }
}
