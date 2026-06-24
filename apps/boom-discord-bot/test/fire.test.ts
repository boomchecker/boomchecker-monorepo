import { describe, it, expect, vi, afterEach } from "vitest";
import { fireRoutine, SafeError } from "../src/routine/fire";
import { ANTHROPIC_BETA } from "../src/constants";
import type { Env } from "../src/env";

const env: Env = {
  DISCORD_PUBLIC_KEY: "pk",
  CLAUDE_ROUTINE_FIRE_URL: "https://api.anthropic.com/v1/claude_code/routines/trig_x/fire",
  CLAUDE_ROUTINE_BEARER_TOKEN: "sk-ant-oat01-secret",
};

afterEach(() => {
  vi.restoreAllMocks();
});

describe("fireRoutine", () => {
  it("returns the session URL and sends the documented headers/body", async () => {
    const fetchMock = vi.fn(async (_url: string, init: RequestInit) => {
      const headers = init.headers as Record<string, string>;
      expect(headers.Authorization).toBe(`Bearer ${env.CLAUDE_ROUTINE_BEARER_TOKEN}`);
      expect(headers["anthropic-beta"]).toBe(ANTHROPIC_BETA);
      expect(JSON.parse(init.body as string)).toEqual({ text: "do a thing" });
      return new Response(
        JSON.stringify({
          type: "routine_fire",
          claude_code_session_id: "session_1",
          claude_code_session_url: "https://claude.ai/code/session_1",
        }),
        { status: 200 },
      );
    });
    vi.stubGlobal("fetch", fetchMock);

    const url = await fireRoutine(env, "do a thing");
    expect(url).toBe("https://claude.ai/code/session_1");
    expect(fetchMock).toHaveBeenCalledOnce();
  });

  it("throws a safe error on 401 (no token leak) and does not retry", async () => {
    const fetchMock = vi.fn(async () => new Response("nope", { status: 401 }));
    vi.stubGlobal("fetch", fetchMock);

    await expect(fireRoutine(env, "x")).rejects.toBeInstanceOf(SafeError);
    expect(fetchMock).toHaveBeenCalledOnce();
  });

  it("maps 429 to a rate-limit message", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response("", { status: 429 })));
    await expect(fireRoutine(env, "x")).rejects.toThrow("rate limited, try again later");
  });

  it("throws a safe error when the network call fails", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => {
        throw new Error("ECONNRESET with secret token sk-ant-oat01-secret");
      }),
    );
    await expect(fireRoutine(env, "x")).rejects.toThrow("could not reach Claude Code");
  });
});
