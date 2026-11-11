#!/bin/bash
# Enable Gateway UDP state-change transitions via the ConfigGUI UDP API.
#
# This is the reference workflow for remote config changes without DB access:
#   1. Edit the three UDP fields (each edit returns a temporary table version;
#      the next edit targets the version the previous reply returned)
#   2. Persist the last temporary version as a real version
#   3. Save a new Context group with the updated table
#   4. Activate the new group
#   5. Restart ots for the change to take effect
#
# The gateway opens a socket on IPAddressForStateChangesOverUDP:PortForStateChangesOverUDP
# at construction. Enabling the flag while those are still the mockup defaults
# (DEFAULT:0) makes the gateway core-dump at startup, so this script REQUIRES
# the IP and port and sets all three fields in the same table version.
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set
#
# Usage: ./enable_gateway_udp_transitions.sh <stateChangeIP> <stateChangePort> [cfg_ip] [cfg_port]
#   stateChangeIP:   numeric address the gateway will listen on (e.g. 127.0.0.1 or the host IP)
#   stateChangePort: unused UDP port for state-change commands (e.g. 3001)
#   cfg_ip/cfg_port: ConfigGUI UDP address (default OTS_CONFIG_UDP_IP/PORT)
#
# The gateway record UID is read from the active group; override with GW_UID=<uid>
# if the table has more than one record.

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <stateChangeIP> <stateChangePort> [cfg_ip] [cfg_port]"
    echo "  Refusing to enable EnableStateChangesOverUDP without an IP and port:"
    echo "  with the defaults (DEFAULT:0) the gateway core-dumps at startup."
    exit 1
fi

STATE_IP="$1"
STATE_PORT="$2"
CFG_IP="${3:-${OTS_CONFIG_UDP_IP:-127.0.0.1}}"
CFG_PORT="${4:-${OTS_CONFIG_UDP_PORT:-2010}}"
TABLE="GatewaySupervisorTable"
COL_IP="IPAddressForStateChangesOverUDP"
COL_PORT="PortForStateChangesOverUDP"
COL_ENABLE="EnableStateChangesOverUDP"

