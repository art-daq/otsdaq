# ConfigurationGUI UDP Remote Control

## Overview

The ConfigurationGUISupervisor can be driven over UDP for command-line and
agent-driven configuration management. It is a second entry point to the same
`request()` handler the web GUI uses, so every ConfigurationGUI request type
(~53) works, with the same parameters and the same XML reply. Enabled by
environment variables set before launching `ots`:

```bash
export OTS_CONFIG_UDP_PORT=2010
export OTS_CONFIG_UDP_IP=127.0.0.1
```

Works in both the full and the OTS_LITE build. The `ots` launcher prints the
address and a test command when enabled.

## CLI Tool: ots_cfg_udp_test

```bash
ots_cfg_udp_test [-y] <ip> <port> <RequestType> [name=value ...]
```

- Prints the raw XML reply. Exit code is 1 if the reply contains an
  `<Error value='...'/>` element or starts with `Error: `.
- **Identity:** the author recorded on table edits is taken from the Kerberos
  principal (`klist`). Without a ticket the tool prints
  `Identity could not be extracted from Kerberos principal (please kinit for this
  to work).` and asks before proceeding anonymously (author `UDP-REMOTE-CONTROL`).
  `-y` skips the question. **Agents:** use `-y` for read-only queries only; for
  edits, NEVER auto-confirm anonymous — ask the user if they want to proceed
  anonymously or `kinit` first. The principal line is matched case-insensitively
  (MIT `klist` prints `Default principal:`, Heimdal prints `Principal:`); if
  extraction fails with a valid ticket, check `klist` output format.
- Run with no arguments for the full usage list.

### Parameter encoding (the cause of most failures)

Values are sent raw, exactly as the browser sends them, and the server
URL-decodes only specific fields. Encode (`%XX`) **only** these:
`targetUID`, `targetColumn`, `newValue`, `tableComment`, `groupComment`, and the
individual comma-separated items of `recordList`, `fieldList`, `valueList`.
Everything else (`startPath=/`, table names, versions, group names) must be sent
unencoded; `startPath=%2F` fails with `Cannot find table named '%2F'`.

### Reading

```bash
# active groups: <Context-ActiveGroupName value='testContext'/> <Context-ActiveGroupKey value='6'/> ...
ots_cfg_udp_test -y 127.0.0.1 2010 getActiveTableGroups

# all table names
ots_cfg_udp_test -y 127.0.0.1 2010 getTables

# members of a group: <MemberName value='GatewaySupervisorTable'/> <MemberVersion value='5'/> ...
ots_cfg_udp_test -y 127.0.0.1 2010 getSpecificTableGroup groupName=testContext groupKey=6

# a table: ColumnHeader list, then one ColCSV per column (cells URL-encoded)
# Omit version for the latest. An unknown version is NOT an error: the reply
# silently falls back to the row-less mockup (TableVersion -1, empty ColCSV).
ots_cfg_udp_test -y 127.0.0.1 2010 getSpecificTable tableName=GatewaySupervisorTable

# browse the configuration tree
ots_cfg_udp_test -y 127.0.0.1 2010 getTreeView \
    tableGroup=testContext tableGroupKey=6 startPath=/ depth=1
ots_cfg_udp_test -y 127.0.0.1 2010 getTreeView \
    tableGroup=testContext tableGroupKey=6 startPath=/GatewaySupervisorTable depth=2
```

Record UIDs come from the first column (`UID` column) of `getSpecificTable`, or
from the tree. Example: the GatewaySupervisorTable record is `Supervisor0`, not
`GatewaySupervisor`.

### Workflow: edit a field → save table → save group → activate

Verified sequence enabling `EnableStateChangesOverUDP` on the gateway:

