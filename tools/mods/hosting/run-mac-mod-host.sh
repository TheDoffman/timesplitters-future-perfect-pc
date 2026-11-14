#!/bin/sh
# Serve an explicit approved snapshot only. Does not enable public access itself.
set -eu
if [ "$(uname -s)" != Darwin ]; then
    echo 'Run this host script on the Mac mini.' >&2
    exit 2
fi
if [ "$#" -ne 2 ]; then
    echo 'Usage: sh tools/mods/hosting/run-mac-mod-host.sh /absolute/public-snapshot /absolute/pinned-root.json' >&2
    exit 2
fi
case "$1" in
    /*) ;;
    *) echo 'Supply an absolute snapshot path.' >&2; exit 2 ;;
esac
case "$2" in
    /*) ;;
    *) echo 'Supply an absolute pinned-root path.' >&2; exit 2 ;;
esac
case "$1" in
    *\"*|*\{*|*\}*) echo 'Snapshot paths must not contain quotes or braces.' >&2; exit 2 ;;
esac
command -v python3 >/dev/null
command -v caddy >/dev/null
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$script_dir/host-mac.py" "$1" --trusted-root "$2"
