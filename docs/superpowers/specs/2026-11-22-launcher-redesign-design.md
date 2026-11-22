# Launcher redesign (Apple style): design

Date: 2026-11-22. Fork `executed007/iss_macos`, which is a **macOS-only fork**: there is no Linux or Windows target. This builds on the launcher spec `2026-11-22-macos-launcher-design.md`, which made upstream's Qt launcher run on macOS.

## Intent

The user finds the launcher's look "AI generated":
- a stencil display font (Big Shoulders Stencil) over Barlow;
- a slogan banner ("DELSIN NEEDS YOU");
- oversized cards, walls of text, and an accent color that changes between pages.

They want it to feel like a real Mac app, inspired by Apple's design: sleek, calm and native, in the spirit of macOS System Settings and the App Store / Apple Arcade game pages. The launcher is where they start the game, tune performance and switch cheats.

## Goals

1. **It looks and behaves like a native Mac app.**
   - The system font (SF Pro), with Apple's type scale: body 13 pt, secondary and captions 11 pt, pane titles 20 pt medium.
   - A sidebar with simple line icons, and grouped inset lists (label on the left, control on the right) with hairline separators.
   - Native controls.
   - One accent color: the Mac's system accent, falling back to Apple blue.
   - Light and dark appearance following macOS.
   - An 8 pt spacing grid.
2. **The game provides the color.** The Play screen shows the game's own key art, read at run time from the user's game folder (`sce_sys/pic1.png`, else `pic0.png`, else `icon0.png`; never copied into the repo). It has one primary action, Play.
3. **Nothing irrelevant to this Mac fork is on screen.**
4. **Every setting, cheat and save action that works today still works,** and writes the same files (`tlg.ini`, `mods.json`, the launcher settings).

## Non-goals and anti-goals

- **Non-goal:** new launcher features or new settings.
- **Non-goal:** a Swift or SwiftUI app.
- **Anti-goal:** the "AI" tells come back: decorative display fonts, slogans, gradients or glow, emoji icons, color-coded pages, or more than one primary button per screen.
- **Anti-goal:** a setting that silently stops reaching the game, or a changed file format.
- **Anti-goal:** clipped controls at the minimum window size (the Play button is cut off today).

## Approaches

1. **A Mac-only redesign of the launcher's screens.** **Chosen by the user.** Since the fork is macOS-only, this means redesigning the existing QML in place, with no platform gating.
2. Restyle upstream's QML while keeping it for other platforms. Moot: there are no other platforms.
3. A separate native Swift app driving the Python backend. Rejected: a second codebase.

## Design

### Technology

- Keep the Qt Quick launcher and its Python backend (`port/launcher/qt/*.py`). Only the QML (`port/launcher/qt/qml/Launcher/`) and the theme change, plus small backend additions the new screens need, such as the key-art path.
- Use Qt Quick Controls' **macOS** style, which ships with the installed PySide6-Essentials 6.12.0 (verified: the `macOS` style is present), for switches, pop-up buttons, sliders, buttons and text fields. The toggle and select components are drawn by the system style.
- Use the system font only. Remove the bundled Barlow and Big Shoulders fonts from use; the files can stay or go.
- Icons: one line-icon set with an SF Symbols feel, bundled as SVG under a permissive licence (Lucide, ISC, or Phosphor, MIT), with its licence file. The builder picks one.

### Window

- Unified title bar (traffic lights over the sidebar), as in System Settings.
- Default size 1080×720, minimum 900×600. Everything must fit at the minimum.

### Screens (sidebar order)

1. **Play** (home).
   - Key art at the top, about 40% of the pane height, with rounded corners.
   - Under it: the game title, then "Continue · <save name>" (or "New game" when there is no save).
   - A single accent capsule **Play** button.
   - A short grouped status list: Mac (chip and Metal), Game (title ID and version), Shader cache (state and size), Cheats ("N on").
   - Problems (a missing game folder, a wrong version) appear as one row with a "Choose…" button, not as a banner.
2. **Graphics.**
   - **Display:** window mode, VSync, overlay or FPS counter.
   - **Performance:** a Preset pop-up (the three M5 Air presets plus Custom), internal resolution (output plus percent), FSR 1 upscaling, sharpening, frame rate limit, and detail level (`quality_floor`, with a one-line description of what the level changes).
   - A footnote: the measured result at the recommended settings ("About 30 FPS in most scenes on this Mac"). Only the measured numbers already in the README are used.
