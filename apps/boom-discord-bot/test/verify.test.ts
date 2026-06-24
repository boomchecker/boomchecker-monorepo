import { describe, it, expect } from "vitest";
import { verifyDiscordRequest } from "../src/discord/verify";

function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
}

async function generateKeyAndSign(message: string) {
  const keyPair = (await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
    "sign",
    "verify",
  ])) as CryptoKeyPair;
  const signature = await crypto.subtle.sign(
    { name: "Ed25519" },
    keyPair.privateKey,
    new TextEncoder().encode(message),
  );
  const rawPublicKey = (await crypto.subtle.exportKey("raw", keyPair.publicKey)) as ArrayBuffer;
  return {
    publicKeyHex: bytesToHex(new Uint8Array(rawPublicKey)),
    signatureHex: bytesToHex(new Uint8Array(signature)),
  };
}

describe("verifyDiscordRequest", () => {
  const timestamp = "1700000000";
  const body = JSON.stringify({ type: 1 });

  it("accepts a valid signature", async () => {
    const { publicKeyHex, signatureHex } = await generateKeyAndSign(timestamp + body);
    expect(await verifyDiscordRequest(body, signatureHex, timestamp, publicKeyHex)).toBe(true);
  });

  it("rejects a tampered body", async () => {
    const { publicKeyHex, signatureHex } = await generateKeyAndSign(timestamp + body);
    expect(await verifyDiscordRequest(`${body}x`, signatureHex, timestamp, publicKeyHex)).toBe(
      false,
    );
  });

  it("rejects missing signature or timestamp", async () => {
    const { publicKeyHex, signatureHex } = await generateKeyAndSign(timestamp + body);
    expect(await verifyDiscordRequest(body, null, timestamp, publicKeyHex)).toBe(false);
    expect(await verifyDiscordRequest(body, signatureHex, null, publicKeyHex)).toBe(false);
  });

  it("rejects malformed hex without throwing", async () => {
    const { publicKeyHex } = await generateKeyAndSign(timestamp + body);
    expect(await verifyDiscordRequest(body, "zz", timestamp, publicKeyHex)).toBe(false);
    expect(await verifyDiscordRequest(body, "abc", timestamp, publicKeyHex)).toBe(false);
  });
});
