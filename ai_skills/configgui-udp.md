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
# active groups (reply gives group names and keys for Context, Backbone, etc.)
ots_cfg_udp_test -y 127.0.0.1 2010 getActiveTableGroups

# all table names
ots_cfg_udp_test -y 127.0.0.1 2010 getTables

# members of a group (use the group name and key from getActiveTableGroups)
ots_cfg_udp_test -y 127.0.0.1 2010 getSpecificTableGroup groupName=<CONTEXT_GROUP> groupKey=<CONTEXT_KEY>

# a table: ColumnHeader list, then one ColCSV per column (cells URL-encoded)
# Omit version for the latest. An unknown version is NOT an error: the reply
# silently falls back to the row-less mockup (TableVersion -1, empty ColCSV).
ots_cfg_udp_test -y 127.0.0.1 2010 getSpecificTable tableName=GatewaySupervisorTable

# browse the configuration tree
ots_cfg_udp_test -y 127.0.0.1 2010 getTreeView \
    tableGroup=<CONTEXT_GROUP> tableGroupKey=<CONTEXT_KEY> startPath=/ depth=1
ots_cfg_udp_test -y 127.0.0.1 2010 getTreeView \
    tableGroup=<CONTEXT_GROUP> tableGroupKey=<CONTEXT_KEY> startPath=/GatewaySupervisorTable depth=2
```

Record UIDs come from the first column (`UID` column) of `getSpecificTable`, or
from the tree. They are instance-specific and are NOT the table name (the
GatewaySupervisorTable record is typically named something like `Supervisor0`,
never `GatewaySupervisor`); always read the UID rather than guessing it.

### Workflow: edit fields → save table → save group → activate

Sequence enabling state changes over UDP on the gateway.

**WARNING — never enable `EnableStateChangesOverUDP` alone.** The gateway
opens a socket on `IPAddressForStateChangesOverUDP:PortForStateChangesOverUDP`
at construction; with the mockup defaults (`DEFAULT`, `0`) `Socket.cc` throws
`Invalid IP:Port combination... DEFAULT:0` in a detached thread, the gateway
core-dumps, and `ots` cannot start until the active group is reverted. All three
fields must be set in the same version (IP must be a numeric address, e.g.
`127.0.0.1` or the host IP; pick an unused UDP port, e.g. `3001`).

```bash
# First, discover the active Context group name and key, and the current
# GatewaySupervisorTable version, using getActiveTableGroups and
# getSpecificTableGroup.  The examples below use placeholders:
#   <CONTEXT_GROUP>  — the active Context group name
#   <CONTEXT_KEY>    — the active Context group key
#   <GW_VERSION>     — the current GatewaySupervisorTable version
#   <GW_UID>         — the GatewaySupervisorTable record UID (from getSpecificTable)
#   <TEMP_VERSION_n> — the temporary version returned by the n-th saveTreeNodeEdit
#   <SAVED_VERSION>  — the real version returned by saveSpecificTable

# 1. edit cells of the current version. The first edit creates a TEMPORARY
#    version (negative number in the reply's <savedVersion value='...'/>). Each
#    following edit must target the temporary version from the PREVIOUS reply,
#    and may return a DIFFERENT temporary number (e.g. -2, then -3); always use
#    the value each reply returns, never assume it stays the same. Temporary
#    versions live only in this UDP user's session and are not visible to the
#    GUI or other users.
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=GatewaySupervisorTable targetTableVersion=<GW_VERSION> \
    targetUID=<GW_UID> targetColumn=IPAddressForStateChangesOverUDP \
    newValue=127.0.0.1 editNodeType=value
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=GatewaySupervisorTable targetTableVersion=<TEMP_VERSION_1> \
    targetUID=<GW_UID> targetColumn=PortForStateChangesOverUDP \
    newValue=3001 editNodeType=value
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=GatewaySupervisorTable targetTableVersion=<TEMP_VERSION_2> \
    targetUID=<GW_UID> targetColumn=EnableStateChangesOverUDP \
    newValue=1 editNodeType=value

# 2. persist the last temporary version as a real one (reply: <savedVersion value='N'/>).
#    This is the exact request the browser sends; groups cannot be built from
#    temporary member versions, so this step is mandatory.
ots_cfg_udp_test 127.0.0.1 2010 saveSpecificTable \
    tableName=GatewaySupervisorTable version=<TEMP_VERSION_3> temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 \
    tableComment=Enable%20UDP%20state%20changes

