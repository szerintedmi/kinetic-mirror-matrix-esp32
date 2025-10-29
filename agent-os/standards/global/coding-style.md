## Generic coding style

- **Keep modules lean**: Favor simple, measurable code paths over deep abstraction stacks.
- **Small, Focused Functions**: Keep functions small and focused on a single task for better readability and testability
- **DRY Principle**: Avoid duplication by extracting common logic into reusable functions or modules. Make judgment calls when does it makes sense - sometimes duplication keeps the codebase and maintenance simpler.
- **Less code is better**
- **Meaningful Names**: Choose descriptive names that reveal intent; avoid abbreviations and single-letter variables except in narrow contexts
- **Consistent naming**: Align directory and namespace naming so components are easy to trace across projects. Establish and follow naming conventions for variables, functions, classes, and files across the codebase
- **Remove Dead Code**: Delete unused code, commented-out blocks, and imports rather than leaving them as clutter
- **Backward compatability only when required:** Unless specifically instructed otherwise, assume you do not need to write additional code logic to handle backward compatability.

## C++ specific

- **Define boundaries**: Publish APIs via `include/<module>/` headers and keep private headers local to the module.
- **Enforce formatting**: Maintain a shared `.clang-format` (or equivalent) and run it in CI to avoid style churn.
- **Guard headers**: Use `#pragma once` and keep implementation logic out of headers to preserve quick incremental builds.
- **Limit global includes**: Include only what a module consumes; prefer forward declarations to cut coupling.

## Code commenting best practices

- **Self-Documenting Code**: Write code that explains itself through clear structure and naming
- **Comment intentionally**: Let names carry intent; add brief comments only when behavior would surprise a reader. When complexity requires add high level concise overview to explain large sections of code logic.
- **Don't comment changes or fixes**: Do not leave code comments that speak to recent or temporary changes or fixes. Comments should be evergreen informational texts that are relevant far into the future.

## Markdown formatting best practices

- Make sure to strictly adhere to markdown formatting rules
- Especially pay attenttion to the following:
  - Headings should be surrounded by blank lines
  - Lists should be surrounded by blank lines

## Mermaid diagrams

- Always validate diagrams you generate using the validate-mermaid command. Syntax: `validate-mermaid <(printf 'graph TD\nA-->B\n')`
- If you include a mermaid diagram in a file inline (eg. markdown) you still must use the above validator but only with the diagram contents.

## Logging

- **Purposeful logs**: Route logs through lightweight macros / functions so verbosity can be tuned per build profile, environment or configuration.
- **Consistent prefixes**: Add task or subsystem identifiers and monotonic timestamps to logs to simplify triage
