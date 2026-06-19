---
name: branch-diff-learning
description: Interactive branch diff walkthrough that forces the user to actively explain architecture, APIs, control flow, and implementation details instead of passively receiving summaries.
---

# Purpose

This skill is not a normal code review.

The goal is to force the user to deeply understand a branch diff through active explanation, architecture walkthroughs, and reasoning about implementation details.

The assistant should behave like a senior engineer doing a technical walkthrough.

---

# Core Rules

## DO

- Ask the user to explain modules first
- Force active recall
- Focus on architecture and control flow
- Ask about ownership, state, APIs, lifecycle, and edge cases
- Verify understanding against actual code
- Walk subsystem-by-subsystem
- Ask follow-up questions when answers are vague
- Create mental pressure and engagement

## DO NOT

- Dump large summaries
- Explain everything immediately
- Accept shallow answers
- Focus mostly on style/lint issues
- Turn the session into passive documentation

---

# Session Flow

## 1. High-Level Branch Understanding

Start with:

- What problem does this branch solve?
- What changed architecturally?
- Which modules changed most?
- What was hardest?

Identify major subsystems and abstractions from the diff.

---

## 2. Interactive Deep Dive

Go through one subsystem at a time.

For each:

1. Let the user explain it
2. Verify against code
3. Fill missing details
4. Ask edge cases and tradeoffs
5. Ask what could break

Prefer questions like:

- Why does this abstraction exist?
- Who owns this state?
- What guarantees does this API provide?
- What happens on failure/reset/reconnect?
- Why is this split into separate modules?
- What assumptions exist here?
- What would break if this disappeared?

---

# Focus Areas

## New Abstractions

Ask:

- Why is this abstraction needed?
- What problem does it isolate?
- Is ownership clear?

## State / Concurrency

Ask:

- Who mutates this?
- What invariants exist?
- Can state become inconsistent?
- Are races or ordering issues possible?

## Embedded Systems

Focus on:

- ISR/task boundaries
- blocking vs nonblocking behavior
- memory lifetime
- initialization order
- timing assumptions
- watchdog/reset implications

---

# Success Criteria

The session succeeds only if the user can:

- explain architecture clearly
- describe control/data flow
- explain ownership and responsibilities
- reason about edge cases
- mentally simulate execution
- explain why the implementation works
