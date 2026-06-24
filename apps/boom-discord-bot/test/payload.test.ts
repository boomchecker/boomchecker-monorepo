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
  it("includes the transcript, the new request, and the thread id", () => {
    const text = buildThreadRoutineText({
      username: "alice",
      userText: "add CI step",
      transcript: "📥 **alice:** earlier request",
      threadId: "t1",
    });
    expect(text).toContain("Conversation so far");
    expect(text).toContain("📥 **alice:** earlier request");
    expect(text).toContain("add CI step");
    expect(text).toContain("THREAD_ID: t1");
  });

  it("omits the context block on a fresh thread", () => {
    const text = buildThreadRoutineText({
      username: "alice",
      userText: "add CI step",
      transcript: "",
      threadId: "t1",
    });
    expect(text).not.toContain("Conversation so far");
    expect(text).toContain("add CI step");
    expect(text).toContain("THREAD_ID: t1");
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
