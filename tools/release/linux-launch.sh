#!/bin/sh
# Relocatable Linux entry point. Keep writable state out of an immutable package.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# GTK's SVG module and fontconfig data are not part of ELF dependency discovery.
# Build a relocatable module cache in user state, never in the package directory.
if [ -x "$root/Support/Desktop/gdk-pixbuf-query-loaders" ]; then
    case "${XDG_CACHE_HOME-}" in /*) desktop_cache=$XDG_CACHE_HOME;; *) desktop_cache=$HOME/.cache;; esac
    desktop_cache=$desktop_cache/timesplitters-future-perfect
    mkdir -p "$desktop_cache"
    module_cache=$(mktemp "$desktop_cache/.pixbuf.XXXXXX")
    if "$root/Support/Desktop/gdk-pixbuf-query-loaders" "$root/Support/Desktop/pixbuf/libpixbufloader-svg.so" > "$module_cache"; then
        mv -f -- "$module_cache" "$desktop_cache/pixbuf-loaders.cache"
        export GDK_PIXBUF_MODULE_FILE="$desktop_cache/pixbuf-loaders.cache"
    else
        rm -f -- "$module_cache"
        echo "Could not initialize the bundled GTK image loader" >&2
        exit 1
    fi
fi
if [ ! -f /etc/fonts/fonts.conf ] && [ -f "$root/Support/Desktop/fonts.conf" ]; then
    export FONTCONFIG_FILE="$root/Support/Desktop/fonts.conf"
fi
if [ "${1-}" != "--game" ]; then
    unset LD_PRELOAD
    export LD_LIBRARY_PATH="$root/Bin"
    exec "$root/TimeSplitters.Launcher" --root "$root" "$@"
fi
shift
data="$root/Data"
if [ "${1-}" = "--data" ]; then
    [ "$#" -ge 2 ] || { echo "--data requires a game directory" >&2; exit 2; }
    data=$2; shift 2
fi
if [ "$#" -gt 1 ]; then echo "Use --game [--data PATH] [--settings-self-test|--debug-self-test|--input-self-test|--renderer-self-test|--online-self-test]" >&2; exit 2; fi
case "${1-}" in ""|--settings-self-test|--debug-self-test|--input-self-test|--renderer-self-test|--online-self-test) ;; *) echo "Unknown game option: $1" >&2; exit 2;; esac
xdg_path() { case "$1" in /*) printf '%s' "$1";; *) printf '%s/%s' "$HOME" "$2";; esac; }
XDG_DATA_HOME=$(xdg_path "${XDG_DATA_HOME-}" .local/share)
XDG_CONFIG_HOME=$(xdg_path "${XDG_CONFIG_HOME-}" .config)
XDG_STATE_HOME=$(xdg_path "${XDG_STATE_HOME-}" .local/state)
XDG_CACHE_HOME=$(xdg_path "${XDG_CACHE_HOME-}" .cache)
export XDG_DATA_HOME XDG_CONFIG_HOME XDG_STATE_HOME XDG_CACHE_HOME
base=$XDG_DATA_HOME/timesplitters-future-perfect
config=$XDG_CONFIG_HOME/timesplitters-future-perfect
state=$XDG_STATE_HOME/timesplitters-future-perfect
cache=$XDG_CACHE_HOME/timesplitters-future-perfect
mkdir -p "$base/Saves" "$config" "$state/online" "$state/crashes" "$cache"
if [ ! -f "$config/pc-controls.json" ]; then
    # Publish a complete seed atomically; never overwrite concurrent/user settings.
    seed=$(mktemp "$config/.controls.XXXXXX")
    trap 'rm -f -- "$seed"' EXIT HUP INT TERM
    cat "$root/Settings/pc-controls.json" > "$seed"
    ln "$seed" "$config/pc-controls.json" 2>/dev/null || [ -f "$config/pc-controls.json" ]
    rm -f -- "$seed"
    trap - EXIT HUP INT TERM
fi
unset LD_PRELOAD LD_LIBRARY_PATH PYTHONPATH PYTHONHOME VK_DRIVER_FILES VK_ICD_FILENAMES VK_LAYER_PATH SDL_VULKAN_LIBRARY
export RECOMP_GAME_DIR="$data" RECOMP_SAVE_DIR="$base/Saves"
export RECOMP_INPUT_CONFIG="$config/pc-controls.json" RECOMP_DISPLAY_CONFIG="$config/runtime.conf" TSFP_DISPLAY_CONFIG="$config/pc-display.ini"
export TSFP_DISCORD_CONFIG="$config/pc-discord.ini" TSFP_FRAME_RATE_CONFIG="$config/pc-frame-rate.ini" TSFP_AUDIO_CONFIG="$config/pc-audio.ini"
export RECOMP_LOG_FILE="$state/game.err" TSFP_CRASH_DIR="$state/crashes" RECOMP_FFMPEG_DIR="$root/Bin"
export TSFP_ONLINE_SETTINGS="$config/pc-online.json" TSFP_ONLINE_HANDSHAKES="$state/online"
export TSFP_ONLINE_PYTHON="$root/Support/Python/bin/python3" TSFP_ONLINE_SCRIPT="$root/Support/Online/online.py" TSFP_ONLINE_MANIFEST="$root/Support/Online/network-identity.json"
export RECOMP_SHADER_CACHE=1
cd "$root"
exec "$root/Bin/futureperfect_recomp" "$@"
