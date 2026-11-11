# Gateway UDP State Machine Control

Drive the Gateway FSM (Configure, Start, Halt, ...) and read status over plain
UDP, with no web GUI or DB access. Implemented by the state-changer thread in
`GatewaySupervisor.cc` (search `UDP State Changer`).

## Enable (config, not env vars)

In the Context group's GatewaySupervisorTable record, all of these must be set
together (see `configgui-udp.md` for the edit/save/activate workflow):

| Field | Value | Why |
|-------|-------|-----|
| `EnableStateChangesOverUDP` | Yes | starts the listener |
| `IPAddressForStateChangesOverUDP` | numeric IP, e.g. `127.0.0.1` | `DEFAULT` crashes the gateway |
| `PortForStateChangesOverUDP` | unused UDP port, e.g. `2014` | `0` crashes the gateway |
| `EnableApplicationStatusMonitoring` | Yes | the Gateway's own status (incl. `Failed:::`) is computed by `AppStatusWorkLoop`; without it every app stays `Not Monitored` and transition results are invisible over UDP |
| `EnableAckForStateChangesOverUDP` | optional | see Acknowledgements |

Restart `ots` after activating the new Context group. Verify with
`ss -ulnp | grep <port>` and, in `$USER_DATA/Logs/otsdaq_quiet_run-gateway-*.txt`,
the lines `Enabling state changes over UDP...` and `enableStateChanges = 1`
(the per-poll `Waiting for UDP State Changer packet on ...` line is a TRACE
detail level and is normally not in the log).

## CLI tool: ots_drive_udp_test (otsdaq-utilities)

The Gateway counterpart of `ots_mm_udp_test` (MacroMaker) and `ots_cfg_udp_test` (ConfigGUI):
it *drives* the FSM and the Gateway's queries. Source: `otsdaq-utilities/test/ots_drive_udp_test.cpp`
and the self-contained client `ots_drive_udp_interface.{h,cpp}` (POSIX sockets, no otsdaq
dependency).

```bash
ots_drive_udp_test 127.0.0.1 2014                                      # FSM names + app status table
ots_drive_udp_test 127.0.0.1 2014 status                               # app status table only
ots_drive_udp_test 127.0.0.1 2014 PhysicsRuns0 Configure defaultSystemAlias
ots_drive_udp_test 127.0.0.1 2014 PhysicsRuns0 Start                    # [runNumber] optional
ots_drive_udp_test 127.0.0.1 2014 PhysicsRuns0 Halt                     # also Stop, Pause, Resume, Initialize, Abort
ots_drive_udp_test 127.0.0.1 2014 raw Help                              # any message, prints the reply
```

After an FSM command it polls the Gateway status until the target state (`Reached Configured.`,
exit 0), a *new* `Failed:::` (status text or `statusChangeTime` changed since the command;
prints the error chain, exit 1), no change at all for 10 s (command rejected: wrong state,
unknown FSM or alias; exit 1), or the timeout (`--timeout <s>`, default 300; exit 2).
`--no-wait` only sends. `Configure` without an alias is refused before sending. The Gateway
reports a state only when every supervisor reached it, including those in other contexts.
*Verified 2026-11-11*: Configure, Start, Stop, Halt and a rejected Start (from Halted).

`ots` prints the address at launch, next to the other remote controls:
`Gateway UDP Remote Control at <ip>:<port> ...` and `at terminal: ots_drive_udp_test <ip> <port>`
(or `(status queries only: EnableStateChangesOverUDP is off)`). The address comes from the
config tables, not env vars, so the Gateway writes `$SERVICE_DATA_PATH/GatewayUDPRemoteControl.dat`
(`<ip> <port> <fsmCommandsEnabled 0|1>` once its socket is bound, or `disabled`) and `ots`
reads it (it removes the file before launching the Gateway and waits up to 10 s for it).

`ai_scripts/gateway_udp_cmd.sh` does the same with bash + python and needs no build.

## Protocol

One datagram per command, plain text, sent to the IP:port above. **Replies
longer than 1500 bytes arrive as several datagrams** (`TransceiverSocket::acknowledge`
chunks at 1500 with no gap); a status reply for a normal context is several
chunks, so a client must keep receiving until it sees `</ROOT>`. A single
`recvfrom` returns only the first chunk, cut mid-element.