```bash
# 1. edit one cell of the current version (5) -> creates a TEMPORARY version
#    (reply: <savedVersion value='-2'/>). Temporary versions live only in this
#    UDP user's session and are not visible to the GUI or other users.
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=GatewaySupervisorTable targetTableVersion=5 \
    targetUID=Supervisor0 targetColumn=EnableStateChangesOverUDP \
    newValue=1 editNodeType=value

# 2. persist the temporary version as a real one (reply: <savedVersion value='6'/>).
#    This is the exact request the browser sends; groups cannot be built from
#    temporary member versions, so this step is mandatory.
ots_cfg_udp_test 127.0.0.1 2010 saveSpecificTable \
    tableName=GatewaySupervisorTable version=-2 temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 \
    tableComment=Enable%20UDP%20state%20changes

# 3. save a new group: tableList is name,version pairs, comma separated, taken
#    from getSpecificTableGroup with the edited table's version replaced.
#    IMPORTANT: tableList must end with a trailing comma (the parser reads
#    name,version, pairs and needs the final comma to find the version).
ots_cfg_udp_test 127.0.0.1 2010 saveNewTableGroup \
    groupName=testContext groupComment=Enable%20UDP \
    tableList=DesktopIconTable,3,DesktopWindowParameterTable,3,GatewaySupervisorTable,6,MessageFacilityTable,3,SlowControlsDashboardSupervisorTable,3,StateMachineTable,4,XDAQApplicationPropertyTable,3,...

# 4. activate the new group (reply gives the new key; parameters are groupName/groupKey)
ots_cfg_udp_test 127.0.0.1 2010 activateTableGroup \
    groupName=testContext groupKey=7 ignoreWarnings=0

# 5. restart ots for Context-group changes to take effect
```

Other useful write requests (same parameter names as the browser):

```bash
# batch set fields on records under a tree path
ots_cfg_udp_test 127.0.0.1 2010 setTreeNodeFieldValues \
    tableGroup=testContext tableGroupKey=6 startPath=/GatewaySupervisorTable \
    recordList=Supervisor0 fieldList=EnableStateChangesOverUDP valueList=1

# which active groups contain these modified tables
ots_cfg_udp_test -y 127.0.0.1 2010 checkAffectedActiveGroups \
    modifiedTables=GatewaySupervisorTable,6

# Backbone aliases
ots_cfg_udp_test 127.0.0.1 2010 setGroupAliasInActiveBackbone \
    groupAlias=Physics groupName=testContext groupKey=7
ots_cfg_udp_test 127.0.0.1 2010 setTableAliasInActiveBackbone \
    tableAlias=Latest tableName=GatewaySupervisorTable version=6
```

## Client Library: ots_cfg_udp_interface

Header: `test/ots_cfg_udp_interface.h`, implementation `.cpp` (otsdaq-utilities).
Self-contained POSIX sockets, no otsdaq dependency.

```cpp
ots_cfg_udp_interface cfg("127.0.0.1", 2010, ots_cfg_udp_interface::getKerberosUsername());
std::string xml = cfg.getActiveTableGroups();
std::string key = ots_cfg_udp_interface::extractXmlField(xml, "Context-ActiveGroupKey");
cfg.saveTreeNodeEdit("GatewaySupervisorTable", "5", "Supervisor0",
                     "EnableStateChangesOverUDP", "1");   // wrappers do the encoding
std::string any = cfg.sendRequest("getTreeView", {{"tableGroup","testContext"},
                     {"tableGroupKey","6"}, {"startPath","/"}, {"depth","1"}});
```

`sendRequest()` sends values as given; the named wrappers (`saveTreeNodeEdit`,
`setTreeNodeFieldValues`, `addTreeNodeRecords`, `saveNewTableGroup`,
`activateTableGroup`, ...) apply the per-field encoding above. The reply is the
XML string, or a `std::runtime_error` if there was no reply.

## Server Side

`ConfigurationGUISupervisor::RemoteControlWorkLoop()` (otsdaq-utilities). Each
datagram is a URL-encoded form (`RequestType=...&name=value...`), wrapped in a
synthetic `cgicc::Cgicc` plus an admin-level `RequestUserInfo`, and passed to
`request()`. Optional `username=` field sets the edit author. Replies are the
`HttpXmlDocument` output, chunked (1500 bytes, 1 ms gap), or `Error: <message>`.

## Troubleshooting

- Port not bound: both env vars must be set before `ots` starts; `ss -ulnp | grep 2010`
- `Cannot find table named '%2F'`: `startPath` was URL-encoded; send `startPath=/`
- Empty `ColCSV` rows / `TableVersion -1`: the requested version does not exist
  (e.g. a temporary version from another session, or already persisted); omit
  `version` to get the latest
- `saveTreeNodeEdit` cannot find the UID: read the real UID from `getSpecificTable`
  (first column) or `getTreeView`
- `Groups can not be created using temporary member tables`: run
  `saveSpecificTable ... temporary=0` first (step 2 above)
- Tool stops with the Kerberos question: no ticket in this shell; `kinit`, or use
  `-y` for read-only queries
