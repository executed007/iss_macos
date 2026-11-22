#!/usr/bin/env bash
# inFAMOUS Second Son launcher, Qt Quick edition (the default launcher of the packages; infamous_launcher.sh is the Tk one, kept as a fallback).
# Usa o venv ~/.../.venv-qt ou o python do sistema com PySide6-Essentials instalado.
# macOS: o python do port (port/out/pyenv, com PySide6-Essentials), sempre como arm64.
here="$(dirname -- "$(readlink -f -- "$0")")"
if [ "$(uname -s)" = Darwin ]; then
  exec arch -arm64 "$here/port/out/pyenv/bin/python" "$here/port/launcher/infamous_launcher_qt.py" "$@"
fi
py=python3
[ -x "$here/.venv-qt/bin/python" ] && py="$here/.venv-qt/bin/python"
exec "$py" "$here/port/launcher/infamous_launcher_qt.py" "$@"
