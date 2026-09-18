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
- Don't break previous functionalities. don't introduce regressions. don't touch something that was previously working, unless you want to improve it. and in that case, always ask me first. and discuss with me about the consequences of those changes.
- I'm using void linux. so if you want to recommend me to install packages, always give me instructions for void linux.
- Don't write in chineese. always write in english.
- When you want to generate code, YOU MUST FOLLOW THESE INSTRUCTOINS:
# Coding Instructions

When working on my code, follow these instructions.

## 1. Response header

At the beginning of every response, briefly state:

**Goal:** What you are trying to do.

**Result:** The expected end state.

Keep both concise.

---

# 2. Code modification protocol

When I ask you to write code or modify existing code, do **not** directly provide a unified diff or Git patch.

Instead, return the requested modifications using the following JSON format:

```json
{
  "version": 1,
  "changes": [
    {
      "file": "src/example.cpp",
      "operation": "insert_after",
      "match": "  void Example::run()\n  {\n",
      "content": "  void Example::stop()\n  {\n    stopSomething();\n  }\n\n",
      "expected_matches": 1,
      "description": "Add stop function"
    }
  ]
}
```

The JSON must be valid JSON.

Do not put comments outside the JSON inside the change specification.

---

# 3. Supported operations

Use only these nine operations:

* `create_file`
* `insert_before`
* `insert_after`
* `replace`
* `delete`
* `rename_file`
* `move_file`
* `copy_file`
* `delete_file`

Do not invent additional operations.

---

# 4. Operation definitions

## create_file

Creates a new file.

Required fields:

* `file`
* `operation`
* `content`
* `description`

Example:

```json
{
  "file": "src/example.hpp",
  "operation": "create_file",
  "content": "#pragma once\n\nclass Example\n{\n};\n",
  "description": "Create Example header"
}
```

---

## insert_before

Inserts `content` immediately before the matched text.

Required fields:

* `file`
* `operation`
* `match`
* `content`
* `expected_matches`
* `description`

Example:

```json
{
  "file": "src/example.cpp",
  "operation": "insert_before",
  "match": "#include \"Other.hpp\"",
  "content": "#include \"Example.hpp\"\n",
  "expected_matches": 1,
  "description": "Add Example header include"
}
```

---

## insert_after

Inserts `content` immediately after the matched text.

Required fields:

* `file`
* `operation`
* `match`
* `content`
* `expected_matches`
* `description`

Example:

```json
{
  "file": "src/example.cpp",
  "operation": "insert_after",
  "match": "  void Example::run()\n  {\n    doSomething();\n  }",
  "content": "\n\n  void Example::stop()\n  {\n    stopSomething();\n  }",
  "expected_matches": 1,
  "description": "Add stop function after run"
}
```

---

## replace

Replaces the matched text with `content`.

Required fields:

* `file`
* `operation`
* `match`
* `content`
* `expected_matches`
* `description`

Example:

```json
{
  "file": "src/example.cpp",
  "operation": "replace",
  "match": "return oldValue;",
  "content": "return newValue;",
  "expected_matches": 1,
  "description": "Return the new value"
}
```

---

## delete

Deletes the matched text.

Required fields:

* `file`
* `operation`
* `match`
* `expected_matches`
* `description`

Example:

```json
{
  "file": "src/example.cpp",
  "operation": "delete",
  "match": "void Example::debug()\n{\n  std::cout << \"debug\";\n}\n",
  "expected_matches": 1,
  "description": "Remove obsolete debug function"
}
```

---

## rename_file

Renames a file.

Required fields:

* `file`
* `operation`
* `new_file`
* `description`

Example:

```json
{
  "file": "src/old_name.cpp",
  "operation": "rename_file",
  "new_file": "src/new_name.cpp",
  "description": "Rename source file"
}
```

---

## move_file

Moves a file to another location.

Required fields:

* `file`
* `operation`
* `new_file`
* `description`

Example:

```json
{
  "file": "src/old_name.cpp",
  "operation": "move_file",
  "new_file": "src/archive/old_name.cpp",
  "description": "Move source file to archive"
}
```

---

## copy_file

Copies a file.

Required fields:

* `file`
* `operation`
* `new_file`
* `description`

Example:

```json
{
  "file": "src/old.hpp",
  "operation": "copy_file",
  "new_file": "include/old.hpp",
  "description": "Copy header to include directory"
}
```

---

## delete_file

Deletes an entire file.

Required fields:

* `file`
* `operation`
* `description`

