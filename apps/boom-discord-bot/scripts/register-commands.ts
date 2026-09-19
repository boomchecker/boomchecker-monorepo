// Registers the /boom-linear guild slash command for fast iteration.
//
// Usage:
//   DISCORD_BOT_TOKEN=... DISCORD_APPLICATION_ID=... DISCORD_GUILD_ID=... \
//     pnpm register:commands
//
// Guild commands update instantly (global commands can take up to an hour). The
// bot token is used ONLY here, never in the Worker runtime.
import { COMMAND_NAME, TEXT_OPTION } from "../src/constants";

function requireEnv(name: string): string {
  const value = process.env[name];
  if (!value) {
    console.error(`Missing required environment variable: ${name}`);
    process.exit(1);
  }
  return value;
}

const token = requireEnv("DISCORD_BOT_TOKEN");
const applicationId = requireEnv("DISCORD_APPLICATION_ID");
const guildId = requireEnv("DISCORD_GUILD_ID");

const command = {
  name: COMMAND_NAME,
  description: "Create a Linear issue from a short task description via Claude Code.",
  type: 1,
  options: [
    {
      name: TEXT_OPTION,
      description: "Task description",
      type: 3,
      required: true,
    },
  ],
};

const url = `https://discord.com/api/v10/applications/${applicationId}/guilds/${guildId}/commands`;
const response = await fetch(url, {
  method: "POST",
  headers: {
    Authorization: `Bot ${token}`,
    "Content-Type": "application/json",
  },
  body: JSON.stringify(command),
});

if (!response.ok) {
  console.error(`Failed to register command: ${response.status} ${response.statusText}`);
  console.error(await response.text());
  process.exit(1);
}

console.log(`Registered /${COMMAND_NAME} on guild ${guildId}.`);
