# macOS Launcher with Settings and Cheats Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The .app opens upstream's Qt launcher on macOS, with its settings screens and a Mods / Cheats screen that offers the community GoldHEN cheats (Infinite Skillpoints / Shards and others) on the user's CUSA00004 dump.

**Architecture:**
- Reuse upstream's Qt launcher (`port/launcher/infamous_launcher_qt.py` + QML), running in the port's own Python (`port/out/pyenv`) with PySide6-Essentials, and fix only what macOS needs.
- Extend the generic cheat loader `port/scripts/mod_patches.py` to read GoldHEN cheat JSON next to PATCHES XML, guarded by an original-bytes check.
- Give the verified EU dump (`APP_VER 01.00`, 1.07 executable) version 01.07 for mods.

**Tech Stack:** Python 3.15 (`port/out/pyenv`), PySide6-Essentials 6.12.0 (Qt Quick), unittest, bash (`make_app.sh`).

**Spec:** `docs/superpowers/specs/2026-11-22-macos-launcher-design.md`

## Global Constraints

- **Linux and Windows unchanged:** every edit is inside `sys.platform == 'darwin'` / `__APPLE__` / `Darwin)` branches, or is a shared change listed in README.md "Changes that affect all platforms (for upstream)". The GoldHEN JSON reader and the 01.07 mods version for verified CUSA00004 dumps are such shared changes.
- **No game, firmware or third-party cheat file is committed or bundled.** The community cheat file lives only in the user's mods folder (`port/user/mods/`).
- **User data:** never touch `port/user/savedata_aside_105102`, `port/user/savedata_player`, `port/user/savedata_player_backups` or `Game/`. Test runs set `ISS_USER_DIR` to a test data folder in the scratchpad, never `port/user`.
- **The user may be playing:** no build and no game or launcher run while a `tlg-probe` process runs or `port/user/.player_save_in_play` exists.
- **PySide6:** `PySide6-Essentials==6.12.0`, installed into `port/out/pyenv` (approved by the user, 116 MB).
- **Numbers:** every performance number written anywhere is measured on this M5, with its command; otherwise it is marked **unverified**.
- **Pushing:** implementers never push. The controller pushes after each reviewed task.

## Review Focus

1. **A cheat file for another game or another version** in the mods folder: it is not offered, and no byte is written. Test in Task 1.
2. **A cheat whose `off` bytes do not match the user's executable** (another 1.07 build, a wrong offset convention): it is not offered, and compile skips it. Tests in Task 1.
3. **A malformed JSON file**, or a JSON file of another kind (`mods.json`, a launcher file), in the mods folder: it is skipped with a log line, and every other mod is still listed. Test in Task 1.
4. **Opening the launcher with the user's existing `tlg.ini`:** the file stays byte-identical. No preset is applied by itself on first run. Check in Task 2.
5. **The Updates screen on macOS:** it never downloads or installs a package, and says that Mac updates come from the fork. Test in Task 2.

---

### Task 1: GoldHEN cheat JSON and the 1.07 mods version for verified EU dumps

**Files:**
- Modify: `port/scripts/mod_patches.py` (`external_patches`, `compile_external`, a new byte-reading helper)
- Modify: `port/scripts/infamous_patches.py` (`verified_107`, `mods_app_version`, `prepare`)
- Modify: `port/launcher/infamous_run.py:85-102` (`camera_build_supported`), `:105-120` (`list_mods`), and the `old_param` check near `:462-476`
- Test: create `port/tests/test_goldhen_cheats.py`; run `port/tests/test_camera_mod_selection.py` and `port/tests/test_patches.py` unchanged
- Modify: `README.md` (Mods / Cheats: where GoldHEN cheat files come from, crediting `GoldHEN/GoldHEN_Cheat_Repository`; the two shared changes listed)

**Interfaces:**
- **Produces** `mod_patches.external_patches(directory, app_version, ids, builtin_files=(), elf=None) -> [(key, path, meta)]`. It is unchanged for XML. It also reads `directory/*.json` GoldHEN cheat files:
  - **Fields:** a top-level `id` (in `ids`), `version` (== `app_version`), `process == "eboot.bin"`, and `mods`.
  - **Checkboxes:** each mod with `type == "checkbox"` becomes one entry. The key is `"<file name>/<mod name>"`, and `meta` is an `xml.etree.ElementTree.Element('Metadata', Name=<mod name>, AppVer=<version>, AppElf='eboot.bin', isEnabled='false')`. It has one `Line(Type='bytes', Address=hex(0x400000 + int(offset, 16)), Value=<on>, Original=<off>)` child per `memory` item, under a `PatchList` child, matching the XML layout.
  - **Other mod types** (`button`, …) are skipped with the stderr line `Mods: <file>/<name>: GoldHEN <type> cheats write runtime data and are not supported`.
  - **Files that are not GoldHEN cheat JSON** (no `mods` list, or unparsable) are skipped with a `Mods: <file>: …` stderr line.
  - **When `elf` is given,** any entry (XML or JSON) with a `Line` whose `Original` differs from the executable's bytes at that address is dropped with `Mods: <key>: original bytes do not match this eboot; not offered`.
