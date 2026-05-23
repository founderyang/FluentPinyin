# FluentPinyin Architecture

FluentPinyin v0.1 targets Windows 11 x64 only. The product goal is a Windows-native
Rime frontend that feels like the built-in Windows 11 input method, while using
Frost Rime as the default full-pinyin schema.

## Product Scope

Included in v0.1:

- Windows 11 x64 only.
- TSF input method, no IMM or legacy Windows support.
- librime backend.
- Frost Rime full-pinyin default schema.
- Candidate UI visually aligned with Windows 11 built-in IME.
- Native configuration app first; WinUI 3 polish after the input loop is stable.
- One-click installer and uninstaller.
- FluentPinyin update check.
- Frost schema update check and deploy flow.
- Custom lexicon import for RIME `.dict.yaml` files.
- Local-first privacy model: input text is processed by local RIME by default;
  update checks and downloads are explicit user actions.

Deferred:

- Wanxiang.
- Local grammar models.
- LLM providers.
- Double-pinyin GUI options.
- Auxiliary-code GUI options.
- Theme marketplace.
- Cloud sync.

## Process Model

```text
fp-tsf.dll
  Loaded by client applications through TSF.
  Owns COM activation, TSF lifecycle, key events, composition, commit, and
  candidate model forwarding.

fp-core.dll
  Owns Rime adapter, schema management, lexicon import, and config state.

fp-ui.exe
  Native fallback candidate UI.
  Uses Win32, Direct2D, DirectWrite, and DirectComposition.

fp-config.exe
  Future WinUI 3 settings app.
  Owns user-facing settings, lexicon import UI, update UI, and diagnostics.

fp-updater.exe
  Owns update checks, downloads, verification, staging, deploy, and rollback.

installer
  Owns install, uninstall, TSF registration, dependency deploy, and first-run
  setup.
```

The TSF DLL must stay small. It is loaded into arbitrary applications, so it
must not perform network I/O, long-running work, package downloads, or complex
UI initialization.

## TSF Layer

The TSF layer currently has a minimal `ITfKeyEventSink` path that can feed ASCII
pinyin into Rime and commit candidates. Later milestones will harden:

- Standard TSF composition lifecycle instead of the current text replacement
  bridge.
- Composition sink support.
- Candidate list model forwarding.
- UIElement / UILess integration where the host can render candidates.

When the host cannot render the candidate list, `fp-ui.exe` provides the
fallback window.

## Rime Layer

The core library exposes a thin adapter around librime. The first real adapter
milestone will cover:

- `setup`
- `initialize`
- `create_session`
- `process_key`
- `get_context`
- `get_commit`
- `destroy_session`
- `finalize`

The Frost schema is installed and updated separately from user customizations.
FluentPinyin must never edit upstream Frost files in place.

## Candidate UI

The candidate UI target is Windows 11 built-in IME behavior and visual rhythm:

- Follow system light/dark mode.
- Use system fonts.
- Keep compact candidate rows.
- Match Windows 11 corner radius, shadow, spacing, and selection colors.
- Support per-monitor DPI v2.
- Support multi-monitor placement.
- Avoid custom branding in the candidate window.

This UI is fallback-only where TSF host/system integration cannot render the
candidate list.

## Configuration App

The configuration app is WinUI 3 in the final product. The current binary is
native Win32 and already exposes the core MVP actions.

v0.1 pages:

- Home: input method status, current schema, redeploy, open user directory.
- Lexicon: import RIME `.dict.yaml`, import history, undo latest import,
  backup, restore.
- Updates: FluentPinyin version, Frost version, check now, update now, auto-check.
- Appearance: follow system theme, candidate count, horizontal/vertical layout.
- Advanced: logs, reset, export diagnostics.

## Update Model

There are two independent update tracks:

- FluentPinyin application components.
- Frost schema package.

The updater stages downloads under `%LOCALAPPDATA%\FluentPinyin\Packages`, verifies
them, backs up the current package, switches atomically where possible, redeploys
Rime, and rolls back on failure.

Program updates must require user confirmation. Frost package downloads may be
automatic, but deployment should be explicit in v0.1.

## Data Layout

```text
%ProgramFiles%\FluentPinyin\
  fp-tsf.dll
  fp-core.dll
  fp-ui.exe
  fp-config.exe
  fp-updater.exe
  librime.dll
  shared-data\

%APPDATA%\FluentPinyin\Rime\
  default.custom.yaml
  rime_frost.custom.yaml
  fp_user_dicts.txt
  fp_frost.dict.yaml
  <imported>.dict.yaml

%LOCALAPPDATA%\FluentPinyin\Packages\frost\
  current\
  previous\
  staging\
  manifest.json
```

Development builds use the CMake build output and per-user registration helper
scripts.

## Main Risks

- TSF compatibility across applications.
- Candidate placement under per-monitor DPI.
- Safe update/rollback while IME components are loaded.
- Frost licensing and package provenance.
- Avoiding user customization loss during schema updates.
