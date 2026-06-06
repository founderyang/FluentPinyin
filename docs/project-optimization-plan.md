# FluentPinyin project optimization plan

This document combines the current code audit, package-size review, user
feedback, and third-party review notes into one staged optimization plan. The
goal is to improve the whole project without weakening the IME behavior,
installer cleanup, or font-uninstall guarantees.

## Goals

- Reduce time from install completion to usable typing.
- Make compact-to-expanded candidate window transitions feel immediate.
- Keep fonts private to the application so uninstall remains clean.
- Improve package, installer, update, and sync safety.
- Add enough observability and tests to make future optimization measurable.
- Reduce long-term maintenance cost in the TSF and settings code.

## Current Findings

### Package and install

- Release payload is about 559 MB.
- `rime-data` is about 432 MB.
- Fonts are about 117 MB and must remain privately loaded, not installed as
  system fonts.
- `wanxiang-lts-zh-hans.gram` is about 235 MB.
- Packaged `rime-data/build` is currently absent, so first use can require Rime
  deployment work on the user's machine.
- The MSI has several custom actions. Some are necessary for TSF registration
  and cleanup, but they need timing logs and clearer failure diagnostics.

### Startup and first input

- `RimeEngine::Initialize` prepares paths, writes managed Rime config, ensures
  Wanxiang runtime files, checks build cache, may deploy, and initializes
  librime.
- `EnsureWanxiangRuntimeFiles` copies the large grammar file into the user Rime
  directory when missing or stale. This can dominate cold start.
- TSF activation mostly avoids synchronous Rime init, but the first alphabet key
  can still fall back to synchronous initialization if warmup has not completed.

### Candidate window

- Expanding the candidate window can increase candidate count from the compact
  3-9 range to as many as 34.
- One expand action currently does candidate query, visible-count calculation,
  window layout, and full layered-window render.
- `CalculateCandidateLayout`, text measurement, fallback-font checks, and
  rendering repeat work across `RefreshCandidates`, `ShowCandidateWindow`, and
  `DrawCandidateWindow`.
- Font fallback and glyph checks are especially expensive when repeated across
  expanded candidate rows.

### Code quality and project health

- `src/tsf/tsf_text_service.cpp` is over 13k lines and owns too many UI,
  keyboard, state, and rendering responsibilities.
- `src/config_winui/main.cpp` is over 7k lines and should be split by settings
  area.
- Encoding conversion helpers are duplicated across the project.
- The updater parses JSON manually and launches downloaded MSI files without
  Authenticode verification.
- Test coverage and CI are currently insufficient for broad refactoring.

## Non-negotiable Constraints

- Do not install bundled fonts into the system font registry.
- Preserve clean uninstall behavior for private font files and app data cleanup.
- Do not introduce blocking installer custom actions for heavy Rime deployment.
- Do not do a large TSF split until performance baselines and core tests exist.
- Keep changes compatible with Windows 11 x64 and the existing CMake/MSI flow.

## Phase 0: Observability and Safety Net

Status: started in `codex/project-optimization`.

Work:

- Add timing logs to Rime initialization, deployment, and cache checks.
- Add slow-path timing logs for candidate refresh, candidate window show, and
  layered render.
- Add installer custom action timing logs for prepare/finalize steps.
- Reduce logging overhead by reusing component log file handles.
- Add a repeatable smoke script for registration, activation, Rime warmup,
  candidate query, and uninstall cleanup checks.
- Add CI build once local scripts are stable.

Acceptance:

- Logs identify whether slow first input is caused by file copy, cache miss,
  deploy, librime initialization, session creation, or candidate query.
- Logs identify whether candidate expansion is dominated by Rime query, layout,
  window positioning, or rendering.
- A release build can be produced from a clean checkout.

## Phase 1: User-Visible Performance

Work:

- Prebuild default Rime cache during packaging and include it under
  `rime-data/build`.
- Prefer direct shared-data reference or hard link for
  `wanxiang-lts-zh-hans.gram`; copy only as a fallback.