3. **Controls.** Mouse camera (sensitivity, invert), stick dead zone, and the keyboard map. A "Controller guides" row opens the existing guide images.
4. **Cheats.**
   - The cheat list as switch rows: the community GoldHEN cheats, then the built-in options (Skip intro movie, Disable motion blur, Camera improvements).
   - A footnote: the source (the GoldHEN community list), that changes apply on the next Play, and how to add cheat files (a "Show cheats folder" button).
5. **Saves.** Data folder, the save list, backups, export and import (the existing actions), as grouped rows with buttons.
6. **Advanced.** Everything expert, in collapsed groups: the in-game menu options not already on Graphics, fixes BP01–BP12, A/B switches, detailed log, light glare (occlusion), shader cache, and a "Report a problem" log export.
7. **About.**
   - Version.
   - "macOS port: github.com/executed007/iss_macos". The updates note lives here: pull and rebuild, no in-app updater.
   - Credits: "Based on iss_pc by HayatoG", with their donation link, the GoldHEN cheat repository, satperm's MoltenVK patches, and the icon set's licence.

**Removed from view:** the PT-BR/EN-US switch (the language follows the system), the "What's new" panel, the "Buy me a coffee" button next to Play (moved to About), presets for other GPUs (AMD, RX580, renderer v2, APolar, "Currently recommended"), the separate Support, Updates and Controller guides pages (folded in as above), and the slogan.

### Visual rules (the reviewer's checklist)

- **One typeface:** the system font. Two weights in normal use (regular and medium; semibold only for pane titles). No all-caps headings: section headers are small regular secondary-color text.
- **Colors** come from semantic tokens in one theme file, with light and dark values: window background, sidebar, grouped background, separator, primary and secondary text, accent. No raw colors in screens.
- **Contrast:** at least 4.5:1 for text in both appearances. Keyboard focus is visible, and every control is reachable by keyboard.
- **No** gradients, glows, drop shadows on content, or emoji. The only image is the game art.

## Verification

- Screenshots of every screen in **light and dark**, at the default and the minimum window size, checked against the visual rules above.
- `./infamous_launcher_qt.sh --selftest` passes, and the launcher's Python tests pass.
- Changing each Graphics setting writes the same `tlg.ini` key and value as before (a before/after diff for each control).
- Cheats switches write `mods.json` as before.
- Play still starts the game.
- The user's own check: they open it and judge the look.

## Claude's calls (for the user to check)

1. **English only.**
   - The redesigned launcher's own text is English. The PT-BR strings and the PT-BR/EN-US switch go: the `ui_language` setting stays at English, and `setLang` is no longer exposed.
   - **Why:** the user works in English, the fork is personal and Mac-only, and keeping Portuguese would double every new string.
   - The **game's** voice and text language is a different setting, and it stays (next item).
2. **Game language** (voice and text, `game_language`, with "Automatic") is a pop-up row in the Play screen's status group: "Language · Automatic (English)".
3. **Presets.**
   - The Graphics Preset pop-up lists only the three M5 Air presets, plus "Custom". "Custom" shows whenever the current values match none of them, including a stored selection that points at a removed preset.
   - No migration is needed: presets only set values, and the values stay as they are.
   - The separate *fixes* profile ("Recommended", which drives BP01–BP12) moves to Advanced unchanged.
   - The baseline for the "same key and value" check is the `tlg.ini` written by the Task 2 launcher.
4. **Updates.** No update check runs, at start or ever. The backend code stays dormant, with no UI and no network call, so its tests still import it. About says how updates work.
5. **Play with no game folder or no art.**
   - The art area shows the window background with the game title, and the primary button becomes "Choose game folder…". Play replaces it once the folder is valid. It fits at 900×600.
   - Missing art with a valid folder shows the same plain area, with Play.
6. **Tests.**
   - Tests that assert on removed UI pieces (languages, the news panel, the Updates page, other-GPU presets) are rewritten to the new design. Tests of backend behaviour (settings, mods, saves, play) must stay green unchanged.
   - The baseline is Task 2's final reviewed commit. The redesign starts only after Task 2 is merged, so there is no parallel editing.
7. **Unified title bar is best effort.** A standard macOS title bar is an acceptable fallback if the expanded client area misbehaves (drag region, overlap).

## Left to the builder

- Component structure.
- The icon set (Lucide or Phosphor).
- The exact key-art crop.
- How the collapsed Advanced groups look.
- Which existing backend properties each row binds to.
