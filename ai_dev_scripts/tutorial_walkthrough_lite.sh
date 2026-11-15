#!/bin/bash
# tutorial_walkthrough_lite.sh - install otsdaq lite as a new user would, start Wiz mode, take the
# standard screenshots, and stop again, fully isolated from any otsdaq already running on this host.
#
# usage: tutorial_walkthrough_lite.sh <new scratch dir> [--branch B] [--reuse] [--port P] [--keep-running]
#
#   <scratch dir>  created by this script (must not exist, unless --reuse); never an existing install area
#   --branch B     installer and repos from branch B (default develop; the installer URL uses the same branch)
#   --reuse        skip the install, use the otsdaq lite already in <scratch dir>/lite
#   --port P       Wiz mode port (default: first free port from 3115)
#   --keep-running leave Wiz mode running (stop later with: cd <dir>/lite; source setup_ots.sh; echo Y | ots -k)
#
# Isolation, so a live system on the same host is never touched:
#   - everything runs in a clean environment (env -i), as a new user's terminal would
#   - Wiz mode gets its own free port (OTS_WIZ_MODE_MAIN_PORT); its message forwarder then uses
#     30000+port, away from a live system's 30000/30001
#   - `ots -k` only kills processes whose command line contains this install's Data path
#   - the PIDs of every other xdaq.exe / ots_lite_main / emulator process are recorded before and
#     compared after; the script reports any difference
#
# Output in <scratch dir>: install_console.txt, start_console.txt, stop_console.txt,
# wiz_url.txt, shot_wiz_intro_tooltip.png, shot_wiz_desktop.png.
set -u
W=${1:?usage: $0 <new scratch dir> [--branch B] [--reuse] [--port P] [--keep-running]}; shift
BRANCH=develop; REUSE=0; PORT=; KEEP=0
while [ $# -gt 0 ]; do case $1 in
    --branch) BRANCH=$2; shift 2;; --reuse) REUSE=1; shift;; --port) PORT=$2; shift 2;;
    --keep-running) KEEP=1; shift;; *) echo "unknown option $1"; exit 1;; esac; done
HERE=$(cd "$(dirname "$0")" && pwd)
W=$(readlink -f "$W")
case "$W" in */ots-lite-test*|*/dev-srcs*|*/srcs) echo "ERROR: $W looks like a real install or source area; use a new scratch directory."; exit 1;; esac
if [ $REUSE -eq 0 ]; then
    [ -e "$W" ] && { echo "ERROR: $W exists. Use a new directory, or --reuse."; exit 1; }
    mkdir -p "$W/lite"
else
    [ -f "$W/lite/setup_ots.sh" ] || { echo "ERROR: no otsdaq lite install in $W/lite to reuse."; exit 1; }
fi
cd "$W/lite" && git rev-parse --is-inside-work-tree >/dev/null 2>&1 && { echo "ERROR: $W is inside a git checkout"; exit 1; }
if [ -z "$PORT" ]; then PORT=3115; while ss -ltn | grep -q ":$PORT "; do PORT=$((PORT+1)); done; fi
ss -ltn | grep -q ":$PORT " && { echo "ERROR: port $PORT is in use"; exit 1; }

clean_env() { env -i HOME=$HOME USER=$USER LOGNAME=$USER TERM=xterm LANG=en_US.UTF-8 \
                  PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin OTS_WIZ_MODE_MAIN_PORT=$PORT \
                  bash --noprofile --norc -c "cd $W/lite && $1"; }
others() { ps -u "$USER" -o pid=,args= | grep -E "xdaq.exe|ots_lite_main|ots_udp_hw_emulator" \
               | grep -v grep | grep -v "$W/" | awk '{print $1, $2}' | sort; }
others > "$W/other_pids_before.txt"
echo "Other otsdaq processes on this host (must stay untouched):"; sed 's/^/  /' "$W/other_pids_before.txt"

if [ $REUSE -eq 0 ]; then
    echo "Installing otsdaq lite from branch $BRANCH (about 5 minutes)..."
    s=$(date +%s)
    clean_env "wget -q https://raw.githubusercontent.com/art-daq/otsdaq-demo/$BRANCH/tools/ots-quick-lite-start.sh && chmod +x ots-quick-lite-start.sh && ./ots-quick-lite-start.sh --branch $BRANCH" > "$W/install_console.txt" 2>&1
    rc=$?; echo "install exit=$rc after $(( $(date +%s) - s )) s (console: $W/install_console.txt)"
    [ $rc -eq 0 ] || { tail -20 "$W/install_console.txt"; exit 1; }
fi

echo "Starting Wiz mode on port $PORT..."
clean_env "source setup_ots.sh && UpdateOTS.sh --tables && ots -w" > "$W/start_console.txt" 2>&1
url=
for i in $(seq 1 40); do
    url=$(grep -rhao "http://[^ \"'<>]*Verify?code=[A-Za-z0-9]*" "$W/lite/tmp" "$W/start_console.txt" 2>/dev/null | grep ":$PORT/" | tail -1)
    [ -n "$url" ] && break; sleep 0.5
done
[ -n "$url" ] || { echo "ERROR: no Wiz mode URL found; see $W/start_console.txt"; exit 1; }
echo "$url" > "$W/wiz_url.txt"; echo "Wiz mode URL: $url"

python3 -I "$HERE/ff_screenshot.py" "$url" "$W/shot_wiz_intro_tooltip.png" --sleep 8
python3 -I "$HERE/ff_screenshot.py" "$url" "$W/shot_wiz_desktop.png" --wait-for ".DesktopIcons-iconDiv" --sleep 3 \
    --eval "Debug.closeErrorPop(); 'tooltip closed'"
rm -rf "$HERE/__pycache__"

if [ $KEEP -eq 0 ]; then
    echo "Stopping (ots -k, answering Y)..."
    echo Y | clean_env "source setup_ots.sh >/dev/null 2>&1 && ots -k" > "$W/stop_console.txt" 2>&1
    sleep 3
    left=$(ps -u "$USER" -o pid=,args= | grep "$W/" | grep -v grep)
    [ -n "$left" ] && { echo "WARNING: still running:"; echo "$left"; }
fi
others > "$W/other_pids_after.txt"
if diff -q "$W/other_pids_before.txt" "$W/other_pids_after.txt" >/dev/null; then
    echo "OK: other otsdaq processes untouched."
else
    echo "WARNING: other otsdaq processes changed:"; diff "$W/other_pids_before.txt" "$W/other_pids_after.txt"
fi
echo "Screenshots: $W/shot_wiz_intro_tooltip.png $W/shot_wiz_desktop.png"