- Add a non-blocking post-install warmup command that validates or builds the
  default cache after MSI completion.
- Cache candidate layout metrics for the current candidate list and visual
  settings, then reuse them across visible-count calculation, positioning, and
  draw.
- Cache text measurement and glyph fallback decisions by text, font family,
  point size, DPI, and simplified/traditional mode.
- Keep private font loading, but make large fallback fonts lazy when possible.
- Cache expanded brand icons by DPI and theme instead of loading them on every
  draw.

Acceptance:

- Warm cache first input does not block on deploy.
- Cold first input has a visible warmup path and no long UI-thread stall.
- Candidate compact-to-expanded transition P95 is below 50 ms on the target
  test machine.
- Candidate render slow-path logs are rare during normal typing.

## Phase 2: Package and Installer

Work:

- Add package-size reporting to `scripts/package_release.py` by top-level
  payload area and largest files.
- Decide whether all dictionaries and fonts must ship in the base MSI or can be
  optional packages.
- Keep font cleanup logic, but avoid unnecessary registry/font cleanup work on
  clean installs when there is no previous footprint.
- Record install and uninstall logs to a deterministic local path.
- Split devtools install commands into narrower helpers where possible:
  registration, activation, cleanup, warmup, diagnostics.

Acceptance:

- Release artifact report shows payload size deltas in every package run.
- Clean install, upgrade, repair, and uninstall all leave no system font
  residues.
- Installer diagnostics explain custom action failures without requiring MSI log
  spelunking first.

## Phase 3: Maintainability Refactor

Work:

- Move encoding helpers into `fluent_pinyin_common`.
- Split `tsf_text_service.cpp` incrementally:
  - candidate layout and drawing
  - candidate interaction
  - toolbar window
  - status tip
  - context menu
  - key handling
  - Rime service coordination
- Split `config_winui/main.cpp` by settings page and shared UI helpers.
- Move UI layout constants into focused headers.
- Generate `constants.h` version values from CMake so product version has one
  source of truth.
- Replace local COM helper duplication with a shared helper or a small
  project-owned `ComPtr`.

Acceptance:

- No behavior change in TSF smoke tests after each split.
- Smaller files have clear ownership and no circular include churn.
- Release version cannot drift between CMake and C++ constants.

## Phase 4: Tests and CI

Work:

- Add unit tests for:
  - encoding helpers
  - path helpers
  - settings parsing and patch generation
  - Rime cache-signature logic
  - candidate page selection behavior
- Add integration smoke tests for COM registration and TSF activation.
- Add packaging smoke tests for generated payload content.
- Add GitHub Actions or equivalent CI for configure, build, and package smoke.

Acceptance:

- Core tests run locally without requiring an installed IME.
- CI catches CMake, packaging, and updater regressions.
- Risky refactors have a test harness before they land.

## Phase 5: Security and Robustness

Work:

- Verify downloaded MSI files with `WinVerifyTrust` before launching updater
  installs.
- Replace handwritten GitHub API JSON parsing with a JSON library or a stricter
  parser.
- Add sync package integrity verification, such as an authenticated tag/HMAC
  over encrypted data.
- Review PBKDF2 iteration policy and document migration behavior.
- Normalize error handling for operations that cross process, filesystem,
  registry, COM, network, or crypto boundaries.

Acceptance:

- Updater refuses unsigned or incorrectly signed MSI files.
- Sync restore rejects corrupted or tampered packages.
- Error logs include enough context to diagnose failed update/sync/install
  operations.

## Current Branch Work Items

The first implementation branch focuses on Phase 0 items:

- Reuse component log file handles in `src/common/logging.cpp`.
- Add Rime initialization and deployment timing in `src/core/rime_engine.cpp`.
- Add candidate refresh/show/render timing in `src/tsf/tsf_text_service.cpp`.
- Add installer prepare/finalize timing in `src/devtools/main.cpp`.
- Add this project optimization plan.

After the branch builds cleanly, merge it to `main` and use the new logs to
choose the next performance patch from Phase 1.
