# FluentPinyin Roadmap

## M0: Project Skeleton

Status: complete. Debug build and development TSF registration are validated.

- CMake workspace.
- Windows 11 x64 guardrails.
- Shared logging and path helpers.
- TSF DLL shell with COM exports.
- Core DLL placeholder.
- UI, config, and updater placeholder executables.
- TSF development diagnostics executable.
- Developer register/unregister scripts.
- Architecture documentation.

Exit criteria:

- Repository has a coherent build graph.
- TSF DLL exports COM registration functions.
- Development scripts can register and unregister a built DLL.

## M1: Empty TSF Input Method

- TSF profile registration validated on Windows 11 x64.
- Input method appears in Windows language/input list.
- Activation and deactivation events are logged.
- No Rime integration yet.

Exit criteria:

- FluentPinyin can be selected as an input method.
- No crash across Notepad and a browser text field.

## M2: librime and Frost Console Validation

Status: complete.

- Add librime dependency.
- Add Frost package installer for development.
- Validate Rime setup, initialize, deploy, and session lifecycle.
- Console test can type pinyin and print candidates/commit text.

Exit criteria:

- Frost full-pinyin works outside TSF.
- Rime data directories are stable and documented.

## M3: TSF + Rime Input Loop

Status: in progress. A minimal key sink can feed ASCII pinyin into Rime and
replace the preedit text with the first candidate on Space.

- Key event sink.
- Rime key translation.
- Composition update.
- Commit through TSF edit sessions.
- Backspace, Esc, Enter, Space, number selection, PageUp/PageDown.

Exit criteria:

- User can type Chinese text with Frost full-pinyin in Notepad.

## M4: Windows 11 Candidate UI

- Fallback candidate UI process.
- Cursor-following placement.
- Per-monitor DPI v2.
- Light/dark mode.
- Candidate selection and paging.

Exit criteria:

- Candidate window visually matches Windows 11 IME closely enough for daily use.

## M5: Installer

Status: in progress. Script-based per-user install and uninstall exist; a
polished installer package is still pending.

- One-click x64 installer.
- Install/uninstall FluentPinyin binaries.
- Register/unregister TSF.
- First-run Frost deploy.

Exit criteria:

- Clean Windows 11 x64 machine can install, type, and uninstall.

## M6: Configuration GUI

- Native Win32 settings app first; WinUI 3 polish follows after the input loop.
- Home, lexicon, updates, appearance, and advanced pages.
- Open user directory and logs.
- Redeploy action.

Exit criteria:

- Common user operations do not require file editing.

## M7: Frost Updates and Lexicon Import

Status: in progress. Update check and package download are implemented; safe
stage/deploy/rollback is pending.

- Frost release check.
- Download, verify, stage, deploy, rollback.
- Import RIME `.dict.yaml` files. Other formats are intentionally delegated to
  imewlconverter.
- Backup and restore.

Exit criteria:

- User can update Frost and manage custom vocabulary from the GUI.
