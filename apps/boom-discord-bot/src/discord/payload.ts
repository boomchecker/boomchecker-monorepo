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
// as context, then a header carrying the thread id and the callback URL/token, then
// the new request. The routine's prompt uses CALLBACK_URL + CALLBACK_TOKEN to POST its
// result back to the Worker (which posts it into the thread).
export function buildThreadRoutineText(args: {
  userText: string;
  transcript: string;
  threadId: string;
  callbackUrl: string;
  callbackToken: string;
}): string {
  const { userText, transcript, threadId, callbackUrl, callbackToken } = args;
  const context = transcript
    ? `Conversation so far in this Discord thread:\n\n${transcript}\n\n---\n\n`
    : "";
  return (
    `${context}` +
    `THREAD_ID: ${threadId}\n` +
    `CALLBACK_URL: ${callbackUrl}\n` +
    `CALLBACK_TOKEN: ${callbackToken}\n\n` +
    `USER_REQUEST:\n${userText}`
  );
}
