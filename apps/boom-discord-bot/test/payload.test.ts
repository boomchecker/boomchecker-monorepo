import { describe, it, expect } from "vitest";
import {
  isThreadChannel,
  buildThreadRoutineText,
  getOptionValue,
  getInvokerUsername,
  ChannelType,
  type Interaction,
} from "../src/discord/payload";

describe("isThreadChannel", () => {
  it("detects thread channel types", () => {
    expect(isThreadChannel(ChannelType.PUBLIC_THREAD)).toBe(true);
    expect(isThreadChannel(ChannelType.PRIVATE_THREAD)).toBe(true);
    expect(isThreadChannel(ChannelType.ANNOUNCEMENT_THREAD)).toBe(true);
    expect(isThreadChannel(ChannelType.GUILD_TEXT)).toBe(false);
    expect(isThreadChannel(undefined)).toBe(false);
  });
});

describe("buildThreadRoutineText", () => {
  it("includes the transcript, callback header, and the request", () => {
    const text = buildThreadRoutineText({
      userText: "add CI step",
      transcript: "📥 **alice:** earlier request",
      threadId: "t1",
      callbackUrl: "https://worker.example/routine-callback",
      callbackToken: "cb-token",
    });
    expect(text).toContain("Conversation so far");
    expect(text).toContain("📥 **alice:** earlier request");
    expect(text).toContain("THREAD_ID: t1");
    expect(text).toContain("CALLBACK_URL: https://worker.example/routine-callback");
    expect(text).toContain("CALLBACK_TOKEN: cb-token");
    expect(text).toContain("USER_REQUEST:\nadd CI step");
  });

  it("omits the context block on a fresh thread", () => {
    const text = buildThreadRoutineText({
      userText: "x",
      transcript: "",
      threadId: "t1",
      callbackUrl: "u",
      callbackToken: "k",
    });
    expect(text).not.toContain("Conversation so far");
    expect(text.startsWith("THREAD_ID: t1")).toBe(true);
  });
});

describe("option and username helpers", () => {
  const interaction = {
    type: 2,
    id: "1",
    token: "tok",
    application_id: "app",
    data: { name: "boom-linear", options: [{ name: "text", type: 3, value: "hi" }] },
    member: { user: { username: "bob" } },
  } as Interaction;

  it("reads a command option value", () => {
    expect(getOptionValue(interaction, "text")).toBe("hi");
    expect(getOptionValue(interaction, "missing")).toBeUndefined();
  });

  it("reads the invoker username from member or user", () => {
    expect(getInvokerUsername(interaction)).toBe("bob");
    expect(getInvokerUsername({ ...interaction, member: undefined, user: { username: "dm" } })).toBe(
      "dm",
    );
  });
});
