# Subsystems: a top-level ots and its satellite subsystems

A **top-level** ots instance controls and monitors other ots instances (**satellites**) as
*Remote Gateway subsystems*: it forwards FSM commands (Configure with a per-subsystem alias,
Start, Stop, ...), shows their status, and can relaunch them. Each instance has its own
`USER_DATA` (`Data_<name>`) and ports. All share one database, so the top level can read the
satellites' Backbone aliases. Several instances can run from one lite area.

Verified 2026-11-11 on the Sunrock lite area: `top` + satellite `sunrock` running side by side,
top lists `sunrock`, gets its status ("Initial") and shows it in Subsystem Launch. The first
relaunch failed because the path had no `user@host:` (fixed, path fetched over scp). Not yet verified:
relaunch after the fix, the `sunrock` Desktop folder, Configure/Start of the satellite from top.

## Example layout (one host)

| Instance | USER_DATA | main port | ConfigGUI UDP | MacroMaker UDP | Gateway UDP | Console/MF UDP | Context |
|----------|-----------|-----------|---------------|----------------|-------------|----------------|---------|
| top | `Data_top` | 2115 | 2110 | 2111 | 2114 | 31000/31001 | `topContext` (FE off, satellite icon, Subsystem Launch) |
| sunrock | `Data_sunrock` | 2015 | 2010 | 2011 | 2014 | 30000/30001 | `testContext`, Backbone `sunrockBackbone` (only `sunrockSystemAlias`) |

`ARTDAQ_DATABASE_URI` is the same for all. Activating a group writes only the edited instance's
`$USER_DATA/ServiceData/ActiveTableGroups.cfg`, so each instance has its own active groups even
though they share one database.

## Choosing and running an instance

```bash
cd <lite-area>
source setup_ots.sh sunrock && ots        # satellite (start it first, or top shows it failing until it is up)
source setup_ots.sh top && ots            # top level; web: http://<host>:2115/urn:xdaq-application:lid=200/
source setup_ots.sh top && echo Y | ots -k   # ots -k kills only processes of $USER_DATA/
```

`setup_ots.sh <name>` sets `OTS_SETUP_TYPE`, `USER_DATA`, `OTSDAQ_DATA`, `OTS_MAIN_PORT` and the
ConfigGUI/MacroMaker UDP exports; with no or an unknown name it lists the names and returns 1.
The block comes from `../ai_scripts/setup_ots_subsystems.sh`. No build is needed. `mz_uc`
regenerates `setup_ots.sh` without the block, so rerun the script after it; `mb`/`mz` do not touch
`setup_ots.sh`. `ots` already keys its quit/lock files on `OTS_SETUP_TYPE`.

## How the top level finds a satellite (two pieces, both in the top's Context group)

1. **Desktop icon** (`DesktopIconTable`) with `WindowContentURL=ots:<ip>:<port>` where
   `<ip>:<port>` is the satellite gateway's UDP state-change socket
   (`GatewaySupervisorTable.IPAddressForStateChangesOverUDP/PortForStateChangesOverUDP`, enabled).
   Any icon URL starting `ots:` is treated as a remote gateway (`GatewaySupervisor.cc`, icon loop).
   Optional `?SetupType=<type>` (the satellite's `setup_ots.sh` argument, used to relaunch it) and
   `&LandingPage=<folder>`. `AlternateText` = the UID of the row in piece 2.
   **`FolderPath`** = the top-level Desktop folder where the satellite's icons go (they are fetched
   with `GetRemoteDesktopIcons` and placed under `<FolderPath>/<their own folder>`). With no folder
   there is **no subsystem folder**: the icons are added loose to the top-level desktop, each caption
   prefixed with `<uid> `.
2. **`SubsystemUserDataPathsTable`** row: `SubsystemUID`, `SubsystemFullName`,
   `SubsystemUserDataPath` = **`user@host:/abs/Data_<sub>`**, even on the same host. Only
   `a-z A-Z 0-9 _ - /` are allowed in the path. The top level reads the satellite's
   `ServiceData/ActiveTableGroups.cfg` through it (with `scp -o BatchMode=yes` when user/host are
   given) to find the satellite's Backbone aliases. **Relaunch** (Subsystem Launch / App Status)
   sends `LAUNCH_INSTANCE;<user>;<host>;<lite dir>;Normal;<SetupType>;<Data path>` to the top's
   `ots` action handler, which runs `ssh -K <user>@<host> ... ots_remote_instance_start <host>
   <lite dir> Normal <type>` after writing `KILL_ALL` into the satellite's
   `ServiceData/StartOTS_action_quit.cmd`. With a bare local path user and host are empty: the
   handler runs `ssh @`, which prints ssh usage, then "Remote start failed", and the satellite is not
   restarted (its uptime does not reset). Kerberos ssh/scp from the top level's account to `user@host`
   must work without a prompt (test: `ssh -K -o BatchMode=yes user@host true`). The table must
   be a **member of the top's Context group**.

