#!/bin/bash
# Make setup_ots.sh take a subsystem (setup type) argument, so several ots instances
# (e.g. a top-level gateway and its subsystems) run from one lite area:
#
#   source setup_ots.sh <subsystem>
#
# sets OTS_SETUP_TYPE=<subsystem>, USER_DATA=<ots dir>/Data_<subsystem>,
# OTSDAQ_DATA=$USER_DATA/OutputData and the subsystem's ports (OTS_MAIN_PORT,
# OTS_CONFIG_UDP_*, OTS_MACROMAKER_UDP_*). ARTDAQ_DATABASE_URI is left as is, so all
# subsystems share one database (needed for a top-level gateway to read the aliases of
# its subsystems). Without an argument, or with an unknown one, setup_ots.sh lists the
# subsystems and returns 1. `ots -k` already kills only the processes of $USER_DATA/,
# and `ots` keys its quit/lock files on OTS_SETUP_TYPE.
#
# The block is written between marker lines after the `export OTS_SOURCE=` line and is
# replaced on every run (idempotent). mz_uc regenerates setup_ots.sh without it: rerun
# this script afterwards.
#
# Usage:
#   ./setup_ots_subsystems.sh <ots dir> <name>:<mainPort>:<cfgUdpPort>:<mmUdpPort> [...]
#   ./setup_ots_subsystems.sh <ots dir> --show       # print the current block
#   e.g. ./setup_ots_subsystems.sh /data/fasic/asic/ots top:2115:2110:2111 sunrock:2015:2010:2011
#
# It does not create the Data_<name> folders (copy an existing Data folder, e.g.
# `cp -a Data Data_<name>`), and warns about any that are missing. Ports that ots binds
# from the configuration tables (gateway UDP state changes, Console/MessageFacility
# forwarding) are not covered here: give each subsystem its own values in its Context.

set -e

OTS_DIR="$1"; shift || true
if [ -z "$OTS_DIR" ] || [ $# -lt 1 ]; then
    sed -n 2,27p "$0" | sed 's/^# \{0,1\}//'; exit 1
fi
SETUP="$OTS_DIR/setup_ots.sh"
[ -f "$SETUP" ] || { echo "ERROR: $SETUP not found."; exit 1; }

BEGIN="# >>> ots subsystems (ai_scripts/setup_ots_subsystems.sh) >>>"
END="# <<< ots subsystems <<<"

if [ "$1" = "--show" ]; then
    awk -v b="$BEGIN" -v e="$END" '$0==b{p=1} p{print} $0==e{p=0}' "$SETUP"; exit 0
fi

NAMES=""; CASES=""
for spec in "$@"; do
    IFS=: read -r name main cfg mm extra <<< "$spec"
    if ! [[ "$name" =~ ^[A-Za-z0-9_]+$ && "$main" =~ ^[0-9]+$ && "$cfg" =~ ^[0-9]+$ && "$mm" =~ ^[0-9]+$ ]] || [ -n "$extra" ]; then
        echo "ERROR: bad subsystem spec '$spec' (expected name:mainPort:cfgUdpPort:mmUdpPort)."; exit 1
    fi
    NAMES="$NAMES $name"
    CASES="$CASES    $name) export OTS_MAIN_PORT=$main OTS_CONFIG_UDP_PORT=$cfg OTS_MACROMAKER_UDP_PORT=$mm ;;
"
    [ -d "$OTS_DIR/Data_$name" ] || echo "WARNING: $OTS_DIR/Data_$name does not exist yet."
done
NAMES="${NAMES# }"

BLOCK="$BEGIN
# source setup_ots.sh <subsystem>: per-subsystem USER_DATA and ports, shared ARTDAQ_DATABASE_URI
case \"\$1\" in
$CASES    *)  echo -e \"setup_ots.sh:\${LINENO} |  \\t  Usage: source setup_ots.sh <subsystem>, one of: $NAMES\"
        [ -n \"\$1\" ] && echo -e \"setup_ots.sh:\${LINENO} |  \\t  Unknown subsystem '\$1'.\"
        return 1 ;;
esac
export OTS_SETUP_TYPE=\"\$1\"
export OTS_CONFIG_UDP_IP=127.0.0.1
export OTS_MACROMAKER_UDP_IP=127.0.0.1
export USER_DATA=\"\$SCRIPT_DIR/Data_\$OTS_SETUP_TYPE\"
export OTSDAQ_DATA=\"\$USER_DATA/OutputData\"
echo -e \"setup_ots.sh:\${LINENO} |  \\t  Subsystem OTS_SETUP_TYPE = \$OTS_SETUP_TYPE (main port \$OTS_MAIN_PORT, ConfigGUI UDP \$OTS_CONFIG_UDP_PORT, MacroMaker UDP \$OTS_MACROMAKER_UDP_PORT)\"
$END"

grep -q '^export OTS_SOURCE=' "$SETUP" || { echo "ERROR: no 'export OTS_SOURCE=' line in $SETUP to anchor the block."; exit 1; }
cp "$SETUP" "$SETUP.bak.$(date +%Y%m%d%H%M%S)"

BLOCK="$BLOCK" BEGIN="$BEGIN" END="$END" python3 - "$SETUP" <<'EOF'
import os, sys
p = sys.argv[1]; b, e, block = os.environ["BEGIN"], os.environ["END"], os.environ["BLOCK"]
lines = open(p).read().split("\n")
if b in lines:                                   # drop the old block
    i, j = lines.index(b), lines.index(e)
    del lines[i:j + 1]
k = next(i for i, l in enumerate(lines) if l.startswith("export OTS_SOURCE="))
lines[k + 1:k + 1] = block.split("\n")
open(p, "w").write("\n".join(lines))
EOF

echo "Updated $SETUP (backup: $(ls -t "$SETUP".bak.* | head -1))"
echo "Subsystems: $NAMES"
echo "Use: cd $OTS_DIR && source setup_ots.sh <subsystem>"
