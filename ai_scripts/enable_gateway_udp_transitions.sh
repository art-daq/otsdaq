#!/bin/bash
# Enable Gateway UDP state-change transitions via the ConfigGUI UDP API.
#
# This is the reference workflow for remote config changes without DB access:
#   1. Edit the field (creates a temporary table version)
#   2. Persist the temporary version as a real version
#   3. Save a new Context group with the updated table
#   4. Activate the new group
#   5. Restart ots for the change to take effect
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set
#
# Usage: ./enable_gateway_udp_transitions.sh [ip] [port]

set -e

CFG_IP="${1:-${OTS_CONFIG_UDP_IP:-127.0.0.1}}"
CFG_PORT="${2:-${OTS_CONFIG_UDP_PORT:-2010}}"
TABLE="GatewaySupervisorTable"
UID_NAME="Supervisor0"
COLUMN="EnableStateChangesOverUDP"

echo "=== Enable Gateway UDP Transitions ==="
echo "ConfigGUI UDP: $CFG_IP:$CFG_PORT"
echo

# Helper: run ots_cfg_udp_test with auto-confirm
cfg() {
    ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1
}

cfg_ro() {
    ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1
}

# Get active context group
echo "--- Getting active context group ---"
ACTIVE=$(cfg_ro getActiveTableGroups)
GROUP_NAME=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupName value='\K[^']+")
GROUP_KEY=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupKey value='\K[^']+")
echo "Active: $GROUP_NAME key $GROUP_KEY"

# Get current table version from the group
echo
echo "--- Getting $TABLE version ---"
MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$GROUP_NAME" groupKey="$GROUP_KEY")
CURRENT_VERSION=$(echo "$MEMBERS" | grep -A1 "MemberName value='$TABLE'" | grep -oP "MemberVersion value='\K[^']+")
echo "$TABLE version: $CURRENT_VERSION"

# Check current value
echo
echo "--- Current value ---"
TREE=$(cfg_ro getTreeView tableGroup="$GROUP_NAME" tableGroupKey="$GROUP_KEY" startPath=/$TABLE depth=2)
CURRENT_VALUE=$(echo "$TREE" | grep -A1 "$COLUMN" | grep -oP "value='\K[^']+" | head -1)
echo "$COLUMN = $CURRENT_VALUE"

if [ "$CURRENT_VALUE" = "Yes" ] || [ "$CURRENT_VALUE" = "1" ]; then
    echo
    echo "Already enabled! Nothing to do."
    exit 0
fi

# Step 1: Edit the cell
echo
echo "--- Step 1: Edit $COLUMN -> 1 ---"
EDIT_RESULT=$(cfg saveTreeNodeEdit \
    targetTable="$TABLE" targetTableVersion="$CURRENT_VERSION" \
    targetUID="$UID_NAME" targetColumn="$COLUMN" \
    newValue=1 editNodeType=value)
TEMP_VERSION=$(echo "$EDIT_RESULT" | grep -oP "savedVersion value='\K[^']+")
echo "Temporary version: $TEMP_VERSION"

# Step 2: Persist the temporary version
echo
echo "--- Step 2: Persist temporary version ---"
SAVE_RESULT=$(cfg saveSpecificTable \
    tableName="$TABLE" version="$TEMP_VERSION" temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 \
    tableComment=Enable%20UDP%20state%20changes)
NEW_VERSION=$(echo "$SAVE_RESULT" | grep -oP "savedVersion value='\K[^']+")
echo "New version: $NEW_VERSION"

# Step 3: Build the table list (replace old version with new)
echo
echo "--- Step 3: Save new group ---"
TABLE_LIST=""
while IFS= read -r line; do
    MNAME=$(echo "$line" | grep -oP "MemberName value='\K[^']+")
    if [ -n "$MNAME" ]; then
        # read the next MemberVersion
        read -r vline
        MVER=$(echo "$vline" | grep -oP "MemberVersion value='\K[^']+")
        if [ "$MNAME" = "$TABLE" ]; then
            MVER="$NEW_VERSION"
        fi
        TABLE_LIST="${TABLE_LIST}${MNAME},${MVER},"
    fi
done <<< "$MEMBERS"

GROUP_RESULT=$(cfg saveNewTableGroup \
    groupName="$GROUP_NAME" groupComment=Enable%20UDP%20state%20changes \
    tableList="$TABLE_LIST")
NEW_KEY=$(echo "$GROUP_RESULT" | grep -oP "TableGroupKey value='\K[^']+")
echo "New group: $GROUP_NAME key $NEW_KEY"

# Step 4: Activate the new group
echo
echo "--- Step 4: Activate $GROUP_NAME key $NEW_KEY ---"
ACTIVATE_RESULT=$(cfg activateTableGroup \
    groupName="$GROUP_NAME" groupKey="$NEW_KEY" ignoreWarnings=0)
ACTIVE_KEY=$(echo "$ACTIVATE_RESULT" | grep -oP "Context-ActiveGroupKey value='\K[^']+")
echo "Active context group key: $ACTIVE_KEY"

echo
echo "=== Done ==="
echo "$COLUMN is now enabled in $GROUP_NAME key $NEW_KEY."
echo "Restart ots for the change to take effect."
