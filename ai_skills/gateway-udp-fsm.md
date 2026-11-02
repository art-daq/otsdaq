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

`ai_scripts/gateway_udp_cmd.sh` sends the command, then polls and prints only the
Gateway status line until it settles (Failed, or the expected state).

## Troubleshooting

- `Skipping potential FSM Command because enableStateChanges=false`: enable flag
  not active in the running Context group (restart after activation).
- Configure fails on a front end with `Timeout period of 5 seconds reached`
  `127.0.0.1 : 4000`: the hardware emulator is not running. `ots` does not launch
  it (it only kills it on exit); start `ots_udp_hw_emulator` from
  `local/install/bin` first.
- Configure fails on a ROOT consumer (e.g. DQM) in the lite build: those plugins
  are not built in lite; disable that consumer in the Configuration group.
- Wrong state errors (`Can only transition to Configured if the current state is
  Initial or Halted`): Halt first.
- `Failed:::` status stays until the next successful transition; Halt or retry to
  clear it.

## Config changes need an ots restart, and the alias must follow the group

- Activating a new Configuration group over ConfigGUI UDP does not reload it in
  already-running supervisors: Configure keeps using the group loaded at startup
  (the old consumer/plugin error repeats). Restart `ots` after changing any group.
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
