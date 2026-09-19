---
"boom-discord-bot": minor
---

Add boom-discord-bot: a Cloudflare Worker that turns the Discord `/boom-linear`
slash command into a Claude Code Routine fire call. The routine (configured at
claude.ai/code/routines) owns the Linear connector and creates the issue; the
Worker only verifies the Discord signature, defers, fires the routine, and posts a
follow-up with the session URL.