#    verify before building a group: all three UDP cells must show the new values
ots_cfg_udp_test -y 127.0.0.1 2010 getSpecificTable tableName=GatewaySupervisorTable version=<SAVED_VERSION>

# 3. save a new group: tableList is name,version pairs, comma separated, taken
#    from getSpecificTableGroup with the edited table's version replaced.
#    IMPORTANT: tableList must end with a trailing comma (the parser reads
#    name,version, pairs and needs the final comma to find the version).
ots_cfg_udp_test 127.0.0.1 2010 saveNewTableGroup \
    groupName=<CONTEXT_GROUP> groupComment=Enable%20UDP \
    tableList=<TABLE1>,<VER1>,<TABLE2>,<VER2>,GatewaySupervisorTable,<SAVED_VERSION>,...

# 4. activate the new group (reply gives the new key; parameters are groupName/groupKey)
ots_cfg_udp_test 127.0.0.1 2010 activateTableGroup \
    groupName=<CONTEXT_GROUP> groupKey=<NEW_KEY> ignoreWarnings=0

# 5. restart ots for Context-group changes to take effect
```

**If ots fails to start after an activation** (`ots` reports the gateway
process exited during launch and prints the crash lines from
`$USER_DATA/Logs/otsdaq_quiet_run-gateway-*.txt`), revert without the GUI using
`ai_scripts/revert_active_table_group.sh` (see below). It edits
`$SERVICE_DATA_PATH/ActiveTableGroups.cfg`, which is plain text — pairs of lines
`<group name>` / `<key>` for Context, Backbone, Iterate, Configuration. Set the
Context key back to the previous working key (the one reported by
`getActiveTableGroups` before the activation) and relaunch `ots`. The bad group
stays in the database; fix the table and save a new group.

Other useful write requests (same parameter names as the browser):

```bash
# batch set fields on records under a tree path (all three UDP fields together,
# never the enable flag alone -- see WARNING above)
ots_cfg_udp_test 127.0.0.1 2010 setTreeNodeFieldValues \
    tableGroup=<CONTEXT_GROUP> tableGroupKey=<CONTEXT_KEY> startPath=/GatewaySupervisorTable \
    recordList=<GW_UID> \
    fieldList=IPAddressForStateChangesOverUDP,PortForStateChangesOverUDP,EnableStateChangesOverUDP \
    valueList=127.0.0.1,3001,1

# which active groups contain these modified tables
ots_cfg_udp_test -y 127.0.0.1 2010 checkAffectedActiveGroups \
    modifiedTables=GatewaySupervisorTable,<SAVED_VERSION>

# Backbone aliases
ots_cfg_udp_test 127.0.0.1 2010 setGroupAliasInActiveBackbone \
    groupAlias=Physics groupName=<CONTEXT_GROUP> groupKey=<NEW_KEY>
ots_cfg_udp_test 127.0.0.1 2010 setTableAliasInActiveBackbone \
    tableAlias=Latest tableName=GatewaySupervisorTable version=<SAVED_VERSION>
