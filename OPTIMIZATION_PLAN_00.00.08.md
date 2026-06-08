# FluentPinyin 00.00.08 Optimization Plan

This plan consolidates the external review notes, the current code audit, and
the 00.00.08 release work. It separates release-blocking fixes from longer
architecture work so the project can ship safely while still keeping the full
optimization path visible.

## Release Goals

- Ship version `00.00.08`.
- Keep the settings UI on the bundled MiSans family.
- Make CapsLock switch Chinese input directly into uppercase English and back.
- Prevent install, upgrade, and uninstall from showing Windows restart or
  shutdown prompts during normal operation.
- Keep sync credentials off plaintext HTTP transports.
- Keep CoreHost IPC scoped to the current user and verified by the TSF client.
- Use a random per-process CoreHost pipe suffix and request secret so same-user
  processes cannot control the active input core through the default pipe name.
- Keep private CoreHost instances tied to the launching TSF process lifetime.
- Make updater publisher verification fail closed unless the expected signing
  certificate SHA-256 thumbprint is configured.

## Release-Blocking Fixes

### Updater Publisher Verification

Status: fixed for 00.00.08 with release configuration required.

- `WinVerifyTrust` remains the chain trust check.
- The updater now compares the primary signer certificate SHA-256 thumbprint
  against `FP_UPDATER_PUBLISHER_THUMBPRINTS`.
- If no thumbprint is configured, the updater refuses to install the MSI.
- Release builds must pass `-DFP_UPDATER_PUBLISHER_THUMBPRINTS=<sha256>`.
- Runtime and packaging validation both require 64-character SHA-256
  hexadecimal thumbprints; empty or malformed values fail closed.
- Packaging checks that the built updater contains the same configured
  thumbprints before producing installers.

### CapsLock Mode Transition

Status: fixed for 00.00.08.

- In Chinese mode, pressing CapsLock enters ASCII mode for uppercase English.
- The status tip shows `En`.
- Pressing CapsLock again returns to Chinese only if CapsLock created the
  English state.
- Existing English mode selected by Shift or toolbar is preserved.
- The state transition is extracted into a small TSF module with unit coverage.

### CoreHost IPC Hardening

Status: fixed for 00.00.08.

- The named pipe ACL is scoped to the current user SID, system, and
  administrators.
- The pipe security descriptor includes a medium-integrity no-write-up label to
  block low-integrity same-user processes from writing to the input core.
- The TSF client verifies the connected pipe server process image path.
- TSF-launched CoreHost instances use a process-random pipe suffix instead of
  the default well-known pipe name.
- CoreHost requires an authenticated request envelope on private pipe
  instances.
- IPC request secrets are compared without early-exit string matching.
- The IPC secret is transferred through an inherited anonymous pipe handle,
  not through the process command line.
- CoreHost no longer accepts a plaintext `--ipc-secret` command-line fallback.
- CoreHost exits when the launching TSF process exits, avoiding stranded
  private input-core processes.
- The shutdown request flag is atomic so future concurrent pipe servicing will
  not race process shutdown state.
- The protocol decoder rejects oversized candidate-count arithmetic before
  multiplying field counts.
- CoreHost and TSF IPC-related diagnostics capture Win32 last-error values
  before formatting log text.

### Install, Upgrade, And Uninstall Prompts

Status: fixed for 00.00.08 normal cleanup paths.

- MSI sets `REBOOT=ReallySuppress`.
- MSI sets `MSIRESTARTMANAGERCONTROL=DisableShutdown`.
- Cleanup no longer registers undeleted files through
  `MOVEFILE_DELAY_UNTIL_REBOOT`.
- Files that cannot be removed immediately are logged and left for a future
  maintenance pass.
- Install log verification now scans for reboot/restart prompt markers.

### Settings UI Font

Status: already implemented and now guarded by package smoke checks.

- `kSettingsUiFontFamily` remains `MiSans`.
- `EnsureUiFontsLoaded()` runs before WinUI startup.
- Package smoke checks require the MiSans Regular, Medium, and Semibold fonts.
- Installed-settings verification writes the canonical `misans` candidate font
  value instead of the previous Microsoft YaHei UI value.
- The bundled font file and registry-name list now lives in
  `src/common/bundled_fonts.h`, shared by settings UI font loading, TSF private
  font loading, and install/uninstall cleanup.

## Already-Resolved Findings

- Sync remote endpoints reject plaintext HTTP by default.
- `BUILD_TESTING` is correct in the current CMake file.
- Settings executable file/product resources are synchronized to `0.0.8.0`.
- Third-party dependency roots can now be overridden through CMake cache
  variables.
- Strict MSVC warnings and Debug iterator checks are available as explicit
  CMake options.
- Release builds use external PDB output (`/Zi`) instead of object-embedded
  debug info, with PDB directories configured per build configuration.

## Architecture Optimization Plan

### Phase 1: Low-Risk Extraction

- Extract input mode transitions from `tsf_text_service.cpp`.
  Status: done for the 00.00.08 CapsLock/ASCII scope. CapsLock and ASCII toggle transitions now live in
  `src/tsf/input_mode_state.*` with focused unit coverage.
