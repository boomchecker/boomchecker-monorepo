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
    const res = await worker.fetch!(req, env, noopCtx);
    expect(res.status).toBe(401);
  });

  it("rejects non-POST with 405", async () => {
    const { env } = await setup();
    const res = await worker.fetch!(new Request("https://bot.example/"), env, noopCtx);
    expect(res.status).toBe(405);
  });

  it("returns an ephemeral error for an unknown command", async () => {
    const { env, privateKey } = await setup();
    const req = await signedRequest(privateKey, { type: 2, data: { name: "not-a-command" } });
    const res = await worker.fetch!(req, env, noopCtx);
    const json = (await res.json()) as { type: number; data: { flags: number } };
    expect(json.type).toBe(4);
    expect(json.data.flags).toBe(64);
  });

  it("defers and fires the routine for /boom-linear", async () => {
    const { env, privateKey } = await setup();
    const fetchMock = vi.fn(async (url: string) => {
      if (url.includes("/routines/")) {
        return new Response(
          JSON.stringify({
            type: "routine_fire",
            claude_code_session_id: "s1",
            claude_code_session_url: "https://claude.ai/code/session_s1",
          }),
          { status: 200 },
        );
      }
      // Discord follow-up webhook.
      return new Response("", { status: 204 });
    });
    vi.stubGlobal("fetch", fetchMock);

    const tasks: Promise<unknown>[] = [];
    const ctx = {
      waitUntil: (p: Promise<unknown>) => tasks.push(p),
      passThroughOnException: () => {},
    } as unknown as ExecutionContext;

    const req = await signedRequest(privateKey, {
      type: 2,
      id: "1",
      token: "tok",
      application_id: "app",
      data: { name: "boom-linear", options: [{ name: "text", type: 3, value: "do a thing" }] },
      guild_id: "g1",
      channel_id: "c1",
      member: { user: { username: "tester" } },
    });
    const res = await worker.fetch!(req, env, ctx);
    expect(await res.json()).toEqual({ type: 5 });

    await Promise.all(tasks);
    const calledUrls = fetchMock.mock.calls.map((c) => c[0] as string);
    expect(calledUrls.some((u) => u.includes("/routines/"))).toBe(true);
    expect(calledUrls.some((u) => u.includes("/webhooks/app/tok/messages/@original"))).toBe(true);
  });
});
