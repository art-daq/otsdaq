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
| `EnableApplicationStatusMonitoring` | Yes | without it every app reports `Not Monitored`, so UDP status is useless |
| `EnableAckForStateChangesOverUDP` | optional | see Acknowledgements |

Restart `ots` after activating the new Context group. Verify the log line
`Waiting for UDP State Changer packet on <ip>:<port>` and `ss -ulnp | grep <port>`.

## Protocol

One datagram per command, plain text, sent to the IP:port above.

| Message | Reply |
|---------|-------|
| `Help` | command list |
| `GetStateMachineNames` | XML list of FSM names (e.g. `PhysicsRuns0`) |
| `GetRemoteAppStatus` | XML: per supervisor `name`, `status`, `progress`, `detail`, ... |
| `GetRemoteGatewayStatusXML` | same data, one `<supervisor .../>` element each |
| `<FSMName>,<Command>[,<param>...]` | FSM transition (reply only if acks enabled) |

Command names: `Initialize`, `Configure`, `Start`, `Pause`, `Resume`, `Stop`,
`Halt`, `Abort`. `Configure` requires the **Configuration Alias as its first
parameter**; without it the log shows
`Can only transition to Configured if a Configuration Alias parameter is provided`.
Find aliases with the ConfigGUI UDP (`getActiveTableGroups` shows
`Configuration-ActiveGroupAlias`, e.g. `defaultSystemAlias`). `Start` takes the
run number as a parameter.

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
- Failure: Gateway `status` starts with `Failed:::` followed by the full error
  chain (which supervisor, which front-end, root cause). The text is URL/XML
  escaped (`&apos;`, `&lt;`).
- Poll every few seconds; Configure can take several seconds to minutes.
- Check `FSM` log in `Data/Logs/otsdaq_quiet_run-gateway-*.txt` for the received
  command (`Received a remote FSM Command attempt!`, `Word[0]`, `Word[1]`).

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
- `Configure,<alias>` resolves the alias through the Backbone `GroupAliasesTable`.
  A new group is ignored until the alias points at it:
  `ots_cfg_udp_test 127.0.0.1 2010 setGroupAliasInActiveBackbone groupAlias=<alias> groupName=<group> groupKey=<newKey>`
- Disabling a plugin that is not built in lite (e.g. `DemoDQMHistosConsumer`):
  set `Status=Off` on its record (e.g. `DB1Consumer1`) in `DataProcessorTable`
  (edit, saveSpecificTable, saveNewTableGroup on the Configuration group,
  activateTableGroup, repoint the alias, restart).
- Start `ots_udp_hw_emulator` before Configure; `ots` does not start it.
