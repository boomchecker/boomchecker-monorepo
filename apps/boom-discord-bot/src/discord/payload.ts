// Minimal typings for the slices of a Discord interaction we read, plus helpers
// to extract the command text, detect threads, and build the routine payload.

interface CommandOption {
  name: string;
  type: number;
  value?: string;
}

export interface Interaction {
  type: number;
  id: string;
  token: string;
  application_id: string;
  data?: {
    name: string;
    options?: CommandOption[];
  };
  guild_id?: string;
  channel_id?: string;
  channel?: { id: string; name?: string; type?: number };
  member?: { user?: { username?: string; id?: string } };
  user?: { username?: string; id?: string };
}

// Discord channel types we care about.
export const ChannelType = {
  GUILD_TEXT: 0,
  ANNOUNCEMENT_THREAD: 10,
  PUBLIC_THREAD: 11,
  PRIVATE_THREAD: 12,
} as const;

export function isThreadChannel(type: number | undefined): boolean {
  return (
    type === ChannelType.ANNOUNCEMENT_THREAD ||
    type === ChannelType.PUBLIC_THREAD ||
    type === ChannelType.PRIVATE_THREAD
  );
}

export function getOptionValue(interaction: Interaction, name: string): string | undefined {
  return interaction.data?.options?.find((option) => option.name === name)?.value;
}

export function getInvokerUsername(interaction: Interaction): string {
  // Guild interactions carry `member.user`; DMs carry `user`.
  return interaction.member?.user?.username ?? interaction.user?.username ?? "unknown user";
}

// Build the freeform text sent to the routine: the prior thread transcript (if any)
// as context, then the new request, then a reminder of where to post the result.
// Contains no secrets. The routine's own prompt is responsible for posting the
// result to the Discord webhook using the THREAD_ID below.
export function buildThreadRoutineText(args: {
  username: string;
  userText: string;
  transcript: string;
  threadId: string;
}): string {
  const { username, userText, transcript, threadId } = args;
  const context = transcript
    ? `Conversation so far in this Discord thread:\n\n${transcript}\n\n---\n\n`
    : "";
  return (
    `${context}New request from ${username} via Discord (THREAD_ID: ${threadId}):\n\n${userText}\n\n` +
    `[When finished, post a concise result and the created Linear issue URL back to ` +
    `this Discord thread (THREAD_ID: ${threadId}).]`
  );
}
