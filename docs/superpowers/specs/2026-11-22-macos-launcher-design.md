# macOS launcher with settings and cheats: design

Date: 2026-11-22. Branch `macos-arm64-m5` of the fork `executed007/iss_macos` (a GitHub fork of `HayatoG/iss_pc`).

## Intent

The user wants a launcher window to open first, instead of the game starting straight away. Two reasons:

- **Performance tinkering.** They want to adjust performance settings themselves, rather than live with the fixed steady-30 settings the .app applies today.
- **Cheats.** They want cheats such as unlimited Blast Shards, the points spent on powers, so they can buy every power.

A separate, later request: reduce the fork to a single branch, so it is not confusing. It is covered at the end.

## What exists today

- **Upstream's Qt launcher** (`port/launcher/infamous_launcher_qt.py`, QML in `port/launcher/qt/qml/Launcher/`, started by `./infamous_launcher_qt.sh`). It is the upstream default. Its screens:
  - **Play**: game folder, Continue/Play.
  - **Graphics**: window mode, VSync, FPS limit, FSR 1, sharpening, GPU accuracy, VRAM budget, shader cache, overlay, presets, and output plus internal resolution (`qt/graphics.py`).
  - **Controls**.
  - **Fixes**: BP01–BP12, A/B switches, and the in-game Insert-menu options, including the game's detail level `quality_floor`.
  - **Saves & data**: data folder, saves, backups before every start, zip export/import.
  - **Mods / Cheats**, **Support**, **Advanced** and **Updates**.

  It needs `PySide6-Essentials>=6.7` (`port/launcher/qt/requirements.txt`). PyPI has 6.12.0 for the port's Python, `port/out/pyenv` (python.org 3.15.2, universal). It has never been opened on macOS.
- **Upstream's Tk launcher** (`infamous_launcher.py`, `./infamous_launcher.sh`). Its features are a subset, and Tk 9.0 is present in the pyenv.
- **The Play path.** Both launchers call `infamous_run.prepare()` then `launch()`: the same path as the command line, which works on macOS.
- **Cheats.** `port/scripts/mod_patches.py` reads only the shadPS4/GoldHEN "PATCHES" XML (`*.xml` in the mods folder, `<Metadata AppVer=… Name=…>`, Line addresses with the eboot at 0x400000). It keeps a patch only when `AppVer` equals the game folder's `param.sfo` `APP_VER`.
  - The user's dump says `01.00`: a merged EU CUSA00004 holding the verified 1.07 executable, ELF SHA-256 `b3afd173…`, which `camera_build_supported` in `infamous_run.py` already checks. So every 1.07 cheat is hidden on it today.
- **The community cheat.** `GoldHEN/GoldHEN_Cheat_Repository`, file `json/CUSA00004_01.07.json` ("inFAMOUS™ Second Son", id CUSA00004, version 01.07, credits Weysincha). It is GoldHEN *cheat JSON*, not PATCHES XML.
  - Checkboxes: Godmode, Infinite Energy, Infinite Bombs / Rockets, Infinite Skillpoints / Shards.
  - Buttons: Max Level True Hero, Max Level Infamous.
  - Each entry has one `memory` line (offset, on bytes, off bytes).
- **How the user plays today.** Through a local, git-excluded `play-infamous.command`, or the .app built with `START=` pointing at it.
  - It keeps their progress in `port/user/savedata_player`, and swaps it into the live save folder `port/user/savedata/1/CUSA00004` only while playing.
  - The swap exists because Claude's test runs restore the live folder from `port/user/savedata_aside_105102`.
- **`user_dir()`** honours `ISS_USER_DIR`, so a test run can use its own data folder.

## Goals

1. **Launch.** The .app (built by `port/scripts/macos/make_app.sh`) and `./infamous_launcher_qt.sh` open the Qt launcher on macOS. The game starts from its Play button.
2. **Settings.** Graphics, Fixes, Controls, Saves & data and Mods / Cheats work on macOS. Settings changed there reach the game, and the user's current steady-30 settings show as the starting values.
3. **Cheats.** The community cheat file's entries appear on the Mods / Cheats screen on the user's dump, can be turned on and off, and **Infinite Skillpoints / Shards** works in game.
4. **Saves.** The user's progress is never lost, and plays as a normal save from now on.

## Non-goals

- Freezing or packaging the launcher (a spec non-goal), or a relocatable .app.
- Committing or bundling third-party cheat files: the cheat file stays in the user's own mods folder.
- Writing new cheats ourselves.
- New launcher features beyond making the existing ones work on macOS.