| Message | Reply |
|---------|-------|
| `Help` | command list (all supported messages) |
| `GetStateMachineNames` | XML `<fsm value='...'/>` per FSM name |
| `GetRemoteAppStatus` | XML: per supervisor, in order, `name`, `id`, `status`, `time`, `stale`, ..., `progress`, `detail`, ..., `class` |
| `GetRemoteGatewayStatusXML` | same data, one `<supervisor .../>` element each |
| `GetRemoteDesktopIcons` | the Desktop icon list as the Gateway loaded it (comma separated: ..., caption, alt text, permission, unique, image, window URL with `?urn=<LID>`, folder, ...); use it to confirm a new icon after a restart |
| `<FSMName>,<Command>[,<param>...]` | FSM transition (reply only if acks enabled) |

Find the Gateway in `GetRemoteAppStatus` by `<class value='ots::GatewaySupervisor'/>`,
not by position: its LID (and so its place in the list) is set by the XDAQ
configuration, not fixed.

Command names: `Initialize`, `Configure`, `Start`, `Pause`, `Resume`, `Stop`,
`Halt`, `Abort`. `Configure` requires the **Configuration Alias as its first
parameter**; without it the log shows
`Can only transition to Configured if a Configuration Alias parameter is provided`.
Find aliases with the ConfigGUI UDP: `getActiveTableGroups` shows
`Configuration-ActiveGroupAlias` **only when** a GroupAliases entry matches the
active Configuration group; otherwise read `getGroupAliases`. `Start` takes the
run number as an optional first parameter (omitted: next number from
`$SERVICE_DATA_PATH/RunNumber/`).

```bash
ai_scripts/gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Configure,defaultSystemAlias
ai_scripts/gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Start,1
ai_scripts/gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Halt
```

## Acknowledgements vs status

With `EnableAckForStateChangesOverUDP=No` (default) FSM commands get **no reply**
(a "no reply" timeout is normal, not a failure). Do not rely on acks: send the
command, then poll `GetRemoteAppStatus`:

- Gateway `status`: `Initial`, `Halted`, `Configured`, `Running`, `Paused`, or a
  transition name while in progress (`progress` is the percentage).
- Success: Gateway `status` reaches the target state and the transition finishes.
- Failure: Gateway `status` starts with `Failed:::` (or `Soft-Error:::` if the
  FSM was Paused) followed by the full error chain (which supervisor, which
  front-end, root cause). The text is XML-escaped (`&apos;`, `&quot;`, `&lt;`,
  `&gt;`, `&amp;`, `&#NNN;`).
- Before the first status poll completes, or with
  `EnableApplicationStatusMonitoring=No`, apps report `UNKNOWN` / `Not Monitored`.
- Poll every few seconds; Configure can take several seconds to minutes.
- Check `$USER_DATA/Logs/otsdaq_quiet_run-gateway-*.txt` for the received
  command (`Received a remote FSM Command attempt!`, `Word[0]`, `Word[1]`) and
  the outcome (`Successfully executed state change command` or
  `UDP State Changer failed to execute command because of the following error`).

`ai_scripts/gateway_udp_cmd.sh <ip> <port> <message> [expectedState] [timeoutSeconds]`
sends the message. With `expectedState` (e.g. `Configured`) it prints the status
before sending, then polls every 3 s until that state is reached (exit 0), the
status starts with `Failed:::` (exit 1, full error printed), or it times out
(exit 2). Exit 3 means the reply had no Gateway entry, usually because
`EnableApplicationStatusMonitoring` is off. If the status was already `Failed:::`
before sending, a later `FAILED` that is identical is flagged as possibly left
over. Replies arrive as 1500-byte datagrams; the script reassembles them and finds
the Gateway by `<class>ots::GatewaySupervisor</class>`, not by position or LID.

When writing your own polling code: read datagrams until `</ROOT>` (one `recvfrom`
returns only the first 1500 bytes, which may not even contain the Gateway entry).

## Verified sequence (lite, from a clean start)

1. `ots_udp_hw_emulator` running (port 4000); `ots` started with the Context
   group enabling UDP state changes, status monitoring, IP and port.
2. `gateway_udp_cmd.sh 127.0.0.1 2014 GetStateMachineNames` lists the FSMs
   (`PhysicsRuns0`, `CalibrationRuns0`, `OtherRuns0` in the demo).
3. `gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Configure,defaultSystemAlias Configured`
   -> `Reached Configured`; the log shows
   `Attempting Configure transition with Configuration table group name: <group>, key: <key>`
   and per-app statuses become `Configured` (apps without an FSM role, such as
   ChatSupervisor, stay `Initial`).
