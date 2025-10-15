Now that we've initiated and planned the details for a new spec, we will now proceed with drafting the specification document, following these instructions:

# Spec Writing

## Core Responsibilities

1. **Analyze Requirements**: Load and analyze requirements and visual assets thoroughly
2. **Search for Reusable Code**: Find reusable components and patterns in existing codebase
3. **Create Specification**: Write comprehensive specification document

## Workflow

### Step 1: Analyze Requirements and Context

Read and understand all inputs and THINK HARD:
```bash
# Read the requirements document
cat agent-os/specs/[current-spec]/planning/requirements.md

# Check for visual assets
ls -la agent-os/specs/[current-spec]/planning/visuals/ 2>/dev/null | grep -v "^total" | grep -v "^d"

# Read the agent registry to know available subagents
cat agent-os/agents-registry.yml
```

Parse and analyze:
- User's feature description and goals
- Requirements gathered by spec-researcher
- Visual mockups or screenshots (if present)
- Available subagents in the registry (if present)
- Any constraints or out-of-scope items mentioned

### Step 2: Search for Reusable Code

Before creating specifications, search the codebase for existing patterns and components that can be reused.

Based on the feature requirements, identify relevant keywords and search for:
- Similar features or functionality
- Existing UI components that match your needs
- Models, services, or controllers with related logic
- API patterns that could be extended
- Database structures that could be reused

Use appropriate search tools and commands for the project's technology stack to find:
- Components that can be reused or extended
- Patterns to follow from similar features
- Naming conventions used in the codebase
- Architecture patterns already established

Document your findings for use in the specification.

### Step 3: Create Core Specification

Write the main specification to `agent-os/specs/[current-spec]/spec.md`.

Do not write actual code in the spec.md document. Just describe the requirements clearly and concisely.

Follow this structure exactly when creating the content of `spec.md`:

```markdown
# Specification: [Feature Name]

## Goal
[1-2 sentences describing the core objective]

## User Stories
- As a [user type], I want to [action] so that [benefit]
- [Additional stories based on requirements]

## Core Requirements
### Functional Requirements
- [User-facing capability]
- [What users can do]
- [Key features to implement]

### Non-Functional Requirements
- [Performance requirements]
- [Accessibility standards]
- [Security considerations]

## Visual Design
[If mockups provided]
- Mockup reference: `planning/visuals/[filename]`
- Key UI elements to implement
- Responsive breakpoints required

## Reusable Components
### Existing Code to Leverage
- Components: [List found components]
- Services: [List found services]
- Patterns: [Similar features to model after]

### New Components Required
- [Component that doesn't exist yet]
- [Why it can't reuse existing code]

## Technical Approach
- Database: [Models and relationships needed]
- API: [Endpoints and data flow]
- Frontend: [UI components and interactions]
- Testing: [Test coverage requirements]

## Out of Scope
- [Features not being built now]
- [Future enhancements]
- [Items explicitly excluded]

## Success Criteria
- [Measurable outcome]
- [Performance metric]
- [User experience goal]
```

## Important Constraints

1. **Always search for reusable code** before specifying new components
2. **Reference visual assets** when available
3. **Do not write actual code** in the spec
4. **Keep each section short**, with clear, direct, skimmable specifications
5. **Document WHY new code is needed** if can't reuse existing


## Display confirmation and next step

Display the following message to the user:

```
The spec has been created at `agent-os/specs/[this-spec]/spec.md`.

Review it closely to ensure everything aligns with your vision and requirements.

Next step: Run the command, 2-create-tasks-list.md
```

## User Standards & Preferences Compliance

IMPORTANT: Ensure that the specification document's content is ALIGNED and DOES NOT CONFLICT with the user's preferences and standards as detailed in the following files:

@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/frontend/embedded-web-ui.md
@agent-os/standards/frontend/serial-interface.md
@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md
