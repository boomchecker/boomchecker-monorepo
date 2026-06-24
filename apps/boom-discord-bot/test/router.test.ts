import { describe, it, expect, vi, afterEach } from "vitest";
import worker from "../src/index";
import type { Env } from "../src/env";

function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
}

async function setup() {
  const keyPair = (await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
    "sign",
    "verify",
  ])) as CryptoKeyPair;
  const rawPublicKey = (await crypto.subtle.exportKey("raw", keyPair.publicKey)) as ArrayBuffer;
  const env: Env = {
    DISCORD_PUBLIC_KEY: bytesToHex(new Uint8Array(rawPublicKey)),
    CLAUDE_ROUTINE_FIRE_URL: "https://api.anthropic.com/v1/claude_code/routines/trig_x/fire",
    CLAUDE_ROUTINE_BEARER_TOKEN: "sk-ant-oat01-secret",
    DISCORD_BOT_TOKEN: "bot-token",
  };
  return { env, privateKey: keyPair.privateKey };
}

async function signedRequest(privateKey: CryptoKey, payload: unknown): Promise<Request> {
  const body = JSON.stringify(payload);
  const timestamp = "1700000000";
  const signature = await crypto.subtle.sign(
    { name: "Ed25519" },
    privateKey,
    new TextEncoder().encode(timestamp + body),
  );
  return new Request("https://bot.example/", {
    method: "POST",
    headers: {
      "x-signature-ed25519": bytesToHex(new Uint8Array(signature)),
      "x-signature-timestamp": timestamp,
      "Content-Type": "application/json",
    },
    body,
  });
}

const noopCtx = {
  waitUntil: () => {},
  passThroughOnException: () => {},
} as unknown as ExecutionContext;

// Routes the various Discord/Anthropic REST calls the command flow makes, and
// records the routine fire body so tests can assert on the context sent.
function makeDiscordMock(transcript: unknown[] = []) {
  const state = { fireBody: "" };
  const fetchMock = vi.fn(async (url: string, init?: RequestInit) => {
    const method = init?.method ?? "GET";
    if (url.includes("/threads")) {
      return new Response(JSON.stringify({ id: "new-thread" }), { status: 200 });
    }
    if (url.includes("/routines/")) {
      state.fireBody = String(init?.body ?? "");
      return new Response(
        JSON.stringify({
          type: "routine_fire",
          claude_code_session_id: "s1",
          claude_code_session_url: "https://claude.ai/code/session_s1",
        }),
        { status: 200 },
      );
    }
    if (url.includes("/@original")) {
      return new Response("", { status: 200 });
    }
    if (url.includes("/messages") && method === "GET") {
      return new Response(JSON.stringify(transcript), { status: 200 });
    }
    if (url.includes("/messages")) {
      return new Response("", { status: 200 }); // user-turn echo
    }
    return new Response("", { status: 404 });
  });
  return { fetchMock, state };
}

function collectingCtx() {
  const tasks: Promise<unknown>[] = [];
  const ctx = {
    waitUntil: (p: Promise<unknown>) => tasks.push(p),
    passThroughOnException: () => {},
  } as unknown as ExecutionContext;
  return { ctx, tasks };
}

afterEach(() => {
  vi.restoreAllMocks();
});

describe("worker.fetch", () => {
  it("responds to PING with PONG", async () => {
    const { env, privateKey } = await setup();
    const res = await worker.fetch!(await signedRequest(privateKey, { type: 1 }), env, noopCtx);
    expect(res.status).toBe(200);
    expect(await res.json()).toEqual({ type: 1 });
  });

  it("rejects an invalid signature with 401", async () => {
    const { env } = await setup();
    const req = new Request("https://bot.example/", {
      method: "POST",
      headers: {
        "x-signature-ed25519": "00".repeat(64),
        "x-signature-timestamp": "1700000000",
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ type: 1 }),
    });
    expect((await worker.fetch!(req, env, noopCtx)).status).toBe(401);
  });

  it("rejects non-POST with 405", async () => {
    const { env } = await setup();
    expect((await worker.fetch!(new Request("https://bot.example/"), env, noopCtx)).status).toBe(
      405,
    );
  });

  it("returns an ephemeral error for an unknown command", async () => {
    const { env, privateKey } = await setup();
    const req = await signedRequest(privateKey, { type: 2, data: { name: "not-a-command" } });
    const json = (await (await worker.fetch!(req, env, noopCtx)).json()) as {
      type: number;
      data: { flags: number };
    };
    expect(json.type).toBe(4);
    expect(json.data.flags).toBe(64);
  });

  it("in a normal channel: defers, creates a thread, and fires the routine", async () => {
    const { env, privateKey } = await setup();
    const { fetchMock, state } = makeDiscordMock();
    vi.stubGlobal("fetch", fetchMock);
    const { ctx, tasks } = collectingCtx();

    const req = await signedRequest(privateKey, {
      type: 2,
      id: "1",
      token: "tok",
      application_id: "app",
      data: { name: "boom-linear", options: [{ name: "text", type: 3, value: "do a thing" }] },
      guild_id: "g1",
      channel_id: "c1",
      channel: { id: "c1", type: 0 },
      member: { user: { username: "tester" } },
    });
    const res = await worker.fetch!(req, env, ctx);
    expect(await res.json()).toEqual({ type: 5 });

    await Promise.all(tasks);
    const calls = fetchMock.mock.calls.map((c) => c[0] as string);
    expect(calls.some((u) => u.endsWith("/channels/c1/threads"))).toBe(true);
    expect(calls.some((u) => u.includes("/routines/"))).toBe(true);
    expect(calls.some((u) => u.includes("/messages/@original"))).toBe(true);
    // New request text reached the routine.
    expect(state.fireBody).toContain("do a thing");
    expect(state.fireBody).toContain("THREAD_ID: new-thread");
  });

  it("inside a thread: reuses the thread, sends transcript as context, no new thread", async () => {
    const { env, privateKey } = await setup();
    const transcript = [
      { content: "🤖 Claude: earlier answer", webhook_id: "w1" },
      { content: "📥 **tester:** earlier question" },
    ];
    const { fetchMock, state } = makeDiscordMock(transcript);
    vi.stubGlobal("fetch", fetchMock);
    const { ctx, tasks } = collectingCtx();

    const req = await signedRequest(privateKey, {
      type: 2,
      id: "1",
      token: "tok",
      application_id: "app",
      data: { name: "boom-linear", options: [{ name: "text", type: 3, value: "follow up" }] },
      guild_id: "g1",
      channel_id: "existing-thread",
      channel: { id: "existing-thread", type: 11 },
      member: { user: { username: "tester" } },
    });
    await worker.fetch!(req, env, ctx);
    await Promise.all(tasks);

    const calls = fetchMock.mock.calls.map((c) => c[0] as string);
    expect(calls.some((u) => u.includes("/threads"))).toBe(false);
    expect(
      calls.some((u) => u.includes("/channels/existing-thread/messages")),
    ).toBe(true);
    expect(state.fireBody).toContain("earlier question");
    expect(state.fireBody).toContain("follow up");
    expect(state.fireBody).toContain("THREAD_ID: existing-thread");
  });
});
