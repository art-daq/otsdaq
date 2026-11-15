#!/bin/bash
# Make a satellite ots instance a Remote Gateway subsystem of a top-level ots instance.
# Run against the TOP-LEVEL instance; adds to its active Context group, in ONE new group:
#   1. a SubsystemUserDataPathsTable row  <uid> -> <user data path> of the satellite
#      (creates the table's first version, or adds the table to the group, if needed)
#   2. a DesktopIconTable row  <uid>: WindowContentURL=ots:<ip>:<port>[?SetupType=..&LandingPage=..],
#      AlternateText=<uid> (the gateway treats any icon URL starting "ots:" as a remote
#      subsystem and finds its user data path through AlternateText), FolderPath=<folder>:
#      the satellite's icons appear inside that Desktop folder of the top level (without a folder
#      they are added loose to the top-level desktop, each caption prefixed with "<uid> ")
# then activates the group. Both pieces are needed: icon alone = no aliases/config info,
# table alone = no subsystem.
#
# Prerequisites:
#   - source setup_ots.sh <top>; top ots running with ConfigGUI UDP; Kerberos ticket
#   - $USER_DATA/TableInfo/SubsystemUserDataPathsTableInfo.xml in the top's user data (the script
#     copies it from $OTS_SOURCE/otsdaq/data-core if missing and asks you to restart ots first)
#   - the satellite's GatewaySupervisorTable has EnableStateChangesOverUDP with IP/port
#     (that is <ip>:<port> below), and the satellite shares the top's database URI
#
# Usage:
#   ./add_remote_subsystem.sh [-n] <uid> <userDataPath> <ip>:<gatewayUdpPort> [setupType] [fullName] [landingPage] [folder]
#     -n           dry run (temporary edits only; a missing table is reported, not created)
#     uid          letters/digits/_ (e.g. sunrock); icon caption is fullName (default uid)
#     userDataPath user@host:/abs/Data_<sub> (needed for RELAUNCH from Subsystem Launch: the top's ots
#                  script runs `ssh -K <user>@<host> ots_remote_instance_start ...`, also for the
#                  same host, so Kerberos ssh to it must work without a prompt; the active-groups
#                  file is then fetched with scp). A bare /abs/Data_<sub> works for monitoring and
#                  aliases but relaunch runs `ssh @` and fails. Only a-z0-9_-/ allowed in the path.
#     setupType    the satellite's `source setup_ots.sh <type>` (used to relaunch it), default uid
#     landingPage  Desktop folder opened when entering the subsystem (optional, "" for none)
#     folder       top-level Desktop folder for the satellite's icons (default: uid)
#   e.g. ./add_remote_subsystem.sh sunrock asic@correlator4.fnal.gov:/data/fasic/asic/ots/Data_sunrock \
#            127.0.0.1:2014 sunrock "Sunrock ASIC"
#
# Restart the top ots afterwards. Then: icon in the Desktop, subsystem in App Status and
# Subsystem Launch (add that page with add_desktop_icon.sh -w subsystemlaunch).

set -e
DRY=""; [ "$1" = "-n" ] && { DRY="-n"; shift; }
if [ $# -lt 3 ]; then sed -n 2,38p "$0" | sed 's/^# \{0,1\}//'; exit 1; fi
SUB_UID="$1"; UDP_PATH="$2"; ADDR="$3"; SETUP="${4:-$1}"; FULL="${5:-$1}"; LANDING="$6"; FOLDER="${7:-$1}"
[[ "$SUB_UID" =~ ^[A-Za-z0-9_]+$ ]] || { echo "ERROR: uid must be letters/digits/_."; exit 1; }
[[ "$ADDR" =~ ^[^:]+:[0-9]+$ ]] || { echo "ERROR: address must be <ip>:<port>."; exit 1; }
[[ "${UDP_PATH##*:}" =~ ^/[A-Za-z0-9_/-]+$ ]] || { echo "ERROR: user data path must be absolute, a-z0-9_-/ only."; exit 1; }
[ "${UDP_PATH%%:*}" = "$UDP_PATH" ] && echo "WARNING: no user@host: in the path, so relaunch from the top level will fail (see usage)."
[ "${UDP_PATH%%:*}" = "$UDP_PATH" ] && [ ! -f "$UDP_PATH/ServiceData/ActiveTableGroups.cfg" ] && \
    echo "WARNING: $UDP_PATH/ServiceData/ActiveTableGroups.cfg not found (has the satellite run yet?)."

HERE="$(cd "$(dirname "$0")" && pwd)"
CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"; CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"
cfg_ro() { ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }

INFO="$USER_DATA/TableInfo/SubsystemUserDataPathsTableInfo.xml"
if [ ! -f "$INFO" ]; then
    SRC="${OTS_SOURCE:-$HERE/../..}/otsdaq/data-core/TableInfo/ContextGroup/SubsystemUserDataPathsTableInfo.xml"
    [ -f "$SRC" ] || { echo "ERROR: $INFO missing and $SRC not found."; exit 1; }
    cp "$SRC" "$INFO"
    echo "Copied SubsystemUserDataPathsTableInfo.xml into $USER_DATA/TableInfo."
    echo "Restart this ots instance (table definitions load at startup), then rerun."; exit 1
fi

ACTIVE=$(cfg_ro getActiveTableGroups)
G=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupName value='\K[^']+" || true)
K=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupKey value='\K[^']+" || true)
[ -n "$G" ] && [ "$K" != "-1" ] || { echo "ERROR: no active Context group (ots running with ConfigGUI UDP?)."; exit 1; }
MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$G" groupKey="$K")
mver() { echo "$MEMBERS" | grep -A1 "MemberName value='$1'" | grep -oP "MemberVersion value='\K[^']+" | head -1 || true; }
uids() { cfg_ro getSpecificTable tableName="$1" version="$2" | grep -oP "<ColCSV value='\K[^']+" | head -1 | python3 -c "import sys,urllib.parse as u; print(' '.join(u.unquote(v) for v in sys.stdin.read().strip().split(',')))"; }