- **Produces** `mod_patches.compile_external(selected, segments, eboot=None)`. It is unchanged, except that a `Line` with `Original` raises `ValueError('original bytes do not match')` when `eboot` is given and its bytes differ, so the patch is skipped whole.
- **Produces** `infamous_patches.verified_107(game_dir) -> bool`: TITLE_ID CUSA00004, and the eboot ELF SHA-256 equal to `b3afd173a379f83e1e408e71fdef22b78f93092d332b79decd48a18a08bfc605`. This logic moves here from `camera_build_supported`.
- **Produces** `infamous_patches.mods_app_version(game_dir) -> str | None`: `'01.07'` when `game_app_version` is `'01.00'` and `verified_107(game_dir)`; otherwise `game_app_version(game_dir)`.
- **Consumes:** `read_eboot_elf(game_dir)` (existing, `infamous_patches.py`), and `game_app_version` (existing, `mod_patches.py`).

- [ ] **Step 1: Write the failing tests** in `port/tests/test_goldhen_cheats.py` (unittest; put `port/scripts` and `port/launcher` on `sys.path` like `test_camera_mod_selection.py`).
  - **Helper `fake_elf(code: bytes) -> bytes`:**
    - a 64-bit ELF header with `e_phoff = 0x40` (packed at 0x20), `e_phentsize = 0x38` and `e_phnum = 1` (at 0x36);
    - one PT_LOAD (type 1, flags 5, offset 0x1000, vaddr 0, filesz = memsz = `len(code)`);
    - `code` at file offset 0x1000.
  - **Helper `cheat_file(dir, **over)`:** writes `CUSA00004_01.07.json` with:
    - `{"name": "inFAMOUS", "id": "CUSA00004", "version": "01.07", "process": "eboot.bin"}`;
    - mods: a checkbox `"Infinite Shards"` with memory `[{"offset": "10", "on": "9090", "off": "C4C1"}]`, and a button `"Max Level"` with memory `[{"offset": "EFADB8", "on": "00", "off": "00"}]`.
  - **Tests:**
    - `test_checkbox_becomes_patch_and_button_is_skipped`: `external_patches(d, '01.07', {'CUSA00004', 'CUSA00223'})` returns exactly one entry. Its key is `'CUSA00004_01.07.json/Infinite Shards'`, and its single Line has `Address == '0x400010'`, `Value == '9090'`, `Original == 'C4C1'`.
    - `test_other_version_or_title_not_offered`: `version` `'01.06'` gives `[]`, and `id` `'CUSA99999'` gives `[]`.
    - `test_malformed_and_foreign_json_skipped`: a file `broken.json` with `{oops` and a `mods.json` `{"enabled": []}` sit next to the cheat file. The cheat entry is still returned, and nothing raises.
    - `test_original_mismatch_not_offered`:
      - with `elf=fake_elf(b'\0' * 0x10 + bytes.fromhex('C4C1'))`, one entry is returned;
      - with `elf=fake_elf(b'\0' * 0x10 + bytes.fromhex('FFFF'))`, `[]` is returned.
    - `test_compile_checks_original`:
      - `compile_external(found, [(0, 0x12)], eboot=<matching fake elf>) == [(0x10, b'\x90\x90', <description>)]`;
      - with a mismatching eboot it returns `[]`.
    - `test_untouched_folder_changes_nothing`: built-ins `port/patches/iss_builtin_intro.xml` with no `mods.json`. `external_selection(external_patches(d, v, ids, builtin_files=BUILTIN), None) == []` for `v` in `('01.00', '01.07')`.
    - `test_mods_app_version_for_verified_eu_dump`, with `prepare.sfo` and `read_eboot_elf` / sha256 mocked as `test_camera_mod_selection.py` does:
      - CUSA00004 `01.00` with the verified hash gives `'01.07'`;
      - with another hash it gives `'01.00'`;
      - CUSA00223 `01.07` gives `'01.07'`.
