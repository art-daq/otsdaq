# Iterator plans: create, change, play, check

An Iterator plan is a list of steps the Gateway runs by itself: choose a state machine, configure, loop with
labels, run for a time, change a configuration value or run a front-end macro for each pass. This is how scans and
repeated runs are done. Plans are configuration (an **Iterate** group), so the rules of `configuration-model.md` apply.

## The model: which table holds what

| Table | One row is | Key columns |
|---|---|---|
| `IterateTable` | a plan | `IterationPlanName`, `Status` On/Off, `LinkToIterationPlanGroupID` (-> its steps) |
| `IterationPlanTable` | a step, in row order | `IterationPlanGroupID`, `IterationPlanCommandID`, `Status`, `CommandType`, `LinkToCommandTable`/`LinkToCommandUID` (-> its parameters), `TargetSubsystem` (remote subsystem, usually empty) |
| `IterationCommand<Kind>Table` | the parameters of one step | `CommandID` + the kind's columns (below) |
| `IterationTargetTable` | one target (a front end) of a macro or modify step | `IterationTargetGroupID`, `LinkToTarget` (table), `LinkToTargetUID` |
| `IterationCommandMacroDimensionalLoopTable` / `...ParameterTable` | the loops of a macro step and the inputs they change | iterations, `StepLabel`, `ParameterName`, `ParameterInitialValue`, `ParameterStepSize` |

All these tables form **one Iterate group** (exactly these members; a group with only some of them is an error). The
active Iterate group is in `ActiveTableGroups.cfg` like the other types; it can be empty of plans.

## Step types

| Step (script word) | CommandType | Parameters | Notes |
|---|---|---|---|
| `fsm <name>` | CHOOSE_FSM | state machine name | first step: which FSM the plan drives |
| `configure <alias>` / `configure-group <g> <k>` / `configure-active` | CONFIGURE_ALIAS / _GROUP / _ACTIVE_GROUP | alias, or group name and key | the Iterator takes over the FSM: it can not if another window drives it |
| `run <s> [wait]` | RUN | `DurationInSeconds`, `WaitForAllFrontEndsRunningThread` | Start, run, Stop; with wait the duration is ignored |
| `start` `stop` `pause` `resume` `halt` | START ... | none | single FSM transitions |
| `label <L>` / `repeat <L> <n>` | BEGIN_LABEL / REPEAT_LABEL | label, repetitions | the steps between run `n` more times; the pass number (0, 1, ...) feeds modify steps and macro loops |
| `modify <targets> <path> <start> [step [track]]` | MODIFY_ACTIVE_GROUP | `RelativePathToField`, `FieldStartValue`, `FieldIterationStepSize`, `DoTrackGroupChanges` | sets the field to start + step x pass, then activates; without `track` nothing is saved; with `track` each pass saves a new group key, but its changed table is the **scratch** version (number 2147483647), so the stored keys do not keep per-pass values (seen: keys 5 and 6 both read the last value): the Gateway log lines `startValue`, `stepSize`, `currentValue` are the record |
| `fe-macro` / `mm-macro <targets> <macro> [out=..] [loop=..]` | EXECUTE_FE_MACRO / EXECUTE_MM_MACRO | `MacroName`, targets, loops, optional output file | loops: n iterations, inputs `name=init:step` |
| `subsystems Set\|Add\|Remove <list>` | SETUP_INCLUDED_SUBSYSTEMS | `Mode`, `Subsystems` | top-level ots with remote subsystems |

A step with `Status` Off is skipped; a plan with `Status` Off can not be played. Not scripted: WAIT, ACTIVATE_ALIAS/GROUP,
`TargetSubsystem` (set it with `set <plan> <n> TargetSubsystem=<name>`).

## Scripts

```bash
ai_scripts/iteration_plan.py list
ai_scripts/iteration_plan.py create AmpScan "fsm PhysicsRuns0" "configure defaultSystemAlias" "label scan" \
    "fe-macro FEInterfaceTable/ExampleInterface0 test loop=3@scan" "run 20" "repeat scan 2"
ai_scripts/iteration_plan.py show AmpScan                   # numbered steps with every value
ai_scripts/iteration_plan.py set AmpScan 5 DurationInSeconds=10
ai_scripts/iteration_plan.py add AmpScan --at 3 "run 2"      # insert before step 3
ai_scripts/iteration_plan.py move AmpScan 3 1
ai_scripts/iteration_plan.py disable AmpScan 4               # a step; without number: the plan
ai_scripts/iteration_plan.py remove AmpScan 3                # removes its parameter, target and loop rows too
ai_scripts/iteration_plan.py delete AmpScan
```
Every change saves new table versions and **one new Iterate group key**, activated; `-n` shows the result without
saving. They act as the Kerberos user. Macro names, target tables and UIDs are not checked against the
configuration: `show` after a change, and play once.

## Applying, playing, checking

- A **new** plan is found after `apply_config_reload.sh <fsm>`. A plan the Gateway has **already played** keeps
  playing its old version until `ots` is restarted (`ots_mode.sh stop`, then `ots`; verified 2026-11-15: an edited
  macro name was ignored after Halt/Shutdown/Startup, used after a restart).
- Play and watch over Gateway UDP (`gateway-udp-fsm.md`, "Iterator over Gateway UDP"):
  ```bash
  ots_drive_udp_test <ip> <port> raw "Iterator,iteratePlay,AmpScan"
  ots_drive_udp_test <ip> <port> raw "Iterator,getIterationPlanStatus"    # active_plan_status, current_command_type, error_message
  ots_drive_udp_test <ip> <port> raw "Iterator,iterateHalt"
  ```
  Done is `active_plan_status` Inactive with `last_finished_plan` the plan name and an empty `error_message`; a failed
  step leaves `Error` and the message (e.g. the front end's reason for a macro).
- Run files: one per RUN step pass, `data-path-configuration.md`.

## Verified 2026-11-15 (ots maxlite, Sunrock + emulator)

A 7-step plan with an FE macro loop, a modify step and a 3-pass label loop was created, shown, edited (set, add,
move, disable, remove, enable) with consistent rows, and played after a restart: the pass count, the macro step and
the modify step ran and the plan ended with no error. `varTest` as an Iterator step failed with the front end's message "Requested argument not found with name 'Plotly_Plot'" in `error_message`
(a direct run of it works); the input-free `test` macro ran fine.
Found in the source and fixed in `Iterator.cc`: an integer `FieldIterationStepSize` was read from the start-value string, so integer
modify steps used the start value as the step. **Verified 2026-11-15 on a rebuilt area (`dev-skills`)**: a plan with `modify ... 9300 7 track`
and a 2-pass label loop logged `stepSize 7`, `currentValue 9300` on pass 0 and `9307` on pass 1 (the old code would log 9300 and 18600).
Not tested: `mm-macro`, remote subsystems, `out=` files.
