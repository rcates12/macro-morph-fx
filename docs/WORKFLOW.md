# Workflow (Ralph Loop) + Anti-Context-Rot Rules

> Read this file before every task. It defines how work gets done on this project.

---

## Part 1 — Ralph Loop (must follow for every task)

### 1) READ
- docs/SPEC.md
- docs/DECISIONS.md
- Source/Params.h
- Relevant source files

### 2) PLAN (5–10 bullets)
- What files you will touch
- What params you will add/change
- Risks / edge cases

### 3) IMPLEMENT
- Smallest change that compiles
- No unrelated refactors

### 4) VERIFY
- Build succeeds (Debug)
- Basic smoke instructions for Ableton

### 5) HANDOFF SUMMARY (required)
Every completed task must end with this summary format:

```
## Handoff — <task title>

### What changed
- <file>: <one-line description of change>
- <file>: <one-line description of change>

### Params added/changed
- <param ID>: <what changed or "new">

### Decisions made
- <decision> (also logged in DECISIONS.md)

### How to build
- cmake --build build --config Release

### How to test (Ableton steps)
1. Copy .vst3 to C:\Program Files\Common Files\VST3\
2. Rescan in Ableton
3. <specific test step>

### Known issues
- <issue or "none">

### Next suggested tasks
1. <next logical task>
2. <next logical task>
```

---

## Part 2 — Task Packet Template

Copy/paste this into the start of every task prompt:

```
Task: <short name>

Branch: feature/<name>

Read first:
- docs/SPEC.md
- docs/DECISIONS.md
- Source/Params.h

Goal (definition of done):
- <clear measurable outcome>

Files to inspect:
- <list>

Constraints:
- VST3 only
- Ableton target
- Keep changes minimal
- Do not rename parameter IDs
- Do not refactor unrelated code

Acceptance tests:
1) Builds on <platform>
2) Plugin loads in Ableton
3) No clicks when automating <params>
4) CPU stable on silence

Output required:
- Handoff summary format (see Part 1, step 5)
```

---

## Part 3 — Six Anti-Context-Rot Rules

These rules are enforced on **every** task. They prevent the codebase from drifting out of sync with its documentation.

### Rule 1 — Single source of truth
`docs/SPEC.md` is the canonical description of what the plugin does. If SPEC.md and code disagree, SPEC.md wins and the code must be updated (or a DECISIONS.md entry must explain the deviation).

### Rule 2 — Parameter registry
All parameter IDs, ranges, and defaults live in `Source/Params.h`. No parameter ID may be invented inline in a .cpp file. If you need a new parameter, add it to Params.h first.

### Rule 3 — Small PR rule
One feature slice per task/branch. Do not bundle unrelated changes. If you discover something else that needs fixing, log it as a follow-up task in the handoff summary — do not fix it now.

### Rule 4 — Verification required
Every task ends with a build command and a smoke-test checklist. The task is not done until the build succeeds and the smoke test is documented.

Minimum verification:
```
cmake --build build --config Release
```

### Rule 5 — Decision log
Every notable choice (library picked, algorithm chosen, format decided, trade-off made) gets a dated entry in `docs/DECISIONS.md`. "Notable" = anything a future reader would wonder "why did they do it this way?"

### Rule 6 — Guardrails in prompts
Every task prompt must include these constraints:
- **Do not refactor** code outside the task scope
- **Do not rename** existing parameter IDs in Params.h
- **Minimal changes** — only what the task requires, nothing more

---

## Part 4 — Parallel Lanes (to avoid merge conflicts)

When running multiple tasks, each must stay inside its lane:

| Lane | Scope | Files |
|------|-------|-------|
| A — DSP modules | Filter, Drive, Delay, Reverb | `Source/DSP/*` |
| B — Params + smoothing + state | Parameter setup, state serialization | `Source/Params.h`, processor param setup |
| C — UI | Editor / GUI only | `PluginEditor.*` |
| D — Scenes/Morph/Macros | Scene data, interpolation, mapping | Scene/Morph/Macro source files |

**Rule:** Parallel agents must stay inside their lane. If a task must touch `PluginProcessor.*` it must be scheduled (not parallel) or kept extremely small.

---

## Quick Reference

| Document             | Purpose                                    | When to update         |
|----------------------|--------------------------------------------|------------------------|
| `docs/SPEC.md`       | What the plugin does (canonical)           | When features change   |
| `docs/DECISIONS.md`  | Why we made each choice                    | Every task (if needed) |
| `docs/WORKFLOW.md`   | How we work (this file)                    | Rarely                 |
| `docs/STATE.md`      | Current milestone status + known issues    | Every handoff          |
| `Source/Params.h`    | Parameter ID/range/default registry        | When params change     |
