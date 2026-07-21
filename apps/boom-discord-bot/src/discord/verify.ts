// Verify a Discord interaction request signature using Ed25519 via the Web Crypto
// API. Works unchanged on Cloudflare Workers and Node (>=20) — no external deps.
//
// Discord signs `timestamp + rawBody` with its private key; we verify against the
// application's public key (hex). See:
// https://discord.com/developers/docs/interactions/receiving-and-responding#security-and-authorization

const encoder = new TextEncoder();

function hexToBytes(hex: string): Uint8Array {
  if (hex.length === 0 || hex.length % 2 !== 0) {
    throw new Error("invalid hex length");
  }
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) {
    const byte = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16);
    if (Number.isNaN(byte)) {
      throw new Error("invalid hex character");
    }
    out[i] = byte;
  }
  return out;
}

export async function verifyDiscordRequest(
  rawBody: string,
  signature: string | null,
  timestamp: string | null,
  publicKeyHex: string,
): Promise<boolean> {
  if (!signature || !timestamp) {
    return false;
  }

  let signatureBytes: Uint8Array;
  let publicKeyBytes: Uint8Array;
  try {
    signatureBytes = hexToBytes(signature);
    publicKeyBytes = hexToBytes(publicKeyHex);
  } catch {
    return false;
  }

  try {
    const key = await crypto.subtle.importKey(
      "raw",
      publicKeyBytes,
      { name: "Ed25519" },
      false,
      ["verify"],
    );
    return await crypto.subtle.verify(
      { name: "Ed25519" },
      key,
      signatureBytes,
      encoder.encode(timestamp + rawBody),
    );
  } catch {
    return false;
  }
}