- Extract shared CoreHost IPC message framing helpers into `common`.
  Status: done. CoreHost and TSF now share length-prefixed read/write helpers.
- Extract common process module path helpers into `common/path_utils`.
  Status: mostly done for exe-hosted modules. Settings UI, toolbar host,
  devtools, and config app paths now share executable/module helpers and a
  safe system-directory current working directory helper. TSF keeps its
  DLL-module path resolver because it runs inside arbitrary host processes.
- Extract common string helpers into `common/encoding`.
  Status: mostly done for sync, TSF toolbar/status-tip parsing, settings
  hotkeys/status-tip blacklist, and devtools paths. Domain-specific lexicon and
  shortcut display trimming now lives with the TSF shortcut parser because it
  is part of the user-visible shortcut contract.
- Extract installer cleanup primitives from `devtools/main.cpp`.
  Status: done for the 00.00.08 command split. Immediate file/tree cleanup,
  safe install-directory checks, process shutdown/restart helpers, registry
  cleanup, Windows App Runtime bootstrap, Rime warmup, scheduled task cleanup,
  bundled font cleanup, and install prepare/finalize/cleanup command flow now
  live in focused `src/devtools/*_utils.*` / command modules. Reboot-deferred
  cleanup is removed from normal paths.
- Move broadcast message helpers shared by TSF, settings, and toolbar host into
  `common`.
  Status: done. TSF, settings, toolbar host, and devtools now share registered
  broadcast helpers; only the system `WM_FONTCHANGE` broadcast remains local to
  installer font cleanup.
- Move duplicated default settings into generated common constants.
  Status: done for shared cross-module setting keys in the 00.00.08 scope.
  Candidate font family, status-tip blacklist,
  fuzzy-pinyin default and common rule strings, toolbar setting keys,
  candidate-window setting keys, status-tip setting keys, default input-state
  setting keys, Rime input/lexicon mode keys, Wanxiang mode keys, sync
  configuration keys, input scheme/layout/charset values, sync interval
  range/defaults, default sync object key, and hotkey setting keys/default
  values now live in generated common constants.
- Extract settings reset defaults from settings `main.cpp`.
  Status: done. `src/config_winui/default_settings.*` now owns full reset-to-
  defaults behavior, including candidate appearance, input defaults, Wanxiang
  mode defaults, hotkey defaults, and sync defaults. The reset path uses the
  shared default sync object key directly, so the settings reset unit no longer
  links the full sync service implementation. Focused unit coverage is
  registered for final verification.
- Extract settings UI visual helpers from settings `main.cpp`.
  Status: done. `src/config_winui/settings_ui_helpers.*` now owns the MiSans
  WinUI resource dictionary, palette brushes, hairline sizing, frame helpers,
  dialog dimensions, and settings-window handle used for DPI-aware visuals and
  modal/file dialogs. This removes a broad UI foundation block from
  `src/config_winui/main.cpp` before page-level splitting.
- Extract settings navigation tag parsing from settings `main.cpp`.
  Status: done. `src/config_winui/settings_navigation.*` now owns command-line
  `--page=` parsing, alias normalization, and stable page tag constants with
  focused unit coverage registered for final verification.
- Extract settings file-dialog helpers from settings `main.cpp`.
  Status: done. `src/config_winui/file_dialogs.*` now owns the Rime dictionary
  import picker and sync backup open/save dialogs, removing direct common-file-
  dialog ownership from the settings page host.
- Extract candidate layout settings compatibility from settings `main.cpp`.
  Status: done. `src/config_winui/candidate_layout_settings.*` now owns
  reading and writing the new `candidate_layout` string while keeping the
  legacy `candidate_horizontal` flag synchronized. Focused unit coverage is
  registered for final verification.
- Move status-tip blacklist setting accessors beside blacklist parsing.
  Status: done. `src/config_winui/status_tip_blacklist.*` now owns current
  persisted blacklist item/count access in addition to token normalization,
  parsing, contains, and join helpers.
- Extract candidate font-family and size setting normalization into `common`.
  Status: done. TSF and settings UI now share
  `src/common/candidate_font.h` for the MiSans/Source Han Sans setting value
  contract, legacy `plangothic` normalization, candidate-count range, 0-3 size
  levels, display labels, and point-size mapping.
- Extract hotkey definitions and conflict checks from settings `main.cpp`.
  Status: done. `src/config_winui/hotkey_helpers.*` now owns the configurable
  hotkey definition table, default labels, and duplicate-shortcut tooltip
  logic. Settings reset and hotkey recording now read from the same definition
  list, with focused unit coverage registered for final verification.
- Extract settings choice tables from settings `main.cpp`.
  Status: done. `src/config_winui/settings_options.*` now owns the input
  scheme, double-pinyin scheme, default input mode, charset, shape,
  punctuation, candidate layout, and automatic sync interval choices. Sync
  interval parsing also uses the shared default.
