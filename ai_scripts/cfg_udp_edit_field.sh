#!/bin/bash
# Generic helper: edit a single field in a config table via ConfigGUI UDP,
# then save a new group and activate it.
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set
#
# Usage:
#   ./cfg_udp_edit_field.sh <table> <uid> <column> <newValue> [groupType]
#
# Examples:
#   ./cfg_udp_edit_field.sh GatewaySupervisorTable Supervisor0 EnableStateChangesOverUDP 1
#   ./cfg_udp_edit_field.sh FEOtsUDPTemplateInterfaceTable ExampleInterface0 Status Off Context

set -e

if [ $# -lt 4 ]; then
    echo "Usage: $0 <table> <uid> <column> <newValue> [groupType]"
    echo "  groupType: Context (default), Configuration, or Backbone"
    exit 1
fi

TABLE="$1"
UID_NAME="$2"
COLUMN="$3"
NEW_VALUE="$4"
GROUP_TYPE="${5:-Context}"

CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"

echo "=== Edit Config Field ==="
echo "Table:  $TABLE"
echo "UID:    $UID_NAME"
echo "Column: $COLUMN"
echo "Value:  $NEW_VALUE"
echo "ConfigGUI UDP: $CFG_IP:$CFG_PORT"
echo

cfg() {
    ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1
}

cfg_ro() {
    ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1
}

# Get active group for the requested type
ACTIVE=$(cfg_ro getActiveTableGroups)
GROUP_NAME=$(echo "$ACTIVE" | grep -oP "${GROUP_TYPE}-ActiveGroupName value='\K[^']+")
GROUP_KEY=$(echo "$ACTIVE" | grep -oP "${GROUP_TYPE}-ActiveGroupKey value='\K[^']+")

if [ -z "$GROUP_NAME" ] || [ "$GROUP_KEY" = "-1" ]; then
    echo "ERROR: No active $GROUP_TYPE group found."
    exit 1
fi
echo "Active $GROUP_TYPE group: $GROUP_NAME key $GROUP_KEY"

# Get current table version
MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$GROUP_NAME" groupKey="$GROUP_KEY")
CURRENT_VERSION=$(echo "$MEMBERS" | grep -A1 "MemberName value='$TABLE'" | grep -oP "MemberVersion value='\K[^']+")

if [ -z "$CURRENT_VERSION" ]; then
    echo "ERROR: $TABLE not found in $GROUP_NAME group."
    exit 1
fi
echo "$TABLE version: $CURRENT_VERSION"

# URL-encode the new value for fields that need it
ENCODED_VALUE=$(python3 -c "import urllib.parse; print(urllib.parse.quote('$NEW_VALUE', safe=''))" 2>/dev/null || echo "$NEW_VALUE")

# Step 1: Edit the cell
echo
echo "Step 1: Edit $COLUMN -> $NEW_VALUE"
EDIT_RESULT=$(cfg saveTreeNodeEdit \
    targetTable="$TABLE" targetTableVersion="$CURRENT_VERSION" \
    targetUID="$UID_NAME" targetColumn="$COLUMN" \
    newValue="$ENCODED_VALUE" editNodeType=value)

if echo "$EDIT_RESULT" | grep -q "<Error"; then
    echo "ERROR in saveTreeNodeEdit:"
    echo "$EDIT_RESULT" | grep -oP "Error value='\K[^']+"
    exit 1
fi

TEMP_VERSION=$(echo "$EDIT_RESULT" | grep -oP "savedVersion value='\K[^']+")
echo "Temporary version: $TEMP_VERSION"

# Step 2: Persist the temporary version
echo
echo "Step 2: Persist temporary version"
ENCODED_COMMENT=$(python3 -c "import urllib.parse; print(urllib.parse.quote('Set $COLUMN=$NEW_VALUE', safe=''))" 2>/dev/null || echo "Set%20${COLUMN}")
SAVE_RESULT=$(cfg saveSpecificTable \
    tableName="$TABLE" version="$TEMP_VERSION" temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 \
    tableComment="$ENCODED_COMMENT")
NEW_VERSION=$(echo "$SAVE_RESULT" | grep -oP "savedVersion value='\K[^']+")
echo "New version: $NEW_VERSION"

# Step 3: Build the table list (replace old version with new)
echo
echo "Step 3: Save new group"
TABLE_LIST=""
while IFS= read -r line; do
    MNAME=$(echo "$line" | grep -oP "MemberName value='\K[^']+")
    if [ -n "$MNAME" ]; then
        read -r vline
        MVER=$(echo "$vline" | grep -oP "MemberVersion value='\K[^']+")
        if [ "$MNAME" = "$TABLE" ]; then
            MVER="$NEW_VERSION"
        fi
        TABLE_LIST="${TABLE_LIST}${MNAME},${MVER},"
    fi
done <<< "$MEMBERS"

GROUP_RESULT=$(cfg saveNewTableGroup \
    groupName="$GROUP_NAME" groupComment="$ENCODED_COMMENT" \
    tableList="$TABLE_LIST")

if echo "$GROUP_RESULT" | grep -q "<Error"; then
    echo "ERROR in saveNewTableGroup:"
    echo "$GROUP_RESULT" | grep -oP "Error value='\K[^']+"
    exit 1
fi

NEW_KEY=$(echo "$GROUP_RESULT" | grep -oP "TableGroupKey value='\K[^']+")
echo "New group: $GROUP_NAME key $NEW_KEY"

# Step 4: Activate
echo
echo "Step 4: Activate $GROUP_NAME key $NEW_KEY"
ACTIVATE_RESULT=$(cfg activateTableGroup \
    groupName="$GROUP_NAME" groupKey="$NEW_KEY" ignoreWarnings=0)
ACTIVE_KEY=$(echo "$ACTIVATE_RESULT" | grep -oP "${GROUP_TYPE}-ActiveGroupKey value='\K[^']+")
echo "Active $GROUP_TYPE group key: $ACTIVE_KEY"

echo
echo "=== Done ==="
echo "$TABLE.$UID_NAME.$COLUMN = $NEW_VALUE"
echo "Active group: $GROUP_NAME key $NEW_KEY"
echo "Restart ots for Context/Backbone changes to take effect."
