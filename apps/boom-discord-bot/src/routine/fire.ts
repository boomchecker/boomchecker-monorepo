import type { Env } from "../env";
import { ANTHROPIC_BETA, ANTHROPIC_VERSION } from "../constants";
import { SafeError } from "../errors";

interface RoutineFireResponse {
  type: string;
  claude_code_session_id: string;
  claude_code_session_url: string;
}

// Fire the Claude Code Routine and return the created session URL.
//
// The endpoint is fire-and-forget (returns as soon as the session is created) and
// has NO idempotency key, so this never retries — a retry would create a duplicate
// session. On any failure it throws a SafeError with a short, secret-free message.
//
// Contract: https://platform.claude.com/docs/en/api/claude-code/routines-fire
export async function fireRoutine(env: Env, text: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(env.CLAUDE_ROUTINE_FIRE_URL, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${env.CLAUDE_ROUTINE_BEARER_TOKEN}`,
        "anthropic-beta": ANTHROPIC_BETA,
        "anthropic-version": ANTHROPIC_VERSION,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ text }),
    });
  } catch {
    throw new SafeError("could not reach Claude Code");
  }

  if (!response.ok) {
    throw new SafeError(mapStatusToMessage(response.status));
  }

  let body: RoutineFireResponse;
  try {
    body = (await response.json()) as RoutineFireResponse;
  } catch {
    throw new SafeError("unexpected response from Claude Code");
  }

  if (!body.claude_code_session_url) {
    throw new SafeError("Claude Code did not return a session URL");
  }
  return body.claude_code_session_url;
}

function mapStatusToMessage(status: number): string {
  switch (status) {
    case 400:
      return "invalid request (the routine may be paused)";
    case 401:
      return "authentication failed";
    case 403:
      return "routine access denied";
    case 404:
      return "routine not found";
    case 429:
      return "rate limited, try again later";
    default:
      return status >= 500 ? "Claude Code is temporarily unavailable" : "request failed";
  }
}