```

## Helper Scripts (otsdaq/ai_scripts)

All take the ConfigGUI UDP address from `OTS_CONFIG_UDP_IP`/`OTS_CONFIG_UDP_PORT`
and discover group names, keys and table versions at run time — nothing about a
particular instance is hard-coded.

| Script | Purpose |
|--------|---------|
| `cfg_udp_browse_tree.sh [startPath] [depth]` | read-only tree browse of the active Context group |
| `cfg_udp_edit_field.sh <table> <uid> <column> <value> [groupType]` | one-field edit → save → new group → activate; refuses the enable-only gateway UDP edit |
| `enable_gateway_udp_transitions.sh <ip> <port> [cfg_ip] [cfg_port]` | sets IP, port and enable flag together, chaining temporary versions |
| `revert_active_table_group.sh [groupType] [key]` | edits `ActiveTableGroups.cfg` when `ots` cannot start; backs the file up first |
| `add_desktop_icon.sh [-n] [-w] appstatus\|subsystemlaunch` or `<caption> <alt> <url> <image> [perm] [app] [unique] [folder]` | adds a Desktop icon to the active Context group (see "Desktop icons" below); `-n` is a dry run, `-w` accepts tree warnings |
| `cfg_udp_edit_group.sh [-n] [-w] [-g newName] <edit>...` | many edits across tables into one new group (cells, links, rows, `new:`/`add:` tables), activate |
| `add_remote_subsystem.sh` | top-level Context: satellite icon + `SubsystemUserDataPathsTable` row (see `ots-subsystems.md`) |

## Editing links and adding rows (saveTreeNodeEdit editNodeType values)

`saveTreeNodeEdit` does more than set a cell. The `editNodeType` values (from
`handleSaveTreeNodeEditXML`) used so far:

| editNodeType | targetColumn | newValue | effect |
|--------------|--------------|----------|--------|
| `value` | the field | new value (URI-encoded) | set one cell on an existing row |
| `link-GroupID` | the link column, e.g. `LinkToParameterTable` | `<tableName>,<groupID>` (raw comma) | point a group link at a table and group |
| `link-UID` | the link column | `<tableName>,<uid>` | point a UID link at a table and row |
| `table-newGroupRow` | any column of the table (required, unused) | `<linkIndex>,<groupID>,<newRowUID>` | add a row to group `<groupID>`; `linkIndex` is the link index of the table's GroupID column |

Notes from a real run (Visualizer window parameter, `mode=waveform`):
- A "value is the same as the current value" reply is an `<Error>` but harmless (for
  example `Enabled` was already `True` on a new row).
- Chain edits on the **version each reply returns** (`-2`, then `-3`, then `-4`);
  do not guess. A failed edit does not consume a number.
- Edits on different tables each start their own temporary chain (both started at
  `-2`); persist each with `saveSpecificTable ... version=<its last temp>`, then put
  both new versions in the `tableList` of one `saveNewTableGroup`.
- Read the result back with `getTreeView ... startPath=/<Table>/<UID>/<LinkColumn>`
  before activating: a link edit shows as the linked rows under that path.

### Tree warnings block a group save

`saveNewTableGroup` (and `activateTableGroup`) can reply with only `<TreeErrors value='...'/>` and no
`TableGroupKey`: warnings the GUI shows for you to accept (e.g. an Off app linking to a table that is not
in the group). Resend with `ignoreWarnings=1`. Parse the key with `<TableGroupKey value='` and take the
first match: an activation with a malformed key leaves the running instance with no active group.

### Reverting to an earlier configuration: "duplicate of an existing group key"

Setting values back recreates a table version that already exists (`saveSpecificTable` with
`lookForEquivalent=1` returns the old version) and so a group identical to an existing one.
`saveNewTableGroup` then refuses it: `<Error ... Failed to create table group: <name>. It is a
duplicate of an existing group key (N)'/>`, with `<TableGroupKey value='N'/>` in the same reply.
Activate key N instead (the helper scripts do). Note `ots_cfg_udp_test` exits 1 whenever the
reply holds an `<Error>`: scripts using `set -e` must not let that end them before they read the
reply (`ots_cfg_udp_test ... || true`).

### Desktop icons (e.g. the App Status icon)

The Configuration GUI has buttons that add standard icons (App Status, Console, Code
Editor, Iterator). They call `DesktopContent.addDesktopIcon`, i.e. the request
`addDesktopIcon`, which is implemented by the **GatewaySupervisor**
(`handleAddDesktopIconRequest` -> `ConfigurationSupervisorBase::handleAddDesktopIconXML`),
not by the ConfigurationGUISupervisor, so it **cannot be sent over ConfigGUI UDP**.
The same result is one record in `DesktopIconTable` plus a new Context group:

| Column | App Status icon value |
|--------|-----------------------|
| `IconName` (UID) | `generatedIconAppStatus` (`generatedIcon` + letters/digits of the caption) |
| `Status` | On |
| `Caption`, `AlternateText` | `App Status`, `STAT` |
| `ForceOnlyOneInstance` | True |
| `RequiredPermissionLevel` | 1 |
| `ImageURL` | `icon-SystemStatus.png` (in `WebGUI/images/iconImages`) |
| `WindowContentURL` | `/WebPath/html/SystemStatus.html` (the page posts `getAppStatus` etc. to the Gateway) |
| `LinkToApplicationTable` / `LinkToApplicationUID` | `XDAQApplicationTable` / `Supervisor` (the app with LID 200, the Gateway) |
| `FolderPath` | `DEFAULT` (means none, like the stock icons) |

Edits: `table-newRow` (`targetColumn=IconName newValue=<uid>`), then `value` edits, then
`link-UID` for the app link (`newValue=XDAQApplicationTable,Supervisor`); persist, save a
new Context group, activate. `ai_scripts/add_desktop_icon.sh appstatus` does all of it.

Icons are loaded by the Gateway at startup, so restart `ots` afterwards. Verify with the
Gateway UDP query `GetRemoteDesktopIcons` (see `gateway-udp-fsm.md`); the reply lists
`...,App Status,STAT,1,1,icon-SystemStatus.png,http://<host>:<port>/WebPath/html/SystemStatus.html?urn=200&...`.
A window parameter (`mode=waveform`, below) is a different mechanism: it needs
`DesktopWindowParameterTable` rows and a parameter link on the icon.

