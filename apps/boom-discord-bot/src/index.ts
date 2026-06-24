import type { Env } from "./env";
import { COMMAND_NAME, TEXT_OPTION, MAX_TEXT_LEN } from "./constants";
import { verifyDiscordRequest } from "./discord/verify";
import {
  InteractionType,
  pong,
  deferredPublic,
  ephemeralMessage,
  editOriginalResponse,
} from "./discord/interactions";
import { buildRoutineText, getOptionValue, type Interaction } from "./discord/payload";
import { fireRoutine, SafeError } from "./routine/fire";

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

// Fire the routine and edit the deferred message with the result. Runs after the
// deferred ack, so all outcomes are reported via the follow-up edit.
async function handleCommand(
  env: Env,
  interaction: Interaction,
  userText: string,
): Promise<void> {
  let content: string;
  try {
    const text = buildRoutineText(interaction, userText).slice(0, MAX_TEXT_LEN);
    const sessionUrl = await fireRoutine(env, text);
    content = `🚀 Task sent to Claude Code. Watch the run: ${sessionUrl}`;
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
