#!/bin/bash
# Generic helper: edit a single field in a config table via ConfigGUI UDP,
# then save a new group and activate it.
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set
#
# Usage:
#   ./cfg_udp_edit_field.sh [--from-key <key>] <table> <uid> <column> <newValue> [groupType]
#
#   --from-key <key>  start from that key of the active group's name instead of the active key,
#                     e.g. the key an alias points to (cfg_udp_list_aliases.sh) without activating it
#                     first. The edit still ends by activating the NEW key, as without the option.
#
# Examples:
#   ./cfg_udp_edit_field.sh FEOtsUDPTemplateInterfaceTable ExampleInterface0 Status Off Context
#   ./cfg_udp_edit_field.sh --from-key 3 DataProcessorTable DB1Consumer1 Status Off Configuration
#
# Record UIDs come from the first column of getSpecificTable or from
# cfg_udp_browse_tree.sh; they differ per instance.
#
# NOTE: enabling GatewaySupervisorTable.EnableStateChangesOverUDP needs the IP
# and port set in the same version or the gateway core-dumps at startup; this
# script refuses that edit -- use enable_gateway_udp_transitions.sh instead.

set -e

FROM_KEY=""
if [ "$1" = "--from-key" ]; then
    FROM_KEY="$2"
    case "$FROM_KEY" in ''|*[!0-9]*) echo "ERROR: --from-key needs a group key (a number), got '$FROM_KEY'."; exit 1;; esac
    shift 2 || true
fi