General rule: GUI requests are only reachable over ConfigGUI UDP when the
ConfigurationGUISupervisor implements them. Requests handled by the Gateway
(`addDesktopIcon`, `getAppStatus`, `restartApps`, ...) need the equivalent table edit, or
the Gateway UDP messages.

### First version of a brand-new table (no saved versions yet)

Tree edits cannot start from the empty template: `saveTreeNodeEdit ... targetTableVersion=-1`
fails with `Invalid latest version`. Create version 0 with `saveSpecificTable` from the
template (`version=-1`) and the rows as data, exactly as the browser table editor does:

```bash
ots_cfg_udp_test 127.0.0.1 2010 saveSpecificTable tableName=FESunrockInterfaceTable \
    version=-1 temporary=0 dataOffset=0 sourceTableAsIs=0 lookForEquivalent=0 \
    tableComment=First%20version \
    "data=SunrockInterface0,sunrock-zynq.placeholder,root,22,5,5,My%20comment,;"
```

`data`: each cell URI-encoded and followed by `,`; each row ends with `;`. All columns in
order, including `CommentDescription`; the last two (Author, RecordInsertionTime) are
filled in automatically and may be omitted. Then add the new table as an extra
`name,version,` pair in the group's `tableList`.

### Example: pass window parameters to a Desktop icon (Context group)

The Visualizer page reads `mode` from its window parameters (`Visualization.html`:
`DesktopContent.getParameter(0, "mode")`; `waveform` launches the waveform viewer,
anything else the ROOT viewer). Window parameters come from `DesktopWindowParameterTable`
through the icon's `LinkToParameterTable`/`LinkToParameterGroupID` (group link index
`windowParameters`).

```bash
# 1. link the icon (record UID = the IconName, here Visualizer) to a new group
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=DesktopIconTable targetTableVersion=<iconVer> targetUID=Visualizer \
    targetColumn=LinkToParameterTable newValue=DesktopWindowParameterTable,Visualizer_params \
    editNodeType=link-GroupID
# 2. add the parameter row to that group, then set key and value
ots_cfg_udp_test 127.0.0.1 2010 saveTreeNodeEdit \
    targetTable=DesktopWindowParameterTable targetTableVersion=<paramVer> \
    targetUID=Visualizer_param0 targetColumn=windowParameterGroupID \
    newValue=windowParameters,Visualizer_params,Visualizer_param0 editNodeType=table-newGroupRow
#    then editNodeType=value on windowParameterKey=mode and windowParameterValue=waveform
#    (Enabled is already True on a new row)
# 3. saveSpecificTable both tables, saveNewTableGroup (trailing comma), activateTableGroup,
#    restart ots (Context group).
```
*Verified* through the tree read-back (Visualizer -> `Visualizer_param0` with
`mode=waveform`, Enabled). The effect in the browser window is checked in
`tutorial-to-visualizer-waveforms.md`.

## Client Library: ots_cfg_udp_interface

Header: `test/ots_cfg_udp_interface.h`, implementation `.cpp` (otsdaq-utilities).
Self-contained POSIX sockets, no otsdaq dependency.

```cpp
ots_cfg_udp_interface cfg("127.0.0.1", 2010, ots_cfg_udp_interface::getKerberosUsername());
std::string xml   = cfg.getActiveTableGroups();
std::string group = ots_cfg_udp_interface::extractXmlField(xml, "Context-ActiveGroupName");
std::string key   = ots_cfg_udp_interface::extractXmlField(xml, "Context-ActiveGroupKey");
std::string r     = cfg.saveTreeNodeEdit("GatewaySupervisorTable", gwVersion, gwUid,
                        "IPAddressForStateChangesOverUDP", "127.0.0.1"); // wrappers do the encoding
std::string temp  = ots_cfg_udp_interface::extractXmlField(r, "savedVersion"); // use in the next edit
std::string any   = cfg.sendRequest("getTreeView", {{"tableGroup", group},
                        {"tableGroupKey", key}, {"startPath","/"}, {"depth","1"}});
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
- Second `saveTreeNodeEdit` returns a different temporary number (e.g. `-3`
  after `-2`): normal; chain each edit from the version in the previous reply
  and persist the last one
- Tool stops with the Kerberos question: no ticket in this shell; `kinit`, or use
  `-y` for read-only queries
