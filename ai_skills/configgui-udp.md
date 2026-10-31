# ConfigurationGUI UDP Remote Control

## Overview

The ConfigurationGUISupervisor can be accessed over UDP for command-line
configuration management. Enabled by environment variables:

```bash
export OTS_CONFIG_UDP_PORT=2010
export OTS_CONFIG_UDP_IP=127.0.0.1
```

**Note:** The `ots` script prints these as `OTS_CONFIGGUI_UDP_*` but the
actual code in `ConfigurationGUISupervisor.cc` reads `OTS_CONFIG_UDP_*`.

**Known issue (as of Oct 2026):** The `RemoteControlWorkLoop` function and
its helper classes are inside `#ifndef OTS_LITE` (line ~10739), so the UDP
handler is compiled out in the lite build. The constructor tries to detect
the env vars but the workloop code doesn't exist. Needs the OTS_LITE guard
removed or the workloop ported.

## CLI Tool: ots_cfg_udp_test

```bash
ots_cfg_udp_test <ip> <port> <RequestType> [name=value ...]
```

### Common Commands

```bash
# List all tables
ots_cfg_udp_test 127.0.0.1 2010 getTables

# List table groups
ots_cfg_udp_test 127.0.0.1 2010 getTableGroups

# Get active groups
ots_cfg_udp_test 127.0.0.1 2010 getActiveTableGroups

# Browse config tree
ots_cfg_udp_test 127.0.0.1 2010 getTreeView \
    tableGroup=Context tableGroupKey=0 startPath=/ depth=1

# Read a specific table
ots_cfg_udp_test 127.0.0.1 2010 getSpecificTable \
    tableName=GatewayTable version=0

# Edit a table field
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=GatewayTable targetTableVersion=0 \
    targetUID=GatewaySupervisor targetColumn=EnableStateChangesOverUDP \
    newValue=1 editNodeType=value

# Set field values (batch)
ots_cfg_udp_test 127.0.0.1 2010 setTreeNodeFieldValues \
    tableGroup=Context tableGroupKey=0 \
    startPath=/mainContext/LinkToApplicationTable/GatewaySupervisor/LinkToSupervisorTable \
    recordList=GatewaySupervisor \
    fieldList=EnableStateChangesOverUDP valueList=1

# Save a new table group (after edits)
ots_cfg_udp_test 127.0.0.1 2010 saveNewTableGroup \
    groupName=Context tableList=GatewayTable,1,... groupComment="Enable UDP"

# Activate a group
ots_cfg_udp_test 127.0.0.1 2010 activateTableGroup \
    tableGroup=Context tableGroupKey=1 ignoreWarnings=0

# Check what active groups are affected by modified tables
ots_cfg_udp_test 127.0.0.1 2010 checkAffectedActiveGroups \
    modifiedTables=GatewayTable,1
```

## Workflow: Edit → Save Group → Activate

1. Edit table field(s) with `saveTreeNodeEdit` or `setTreeNodeFieldValues`
2. Save modified tables into a new group with `saveNewTableGroup`
3. Activate the new group with `activateTableGroup`
4. Restart `ots` for changes to take effect

## Client Library: ots_cfg_udp_interface

Header: `test/ots_cfg_udp_interface.h`
Implementation: `test/ots_cfg_udp_interface.cpp`

Simple interface — sends any request type with arbitrary name=value parameters:

```cpp
ots_cfg_udp_interface cfg("127.0.0.1", 2010);
std::string response = cfg.sendRequest("getActiveTableGroups", {});
```
