import { describe, it, expect, vi, afterEach } from "vitest";
import { fetchTranscript, createThread } from "../src/discord/rest";
import type { Env } from "../src/env";

const env: Env = {
  DISCORD_PUBLIC_KEY: "pk",
  CLAUDE_ROUTINE_FIRE_URL: "https://api.anthropic.com/v1/claude_code/routines/trig_x/fire",
  CLAUDE_ROUTINE_BEARER_TOKEN: "sk-ant-oat01-secret",
  DISCORD_BOT_TOKEN: "bot-token",
};

afterEach(() => {
  vi.restoreAllMocks();
});

describe("fetchTranscript", () => {
  it("keeps echoes and webhook results in chronological order, drops everything else", async () => {
    // Discord returns newest-first.
    const messages = [
      { content: "plain user message" }, // no prefix, no webhook -> dropped
      { content: "result two", webhook_id: "w1" }, // claude
      { content: "📥 **alice:** second" }, // echo
      { content: "🤖 working…" }, // bot ack (not an echo, no webhook) -> dropped
      { content: "result one", webhook_id: "w1" }, // claude
      { content: "📥 **alice:** first" }, // echo
    ];
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response(JSON.stringify(messages), { status: 200 })),
    );

    const transcript = await fetchTranscript(env, "t1");
    expect(transcript).toBe(
      "📥 **alice:** first\n\n🤖 Claude: result one\n\n📥 **alice:** second\n\n🤖 Claude: result two",
    );
  });

  it("returns empty string (best effort) on a non-ok response", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response("", { status: 403 })),
    );
    expect(await fetchTranscript(env, "t1")).toBe("");
  });
});

describe("createThread", () => {
  it("returns the new thread id and authenticates as the bot", async () => {
    const fetchMock = vi.fn(async (_url: string, init: RequestInit) => {
      const headers = init.headers as Record<string, string>;
      expect(headers.Authorization).toBe("Bot bot-token");
      return new Response(JSON.stringify({ id: "thread9" }), { status: 200 });
    });
    vi.stubGlobal("fetch", fetchMock);
    expect(await createThread(env, "c1", "a task name")).toBe("thread9");
  });

  it("throws a safe error when thread creation fails", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response("", { status: 403 })),
    );
    await expect(createThread(env, "c1", "n")).rejects.toThrow("could not create thread");
  });
});
