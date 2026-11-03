#!/bin/bash
# Revert an active table group key in ActiveTableGroups.cfg without the GUI.
#
# Use when ots cannot start because the last activated group is broken (the
# ots script reports the gateway process exited during launch). The bad group
# stays in the database; this only changes which key is loaded at startup.
#
# ActiveTableGroups.cfg is plain text written by ConfigurationManagerRW:
# eight lines, name/key pairs in the order Context, Backbone, Iterate, Configuration.
#
# Prerequisites:
#   - source setup_ots.sh (sets SERVICE_DATA_PATH or USER_DATA)
#   - ots NOT running (the gateway rewrites this file on activation)
#
# Usage:
#   ./revert_active_table_group.sh                      # show the current file
#   ./revert_active_table_group.sh [groupType] <key>    # set the key
#     groupType: Context (default), Backbone, Iterate, Configuration
#     key:       the previous working key (what getActiveTableGroups reported
#                before the activation, or what the enable script printed)

set -e

CFG_FILE="${SERVICE_DATA_PATH:-${USER_DATA}/ServiceData}/ActiveTableGroups.cfg"

if [ ! -f "$CFG_FILE" ]; then
    echo "ERROR: $CFG_FILE not found. Source setup_ots.sh first."
    exit 1
fi

show_file() {
    echo "$CFG_FILE:"
    awk '
        BEGIN { t[1]="Context"; t[2]="Backbone"; t[3]="Iterate"; t[4]="Configuration" }
        NR % 2 == 1 { name = ($0 == "" ? "(none)" : $0) }
        NR % 2 == 0 { printf "  %-14s %s key %s\n", t[NR/2], name, $0 }' "$CFG_FILE"
}

if [ $# -eq 0 ]; then
    show_file
    exit 0
fi

if [ $# -eq 1 ]; then
    GROUP_TYPE="Context"
    NEW_KEY="$1"
else
    GROUP_TYPE="$1"
    NEW_KEY="$2"
fi

case "$GROUP_TYPE" in
    Context)       KEY_LINE=2 ;;
    Backbone)      KEY_LINE=4 ;;
    Iterate)       KEY_LINE=6 ;;
    Configuration) KEY_LINE=8 ;;
    *) echo "ERROR: groupType must be Context, Backbone, Iterate, or Configuration"; exit 1 ;;
esac

if ! [[ "$NEW_KEY" =~ ^-?[0-9]+$ ]]; then
    echo "ERROR: key '$NEW_KEY' must be an integer."
    exit 1
fi

if pgrep -f "xdaq.exe|ots_lite_main" >/dev/null 2>&1; then
    echo "WARNING: ots processes are running; they rewrite $CFG_FILE on activation."
    echo "Stop ots (ots -k) before reverting, or the change may be overwritten."
fi

NAME_LINE=$(( KEY_LINE - 1 ))
GROUP_NAME=$(sed -n "${NAME_LINE}p" "$CFG_FILE")
OLD_KEY=$(sed -n "${KEY_LINE}p" "$CFG_FILE")

echo "Before:"
show_file
echo

BACKUP="${CFG_FILE}.$(date +%s).bak"
cp "$CFG_FILE" "$BACKUP"
echo "Backup: $BACKUP"

sed -i "${KEY_LINE}s/.*/${NEW_KEY}/" "$CFG_FILE"

echo "$GROUP_TYPE group $GROUP_NAME: key $OLD_KEY -> $NEW_KEY"
echo
echo "After:"
show_file
echo
echo "Relaunch ots. The group with key $OLD_KEY is still in the database."