- Extract fuzzy-pinyin built-in rule metadata and parsing from settings
  `main.cpp`.
  Status: done. Rule ids live in `src/common/fuzzy_pinyin.h`; settings-only
  rule labels, examples, parse, and join helpers live in
  `src/config_winui/fuzzy_pinyin_rules.*` with focused unit coverage registered
  for final verification.
- Extract Wanxiang mode metadata from settings `main.cpp`.
  Status: done. Wanxiang mode titles, setting keys, defaults, icons, and legacy
  migration keys now live in `src/config_winui/wanxiang_modes.*` with focused
  unit coverage registered for final verification.
- Move bundled font file lists into a shared common header.
  Status: done. Settings, TSF, and devtools cleanup now consume the same
  MiSans, Source Han Sans, and Plangothic font list. The settings UI MiSans
  family name also lives in the shared font header.
- Move duplicated Fluent SVG path constants into `common`.
  Status: done for shared TSF/settings icons. Emoji, moon, circle, chevron,
  candidate-page triangle, settings, refresh, repeat, checkmark, board-heart,
  and heart paths now live in `src/common/svg_icons.h`, with TSF and settings
  keeping local aliases only for call-site stability.
- Extract TSF shortcut parsing into a focused model.
  Status: done. `src/tsf/shortcut_model.*` owns shortcut display trimming,
  token normalization, virtual-key parsing, chord parsing, modifier-key
  equivalence, and modifier-key classification with focused unit coverage.
- Extract TSF font-family rules into a focused model.
  Status: done. `src/tsf/font_model.*` owns the MiSans/MiSans TC UI family
  contract, Source Han Sans candidate family selection, fallback family lists,
  and localized font-name matching with focused unit coverage.
- Extract TSF candidate layout/tool models.
  Status: done for the low-risk pure-data layer. `candidate_layout_model.h`
  owns candidate layout metrics; `candidate_layout_math.*` owns compact/
  expanded page sizing plus selectable-index filtering; `candidate_tool_model.*`
  owns candidate tool IDs, rect lookup, icon fallback rects, hit testing,
  enablement checks, and feedback rect geometry.
- Extract TSF toolbar model geometry.
  Status: done. `src/tsf/toolbar_model.*` now owns toolbar visible-item parsing
  plus item geometry, toolbar pixel sizing, drag/item hit testing, and icon
  inset rectangles with focused unit coverage.
- Share status-tip blacklist parsing and runtime process matching.
  Status: done. `src/common/status_tip_blacklist.*` now owns blacklist token
  normalization, setting parsing/joining, duplicate checks, wildcard process
  matching, and list matching. The settings UI keeps only persisted-setting
  wrappers, while TSF uses the shared runtime matcher.

### Phase 2: TSF Split

Split `src/tsf/tsf_text_service.cpp` by responsibility:

- `key_handler.cpp`: key routing, input-mode transitions, CapsLock/Shift logic.
- `composition_session.cpp`: composition lifecycle, edit sessions, commits.
- `candidate_layout.cpp`: pure candidate layout and hit testing.
- `candidate_window.cpp`: candidate window creation, positioning, painting.
- `toolbar_window.cpp`: toolbar window and toolbar menu.
- `status_tip.cpp`: status tip window and rendering.
- `context_menu.cpp`: context menu/submenu rendering and interaction.
- `display_attributes.cpp`: TSF display attributes and language bar items.

Status: in progress for 00.00.08. The highest-confidence pure model seams have
already been extracted without moving COM/window lifetimes: input mode state,
candidate layout math, candidate layout metrics, candidate tool geometry,
toolbar visible-item and geometry models, context menu command mapping,
shortcut parsing, font-family rules, and status-tip blacklist matching. The
remaining window/rendering splits
should be done after a full Release build and UI smoke pass because they touch
message dispatch, layered-window painting, and TSF edit-session lifetimes.

### Phase 3: Settings Split

Status: done for the 00.00.08 page/shell split. `src/config_winui/main.cpp`
now owns only the process entrypoint, single-instance handling, startup font
loading, auto-sync dispatch, and settings app launch.

- `settings_app.cpp`
- `general_page.cpp`
- `appearance_page.cpp`
- `lexicon_page.cpp`
- `hotkeys_page.cpp`
- `sync_page.cpp`
- `advanced_page.cpp`
- `about_page.cpp`
- `controls/setting_row.cpp`
- `settings_resources.cpp`

Remaining optional follow-up: move the settings control implementations into a
`controls/` directory and split resource/font bootstrap naming if the project
wants a deeper folder reorganization. The behavioral page split is complete.

## Final Verification Plan

These checks are final verification tasks and were not run during the
implementation pass because this work was requested without mid-run tests.

- Configure release with `-DFP_UPDATER_PUBLISHER_THUMBPRINTS=<sha256>`.
- Build Release.
- Run CTest.
- Run package smoke.
- Install 00.00.08 over 00.00.07.
- Verify no Windows restart/shutdown prompt appears.
- Verify settings UI uses MiSans.
- Verify CapsLock Chinese -> uppercase English -> Chinese.
- Verify sync rejects plaintext remote endpoints.
- Verify updater rejects unsigned, wrong-signed, and unconfigured-publisher MSI.