4. `gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Halt Halted` returns to Halted.

Every failure met on the way (missing alias parameter, emulator down, ROOT consumer
not built in lite, alias pointing at the old group) is listed under Troubleshooting
below with its cause.

## Troubleshooting

- `Skipping potential FSM Command because enableStateChanges=false`: enable flag
  not active in the running Context group (restart after activation).
- Configure fails on a front end with `Timeout period of 5 seconds reached`
  `127.0.0.1 : 4000`: the hardware emulator is not running. `ots` does not launch
  it (only the hard kill, `ots -K`, stops it); start `ots_udp_hw_emulator` from
  `local/install/bin` first.
- Configure fails on a ROOT consumer (e.g. DQM) in the lite build: those plugins
  are not built in lite; disable that consumer in the Configuration group.
- Wrong state errors (`Can only transition to Configured if the current state is
  Initial or Halted`): Halt first.
- `Failed:::` status stays until the next successful transition; Halt or retry to
  clear it.

## Config changes and the alias must follow the group

Which restart a change needs is in "Applying Active Context changes" below.


- Activating a new Configuration group over ConfigGUI UDP does not reload it in
  already-running supervisors: Configure keeps using the group loaded at startup
  (the old consumer/plugin error repeats). Restart `ots` after changing any group
  (observed 2026-11-02 with a total restart; see the matrix below for lighter options).
- `Configure,<alias>` resolves the alias through the **active Backbone group**,
  not the latest `GroupAliasesTable`. A new Configuration group is ignored until
  (1) `setGroupAliasInActiveBackbone groupAlias=<alias> groupName=<group> groupKey=<newKey>`
  saves a new `GroupAliasesTable` version, AND (2) that version goes into a new
  Backbone group that is activated: `saveNewTableGroup groupName=<backbone>
  tableList=GroupAliasesTable,<newVer>,VersionAliasesTable,<ver>,` then
  `activateTableGroup`. Skipping (2) leaves the Gateway on the old key; confirm
  with the log line `Attempting Configure transition with Configuration table
  group name: <group>, key: <key>`. The Backbone is read at startup, so restart.
- Disabling a plugin that is not built in lite (e.g. `DemoDQMHistosConsumer`):
  set `Status=Off` on its record (e.g. `DB1Consumer1`) in `DataProcessorTable`
  (edit, saveSpecificTable, saveNewTableGroup on the Configuration group,
  activateTableGroup, repoint the alias, restart).
- Start `ots_udp_hw_emulator` before Configure; `ots` does not start it.

## Applying Active Context changes: total restart vs Shutdown/Startup vs App Status relaunch

Three ways to make running processes follow a newly activated Context group. They renew
different things; pick the lightest one that covers the change.

### How to run each

1. **Total restart**: every process is recreated, everything is renewed.
   - terminal: `echo Y | ots -k` then `ots` (`-k` asks "Are you sure"; for a script use
     `nohup ots > /tmp/ots_run.log 2>&1 &`), or `kx; ots` interactively.
   - GUI: App Status window, relaunch button (request `gatewayLaunchOTS`, ots action
     `LAUNCH_OTS` on every context host: kill all and relaunch; keeps login sessions).
   - ends at `Initial` for the whole system.
2. **Shutdown -> Startup** (FSM transitions; the Gateway process keeps running).
   - Shutdown is allowed only from `Halted` or `Failed` (`Initial`: Initialize first;
     `Configured`/`Running`: Halt first). Startup goes `Shutdown` -> `Initial`, then Initialize.
   - UDP: `ots_drive_udp_test <ip> <port> <FSM> Shutdown`, then `... Startup`, then
     `... Initialize`. GUI: the State Machine window buttons.
   - Shutdown writes `OTS_APP_SHUTDOWN` to every context host's ots action file: the ots
     script kills all **non-gateway** contexts. Startup writes `OTS_APP_STARTUP`: they are
     relaunched. Measured 2026-11-11: Startup takes ~17 s; the 2016 context came back with a
     new pid, all apps at `Initial`.
