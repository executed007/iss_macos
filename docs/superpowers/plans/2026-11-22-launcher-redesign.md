# Launcher Redesign (Apple Style) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the Qt launcher's screens to look and behave like a native Mac app (System Settings style), keeping every setting, cheat and save action working as it does today.

**Architecture:**
- Replace the QML layer (`port/launcher/qt/qml/Launcher/`) with a sidebar-and-panes app built from a small set of Apple-style components, using Qt Quick Controls' `macOS` style.
- The Python backend (`port/launcher/qt/*.py`) keeps its behaviour. It gains only a key-art property, a Mac-only preset list, and the English-only and no-update-check switches.

**Tech Stack:** PySide6-Essentials 6.12.0 (Qt Quick, Qt Quick Controls `macOS` style), Python 3.15 (`port/out/pyenv`), unittest, Lucide SVG icons (ISC).

**Spec:** `docs/superpowers/specs/2026-11-22-launcher-redesign-design.md` (approved by the user 2026-11-22)

## Global Constraints

- **Mac only.** The fork is macOS-only (user, 2026-11-22). No platform gating is needed.
- **Behaviour unchanged:** every control writes the same file, key and value as the Task 2 launcher does (`tlg.ini`, `mods.json`, the launcher's `settings.json`). Backend behaviour tests (settings, mods, saves, play) stay green without edits.
- **Visual rules, from the spec:**
  - One typeface, the system font, with the type scale body 13 pt, captions 11 pt and pane titles 20 pt medium.
  - Colors only from the semantic tokens in `Theme.qml` (light and dark).
  - One accent: the system accent, else `#0A84FF`.
  - No gradients, glows, content shadows or emoji. The only image is the game art.
  - Text contrast at least 4.5:1 in both appearances, visible keyboard focus, and an 8 pt spacing grid.
  - Window default 1080×720, minimum 900×600.
- **User data:** never touch `port/user/` or `Game/`, beyond reading the game folder's `sce_sys/pic*.png` at run time. Launcher runs set `ISS_USER_DIR=<test data folder>` and `XDG_CONFIG_HOME=<test config folder>` (the launcher's `settings.json` lives in `~/.config/infamous-launcher` otherwise). Copy the test data folder layout from `/private/tmp/claude-501/-Users-ziad-Documents-SecondSonMac/d3d80260-f0d9-48d0-b134-56eda303d36a/scratchpad/launcher/`.
- **The user may be playing:** no launcher or game run while a `tlg-probe` process runs or `port/user/.player_save_in_play` exists.
- **No shader-recompiler or game-binary change.** This plan touches only the launcher.
- **Pushing:** implementers never push. The controller pushes after each reviewed task.

## Review Focus

1. **The minimum window size, 900×600:** nothing clipped. Play and its pane are fully visible, and long labels wrap or elide with the full text in a tooltip. Screenshots at 900×600 in every task.
2. **Dark appearance:** every text, separator and control is readable. Screenshots in light and dark in every task.
3. **Keyboard:** Tab reaches every control in visual order, focus is visible, and Return on the Play pane starts Play. Check in Task 1 for the shell, and in each pane task for its pane.
4. **No game folder, or no art:** the Play pane shows a plain art area, and the primary button reads "Choose game folder…". Test in Task 2.
5. **A stored preset that no longer exists** (AMD, RX580, renderer v2, APolar, "Currently recommended") shows "Custom" with no error. Test in Task 3.

---

### Task 1: Theme, window, sidebar and components

**Files:**
- Modify: `port/launcher/infamous_launcher_qt.py`
  - set `QQuickStyle.setStyle("macOS")` before the engine loads;
  - stop loading the bundled Barlow / Big Shoulders fonts.
- Rewrite: `port/launcher/qt/qml/Launcher/Theme.qml`. Semantic tokens with light and dark values:
  - `windowBg`, `sidebarBg`, `groupBg`, `separator`, `textPrimary`, `textSecondary`, `accent`;
  - type sizes `body` 13, `caption` 11, `title` 20;
  - spacing `s1` 8, `s2` 16, `s3` 24;
  - `radius` 10.
  - Dark follows `Qt.styleHints.colorScheme`.
- Modify: `Window.qml` / `Main.qml`:
  - 1080×720 default, 900×600 minimum;
  - unified title bar (best effort; standard title bar acceptable);
  - a sidebar and a pane stack.
- Create:
  - `Sidebar.qml`: items Play, Graphics, Controls, Cheats, Saves, Advanced, About, each with a Lucide line icon;
  - `Pane.qml`: a title plus a scrolling column;
  - `Group.qml`: the inset grouped list;
  - `SettingRow.qml`: label, optional secondary line, a trailing control slot, and hairline separators between rows;
  - `SectionHeader.qml`;
  - `Footnote.qml`;
  - `icons/*.svg` (Lucide) with `icons/LICENSE` (ISC).
- Test: `port/tests/test_qt_launcher.py` (add `test_sidebar_order`, `test_theme_has_light_and_dark`); `--selftest`.