`../ai_scripts/add_remote_subsystem.sh` adds both in one group and activates it. It creates the
table's first version, adds an existing version to the group, or adds a row, as needed. Restart
the top level afterwards.

**Which alias a satellite is configured with.** The top level sends `<fsm>,Configure,<alias>`
with its per-satellite *selected config alias*. The choices are the satellite's Backbone aliases
(read through piece 2), sorted. If the selection is empty or invalid, it is reset to the **first**
alias (`Resetting invalid selected_config_alias '' ... to first available` in the top's log). The GUI
(Subsystem Launch / App Status, request `setRemoteSubsystemFsmControl`, web only) stores the choice in
`$USER_DATA/ServiceData/RemoteSubsystems.txt` of the top level (`Remote Gateway Settings, file format v1.0`, then
4 lines per satellite: icon UID, `Follow FSM` | `Do Not Halt` | `Only Configure`, included `1`/`0`,
alias). The gateway reads that file back at startup only when `EnableAckForStateChangesOverUDP`
is set and `EnableStateChangesOverUDP` is not (`GatewaySupervisor.cc`, constructor). So for a
top level with UDP state changes on, **every restart falls back to the first alias**.
Seen: satellites copied from the demo share the Backbone with `defaultSystemAlias`, which sorts
first, so top configured `sunrock` with the demo `defaultConfig`. The satellite then failed
loading `FEOtsUDPTemplateInterface` (`undefined symbol: typeinfo for ots::OtsUDPHardware` in this
lite build). **Fix:** give each satellite its own Backbone that offers only its own aliases,
activated only in the satellite:
`cfg_udp_edit_group.sh -w -t Backbone -g sunrockBackbone -- -GroupAliasesTable/defaultSystemAlias`
(on the satellite's ConfigGUI UDP). Recover a Failed satellite with
`ots_drive_udp_test <ip> <sat gateway UDP port> <fsm> Halt`.

## Subsystem Launch page (monitor, configure/start, relaunch satellites)

`/WebPath/html/SubsystemLaunch.html` (otsdaq-utilities) is the top-level page for subsystems. It is
not in the demo Context; add it with
`../ai_scripts/add_desktop_icon.sh -w subsystemlaunch` (link to the Gateway, LID 200, no
`fsm_name` so the page offers a Run Type dropdown kept on the gateway in
`ServiceData/SubsystemLaunchRunType.txt`), then restart the top level. App Status (`add_desktop_icon.sh -w
appstatus`) also lists remote subsystems.

## Creating a new instance from an existing one (recipe)

1. `cp -a Data Data_<name>` (source instance stopped or idle), then
   `setup_ots_subsystems.sh <lite-area> <name>:<main>:<cfg>:<mm> ...` listing **every** instance.
2. **Fix the launch port in the copy** when it differs from the source: `ots` reads the gateway
   port from `XDAQConfigurations/otsConfiguration_CMake_Run.xml`, which the gateway wrote with
   `${OTS_MAIN_PORT}` already resolved. Without the fix `ots` reports the source's port "already in
   use". Edit `url="http://<host>:<port>"`; every later launch rewrites it correctly.
3. Ports that come from Context tables, not from the environment, must differ per instance or the
   second one fails to bind: `GatewaySupervisorTable.PortForStateChangesOverUDP` and
   `MessageFacilityTable.WebConsoleForwardingPort0/1` (+ `QTViewerForwardingPort`). (NUMBER
   columns do accept `${ENV_VAR}` values, as `XDAQContextTable.PORT=${OTS_MAIN_PORT}` does, but no
   environment variables exist yet for these.)
4. Start the new instance **alone** (its copied table ports still collide), give it its own
   Context group with `../ai_scripts/cfg_udp_edit_group.sh -w -g <name>Context ...`, restart it.
   The old instance can run again after that. If you cannot stop the old one, `ots --wiz` runs only
   the ConfigGUI (port `OTS_WIZ_MODE_MAIN_PORT`/`OTS_MAIN_PORT`, Console ports offset), but only
   if ConfigGUI UDP is enabled in that environment.
5. For a top level: turn its `FESupervisor` Off if it drives no hardware itself, run
   `add_remote_subsystem.sh` per satellite, and `add_desktop_icon.sh -w subsystemlaunch`.

What `top` ended up with (it took several groups, each fix found along the way: the path first had no
`user@host:` and the icon had no folder; today `add_remote_subsystem.sh` does the satellite part in one go):

```bash
source setup_ots.sh top
cfg_udp_edit_group.sh -w -g topContext \
  "new:SubsystemUserDataPathsTable=sunrock,Sunrock ASIC,asic@correlator4.fnal.gov:/data/fasic/asic/ots/Data_sunrock,Sunrock subsystem" \
  GatewaySupervisorTable/Supervisor0/PortForStateChangesOverUDP=2114 \
  MessageFacilityTable/MF_Entry/WebConsoleForwardingPort0=31000 \
  MessageFacilityTable/MF_Entry/WebConsoleForwardingPort1=31001 \
  MessageFacilityTable/MF_Entry/QTViewerForwardingPort=31000 \
  XDAQApplicationTable/FESupervisor/Status=Off \
  +DesktopIconTable/sunrock DesktopIconTable/sunrock/Caption=Sunrock \
  DesktopIconTable/sunrock/AlternateText=sunrock DesktopIconTable/sunrock/ImageURL=icon-Physics.gif \
  DesktopIconTable/sunrock/FolderPath=sunrock \
  "DesktopIconTable/sunrock/WindowContentURL=ots:127.0.0.1:2014?SetupType=sunrock"
add_desktop_icon.sh -w subsystemlaunch
```

## Checking it worked (top-level gateway log `Data_top/Logs/otsdaq_quiet_run-gateway-<host>-<port>.txt`)

- `Context group 'topContext(<key>)'` - the expected Context was loaded.
- Relaunch: the top's action-handler log `<lite dir>/tmp/otsActionsHandler_<type>-<host>.<pid>` shows
  `LAUNCH_INSTANCE`, `inst_user`/`inst_host` (must not be empty) and the ssh commands. The
  gateway log shows `gatewayLaunchOTSInstance` and the `LAUNCH_INSTANCE;...` string it sent.
- `Adding new remote gateway app '<uid>'`, `Instantiating Remote Gateway App Status Socket`
  (top binds an extra UDP port, e.g. 10000) and `First good status received ... Remote subapp =
  '<uid>'`. One `New failure getting '<uid>' Remote Gateway App status` while the satellite is
  still starting is normal.
- With a bare local path, a `Local user date path identified.` DEBUG line appears every second as
  the top level polls the satellite's active groups. With `user@host:` it uses scp instead.
- `gateway_udp_cmd.sh <ip> <port> GetRemoteDesktopIcons` returns only the first part of the icon
  list, so it cannot confirm icons late in the table. Use the log or the browser.

## Pitfalls seen

- `SubsystemUserDataPathsTableInfo.xml` is not in a demo-derived `Data/TableInfo`. Saving fails
  with "Table not found with name". Copy it from `otsdaq/data-core/TableInfo/ContextGroup/` and
  restart ots (`add_remote_subsystem.sh` does the copy).
- `saveNewTableGroup` can return no key, only `<TreeErrors .../>`: warnings the GUI asks you to
  accept (e.g. Off apps linking to `DataManagerSupervisorTable`, which is not in the Context group).
  Send `ignoreWarnings=1` (`-w` in `cfg_udp_edit_group.sh` and `add_desktop_icon.sh`).
- A group that saved but was not activated is refused the next time as a "duplicate of an existing
  group key (N)". The reply also holds `<TableGroupKey value='N'/>`, so activate N.
- Parse keys with `<TableGroupKey value='` and take the first match. A looser pattern picked up
  `_0_1`, and activating that key left the running instance with **no active Context** (`-1`, in
  memory only; `ActiveTableGroups.cfg` kept the old key). Re-activate the right key.
- A `new:` edit cannot be repeated after its table exists. Use `add:<Table>=<version>` instead.

## Related

`configgui-udp.md` (request formats, first table version, Desktop icons), `gateway-udp-fsm.md`
(gateway UDP, kinds of restart), `ots-lite-architecture.md` (`mb`/`mz`/`mz_uc`, scripted start/stop).
