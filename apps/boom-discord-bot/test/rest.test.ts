import { describe, it, expect, vi, afterEach } from "vitest";
import { fetchTranscript, createThread, postToThread } from "../src/discord/rest";
import type { Env } from "../src/env";

const env: Env = {
  DISCORD_PUBLIC_KEY: "pk",
  CLAUDE_ROUTINE_FIRE_URL: "https://api.anthropic.com/v1/claude_code/routines/trig_x/fire",
  CLAUDE_ROUTINE_BEARER_TOKEN: "sk-ant-oat01-secret",
  DISCORD_BOT_TOKEN: "bot-token",
  ROUTINE_CALLBACK_TOKEN: "cb-token",
  PUBLIC_WORKER_BASE_URL: "",
};

afterEach(() => {
  vi.restoreAllMocks();
});

describe("fetchTranscript", () => {
  it("keeps echoes and claude results in chronological order, drops everything else", async () => {
    // Discord returns newest-first.
    const messages = [
      { content: "plain user message" }, // no marker -> dropped
      { content: "**Result**\nresult two" }, // claude
      { content: "**Request** alice: second" }, // echo
      { content: "Working on it — run: x" }, // deferred ack -> dropped
      { content: "**Result**\nresult one" }, // claude
      { content: "**Request** alice: first" }, // echo
    ];
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response(JSON.stringify(messages), { status: 200 })),
    );

    const transcript = await fetchTranscript(env, "t1");
    expect(transcript).toBe(
      "**Request** alice: first\n\n**Result**\nresult one\n\n**Request** alice: second\n\n**Result**\nresult two",
    );
  });

  it("includes question turns in the transcript", async () => {
    const messages = [
      { content: "**Question**\nWho is the assignee?" },
      { content: "**Request** alice: do x" },
    ];
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response(JSON.stringify(messages), { status: 200 })),
    );
    expect(await fetchTranscript(env, "t1")).toBe(
      "**Request** alice: do x\n\n**Question**\nWho is the assignee?",
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
      expect((init.headers as Record<string, string>).Authorization).toBe("Bot bot-token");
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

describe("postToThread", () => {
  it("posts the content as the bot", async () => {
    const fetchMock = vi.fn(async (_url: string, init: RequestInit) => {
      expect((init.headers as Record<string, string>).Authorization).toBe("Bot bot-token");
      expect(JSON.parse(init.body as string)).toEqual({ content: "hello" });
      return new Response("", { status: 200 });
    });
    vi.stubGlobal("fetch", fetchMock);
    await postToThread(env, "t1", "hello");
    expect(fetchMock).toHaveBeenCalledOnce();
  });

  it("throws a safe error on failure", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => new Response("", { status: 403 })),
    );
    await expect(postToThread(env, "t1", "x")).rejects.toThrow("could not post to thread");
  });
});
