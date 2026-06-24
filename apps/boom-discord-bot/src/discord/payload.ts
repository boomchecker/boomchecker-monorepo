// Minimal typings for the slices of a Discord interaction we read, plus helpers
// to extract the command text and build the enriched payload for the routine.

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
  channel?: { id: string; name?: string };
  member?: { user?: { username?: string; id?: string } };
  user?: { username?: string; id?: string };
}

export function getOptionValue(interaction: Interaction, name: string): string | undefined {
  return interaction.data?.options?.find((option) => option.name === name)?.value;
}

function getInvokerUsername(interaction: Interaction): string {
  // Guild interactions carry `member.user`; DMs carry `user`.
  return interaction.member?.user?.username ?? interaction.user?.username ?? "unknown user";
}

// Build the freeform text sent to the routine. Adds light Discord context so the
// routine can attribute who asked and from where. Contains no secrets.
export function buildRoutineText(interaction: Interaction, userText: string): string {
  const username = getInvokerUsername(interaction);
  const channel = interaction.channel?.name
    ? `#${interaction.channel.name}`
    : interaction.channel_id
      ? `channel ${interaction.channel_id}`
      : "an unknown channel";
  const guild = interaction.guild_id ? ` (guild ${interaction.guild_id})` : "";
  return `Requested by ${username} in ${channel}${guild} via Discord:\n\n${userText}`;
}