if [ $# -lt 4 ]; then
    echo "Usage: $0 [--from-key <key>] <table> <uid> <column> <newValue> [groupType]"
    echo "  groupType: Context (default), Configuration, or Backbone"
    exit 1
fi

TABLE="$1"
UID_NAME="$2"
COLUMN="$3"
NEW_VALUE="$4"
GROUP_TYPE="${5:-Context}"

if [ "$TABLE" = "GatewaySupervisorTable" ] && [ "$COLUMN" = "EnableStateChangesOverUDP" ] \
   && { [ "$NEW_VALUE" = "1" ] || [ "$NEW_VALUE" = "Yes" ] || [ "$NEW_VALUE" = "true" ]; }; then
    echo "ERROR: refusing to set $COLUMN alone -- IPAddressForStateChangesOverUDP and"
    echo "PortForStateChangesOverUDP must be set in the same table version or the gateway"
    echo "core-dumps at startup. Use enable_gateway_udp_transitions.sh <ip> <port> instead."
    exit 1
fi

CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"

echo "=== Edit Config Field ==="
echo "Table:  $TABLE"
echo "UID:    $UID_NAME"
echo "Column: $COLUMN"
echo "Value:  $NEW_VALUE"
echo "ConfigGUI UDP: $CFG_IP:$CFG_PORT"
echo

# ots_cfg_udp_test exits 1 when the reply holds an <Error> (even a harmless one), which
# would end this script under set -e before it can read the reply; every call below
# inspects its own reply, so the exit status is deliberately ignored.
cfg() {
    ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true
}

cfg_ro() {
    ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true
}

# Get active group for the requested type
ACTIVE=$(cfg_ro getActiveTableGroups)
GROUP_NAME=$(echo "$ACTIVE" | grep -oP "${GROUP_TYPE}-ActiveGroupName value='\K[^']+" || true)
GROUP_KEY=$(echo "$ACTIVE" | grep -oP "${GROUP_TYPE}-ActiveGroupKey value='\K[^']+")

if [ -z "$GROUP_NAME" ] || [ "$GROUP_KEY" = "-1" ]; then
    echo "ERROR: No active $GROUP_TYPE group found."
    exit 1
fi
echo "Active $GROUP_TYPE group: $GROUP_NAME key $GROUP_KEY"
if [ -n "$FROM_KEY" ]; then
    GROUP_KEY="$FROM_KEY"
    echo "Starting from $GROUP_NAME key $GROUP_KEY (--from-key)"
fi

# Get current table version
MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$GROUP_NAME" groupKey="$GROUP_KEY")
if echo "$MEMBERS" | grep -q "<Error"; then
    echo "ERROR: cannot read $GROUP_NAME key $GROUP_KEY:"
    echo "$MEMBERS" | grep -oP "Error value='\K[^']+" | head -3
    exit 1
fi
CURRENT_VERSION=$(echo "$MEMBERS" | grep -A1 "MemberName value='$TABLE'" | grep -oP "MemberVersion value='\K[^']+" || true)

if [ -z "$CURRENT_VERSION" ]; then
    echo "ERROR: $TABLE not found in $GROUP_NAME group."
    exit 1
fi
echo "$TABLE version: $CURRENT_VERSION"

# URL-encode via argv so quotes/backslashes in the value never reach Python source
urlencode() {
    python3 -c "import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=''))" "$1"
}
ENCODED_VALUE=$(urlencode "$NEW_VALUE") || { echo "ERROR: python3 is required to URL-encode the value."; exit 1; }

# Step 1: Edit the cell
echo
echo "Step 1: Edit $COLUMN -> $NEW_VALUE"
EDIT_RESULT=$(cfg saveTreeNodeEdit \
    targetTable="$TABLE" targetTableVersion="$CURRENT_VERSION" \
    targetUID="$UID_NAME" targetColumn="$COLUMN" \
    newValue="$ENCODED_VALUE" editNodeType=value)

if echo "$EDIT_RESULT" | grep -q "is the same as the current value"; then
    echo "$TABLE.$UID_NAME.$COLUMN is already '$NEW_VALUE' in $GROUP_NAME key $GROUP_KEY. Nothing to change."
    exit 0
fi
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
ENCODED_COMMENT=$(urlencode "Set $COLUMN=$NEW_VALUE") || { echo "ERROR: python3 is required to URL-encode the comment."; exit 1; }
SAVE_RESULT=$(cfg saveSpecificTable \
    tableName="$TABLE" version="$TEMP_VERSION" temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 \
    tableComment="$ENCODED_COMMENT")
if echo "$SAVE_RESULT" | grep -q "<Error"; then
    echo "ERROR in saveSpecificTable:"
    echo "$SAVE_RESULT" | grep -oP "Error value='\K[^']+"
    exit 1
fi
NEW_VERSION=$(echo "$SAVE_RESULT" | grep -oP "savedVersion value='\K[^']+" || true)
if [ -z "$NEW_VERSION" ]; then
    echo "ERROR: no savedVersion in saveSpecificTable reply."
    exit 1
fi
echo "New version: $NEW_VERSION"

# Step 3: Build the table list (replace old version with new)
echo
echo "Step 3: Save new group"
TABLE_LIST=""
while IFS= read -r line; do
    MNAME=$(echo "$line" | grep -oP "MemberName value='\K[^']+" || true)  # non-member lines must not trip set -e
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

# Reverting to an earlier set of table versions recreates an existing group, which the
# server refuses as a duplicate while naming that group's key: activate that key instead.
DUP_KEY=$(echo "$GROUP_RESULT" | grep -oP "duplicate of an existing group key \(\K[0-9]+" || true)
if [ -n "$DUP_KEY" ]; then
    echo "Group is identical to existing $GROUP_NAME key $DUP_KEY; activating that key."
    GROUP_RESULT="<TableGroupKey value='$DUP_KEY'/>"
fi

if echo "$GROUP_RESULT" | grep -q "<Error"; then
    echo "ERROR in saveNewTableGroup:"
    echo "$GROUP_RESULT" | grep -oP "Error value='\K[^']+"
    exit 1
fi

NEW_KEY=$(echo "$GROUP_RESULT" | grep -oP "<TableGroupKey value='\K[^']+" || true)
[ -n "$NEW_KEY" ] || { echo "ERROR: no group key in the saveNewTableGroup reply."; exit 1; }
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
