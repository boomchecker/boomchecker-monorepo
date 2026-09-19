// Error whose message is safe to surface to Discord (no tokens, URLs, or stack traces).
export class SafeError extends Error {}