- [ ] **Step 2: Run** `cd port/tests && python3 -m unittest test_goldhen_cheats -v`. Expected: FAIL (`external_patches` ignores `.json`; `mods_app_version` is missing).
- [ ] **Step 3: Implement.**
  - the JSON reading and the `elf` filter in `external_patches`;
  - the `Original` check in `compile_external`, with a helper `_elf_bytes(elf, vaddr, size) -> bytes | None` that reads through the PT_LOAD `p_offset`/`p_filesz` and returns `None` outside file-backed data;
  - `verified_107` and `mods_app_version` in `infamous_patches.py`;
  - `prepare()` uses `mods_app_version` and passes `elf=eboot` to `external_patches`;
  - `infamous_run.camera_build_supported` calls `verified_107`;
  - `list_mods` uses `mods_app_version` and passes the eboot ELF (`read_eboot_elf`, once per call);
  - `check_game` no longer reports `old_param` when `verified_107(game)`.
- [ ] **Step 4: Run** `cd port/tests && python3 -m unittest test_goldhen_cheats test_camera_mod_selection test_patches -v`. Expected: all pass.
- [ ] **Step 5: Real-file check, read-only.**
  - Copy `/private/tmp/claude-501/-Users-ziad-Documents-SecondSonMac/d3d80260-f0d9-48d0-b134-56eda303d36a/scratchpad/cheatcheck/CUSA00004_01.07.json` into a scratch mods folder.
  - Run `ISS_MODS_DIR=<that folder> port/out/pyenv/bin/python -c "import sys; sys.path[:0]=['port/launcher','port/scripts']; import infamous_run as r; print(r.list_mods({'game_dir': 'Game/Extracted Game/CUSA00004'}))"`.
  - **Expected:** rows for Godmode, Infinite Energy, Infinite Bombs / Rockets and Infinite Skillpoints / Shards (all off), plus the built-ins and the camera mod. No Max Level rows, and two "not supported" stderr lines.
- [ ] **Step 6: README.** In Mods / Cheats, say:
  - GoldHEN cheat JSON files (from `GoldHEN/GoldHEN_Cheat_Repository`, `json/<TITLE>_<VERSION>.json`) go in the mods folder, next to XML;
  - only checkbox cheats whose original bytes match are offered;
  - button cheats are not supported.

  List the JSON reader and the 01.07 mods version for verified CUSA00004 dumps under "Changes that affect all platforms (for upstream)".
- [ ] **Step 7: Commit:** `mods: GoldHEN cheat JSON next to PATCHES XML, guarded by original bytes; 1.07 mods on verified CUSA00004 dumps`

### Task 2: The Qt launcher on macOS

**Files:**
- Modify: `infamous_launcher_qt.sh`. On Darwin, use `port/out/pyenv/bin/python` (re-exec as arm64 like `make_app.sh`). Keep the `.venv-qt` and `python3` rules for other systems.
- Modify: `port/launcher/qt/*.py` and `port/launcher/qt/qml/Launcher/*.qml`, only where macOS needs it, darwin-gated.
- Modify: `port/launcher/qt/updates.py`. On darwin:
  - `check()` and `checkOnOpen()` contact no server, and nothing can download or install;
  - the screen's note is `Updates for the macOS port come from github.com/executed007/iss_macos: pull the latest version and rebuild.` (with a Portuguese string in the launcher's string table).
- Modify: `README.md`, Build/Run: `port/out/pyenv/bin/pip install PySide6-Essentials==6.12.0`, then `./infamous_launcher_qt.sh`.
- Test: `port/tests/test_qt_updater.py` (add a test); the existing Qt test modules; the launcher's own `--selftest`.

**Interfaces:**
- **Consumes:** nothing from Task 1 beyond `infamous_run.list_mods` (the Mods screen calls it through `qt/backend.py`).
- **Produces:** `./infamous_launcher_qt.sh [--selftest|--play]` works on macOS. Task 3's app runs `port/out/pyenv/bin/python port/launcher/infamous_launcher_qt.py`.

- [ ] **Step 1: Install** with `port/out/pyenv/bin/pip install PySide6-Essentials==6.12.0`. Then `port/out/pyenv/bin/python -c "import PySide6; print(PySide6.__version__)"` prints `6.12.0`.
- [ ] **Step 2: Write the failing test** `test_macos_never_checks_or_installs` in `test_qt_updater.py`. With `sys.platform` patched to `'darwin'`, a `check()` call makes no network request (patch the request function and assert it was not called), `can_update` / the install action report False, and the note text equals the string above.
- [ ] **Step 3: Run** `cd port/tests && python3 -m unittest test_qt_updater -v` with the pyenv Python. Expected: the new test FAILS. Note which other Qt tests now run instead of skipping, and whether they pass.
- [ ] **Step 4: Make the launcher work on macOS.**
  - Run `./infamous_launcher_qt.sh --selftest` with `ISS_USER_DIR=<test data folder>`, then fix each macOS failure (darwin-gated). The test data folder is a copy of `port/user` without `savedata_player*`, with the live save equal to the aside, `tlg.ini` copied, and `cache/` symlinked to `port/user/cache`.
  - Implement the darwin rule in `updates.py`.