## Anti-goals (failure even if it "works")

- Any step that can lose or overwrite the user's progress.
- The Updates screen downloading and installing an upstream (Windows/Linux) release over the macOS port.
- Changed Linux/Windows behaviour that is not listed in README "Changes that affect all platforms".
- A cheat applied to the wrong bytes. A cheat whose original bytes do not match the user's executable must not be applied.

## Approaches

**Launcher:**
- (1) **Upstream's Qt launcher.** Every screen the user asked for; the upstream default. Costs a one-time PySide6-Essentials download into the pyenv, and it is untested on macOS. **Chosen** (Claude's call, stated to the user, not objected to).
- (2) The Tk launcher. No download, but the older fallback UI, with fewer screens.
- (3) A new launcher. Duplicates existing work: rejected.

**Cheat source** (the user chose):
- (1) **Fetch the GoldHEN community cheat file.** **Chosen by the user**, who also gave the OK to download it.
- (2) The user supplies files.
- (3) Write our own cheat.

**Cheat format** (Claude's call):
- (a) **Teach `mod_patches` to read GoldHEN cheat JSON** (`*.json` in the mods folder) next to PATCHES XML, so any GoldHEN cheat file the user drops in later works too. **Chosen.**
- (b) A one-time conversion to XML. A manual step for every future file.

**Progress handling** (Claude's call):
- (a) **Retire the swap.** Move the user's progress into the live save folder once, keep `savedata_aside_105102` as the test source, and from now on run Claude's test runs with `ISS_USER_DIR` set to their own data folder. The launcher's own save backups protect the user. **Chosen**, because it is the simplest for the user: one launcher and normal saves.
- (b) Keep the swap, and make the launcher's Play call the play script. That keeps a fragile local script in the loop.

## Design

1. **PySide6.** Install `PySide6-Essentials==6.12.0`, pinned, into `port/out/pyenv`. The README build steps say so. Python stays the pyenv's (arm64 at run time; `make_app.sh` already re-execs arm64).
2. **The Qt launcher on macOS.**
   - Fix what stops it working on macOS, every fix `darwin`-gated: fonts, paths, the game process launch and detach, opening files and folders with `open`, window and menu behaviour.
   - Upstream's `--selftest` (opens the launcher off screen) is part of the verification.
   - The Graphics screen must show the internal resolution, and the Fixes screen must show `quality_floor` and the FPS limit, as the user tunes them.
3. **Updates screen.** On macOS it must never download or install an upstream package. It shows the installed version and says that macOS updates come from this fork (pull and rebuild). Darwin-only.
4. **Version gate for mods.** When `param.sfo` says CUSA00004 `01.00`, but the eboot is the verified 1.07 executable (the same hash check `camera_build_supported` uses), mods use version `01.07`.
   - This is a shared behaviour change, like Rulings AI and AT. List it in README "Changes that affect all platforms".
5. **GoldHEN cheat JSON in `mod_patches`.**
   - Read `*.json` files in the mods folder that have `id`, `version`, `process == "eboot.bin"` and `mods`.
   - Each mod becomes one selectable patch, keyed `<file>/<name>`, as XML patches are. The `id` must be one of the game's title IDs, and the `version` must match (after rule 4).
   - Each `memory` line writes its `on` bytes at eboot address 0x400000 + `offset`, the same convention as the XML Lines.
   - **Safety:** a mod is offered only when every line's `off` bytes equal the executable's bytes at that address. A mismatch skips that mod, with a log line.
   - Buttons: offer them only if they are code patches that pass the same check. Otherwise skip them, and say so in the Mods screen note or the log.
   - Unit tests cover parsing, the version and ID filters, the off-bytes check (match and mismatch), and lines outside the executable segments.
6. **Fetch.** Download `json/CUSA00004_01.07.json` from `GoldHEN/GoldHEN_Cheat_Repository`, pinned to a commit, into the user's mods folder (`port/user/mods/`, git-ignored). Never commit it.
   - The README says where such files come from, and credits the repository and the author named in the file.
7. **Progress migration** (one time, done by Claude with checks):
   - back up the current live folder;
   - copy `savedata_player` into the live folder and verify with `diff -rq`;
   - keep `savedata_player_backups` and `savedata_aside_105102`.
   - From then on the live folder holds the user's progress.
   - The play script is no longer used by the .app. It stays on disk, unused, unless the user asks to delete it.
   - Claude's memory note `play-script-and-test-safety` is updated: test runs use `ISS_USER_DIR=<a test data folder>` (a copy of `port/user` with the aside save, its shader cache shared via symlink), and never `port/user`.
8. **The .app.** `make_app.sh` builds an app that opens the Qt launcher by default. `START` stays as an option. Rebuild the user's app without `START`.
9. **Starting settings.** The user's current `tlg.ini` already holds the steady-30 lines (35%, quality level 8, FSR 1, RCAS, cap 30). The launcher shows and keeps them, and nothing resets them.

## Verification

- **Launcher.**
  - `./infamous_launcher_qt.sh --selftest` passes on macOS.
  - The launcher opens from the .app, with screenshots of each screen.
  - A setting changed in the launcher lands in `tlg.ini`.
- **Game.**
  - Play starts the game from the launcher (one run on a test data folder via `ISS_USER_DIR`, never `port/user`).
  - The game window close ends cleanly, with no error dialog.
- **Cheats.**
  - On the user's dump, the community file's entries appear on Mods / Cheats.
  - With Infinite Skillpoints / Shards on, the log shows the patch applied at the expected address with the expected bytes.
  - The in-game effect (shards not decreasing when buying a power) is confirmed by the user, unless the builder finds an automated way.
- **Regression.** Linux/Windows unchanged except listed changes. `./build.sh --test` and the launcher tests pass.

## Checked before planning (2026-11-22, read-only)

- **The community cheat matches the user's executable.** The file was fetched, with the user's OK, to the session scratchpad only. The executable was read with `infamous_patches.read_eboot_elf`. Its PT_LOAD segments are code at vaddr 0x0 (file 0x4000, flags R+X) and data at 0xda0000.
  - For each checkbox cheat, the `off` bytes equal the executable's bytes at vaddr = `offset`, in the code segment. This confirms the XML convention: address 0x400000 + offset.
  - The four matching cheats: Godmode (0x215c17), Infinite Energy (0x20c7f9), Infinite Bombs / Rockets (0x1e35a8), Infinite Skillpoints / Shards (0x1e22a4). The shards patch sets the count to 999 whenever the game writes it.
  - The two buttons, Max Level True Hero and Max Level Infamous, both write at 0xefadb8. That address is runtime data past the file-backed data, and their on and off bytes are equal. A start-up patch cannot do them, so the loader skips them and the Mods screen or the log says so.
  - **Fallback**, should a future file not match: the off-bytes check skips it. Nothing is written, and the log names it.
- **Built-in mods after the version gate.** `BUILTIN_MODS` is `port/patches/iss_builtin_intro.xml`, with "Skip intro movie" and "Disable motion blur". Both are `AppVer="01.07"` and `isEnabled="false"`, and there is no `mods.json` yet.
  - So the gate only makes them *available*, off, on the user's dump. With nothing enabled, the applied patch set must stay byte-identical before and after (a pass criterion).
- **PySide6.** `pyside6_essentials-6.12.0-cp310-abi3-macosx_14_0_universal2.whl` is 116.0 MB on PyPI, plus its small `shiboken6` dependency.

## Claude's calls (for the user to check)

- **Progress migration timing.** Claude does it after the launcher passes its checks, and just before the .app switches to it.
  - **Stop and ask** if a game is running, `.player_save_in_play` exists, the live folder differs from `savedata_aside_105102`, or `savedata_player` is missing.
  - The source of truth is `savedata_player`. The play script restores the live folder to the aside after every session, which was verified on 2026-11-21.
- **`tlg.ini` is untouched by opening the launcher.** Opening it and pressing Play without changing anything must leave the file byte-identical (checked with `cmp`). The launcher must not apply a preset by itself on first run.
- **Buttons skipped.** Runtime-data cheats (like the Max Level buttons) are out of scope. They need a live memory writer.

## Left to the builder

- The exact macOS fixes the launcher needs.
- How the Updates screen is gated.
- The JSON parser's shape inside `mod_patches`.
- How buttons are handled.
- How the test data folder is laid out.

## Afterwards: one branch on the fork

The fork has 21 branches; its default is `macos-arm64-m5`. The other 20 are copies of upstream's branches. Proposal:
- delete the other 20 on the fork;
- optionally rename `macos-arm64-m5` to `main`.

Claude shows the exact list and asks before deleting, since deleting branches on GitHub is outward-facing and the user confirms the list. Local remote-tracking refs, and the memory note naming the branch, are updated after a rename.
