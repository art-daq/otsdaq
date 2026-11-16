# Application status monitoring (the Gateway's status threads)

What the `EnableApplicationStatusMonitoring` switch does, what "Not Monitored" means, how a dead
application turns the system to `Failed`, and what the status fields say. Agents need it on:
`gateway_udp_cmd.sh`, `apply_config_reload.sh` and `ots_drive_udp_test` wait for states by reading
the Gateway status, which only exists when monitoring is on.

## The switch

`GatewaySupervisorTable` column `EnableApplicationStatusMonitoring` (Context group). The Gateway reads
it when it starts its background threads: at its start and at every **Startup** transition (which
stops and restarts them). `enable_gateway_udp_transitions.sh` sets it together with the UDP state
changes.

| Monitoring | Every application's status | Consequences |
|---|---|---|
| **off** | `Not Monitored` (set once, never updated) | the state machine does not wait for missing statuses; tools that wait for a state cannot see it (`gateway_udp_cmd.sh` says "No Gateway status ... Check EnableApplicationStatusMonitoring"); a crashed application is not noticed |
| **on** | its state (`Initial`, `Halted`, `Configured`, `Running`, ..., `Failed:::<message>`), progress, detail | dead applications are found (below); disk space is watched; Desktop icons and remote subsystem status refresh |

## The two threads (`GatewaySupervisor::AppStatusWorkLoop`)

- **Connected applications**: one pass over every application that answered last time, asking each
  for its status (`ApplicationStatusRequest`), then 0.5 s of sleep; a pass takes longer when
  applications answer slowly (a warning is printed when a status is more than 1.5 s old).
- **Disconnected applications**: the same for applications whose last request failed, with long
  timeouts, so one dead application does not slow the status of the others. An application that
  answers again moves back to the first thread.
- Both start 5 s after the Gateway (applications are still starting); the second 5 s later still.
- Cost: one status request per application per pass. On the 9-application test system this is not
  noticeable; on large systems the pass time grows with the slowest application.

## When an application stops answering

If a request fails while the application should be running, the Gateway logs
`Did a supervisor crash? Failed getting status from <app>` and sets the state machine error
(`... crashed.`): the Gateway status becomes `Failed:::<message>`. Not an error: during
**Shutdown** and **Startup** (they stop and start the non-gateway contexts on purpose) and in the
`Shutdown` state (since 2026-11-11, `gateway-udp-fsm.md`). A `Failed:::` status stays until the next
successful transition: `Halt` (from `Failed`) clears it.

## Disk space

The connected thread also checks the free space of the disks holding `$OTSDAQ_LOG_DIR`, `$OTSDAQ_DATA`
and `/var` on every context host, and alarms below 1 GB (environment `OTSDAQ_LOG_DISK_MINIMUM`,
`OTSDAQ_DATA_DISK_MINIMUM`, `OTSDAQ_VAR_DISK_MINIMUM` in kB; `0` turns a check off) or when the space
falls fast. The values appear in the status of each application (`availableLogSpaceKB`, ...).

## Reading the status

```bash
gateway_udp_cmd.sh <ip> <gateway UDP port> GetRemoteAppStatus      # XML, one block per application
ots_drive_udp_test <ip> <gateway UDP port> status                  # table: name, LID, status, progress, URL
```

Per application: `name`, `id` (LID), `status`, `time` and `stale` (seconds since the last answer),
`statusChangeTime`, `progress` (0-100, during transitions), `detail` (the Gateway: uptime, time in
state, the alias and group key it was Configured with), disk space, `class`, `url`. The Gateway's
own entry is the one with `class` `ots::GatewaySupervisor` (its name is whatever the Context calls
it, e.g. `Supervisor`). Applications without a state machine role (Chat, Console, ConfigurationGUI,
MacroMaker) stay `Initial`.

Verified 2026-11-14 on ots maxlite: statuses and `detail` as above through a Halt, Shutdown, Startup,
Initialize, Configure sequence (`apply_config_reload.sh`) with no false `Failed`; with monitoring off
the tutorial run read `Not Monitored` for every application (`AI_SKILLS_GAPS.md`). The thread
structure, timeouts and disk checks are read from `GatewaySupervisor.cc`.