- [ ] **Step 5: Run** the Qt tests (`test_qt_updater`, `test_qt_launcher`, `test_qt_screens_a`, and any other `test_qt_*`), plus `--selftest`. Expected: pass. Tests that only fail on macOS for a reason outside this task are listed in the report with their output, not hidden.
- [ ] **Step 6: Check the screens and settings** (Review Focus 4). Open the launcher on the test data folder and screenshot every screen (`screencapture -l <window id>`).
  - **Expected:**
    - Graphics shows internal resolution 35%;
    - Fixes shows `quality_floor` 8 and the FPS limit 30 from the copied `tlg.ini`;
    - Mods / Cheats lists the cheats once the cheat file is in the test folder's `mods/`;
    - Updates shows the note.
  - `cmp` of the test folder's `tlg.ini` before and after opening and closing the launcher: identical.
- [ ] **Step 7: Play from the launcher, one run** on the test data folder: `ISS_USER_DIR=<test folder> ./infamous_launcher_qt.sh --play`, with the usual run checks.
  - **Expected:** the game reaches gameplay. Closing the game window ends it cleanly, and the launcher process exits or returns without an error.
  - With Infinite Skillpoints / Shards enabled in the test folder's `mods.json`, the log shows `Mod CUSA00004_01.07.json/Infinite Skillpoints / Shards (0x5e22a4)` with the bytes `bbe7030000899fe00000009090`.
- [ ] **Step 8: README.** Build and run lines for the Qt launcher on macOS.
- [ ] **Step 9: Commit** in small steps: `launcher: Qt launcher runs on macOS (pyenv + PySide6)`, `launcher: on macOS the Updates screen points at the fork and never installs`, README.

### Task 3: The .app opens the launcher

**Files:**
- Modify: `port/scripts/macos/make_app.sh` (the default command and the venv check)
- Modify: `README.md` (Run section)

**Interfaces:**
- **Consumes:** Task 2's command `port/out/pyenv/bin/python port/launcher/infamous_launcher_qt.py`.
- **Produces:** `make_app.sh GAME_DIR` builds an app whose default command is that launcher, with `ISS_GAME_DIR` set as before. `START=<executable>` still overrides it. The error dialog on a non-zero exit is unchanged.

- [ ] **Step 1:**
  - Change the default command, and require `port/out/pyenv` to have PySide6 when `START` is not set; build stops with one line naming the pip command otherwise.
  - On first run, the launcher's game folder comes from `ISS_GAME_DIR` when its own setting is empty.
- [ ] **Step 2: Verify:**
  - `bash -n port/scripts/macos/make_app.sh`;
  - build a scratch app with `OUT=<scratchpad>`, and `codesign -v --deep` it;
  - `open` it with `ISS_USER_DIR` pointing at the test data folder (for example through a `START` wrapper that sets it and then runs the default command): the launcher window appears (screenshot), and quitting it gives no error dialog.
- [ ] **Step 3: README.** The .app opens the launcher; build it with `port/scripts/macos/make_app.sh "<game folder>"`.
- [ ] **Step 4: Commit:** `macos: the app opens the Qt launcher`

### Controller step (not dispatched): the cheat file, the user's progress, the user's app

Done by the controller after Tasks 1–3 pass review, with these stop rules. Stop and ask the user if:
- a game or launcher is running;
- `port/user/.player_save_in_play` exists;
- the live save differs from `savedata_aside_105102`;
- `savedata_player` is missing.

1. Copy the community cheat file into `port/user/mods/CUSA00004_01.07.json`. Record its source commit in the ledger.
2. Back up the live save folder into `port/user/savedata_player_backups/pre-launcher-<date>`. Copy `port/user/savedata_player` into the live folder, and `diff -rq` it clean.
3. Rebuild the user's app without `START`, and `codesign -v --deep` it.
4. Update Claude's memory note `play-script-and-test-safety`: test runs use `ISS_USER_DIR`, and the live folder now holds the user's progress.
5. Ask the user to open the app, turn on Infinite Skillpoints / Shards in Mods / Cheats, and check that buying a power leaves the shard count at 999.
