#!/bin/bash
# Builds "port/out/inFAMOUS Second Son.app" for THIS checkout (macOS only).
#   usage: VULKAN_SDK=~/VulkanSDK/<ver>/macOS port/scripts/macos/make_app.sh /path/to/CUSA00004
#          START=/path/to/script  ...   the app runs that executable instead of the launcher
#          OUT=/folder                  where to put the app (default port/out)
# The app is a script, not a copy: it runs the checkout's pyenv and launcher and records the checkout's
# absolute path and GAME_DIR. No game file is copied. Moving the checkout means building the app again.
set -euo pipefail
GAME_DIR=${1:?usage: make_app.sh GAME_DIR}
[[ -d $GAME_DIR ]] || { echo "Not a folder: $GAME_DIR" >&2; exit 1; }
[[ -f ${VULKAN_SDK:-/nonexistent}/lib/libvulkan.dylib ]] || { echo 'Set VULKAN_SDK (README.md).' >&2; exit 1; }
PORT=$(cd "$(dirname "$0")/../.." && pwd)
GAME_DIR=$(cd "$GAME_DIR" && pwd)
START=${START:-}
[[ -z $START || -x $START ]] || { echo "START is not an executable file: $START" >&2; exit 1; }
[[ -n $START || -x $PORT/out/pyenv/bin/python ]] || { echo "Missing $PORT/out/pyenv (README: Build)." >&2; exit 1; }
APP=${OUT:-$PORT/out}/inFAMOUS\ Second\ Son.app
rm -rf "$APP"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

sips -s format icns "$PORT/launcher/infamous.ico" --out "$APP/Contents/Resources/AppIcon.icns" >/dev/null
cat > "$APP/Contents/Info.plist" <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>inFAMOUS Second Son</string>
<key>CFBundleDisplayName</key><string>inFAMOUS Second Son</string>
<key>CFBundleIdentifier</key><string>io.github.executed007.iss-macos</string>
<key>CFBundleExecutable</key><string>run</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>1</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>LSArchitecturePriority</key><array><string>arm64</string><string>x86_64</string></array>
</dict></plist>
PL

# printf %q quotes every value, so a path with spaces or quotes stays one word.
{
  echo '#!/bin/bash'
  printf 'ROOT=%q\nexport VULKAN_SDK=%q\nexport ISS_GAME_DIR=%q\nSTART=%q\n' "$(dirname "$PORT")" "$VULKAN_SDK" "$GAME_DIR" "$START"
  cat <<'BODY'
# `open` can start this script as x86_64 (Rosetta), where the pyenv's arm64 iced-x86 cannot load.
[ "$(uname -m)" = arm64 ] || exec arch -arm64 "$0" "$@"
LOG=$(mktemp -t iss-app)
# cd inside a subshell: a moved checkout (no $ROOT) must reach the dialog below, not exit silently.
( cd "$ROOT" || { echo "The checkout is not at $ROOT: build the app again (make_app.sh)."; exit 1; }
  if [ -n "$START" ]; then "$START"; else port/out/pyenv/bin/python port/launcher/infamous_run.py; fi
) >"$LOG" 2>&1
code=$?
if [ $code -ne 0 ]; then
  # An error exit only: the last lines go to a dialog (passed as an argument, so no quoting problem).
  osascript -e 'on run argv' -e 'display dialog (item 1 of argv) with title "inFAMOUS Second Son" buttons {"OK"} with icon stop' \
    -e 'end run' "Stopped (exit $code):
$(tail -n 12 "$LOG")" >/dev/null 2>&1
fi
rm -f "$LOG"
exit $code
BODY
} > "$APP/Contents/MacOS/run"
chmod +x "$APP/Contents/MacOS/run"
codesign --force --deep -s - "$APP"
codesign -v --deep "$APP" && echo "Built: $APP"
