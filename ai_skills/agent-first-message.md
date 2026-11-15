# The first message to an agent that works on otsdaq

What a person (or another agent) should put in the first message to an agent that will use
these skills, and why agents stop when it is missing. Written from the tutorial runs of
November 2026 (Claude Sonnet agents on otsdaq lite, each in its own installation).

## The template

```
Use the otsdaq skills and scripts in srcs/otsdaq/ai_skills and srcs/otsdaq/ai_scripts for everything I ask.
Only talk to this installation: <the remote controls it may use, with IP and UDP port>.
<What it may do beyond reading: state machine commands, configuration changes, starting or stopping processes.>
Change the configuration only as I ask, and tell me which table, group and alias you changed.
Run each command and read its output before you report.
```

Fill the second line from the installation's environment (`source setup_ots.sh`):

| Remote control | Address | Needed for |
|---|---|---|
| Gateway (state machine, status, Iterator) | `$USER_DATA/ServiceData/GatewayUDPRemoteControl.dat` (written by the Gateway at start; `ots` prints it) | `gateway-udp-fsm.md`, `apply_config_reload.sh`, `gateway_udp_cmd.sh` |
| Configuration | `$OTS_CONFIG_UDP_IP` : `$OTS_CONFIG_UDP_PORT` | `configgui-udp.md`, every `cfg_udp_*.sh` |
| Macro Maker (registers, FE macros) | `$OTS_MACROMAKER_UDP_IP` : `$OTS_MACROMAKER_UDP_PORT` | `macromaker-udp.md`, `emulator_sine_sweep.sh`, `read_emulator_registers.sh` |
| Hardware emulator | the FE interface's `InterfacePort` (4000 in the tutorial data) | `emulator-registers.md`, `emulator_ctl.sh` |

## Why each line is there

- **Skills and scripts first.** Without it, agents rediscover the UDP formats by reading C++
  and spend many more tokens; with it they open one skill and run its script.
- **Only these addresses.** Several installations can run on one host (other users, tutorial
  copies, a satellite subsystem). Naming the addresses keeps the agent off the others. An
  agent that was allowed only two ports did not read the Macro Maker remote control, even
  when the request needed it: list every remote control the requests will need.
- **What it may do.** In the tutorial run, the first message said "do not start, stop or kill
  any process". The agent changed the table, group and alias as asked, then stopped before
  Halt, Shutdown, Startup and Initialize: the permission check of the agent's tool treated
  them as destructive. With "You may send state machine commands to the Gateway, including
  Halt, Shutdown, Startup and Initialize, when a change needs them" it finished. Say it
  explicitly when a change must reach a running system (`configuration-model.md`, "Which
  step makes a change take effect").
- **Name what changed.** Every edit makes a new table version, a new group key and usually a
  new Backbone key (the alias). Asking for the names gives a record that can be undone with
  `revert_active_table_group.sh` or `cfg_udp_set_alias.sh <alias> <group> <old key>`.
- **Read the output.** Agents otherwise report what a command should have done. The scripts
  print what they changed and exit non-zero on failure.

## What to forbid, for an agent that shares a host

- `killall` of anything, `ots -K` (kills every user's ots on the host), `reset_ots_tutorial.sh`
  (it runs `killall -9 ots_udp_hw_emulator`). Use `ots -k` (only this `USER_DATA`) and
  `emulator_ctl.sh stop <port>`.
- Editing another installation's `Data` or `setup_ots.sh`.

## When an agent stops

A stop is usually the agent's permission check refusing an action the first message did not
allow. The agent says what it wanted to run: allow it in your next message (or run it
yourself), rather than telling it to find another way.

## Examples of first messages

Read only:
```
Use the otsdaq skills and scripts in srcs/otsdaq/ai_skills and srcs/otsdaq/ai_scripts for everything I ask.
Only talk to this installation: the Gateway UDP remote control at 127.0.0.1 UDP port 3014.
Do not start, stop or kill any process, and do not change any configuration, unless I ask.
Run each command and read its output before you report.
```

Configuration changes carried through to the running system:
```
Use the otsdaq skills and scripts in srcs/otsdaq/ai_skills and srcs/otsdaq/ai_scripts for everything I ask.
Only talk to this installation: the Configuration remote control at 127.0.0.1 UDP port 3012 and the Gateway remote control at 127.0.0.1 UDP port 3014.
Do not start, stop or kill any process. You may send state machine commands to the Gateway, including Halt, Shutdown, Startup and Initialize, when a change needs them.
Change the configuration only as I ask, and tell me which table, group and alias you changed.
Run each command and read its output before you report.
```

Verified 2026-11-14: the two examples are the first messages of the tutorials
*Using AI/SI LLM Agents to Drive the Gateway* and *... to Control Configuration*; the stop
described above happened with the first version of the second one.