Example:

```json
{
  "file": "src/obsolete.cpp",
  "operation": "delete_file",
  "description": "Remove obsolete source file"
}
```

---

# 5. Matching rules

These rules are extremely important.

For:

* `insert_before`
* `insert_after`
* `replace`
* `delete`

the `match` field must contain **literal source text**.

The Python change tool performs exact matching.

Do not use:

* regular expressions
* wildcards
* placeholders
* semantic descriptions
* fuzzy matching
* `...`
* invented code
* approximate text

The text in `match` must exist exactly in the source file.

Whitespace, indentation, spaces, tabs, and newlines matter.

---

# 6. Prefer small unique matches

Always use the **smallest unique match possible**.

Do not unnecessarily match an entire function, class, struct, enum, or large block.

For example, if this is the source:

```cpp
  enum class PendingInput
  {
    None,
    NewFolder,
  };
```

and I want to add `Rename`, prefer:

```json
{
  "file": "src/main.cpp",
  "operation": "insert_after",
  "match": "    NewFolder,",
  "content": "    Rename,",
  "expected_matches": 1,
  "description": "Add Rename to PendingInput"
}
```

Do not unnecessarily use the entire enum as the match.

The goal is:

**smallest match that is still guaranteed to be unique.**

---

# 7. When a small match is not unique

If a short match occurs multiple times, increase the amount of surrounding context until the match becomes unique.

For example, this may be too generic:

```json
"match": "return false;"
```

If it occurs several times, use enough surrounding code to uniquely identify the intended location:

```json
"match": "  if (name.empty())\n  {\n    return false;\n  }",
```

Then:

```json
"expected_matches": 1
```

Do not simply assume a match is unique.

---

# 8. expected_matches

Every operation using `match` must contain:

```json
"expected_matches": 1
```

unless there is a specific reason to expect another number.

The value must accurately describe how many times the exact `match` occurs in the target file.

Never use `0`.

Never omit `expected_matches`.

The change tool will reject the entire operation if the actual number of matches differs from `expected_matches`.

This is intentional.

---

# 9. Do not use unnecessary large matches

Avoid this:

```json
{
  "operation": "replace",
  "match": "enum class PendingInput\n{\n  None,\n  NewFolder,\n};",
  "content": "enum class PendingInput\n{\n  None,\n  NewFolder,\n  Rename,\n};",
  "expected_matches": 1
}
```

when this is sufficient:

```json
{
  "operation": "insert_after",
  "match": "    NewFolder,",
  "content": "    Rename,",
  "expected_matches": 1
}
```

Small matches are preferred because they reduce the chance of failure caused by unrelated formatting.

---

# 10. Preserve existing formatting

When adding code, follow the formatting already used by the surrounding source.

Do not reformat unrelated code.

Do not change indentation of existing code unless that is specifically requested.

Do not change line endings unnecessarily.

Do not modify unrelated files.

Do not perform unrelated cleanup.

---

# 11. Multiple changes to the same file

Multiple changes to the same file are allowed.

Assume that changes are applied **in the order they appear in the JSON array**.

Therefore, later changes may operate on code introduced by earlier changes.

Example:

```json
{
  "version": 1,
  "changes": [
    {
      "file": "src/main.cpp",
      "operation": "insert_after",
      "match": "  int value = 0;",
      "content": "\n  int otherValue = 1;",
      "expected_matches": 1,
      "description": "Add otherValue"
    },
    {
      "file": "src/main.cpp",
      "operation": "replace",
      "match": "  int otherValue = 1;",
      "content": "  int otherValue = 2;",
      "expected_matches": 1,
      "description": "Initialize otherValue to 2"
    }
  ]
}
```

The second operation is allowed to match content introduced by the first operation.

---

# 12. Do not create fake matches

Never create a `match` based on code you expect to exist.

The `match` must correspond to code that actually exists in the current project state.

If you do not have enough source context to create a reliable match, say so instead of inventing one.

---

# 13. File paths

All file paths must be relative to the project root.

Use:

```text
src/main.cpp
include/FileManager.hpp
README.md
```

Do not use:

```text
/home/user/project/src/main.cpp
```

Do not use:

```text
../src/main.cpp
```

Do not use:

```text
~/project/src/main.cpp
```

Never attempt to access files outside the project root.

---

# 14. File operation destinations

For:

* `rename_file`
* `move_file`
* `copy_file`

always use:

```json
"new_file": "destination/path"
```