ICONV=$(mver DesktopIconTable)
case " $(uids DesktopIconTable "$ICONV") " in *" $SUB_UID "*) echo "ERROR: icon '$SUB_UID' already in $G($K)."; exit 1;; esac

URL="ots:$ADDR?SetupType=$SETUP"; [ -n "$LANDING" ] && URL+="&LandingPage=$LANDING"
COMMENT="Subsystem $SUB_UID at $UDP_PATH"
EDITS=()
SUBV=$(mver SubsystemUserDataPathsTable)
if [ -n "$SUBV" ]; then
    case " $(uids SubsystemUserDataPathsTable "$SUBV") " in *" $SUB_UID "*) echo "ERROR: subsystem '$SUB_UID' already in SubsystemUserDataPathsTable v$SUBV."; exit 1;; esac
    EDITS+=( "+SubsystemUserDataPathsTable/$SUB_UID" )
else
    LATEST=$(cfg_ro getSpecificTable tableName=SubsystemUserDataPathsTable | grep -oP "TableVersion value='\K[^']+" | head -1 || true)
    if [ -n "$LATEST" ] && [ "$LATEST" != "-1" ]; then
        EDITS+=( "add:SubsystemUserDataPathsTable=$LATEST" )
        case " $(uids SubsystemUserDataPathsTable "$LATEST") " in
            *" $SUB_UID "*) echo "Note: '$SUB_UID' already in SubsystemUserDataPathsTable v$LATEST; reusing its row (path not changed).";;
            *) EDITS+=( "+SubsystemUserDataPathsTable/$SUB_UID" );;
        esac
    else
        EDITS+=( "new:SubsystemUserDataPathsTable=$SUB_UID,$FULL,$UDP_PATH,$COMMENT" )
    fi
fi
if [[ " ${EDITS[*]} " == *"+SubsystemUserDataPathsTable/$SUB_UID"* ]]; then
    EDITS+=( "SubsystemUserDataPathsTable/$SUB_UID/SubsystemFullName=$FULL"
             "SubsystemUserDataPathsTable/$SUB_UID/SubsystemUserDataPath=$UDP_PATH" )
fi
EDITS+=( "+DesktopIconTable/$SUB_UID"
         "DesktopIconTable/$SUB_UID/Caption=$FULL"
         "DesktopIconTable/$SUB_UID/AlternateText=$SUB_UID"
         "DesktopIconTable/$SUB_UID/ForceOnlyOneInstance=True"
         "DesktopIconTable/$SUB_UID/RequiredPermissionLevel=1"
         "DesktopIconTable/$SUB_UID/ImageURL=icon-Physics.gif"
         "DesktopIconTable/$SUB_UID/FolderPath=$FOLDER"
         "DesktopIconTable/$SUB_UID/WindowContentURL=$URL" )

echo "Adding subsystem '$SUB_UID' ($FULL) -> $UDP_PATH, gateway $ADDR, setup type $SETUP, to $G($K)"
exec "$HERE/cfg_udp_edit_group.sh" $DRY -w -c "$COMMENT" "${EDITS[@]}"
