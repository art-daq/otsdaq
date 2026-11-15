#!/bin/bash
# Point a table version alias at a table version (VersionAliasesTable of the active Backbone).
#
# A version alias names a table version (for example a "Calibration" version of one table); groups and
# tools can refer to the alias instead of the number. Like group aliases, version aliases live in the
# active BACKBONE group. This script (the table counterpart of cfg_udp_set_alias.sh):
#   1. saves a new VersionAliasesTable version in which <alias> points at <table> <version>
#      (setTableAliasInActiveBackbone),
#   2. saves a new key of the active Backbone group with that table version in place of the old one,
#   3. activates that Backbone key.
#
# Prerequisites:
#   - source setup_ots.sh; ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set; Kerberos ticket
#
# Usage: ./cfg_udp_set_table_alias.sh <alias> <table> <version>
#   List the existing aliases with cfg_udp_list_aliases.sh.
#
# The Gateway reads the Backbone group when its configuration manager starts. To make a running system
# use the new version alias, either restart ots (echo Y | ots -k; ots), or: Halt, Shutdown, Startup and Initialize
# with ots_drive_udp_test (see gateway-udp-fsm.md, "Applying Active Context changes").

set -e
if [ $# -ne 3 ]; then
    echo "Usage: $0 <alias> <table> <version>"
    exit 1
fi
ALIAS="$1"; TABLE_NAME="$2"; TABLE_VERSION="$3"
CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"

cfg()    { ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }       # exit 1 on a harmless <Error>: inspect the reply
cfg_ro() { ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }

echo "=== Set Table Version Alias ==="
ACTIVE=$(cfg_ro getActiveTableGroups)
BB_NAME=$(echo "$ACTIVE" | grep -oP "Backbone-ActiveGroupName value='\K[^']+" | head -1 || true)
BB_KEY=$(echo "$ACTIVE" | grep -oP "Backbone-ActiveGroupKey value='\K[^']+" | head -1 || true)
if [ -z "$BB_NAME" ] || [ "$BB_KEY" = "-1" ]; then echo "ERROR: no active Backbone group."; exit 1; fi
echo "Active Backbone: $BB_NAME key $BB_KEY"
echo "Version alias '$ALIAS' -> $TABLE_NAME version $TABLE_VERSION"

# Already there?
CURRENT=$(cfg_ro getVersionAliases | python3 -c '
import sys,re
t=sys.stdin.read(); alias=sys.argv[1]
names=re.findall(r"<VersionAlias value=.([^\x27]+)",t); tn=re.findall(r"<TableName value=.([^\x27]+)",t); tv=re.findall(r"<Version value=.([^\x27]+)",t)
for a,n,v in zip(names,tn,tv):
    if a==alias: print(n,v)
' "$ALIAS")
echo "Alias now points at: ${CURRENT:-(not defined)}"
if [ "$CURRENT" = "$TABLE_NAME $TABLE_VERSION" ]; then echo "Already set. Nothing to do."; exit 0; fi

# Step 1: new GroupAliasesTable version
echo
echo "Step 1: save a VersionAliasesTable version with the alias"
R1=$(cfg setTableAliasInActiveBackbone tableAlias="$ALIAS" tableName="$TABLE_NAME" version="$TABLE_VERSION")
if echo "$R1" | grep -q "<Error"; then echo "ERROR:"; echo "$R1" | grep -oP "Error value='\K[^']+"; exit 1; fi
NEW_VER=$(echo "$R1" | grep -oP "savedVersion value='\K[^']+" | head -1 || true)
[ -n "$NEW_VER" ] || { echo "ERROR: no savedVersion in the reply."; exit 1; }
echo "VersionAliasesTable version $NEW_VER"

# Step 2: new Backbone group key with that version
echo
echo "Step 2: save a new key of the Backbone group $BB_NAME"
MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$BB_NAME" groupKey="$BB_KEY")
TABLE_LIST=""
while IFS= read -r line; do
    MNAME=$(echo "$line" | grep -oP "MemberName value='\K[^']+" || true)
    if [ -n "$MNAME" ]; then
        read -r vline
        MVER=$(echo "$vline" | grep -oP "MemberVersion value='\K[^']+")
        [ "$MNAME" = "VersionAliasesTable" ] && MVER="$NEW_VER"
        TABLE_LIST="${TABLE_LIST}${MNAME},${MVER},"      # the trailing comma is required
    fi
done <<< "$MEMBERS"
R2=$(cfg saveNewTableGroup groupName="$BB_NAME" groupComment="version%20alias%20$ALIAS" tableList="$TABLE_LIST")
DUP=$(echo "$R2" | grep -oP "duplicate of an existing group key \(\K[0-9]+" || true)
[ -n "$DUP" ] && { echo "Identical to existing $BB_NAME key $DUP; activating that key."; R2="<TableGroupKey value='$DUP'/>"; }
if echo "$R2" | grep -q "<Error"; then echo "ERROR in saveNewTableGroup:"; echo "$R2" | grep -oP "Error value='\K[^']+"; exit 1; fi
BB_NEW=$(echo "$R2" | grep -oP "<TableGroupKey value='\K[^']+" | head -1 || true)
[ -n "$BB_NEW" ] || { echo "ERROR: no group key in the reply."; exit 1; }
echo "New Backbone: $BB_NAME key $BB_NEW"

# Step 3: activate
echo
echo "Step 3: activate $BB_NAME key $BB_NEW"
R3=$(cfg activateTableGroup groupName="$BB_NAME" groupKey="$BB_NEW" ignoreWarnings=0)
echo "Active Backbone key: $(echo "$R3" | grep -oP "Backbone-ActiveGroupKey value='\K[^']+" | head -1)"

echo
echo "=== Done ==="
echo "Version alias '$ALIAS' -> $TABLE_NAME version $TABLE_VERSION, in Backbone $BB_NAME key $BB_NEW."
echo "The running Gateway still uses the old Backbone: restart ots, or Halt, Shutdown, Startup and Initialize."
