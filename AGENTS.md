# AGENTS.md - Course Template with Interaction Log

## Project Overview
This project is being developed for an AI-Assisted Software Engineering course.
Read the README and relevant documentation before substantial changes.

## Project Context
This project is an Unreal Engine FPS focused on recreating the feel of
classic trickshotting gameplay similar to older arcade-style console shooters.

Primary project goals include:

- First-person player movement.
- Sprinting, jumping, crouching, and other movement useful for trickshots.
- Weapon handling and firing.
- Sniper-style weapons suitable for trickshot gameplay.
- Mid-air rotation and shooting.
- Hit detection and target feedback.
- A placeable target dummy that the player can position before attempting a shot.
- Systems for resetting/repeating trickshot attempts quickly.
- Future expansion for scoring or tracking trickshots.

Prefer implementing gameplay systems in a modular way so additional weapons,
movement mechanics, maps, and trickshot systems can be added later.

## Unreal Engine Development Rules

- This is an Unreal Engine project.
- Prefer C++ for core gameplay systems that benefit from maintainability,
  reusable architecture, or source control.
- Blueprints may be used for configuration, level setup, visual scripting,
  animation hookups, UI, and designer-adjustable values.
- Expose useful gameplay parameters to Blueprints when appropriate using
  Unreal properties and functions.
- Follow Unreal naming and class conventions.
- Do not make unnecessary changes to project settings.

### Unreal Generated Files

Do not manually modify generated Unreal Engine files or build artifacts.

Generally do not edit or commit files from:

- `Binaries/`
- `DerivedDataCache/`
- `Intermediate/`
- `Saved/`
- `.vs/`

Do not delete these directories unless specifically needed to resolve an
Unreal build/project-generation issue.

### Unreal Assets
- `.uasset` and `.umap` files are binary Unreal assets.
- Never attempt to edit them as text.
- If a task requires changes that must be performed inside the Unreal Editor,
  explain what needs to be changed in the editor if it cannot be performed
  safely through project source code.
- Do not replace or regenerate unrelated assets.

### Gameplay Architecture
Prefer reusable gameplay components rather than placing unrelated logic
directly into a Character class.

Examples:

- Character: player movement/input coordination
- Weapon classes/components: weapon behavior
- Target actor: target dummy functionality
- GameMode/GameState: game rules
- PlayerController: player-specific control logic
- Components: reusable mechanics

## Engineering Process
1. Inspect relevant code before editing.
2. Identify material ambiguity.
3. For non-trivial changes, propose a short plan first.
4. Make the smallest reasonable change satisfying approved requirements.
5. Add or update tests.
6. Build and test locally.
7. Review `git status` and `git diff`.
8. Summarize changes and verification.

## Requirements and Scope
- Treat documented requirements as authoritative.
- Do not silently add, remove, or weaken requirements.
- Do not add unrelated features.
- Ask for clarification when ambiguity could materially affect implementation.
- Do not refactor unrelated code unless requested.

## Build, Test, and CI Policy
All development builds and tests must be run locally.

- Run build, test, lint, and formatting commands locally.
- Do not use GitHub Actions or another remote CI/CD service merely to verify changes.
- Do not create, modify, enable, or trigger GitHub Actions workflows unless explicitly requested.
- Do not add CI/CD configuration as part of a feature or bug-fix task unless explicitly requested.
- Existing GitHub Actions workflows are not permission to run or modify them.
- Prefer documented local build and test commands.
- Before reporting completion, run appropriate tests locally and report commands and results.

## External Services and Resource Usage
Do not create, configure, or use external or paid services without explicit user approval.

This includes:
- GitHub Actions and hosted CI/CD
- cloud computing resources
- hosted databases
- paid APIs
- deployment platforms
- external monitoring services

If a task appears to require an external service, explain why and ask for approval first.

## AI Interaction Log
Maintain `docs/ai-interaction-log.md`.

For every development-related user request, append:
1. Date and time, if available.
2. The user's prompt exactly as provided.
3. A concise interpretation of the request.
4. Requirements or acceptance criteria.
5. Actions performed.
6. Files created or modified.
7. Commands/tests executed.
8. Verification results.
9. Notes or follow-up work, if applicable.
10. A concise summary of the final response.

Use this format:

---
## Interaction <number>

### User Prompt
<exact user prompt>

### Interpretation
<what you understood>

### Requirements / Acceptance Criteria
- <what must be true for the request to be considered complete>

### Actions Taken
<summary>

### Files Changed
- file1
- file2

### Verification
- command/test and result

### Notes / Follow-up
- <remaining work, editor steps, limitations, or future improvements>

### Response Summary
<summary>
---

Append new interactions. Never overwrite or delete earlier entries.
The log is course documentation and must remain in the repository.

This is an agent-maintained engineering record, not a guaranteed verbatim
transcript of hidden reasoning, tool internals, or every token produced.

## Git
- Inspect `git status` and `git diff` before finishing.
- Do not modify unrelated files.
- Do not create or amend commits unless explicitly instructed.