**Interfaces:**
- **Produces** QML types `Pane { title: string; default property alias content }`, `Group { default property alias rows }`, `SettingRow { label: string; detail: string; default property alias trailing }`, `SectionHeader { text }` and `Footnote { text }`, plus the `Theme` singleton with the tokens above.
- **Produces** `Sidebar { model: [{key, label, icon}]; currentKey: string }`. Keys: `play graphics controls cheats saves advanced about`.
- Until Tasks 2–5 replace them, the old pages load inside the new shell under their new sidebar keys, so the app works between tasks.

- [ ] **Step 1: Write the failing tests.**
  - `test_sidebar_order`: load `Main.qml` off screen (as `--selftest` does) and assert the sidebar keys equal `['play', 'graphics', 'controls', 'cheats', 'saves', 'advanced', 'about']`.
  - `test_theme_has_light_and_dark`: each token resolves to a different value under `colorScheme` light and dark, except `accent` and the sizes.
- [ ] **Step 2: Run** `cd port/tests && ../out/pyenv/bin/python -m unittest test_qt_launcher -v`. Expected: the two new tests FAIL.
- [ ] **Step 3: Implement** the theme, window, sidebar and components. Wrap the old pages in `Pane`.
- [ ] **Step 4: Run** the Qt tests, then `ISS_USER_DIR=… XDG_CONFIG_HOME=… ./infamous_launcher_qt.sh --selftest`. Expected: pass, `selftest ok`.
- [ ] **Step 5: Screenshots** of the shell in light and dark, at 1080×720 and 900×600. Check Tab order through the sidebar.
- [ ] **Step 6: Commit:** `launcher: Apple-style theme, window, sidebar and grouped-list components`

### Task 2: Play pane

**Files:**
- Modify: `port/launcher/qt/backend.py`. Add `keyArtUrl` (`Property(str, notify=gameDirChanged)`): the file URL of the first of `sce_sys/pic1.png`, `sce_sys/pic0.png` and `sce_sys/icon0.png` that exists in the game folder, else `""`.
- Create: `PlayPane.qml`, replacing `HomePage.qml`.
- Test: `port/tests/test_qt_launcher.py`.

**Interfaces:**
- **Consumes:** Task 1's components, and the existing backend properties for the game folder, save and continue text, play state, status (GPU, version, shader cache), and game language (`gameLanguage*`, `backend.py:746-757`).
- **Produces:** `backend.keyArtUrl: str`.

- [ ] **Step 1: Write the failing test** `test_key_art_fallback`. With a temp game folder:
  - holding only `sce_sys/pic0.png` and `sce_sys/icon0.png`, `keyArtUrl` ends with `/sce_sys/pic0.png`;
  - with none of the three, it is `""`;
  - with no game folder, it is `""`.
- [ ] **Step 2: Run it.** Expected: FAIL (no property).
- [ ] **Step 3: Implement `keyArtUrl` and `PlayPane.qml`:**
  - art at about 40% of the pane height, with a radius of 10;
  - the title "inFAMOUS Second Son";
  - "Continue · <save>" or "New game";
  - one accent capsule button, "Play", or "Choose game folder…" when the folder is not valid (Review Focus 4);
  - a status `Group`: Mac, Game, Shader cache, Cheats ("N on"), Language (a pop-up bound to the game-language list);
  - a problem shows as one row with a "Choose…" button.
- [ ] **Step 4: Run** the Qt tests and `--selftest`. Expected: pass.
- [ ] **Step 5: Screenshots** in light and dark, at both sizes, with the test folder's art, then with a game folder that has no art and then none at all. Check that Return starts Play, without running it: assert the play slot is invoked, through the selftest hooks or a mock.
- [ ] **Step 6: Commit:** `launcher: Play pane with the game's key art and one Play button`

### Task 3: Graphics and Controls panes

**Files:**
- Modify: `port/launcher/qt/graphics.py`. The pane's preset list holds only the three M5 Air presets (`presets_m5air.M5AIR_PRESETS`) plus "Custom". "Custom" is current whenever the values match none of them, including a stored id of a removed preset.
- Create: `GraphicsPane.qml` and `ControlsPane.qml`, replacing `GraphicsPage.qml` and `ControlsPage.qml`.
- Test: `port/tests/test_qt_screens_a.py` (or the module holding the graphics tests).

**Interfaces:**
- **Consumes:** the existing graphics and controls backend items (rows, values, setters).
- **Produces:** the graphics backend's preset list for the pane, as `[{id, name, what}]` + `{id: 'custom'}`, and the current preset id.

- [ ] **Step 1: Write the failing tests.**
  - `test_mac_presets_only`: the pane's preset ids equal `['m5air_balanced', 'm5air_performance', 'm5air_battery', 'custom']`, in that order.
  - `test_removed_preset_shows_custom`: a settings file with preset `rx580` makes the current preset `custom`, and no exception is raised.
  - `test_controls_write_same_keys`: for each Graphics and Controls row, setting a value through the backend writes the same `tlg.ini` key and value as the Task 2 backend. Use a temp `ISS_USER_DIR`, and compare against a table of expected key/value pairs taken from the current code.