3. **App Status relaunch of a context** (no FSM transition).
   - GUI only: App Status window, the restart button on a context row (request
     `restartApps&contextName=<ContextUID>`, handled by the Gateway over HTTP; no UDP form).
   - It sends `OTS_APP_SHUTDOWN` then `OTS_APP_STARTUP` to the ots script on **that
     context's host**, which kills and relaunches **all non-gateway contexts on that host**
     (the button says "restarting server ... for 'non-gateway apps'"), not only the chosen
     one. The Gateway's own context is never relaunched this way.
   - The Gateway FSM state is unchanged; relaunched apps come up `Initial`, out of step with
     a Configured/Running system: Halt (and Initialize/Configure) to re-align them.

### What is renewed to match the Active Context

| Gateway feature / process | Total restart | Shutdown -> Startup | App Status relaunch |
|---------------------------|:---:|:---:|:---:|
| Non-gateway context processes (their apps reload all active groups when they start) | yes | yes, all hosts | yes, all non-gateway contexts on that host |
| Context **layout**: contexts/apps added or removed, host/port, LID, class, module | yes | **no** | **no** |
| Gateway configuration manager (its table lookups, Backbone/aliases, Desktop icons) | yes | yes (`cfgMgr->init()`) | yes (`cfgMgr->init()`) |
| Gateway UDP listener: `EnableStateChangesOverUDP`, IP/port, ack/login verification, `GatewayUDPRemoteControl.dat` | yes | **yes (since 2026-11-11)** | no |
| App status monitoring on/off and its two threads | yes | **yes (since 2026-11-11)** | no |
| Gateway request permissions (properties of the Gateway app: `UserPermissionsThreshold`, groups allowed, ...) | yes | **yes (since 2026-11-11, re-read on the next request)** | no |
| Gateway list of apps it commands and monitors (`allSupervisorInfo_`, from its XDAQ context) | yes | no | no |
| Apps in the Gateway's own context (ConfigGUI, MacroMaker, Visualizer, ... in `mainContext`) | yes | no (process kept; their permissions reset at their own Initialize) | no |
| Env-var features (`OTS_MACROMAKER_UDP_*`, `OTS_CONFIG_UDP_*`, `OTSDAQ_*_DISK_MINIMUM`, `OTS_PORT_TRANSLATION_MAP_FILE`, `OTS_LOG_*`) | yes (env of the new processes) | no | no (relaunched contexts get the ots script's env) |
| Web users / security mode | yes | no | no |
| FSM state | -> `Initial` | -> `Initial` | unchanged (relaunched apps `Initial`) |
| `ots_udp_hw_emulator` and other external processes | no | no | no |

Why the layout is not renewed: the Gateway rewrites `XDAQConfigurations/otsConfiguration_CMake_Run.xml`
from the active Context whenever its configuration manager initializes (`XDAQContextTable::init`),
but the ots script launches contexts from its copy `ots.xml`, taken only at a full (gateway)
launch; a non-gateway relaunch copies it only if missing. The Gateway's `allSupervisorInfo_` is
also built once, from the XDAQ context it was launched with. So adding a context (e.g. the Sunrock
FE context), an application, or changing a port/LID needs a total restart.

What Startup does in the Gateway (since 2026-11-11, `GatewaySupervisor::transitionStartingUp`):
stop the UDP state changer and both app status threads (`stopBackgroundWorkLoops()`: they exit at
their next pass, joined so the UDP port is released), re-initialize the configuration manager and
send `OTS_APP_STARTUP` (`launchStartOTSCommand`), start the threads again from the reloaded
`GatewaySupervisorTable` (`startBackgroundWorkLoops()`), and `resetPropertiesAreSetup()` so request
permissions are re-read on next use. If sending the command fails, the threads are restarted anyway.
The Gateway's own Initialize does **not** reset its permissions (other supervisors do, in
`CoreSupervisorBase::transitionInitializing`).

App status during Shutdown/Startup: before 2026-11-11 the app status thread treated the apps that
Shutdown deliberately kills as crashed ("Did a supervisor crash? Failed getting status from ...")
and set the Gateway to `Failed` even though the FSM reached `Shutdown` (observed; Startup still
worked from there). Now an unreachable app is not an error while a Shutdown or Startup transition
is running, or in the `Shutdown` state.

Verification status: the old-code behavior above (17 s Startup, new 2016 pid, false crash on
Shutdown, Startup rejected from `Initial`) was observed 2026-11-11. The new Gateway code compiled
(syntax check with the build's flags) but was **not yet run**: after a rebuild, check that Startup
moves the UDP listener when its port is changed in the Context, that `GatewayUDPRemoteControl.dat`
is rewritten, that Shutdown no longer ends in `Failed`, and that a changed Gateway permission takes
effect after Startup.

