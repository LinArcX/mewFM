---
name: general-programming
description: Guidelines and rules for writing any piece of code. Use when writing, reviewing, refactoring, or generating any source code.
---

# General Coding Guidelines

Apply these rules strictly when writing or editing any piece of code.

- Always write Safe and Minimal code. (Highest priority)
  -Simplicity and security and efficiency is the highest priority for me.
- Don't write bloated code.
- Don't over-engineer/over-complicate things.
- Don't over-explain things.
- Makes software that is easy to understand, easy to debug, easy to maintain.
- Safety/minimalism should be your highest priority.
- Try to NOT use external libraries as much as you can.
  - If you wanted to use any external libraries, choose the most minimal and safe one.
- always use meaningful functions and variable names.
- always remove dead code commented code. (try your best to keep the code clean and understandable)
- I'm using void linux. so if you want to recommend me to install packages, always give me instructions for void linux.
- When i ask you to write code or modify some part of code, use the snippet format:
  - Show only the relevant piece of code, with a few lines of context before and after each change.
  - Do NOT paste whole functions, classes, or files unless the unit is entirely new or was fully rewritten.
  - New files are shown in full under a heading like `### New file: src/Foo.hpp`.
  - Mark each change with these exact comment markers placed INSIDE the code snippet:
    // --> Start of Change
    ...your changes...
    // --> End of Change
  - Never use `--> start of change` / `--> end of change` style markers.
  - Never paste the whole file just to show a small edit.
- At the beginning of every response, briefly state:
  - **Goal:** what you are trying to do.
  - **Result:** the expected end state.
  - Keep it short and concise.
- Don't responde with explanatation. just give me the chagnes. just code. nothing more. Unless i ask you EXPLICITLY to exaplin.
- Use less tokens as much as you can.
- Don't break previous functionalities. don't introduce regressions. don't touch something that was previously working, unless you want to improve it. and in that case, always ask me first. and discuss with me about the consequences of those changes.
- Don't write in chineese. always write in english.
