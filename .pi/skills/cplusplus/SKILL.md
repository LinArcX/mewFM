---
name: cplusplus
description: Guidelines and rules for writing C++ applications. Use when writing, reviewing, refactoring, or generating C++ code. Triggers on C++, cpp, C++ style, coding standards, or requests for C++ implementation that should follow these conventions.
---

# C++ Coding Guidelines

Apply these rules strictly when writing or editing C++ code.

## Indentation and Formatting

- Use exactly 2 spaces for indentation. Never use tabs or 4 spaces.
- Opening and Closing brackets for functions, if statements, switch, while loops, for loops and similar constructs
  always start on a new line.
- If statements should always have Curly Braces. even if the body of "if" is just one line.
- Between functions, put exactly 1 empty line. Not more.
- Inside a function, related sections of the code should have no empty lines between them.
  Unrelated parts should be separated by exactly 1 empty line.
- No function should be longer than what can be printed on a single sheet of paper in a standard format
  with one line per statement and one line per declaration. Typically,
  this means no more than about 60 lines of code per function.

## Naming Conventions

- Function names, function arguments, and local variable names should be camelCase.
- Member variables of classes start with `m_` followed by camelCase pattern (example - `m_maxSpeed`).
- Pointer variables start with `p` (example - `pName`).
  If they are also member variables, they start with `m_p` (example - `m_pName`).
- Classes, Enums, and Structs names should be PascalCase.
- Cpp file names should be PascalCase. (their header files should be .hpp not .h)
- Choose descriptive names for variables and functions.

## Language Features and Safety

### General
- Write safe code. Follow MISRA C++ guidelines as much as possible.
- Prefer simplicity over complexity. Do not create complicated or confusing code.
- Prefer composition over inheritance. Avoid inheritance as much as possible.
  - if you forced to use inheritance, it should be MAXIMUM one level of inheritance, not more.
- write understandable code.
- write efficient and fast code.
- Avoid templates as much as you can.
- Do not use macros. (except for inclusion of header files)
- Don’t waste time or space.
- Don't use exceptions or any functions/libraries from standard library that use exceptions internally.
  - use error codes instead.
- Avoid non-const global variables.
- Prefer immutable data to mutable data
- Don’t leak any resources.
- Initialize all variables/member variables.
- Restrict all code to very simple control flow constructs.
  do not use goto statements, setjmp or longjmp constructs, or direct or indirect recursion.
- Don't introduce bugs (from previously working code). I don't want to deal with regressions.
  when something is working, don't touch it unless i ask it explicitly. if you really want to touch older code,
  examine it's consequences and ask me first.
- Do not use operator overloading.
- Use compile-time features of C++ as much as possible.
  Prefer compile-time checking to run-time checking
- Use class enums instead of c-style enums.
- Each line should not overpass 120 characters.

### Functions
- Return early as much as you can.
- A function should perform a single logical operation
- Do pre-condition and post-condition checks in every function.
  for example: Checking the boundaries of array, checking pointers, etc..
- Each calling function must check the return value of nonvoid functions,
    and each called function must check the validity of all parameters provided by the caller.
- Do not use unsafe versions of functions.
- Every non void function should return error. other output data, should be return as output arguments.
  - non void functions should have [[nodiscard]] to indicate checking the output is mandatory.
- For function calls that has more than 3 arguments, put each argument in new line.
  - For function calls that has 3 or less, keep all argument at same line.
    - If keeping at same line exceed 120 character, break each argument in new line.
      - Do the same patterns for function declarations and function definitions also.

### (Smart)Pointers
- Use RAII and smart pointers as much as possible.
- Declare a pointer that must not be null as not_null.