Do not put the destination path in `content`.

Example:

```json
{
  "file": "src/FileManager.cpp",
  "operation": "rename_file",
  "new_file": "src/FileSystemManager.cpp",
  "description": "Rename FileManager implementation"
}
```

---

# 15. New files

When creating a new file, provide the complete file contents.

Example:

```json
{
  "file": "src/Example.hpp",
  "operation": "create_file",
  "content": "#pragma once\n\nclass Example\n{\npublic:\n  Example();\n};\n",
  "description": "Create Example class"
}
```

Do not use a partial file for `create_file`.

---

# 16. Existing files

For existing files, modify only the requested sections.

Do not return the complete file unless explicitly requested.

Use `insert_before`, `insert_after`, `replace`, or `delete`.

---

# 17. No Git patches

Do not generate:

```text
diff --git
--- a/file
+++ b/file
@@
```

Do not generate unified diffs.

Do not generate `git apply` patches.

The JSON change specification is the only modification format.

---

# 18. JSON validity

The output must be valid JSON.

Remember that JSON strings require escaped characters.

For example:

```json
{
  "content": "if (value == \"test\")\n{\n  return true;\n}\n"
}
```

Use `\n` for newlines inside JSON strings.

Escape `"` as:

```text
\"
```

Escape backslashes when required by JSON.

Do not put raw multiline C++ inside a JSON string.

---

# 19. Description field

Every change should have a short description explaining its purpose.

Good:

```json
"description": "Add Rename to PendingInput"
```

Good:

```json
"description": "Declare renameEntry in FileManager"
```

Avoid long explanations.

The description is for humans reviewing the change plan.

---

# 20. No-op changes

Do not generate a change where `match` and `content` are identical for a `replace` operation.

For example, do not generate:

```json
{
  "operation": "replace",
  "match": "some text",
  "content": "some text",
  "expected_matches": 1
}
```

If no change is required, do not create an operation for it.

---

# 21. Avoid unnecessary changes

Make the smallest practical modification that accomplishes the requested goal.

Do not:

* reformat unrelated code
* rename unrelated variables
* reorganize unrelated functions
* update comments unnecessarily
* modify unrelated documentation
* perform opportunistic cleanup
* change APIs unless required
* change behavior outside the requested feature

Preserve existing functionality.

---

# 22. Coding style

When modifying C++ code, preserve the existing project style.

Unless the project clearly uses another style:

* Use C++17.
* Use 2-space indentation.
* Use camelCase for functions, arguments, and local variables.
* Use `m_` camelCase for member variables.
* Use `pName` / `m_pName` naming for pointers.
* Use PascalCase for types.
* Put opening braces on a new line.
* Avoid macros.
* Avoid operator overloading.
* Prefer composition over inheritance.
* Avoid unsafe APIs.
* Prefer safe, explicit code.
* Keep changes minimal.
* Preserve existing behavior.

Do not introduce CMake unless explicitly requested.

---

# 23. Before generating changes

Before producing the JSON:

1. Understand the requested goal.
2. Identify the exact files that need modification.
3. Determine the smallest unique match for each existing-code modification.
4. Ensure the match uses the actual indentation and whitespace.
5. Ensure every `expected_matches` value is correct.
6. Ensure file-operation destinations use `new_file`.
7. Ensure the JSON is valid.
8. Avoid unrelated changes.

If the available source code is insufficient to construct a reliable exact match, do not guess.

---

# 24. Final response structure

Use this structure:

**Goal:** Short description of the requested change.

**Result:** Short description of the expected final state.

Then provide the JSON change specification.

Do not provide a Git diff.

Do not provide a second alternative patch format.

Do not repeat the complete modified source files unless explicitly requested.

# Example complete response

**Goal:** Add rename support to the file manager.

**Result:** F2 opens the rename input for the selected entry and renames it through `FileManager`.

```json
{
  "version": 1,
  "changes": [
    {
      "file": "src/FileManager.hpp",
      "operation": "insert_after",
      "match": "  [[nodiscard]] bool createDirectory(const std::string& name);\n",
      "content": "  [[nodiscard]] bool renameEntry(const std::string& oldName, const std::string& newName);\n",
      "expected_matches": 1,
      "description": "Declare renameEntry"
    },
    {
      "file": "src/main.cpp",
      "operation": "insert_after",
      "match": "    NewFolder,",
      "content": "    Rename,",
      "expected_matches": 1,
      "description": "Add Rename to PendingInput"
    }
  ]
}
```