- [ ] **Step 2: Run them.** Expected: the first two FAIL; the third passes or fails only where the new list changed something.
- [ ] **Step 3: Implement the backend list, then the panes.**
  - **Graphics, Display group:** window mode, VSync, overlay.
  - **Graphics, Performance group:** Preset, internal resolution (output plus percent), FSR 1, sharpening, frame rate limit, detail level with a one-line description.
  - **Graphics footnote:** "About 30 FPS in most scenes at the Balanced settings on this Mac."
  - **Controls:** mouse camera (sensitivity, invert), stick dead zone, keyboard map, and a "Controller guides" row that opens the existing guide images.
- [ ] **Step 4: Run** the Qt tests and `--selftest`. Expected: pass.
- [ ] **Step 5: Screenshots** in light and dark, at both sizes. Check the Tab order on both panes.
- [ ] **Step 6: Commit:** `launcher: Graphics and Controls panes as grouped settings`

### Task 4: Cheats and Saves panes

**Files:**
- Create: `CheatsPane.qml` (replacing `ModsPage.qml`) and `SavesPane.qml` (replacing `SavesPage.qml`).
- Test: the existing mods and saves tests, unchanged.

**Interfaces:**
- **Consumes:** the existing mods model (keys, names, notes, enabled, toggles) and the saves backend (list, backups, export, import, data folder).

- [ ] **Step 1: Cheats.**
  - Switch rows: community cheats first, then the built-ins (Skip intro movie, Disable motion blur, Camera improvements).
  - Each row's secondary line is the cheat's note, if it has one.
  - Footnote: "From the GoldHEN community cheat list. Changes apply the next time you press Play."
  - A "Show cheats folder" button opens the mods folder in Finder.
- [ ] **Step 2: Saves.** The data folder, the save list, backups, and export and import, as grouped rows with buttons, using the existing slots.
- [ ] **Step 3: Run** the Qt tests, the mods tests (`test_goldhen_cheats`, `test_camera_mod_selection`) and `--selftest`. Toggle a cheat through the pane in the test folder, and check that `mods.json` changes exactly as `set_mod_enabled` does.
- [ ] **Step 4: Screenshots** in light and dark, at both sizes, with the test folder's community cheats listed.
- [ ] **Step 5: Commit:** `launcher: Cheats and Saves panes`

### Task 5: Advanced, About, and removals

**Files:**
- Create: `AdvancedPane.qml` and `AboutPane.qml`.
- Modify: `port/launcher/qt/backend.py` and `updates.py`:
  - `ui_language` is fixed to English, and `setLang` is not exposed to QML;
  - no update check runs at start or on demand. The code stays, but nothing calls it.
- Delete: QML no longer referenced (`HomePage`, `GraphicsPage`, `ControlsPage`, `FixesPage`, `SavesPage`, `GuidesPage`, `SupportPage`, `UpdatesPage`, `ModsPage`, `Rail`, the old styled controls), and the Barlow / Big Shoulders font files with their licence files.
- Modify: `port/launcher/qt/i18n/strings.json`. New English strings only; PT-BR entries of deleted screens may go.
- Test: rewrite tests that assert on removed pieces (languages, news, the Updates page, other-GPU presets). Backend behaviour tests stay unchanged.

**Interfaces:**
- **Consumes:** the fixes and A/B rows, the in-game menu rows not on Graphics, detailed log, light glare (occlusion), shader cache actions, the problem report export, and the version.

- [ ] **Step 1: Write the failing test** `test_no_update_check`: with the backend constructed, after `startProbes` / `checkOnOpen`, the release fetch function was never called. Also assert `setLang` is absent from the QML-visible slots.
- [ ] **Step 2: Run it.** Expected: FAIL.
- [ ] **Step 3: Implement.**
  - **Advanced:** collapsed groups, in this order: in-game menu options not on Graphics; fixes (the profile pop-up plus BP01–BP12); A/B switches; diagnostics (detailed log, light glare); shader cache; "Report a problem".
  - **About:**
    - the version;
    - "macOS port: github.com/executed007/iss_macos";
    - "Updates: pull the latest version and rebuild.";
    - credits: "Based on iss_pc by HayatoG" with their donation link, the GoldHEN cheat repository, satperm's MoltenVK patches, and the Lucide licence.
  - Remove the language switch and the deleted screens.
- [ ] **Step 4: Run** all Qt tests, `--selftest`, and `./build.sh --test` (the launcher tests only, if it is long). Expected: pass.
- [ ] **Step 5: Full screenshot pass.** All seven panes in light and dark, at 1080×720 and 900×600: 28 shots in a contact sheet, checked against the spec's visual rules line by line in the report.
- [ ] **Step 6: Commit:** `launcher: Advanced and About panes; English only; no update check; old screens and fonts removed`
