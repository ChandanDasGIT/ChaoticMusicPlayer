#!/bin/bash
# Runs the screenshot harness (build/shot/shot, from scripts/shot-build.sh) safely:
#  - refuses while the real app is running (never touch the user's instance)
#  - isolated config / data / cache / runtime dirs under $SHOT_ENV
#  - OFFLINE by default: every request goes to a dead proxy, so tests never
#    contact YouTube (many requests get the connection rate-limited).
#    SHOT_ONLINE=1 allows network access for a test that really needs it.
# Usage: SHOT_ENV=/path/to/env [SHOT_* options] scripts/shot-run.sh
set -e
cd "$(dirname "$0")/.."
if pgrep -x ChaoticMusicPl >/dev/null || playerctl -l 2>/dev/null | grep -q chaoticmusicplayer; then
    echo "ABORT: ChaoticMusicPlayer is running" >&2
    exit 1
fi
T="${SHOT_ENV:-$PWD/build/shot/env}"
mkdir -p "$T"/{config/ChaoticMusicPlayer,data/ChaoticMusicPlayer,cache,run}
chmod 700 "$T/run"
export XDG_RUNTIME_DIR="$T/run" PIPEWIRE_RUNTIME_DIR="/run/user/$(id -u)" \
       XDG_CONFIG_HOME="$T/config" XDG_DATA_HOME="$T/data" XDG_CACHE_HOME="$T/cache" \
       QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1
if [ -z "$SHOT_ONLINE" ]; then
    export http_proxy=http://127.0.0.1:9 https_proxy=http://127.0.0.1:9 all_proxy=http://127.0.0.1:9 \
           HTTP_PROXY=http://127.0.0.1:9 HTTPS_PROXY=http://127.0.0.1:9 ALL_PROXY=http://127.0.0.1:9 no_proxy= NO_PROXY=
fi
exec timeout 40 build/shot/shot
