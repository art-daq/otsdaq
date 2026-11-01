#!/bin/bash
# Browse the configuration tree via ConfigGUI UDP.
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set
#
# Usage:
#   ./cfg_udp_browse_tree.sh [startPath] [depth]
#
# Examples:
#   ./cfg_udp_browse_tree.sh /                               # root, depth 1
#   ./cfg_udp_browse_tree.sh /GatewaySupervisorTable 2       # gateway fields
#   ./cfg_udp_browse_tree.sh /FEInterfaceTable 3             # FE with children

START_PATH="${1:-/}"
DEPTH="${2:-1}"
CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"

# Get active context group
ACTIVE=$(ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" getActiveTableGroups 2>&1)
GROUP_NAME=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupName value='\K[^']+")
GROUP_KEY=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupKey value='\K[^']+")

echo "Tree: $GROUP_NAME key $GROUP_KEY  path=$START_PATH  depth=$DEPTH"
echo "---"

ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" getTreeView \
    tableGroup="$GROUP_NAME" tableGroupKey="$GROUP_KEY" \
    startPath="$START_PATH" depth="$DEPTH" 2>&1 \
    | grep -E "<tree |<node |<value |<GroupID " \
    | sed 's/^[[:space:]]*//'