if ! [[ "$STATE_IP" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "ERROR: stateChangeIP '$STATE_IP' must be a numeric IPv4 address (Socket.cc rejects names and DEFAULT)."
    exit 1
fi
if ! [[ "$STATE_PORT" =~ ^[0-9]+$ ]] || [ "$STATE_PORT" -lt 1 ] || [ "$STATE_PORT" -gt 65535 ]; then
    echo "ERROR: stateChangePort '$STATE_PORT' must be 1-65535."
    exit 1
fi

echo "=== Enable Gateway UDP Transitions ==="
echo "ConfigGUI UDP: $CFG_IP:$CFG_PORT"
echo

# Helper: run ots_cfg_udp_test with auto-confirm
# ots_cfg_udp_test exits 1 when the reply holds an <Error> (even a harmless one), which
# would end this script under set -e before it can read the reply; every call below
# inspects its own reply, so the exit status is deliberately ignored.
cfg() {
    ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true
}

cfg_ro() {
    ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true
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

# Find the gateway record UID (first row of the UID column) unless given
if [ -z "$GW_UID" ]; then
    GW_UID=$(cfg_ro getSpecificTable tableName="$TABLE" version="$CURRENT_VERSION" \
        | grep -oP "<ColCSV value='\K[^']+" | head -1 | cut -d, -f1)
fi
if [ -z "$GW_UID" ]; then
    echo "ERROR: could not determine the $TABLE record UID; set GW_UID=<uid> and retry."
    exit 1
fi
echo "$TABLE record UID: $GW_UID"

# Check current values
echo
echo "--- Current values ---"
TREE=$(cfg_ro getTreeView tableGroup="$GROUP_NAME" tableGroupKey="$GROUP_KEY" startPath=/$TABLE depth=2)
tree_value() { echo "$TREE" | grep -A1 "$1" | grep -oP "value='\K[^']+" | head -1; }
CUR_IP=$(tree_value "$COL_IP")
CUR_PORT=$(tree_value "$COL_PORT")
CUR_ENABLE=$(tree_value "$COL_ENABLE")
echo "$COL_IP = $CUR_IP"
echo "$COL_PORT = $CUR_PORT"
echo "$COL_ENABLE = $CUR_ENABLE"

if [ "$CUR_IP" = "$STATE_IP" ] && [ "$CUR_PORT" = "$STATE_PORT" ] && \
   { [ "$CUR_ENABLE" = "Yes" ] || [ "$CUR_ENABLE" = "1" ]; }; then
    echo
    echo "Already enabled on $STATE_IP:$STATE_PORT! Nothing to do."
    exit 0
fi

# Step 1: Edit the three cells, chaining the temporary version from each reply
# (the server may return a different temporary number on each edit).
edit_cell() {
    local version="$1" column="$2" value="$3" result saved
    result=$(cfg saveTreeNodeEdit \
        targetTable="$TABLE" targetTableVersion="$version" \
        targetUID="$GW_UID" targetColumn="$column" \
        newValue="$value" editNodeType=value)
    if echo "$result" | grep -q "<Error"; then
        echo "ERROR in saveTreeNodeEdit $column:" >&2
        echo "$result" | grep -oP "Error value='\K[^']+" >&2
        exit 1
    fi
    saved=$(echo "$result" | grep -oP "savedVersion value='\K[^']+" || true)
    if [ -z "$saved" ]; then
        echo "ERROR: no savedVersion in reply to saveTreeNodeEdit $column" >&2
        exit 1
    fi
    echo "$saved"
}

echo
echo "--- Step 1: Edit $COL_IP, $COL_PORT, $COL_ENABLE ---"
TEMP_VERSION=$(edit_cell "$CURRENT_VERSION" "$COL_IP" "$STATE_IP")
echo "$COL_IP -> $STATE_IP (temporary version $TEMP_VERSION)"
TEMP_VERSION=$(edit_cell "$TEMP_VERSION" "$COL_PORT" "$STATE_PORT")
echo "$COL_PORT -> $STATE_PORT (temporary version $TEMP_VERSION)"
TEMP_VERSION=$(edit_cell "$TEMP_VERSION" "$COL_ENABLE" "1")
echo "$COL_ENABLE -> 1 (temporary version $TEMP_VERSION)"

# Step 2: Persist the temporary version
echo
echo "--- Step 2: Persist temporary version ---"
SAVE_RESULT=$(cfg saveSpecificTable \
    tableName="$TABLE" version="$TEMP_VERSION" temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 \
    tableComment=Enable%20UDP%20state%20changes)
if echo "$SAVE_RESULT" | grep -q "<Error"; then
    echo "ERROR in saveSpecificTable:"
    echo "$SAVE_RESULT" | grep -oP "Error value='\K[^']+"
    exit 1
fi
NEW_VERSION=$(echo "$SAVE_RESULT" | grep -oP "savedVersion value='\K[^']+")
echo "New version: $NEW_VERSION"

# Verify all three cells before building a group
VERIFY=$(cfg_ro getSpecificTable tableName="$TABLE" version="$NEW_VERSION")
for want in "$STATE_IP" "$STATE_PORT"; do
    if ! echo "$VERIFY" | grep -q "$want"; then
        echo "ERROR: version $NEW_VERSION of $TABLE does not contain '$want'; not building a group."
        exit 1
    fi
done

# Step 3: Build the table list (replace old version with new)
echo
echo "--- Step 3: Save new group ---"
TABLE_LIST=""
while IFS= read -r line; do
    MNAME=$(echo "$line" | grep -oP "MemberName value='\K[^']+" || true)  # non-member lines must not trip set -e
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
# Reverting to an earlier set of table versions recreates an existing group, which the
# server refuses as a duplicate while naming that group's key: activate that key instead.
DUP_KEY=$(echo "$GROUP_RESULT" | grep -oP "duplicate of an existing group key \(\K[0-9]+" || true)
if [ -n "$DUP_KEY" ]; then
    echo "Group is identical to existing $GROUP_NAME key $DUP_KEY; activating that key."
    GROUP_RESULT="<TableGroupKey value='$DUP_KEY'/>"
fi
NEW_KEY=$(echo "$GROUP_RESULT" | grep -oP "TableGroupKey value='\K[^']+" || true)
if [ -z "$NEW_KEY" ]; then
    echo "ERROR in saveNewTableGroup: $(echo "$GROUP_RESULT" | grep -oP "Error value='\K[^']+" | head -1)"
    exit 1
fi
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
echo "$COL_ENABLE is now enabled on $STATE_IP:$STATE_PORT in $GROUP_NAME key $NEW_KEY (was key $GROUP_KEY)."
echo "Restart ots for the change to take effect."
echo "If ots then fails to start, revert with: revert_active_table_group.sh Context $GROUP_KEY"
