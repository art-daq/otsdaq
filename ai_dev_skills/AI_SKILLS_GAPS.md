# Missing and weak ai_skills and ai_scripts of the otsdaq repository

A running list, started 2026-11-14, for the agent that will fill the gaps. It was written while rewriting the
tutorials (in the otsdaq-web repository, where it lived until 2026-11-17) and is about the files of this repository
(`ai_skills/`, `ai_scripts/`). Keep adding to it here; move an item to "Done" with the commit when it is finished.

Each item is marked **Seen** (I ran into it while writing and walking through the tutorials, with the evidence) or
**Inferred** (from the feature list of otsdaq, not observed).

## Done in this session (so nobody repeats it)

| Item | Where | Commit (otsdaq, branch feature/ots-maxlite, local) |
|------|-------|------|
| `cfg_udp_set_alias.sh <alias> [group key]`: alias to group key, new Backbone key, activate | `ai_scripts/` | `daedbdf2` |
| Skill section "Propagating a change: table, group, alias, apply" (the order, and the check in the Gateway log) | `ai_skills/configgui-udp.md`, `gateway-udp-fsm.md` | `daedbdf2` |
| `enable_gateway_udp_transitions.sh` also sets `EnableApplicationStatusMonitoring` (without it every app reads `Not Monitored`); fixed its value parsing and the "same as current value" case | `ai_scripts/` | `052e0bcc` |
| `<TableGroupKey>` pattern anchored (the key printed as `_2_3 / 3`); "already set, nothing to change" in `cfg_udp_edit_field.sh` | `ai_scripts/` | `daedbdf2` and the working tree |
| Script 1: `apply_config_reload.sh <fsm> [<alias>]` | `ai_scripts/` | `c251cb3f` (in `dev-srcs/otsdaq`, on top of `daedbdf2`) |
| Script 2: `gateway_log_events.sh [-n N] [-a] [-s] [-f log]` | `ai_scripts/` | `c251cb3f` |
| Script 3: `emulator_ctl.sh start\|stop\|status\|restart <port>` | `ai_scripts/` | `c251cb3f` |
| Script 5: `cfg_udp_list_aliases.sh`, `cfg_udp_set_table_alias.sh <alias> <table> <version>` | `ai_scripts/` | `c251cb3f` |
| Skill 1: `agent-first-message.md`; Skill 4: `configuration-model.md` | `ai_skills/` | `c251cb3f` |
| Script 4: `isolate_ots_ports.py [--offset N [--apply [--accept-warnings]]]` | `ai_scripts/` | `b1d20736` |
| Script 7: `cfg_udp_diff_groups.py` (cell-level, values) | `ai_scripts/` | `ae5f82a6` |
| Script 9: `cfg_udp_table_csv.py get\|put` (through the running GUI) | `ai_scripts/` | `e3a58dcb` |
| Script 10: `read_raw_data.py` | `ai_scripts/` | `8ed843f8` |
| Script 13: `desktop_icon_ctl.py list\|on\|off\|delete` | `ai_scripts/` | `cb0091d4` |
| Script 11: `mm_make_mom.py` (found: FE macro inputs whose names have spaces fail over UDP) | `ai_scripts/`, `macromaker-udp.md` | `770aba89` |
| Script 12: `ots_mode.sh status\|wiz\|normal\|stop` | `ai_scripts/` | `9c041b3c` |
| Script 14: `switch_ots_tutorial.sh` (derived from `reset_ots_tutorial.sh`, kills nothing) | `ai_scripts/` | `8ff5cd1c` |
| Scripts 8 and 6: `tableinfo.py new\|edit\|check`, `cfg_udp_repair_table.py` | `ai_scripts/` | `4204537e` |
| Script 15: `iteration_plan.py create|list` (fsm, configure, configure-group, run, label, repeat; macro/modify steps not covered) | `ai_scripts/` | see `git log` of `feature/ots-maxlite` |
| Skill 7: `security-and-users.md` (source-based; digest logins, account changes, IP lists, CILogon not tested) | `ai_skills/` | `10cdd026` |
| Script 15 and Iterator skill: `iteration_plan.py` (show/add/remove/move/set/enable/delete, macro, modify, subsystem steps), `iterator-plans.md` | `ai_scripts/`, `ai_skills/` | `1edba62b` |
| Skill 8 (FE macros and Macro Maker macros part): `fe-macros-and-macromaker.md`, script `mm_macro.py`; plugin skills for data processors and tables still open | `ai_skills/`, `ai_scripts/` | see `git log` of `feature/ots-maxlite` |
| Skill 8 (data processor plugins part): `data-processor-plugins.md`, script `new_data_processor.py`; found and fixed (C++, needs `mb`): the raw saver lost all data after a Pause/Resume | `ai_skills/`, `ai_scripts/`, `otsdaq/DataManager` | `8dbe7334` |
| Skill 12: `console-without-browser.md` (no UDP remote control exists for the Console) | `ai_skills/` | `d785f0e6` |
| Bugs: rejected command silent; "Reached" by someone else's move (`gateway_udp_cmd.sh`: log errors, exit 4) | `ai_scripts/` | see `git log` after `d785f0e6` |
| Skill 5: `isolated-agent-sandbox.md`; Skill 3: `app-status-monitoring.md`; Skill 2: `data-path-configuration.md` | `ai_skills/` | `dec9341d`, `2478cad0`, `243f80c0` |

## Missing scripts (most useful first)

1. **Apply a configuration change to a running system.** **Seen.** Every change of a Backbone or Context group needs
   `Halt`, `Shutdown`, `Startup`, `Initialize` (and then `Configure <alias>`), and the agent has to compose it from the skill.
   Proposed `apply_config_reload.sh <fsm> [<alias>]`: run the sequence with `ots_drive_udp_test`, stop at the first failure,
   print the log line `Attempting Configure transition with Configuration table group name: ..., key: ...` as proof.
   Evidence: in the tutorial run the agent only got through this step after the first message allowed it.
2. **Gateway log digest.** **Seen.** I grepped the same patterns by hand a dozen times: `Received a remote FSM Command`,
   `Attempting Configure transition`, `Error - `, `UDP State Changer failed`. Proposed `gateway_log_events.sh [-n N]` that prints
   only those, with the time, from `$USER_DATA/Logs/otsdaq_quiet_run-gateway-*.txt`, with the ANSI colours removed.
3. **Safe emulator control.** **Seen.** `ots_udp_hw_emulator` is started by hand, and the only stop is `kill <pid>`.
   `reset_ots_tutorial.sh` runs `killall -9 ots_udp_hw_emulator`, which kills an emulator of *another* installation on the
   same host. Proposed `emulator_ctl.sh start|stop|status <port>` with a pid file per port, and a check that the port is free.
4. **Port isolation for a second installation on one host.** **Seen.** I moved every fixed port of an installation by
   hand (all tables with `*_PORT` columns: front-end, listeners, streamer, slow controls; the message facility forwarding
   ports; the `OTS_*` environment). Proposed `isolate_ots_ports.sh <offset>`: list the ports of the active groups, show the
   conflicts with the ports that are listening, apply an offset with `cfg_udp_edit_group.sh`. This is the prerequisite of the
   planned multi-subsystem tutorial and of any second agent sandbox.
5. **Alias and version-alias lists.** **Seen.** `cfg_udp_list_aliases.sh` (group aliases with their group and key, table
   version aliases of the `VersionAliasesTable`), and `cfg_udp_set_table_alias.sh <alias> <table> <version>` next to
   `cfg_udp_set_alias.sh`. `setTableAliasInActiveBackbone` exists in the skill but has no script.
6. **Repair a table after its definition changed.** **Seen** (the Saving With Errors tutorial). After a column is renamed
   or added, old versions load with warnings, and the browser offers *Reload with loose column matching*. Proposed
   `cfg_udp_repair_table.sh <table> <version>`: report the warnings, reload with loose matching when columns were renamed,
   save a new version, and print the new version. Needs a look at which ConfigGUI request does the loose reload.
7. **Diff two group keys, or a group and its alias.** **Inferred** (the GUI has "Diff with the Active table group" and
   "Diff with another Key of this group"). `cfg_udp_diff_groups.sh <group> <keyA> <keyB>` would let an agent show what a
   change really changed before it activates it.
8. **Table definitions from a script.** **Seen.** Creating and changing a table definition (`$USER_DATA/TableInfo/<Name>Info.xml`)
   is a Wiz-mode, browser-only job; I did it by driving the Table Editor's JavaScript. Proposed `tableinfo_new.sh` that writes the XML from a
   short column list and validates it (names may only contain letters and digits: Save refuses others), and `tableinfo_edit.sh`
   to add, rename or remove a column. Check what the server needs to pick up the file (a restart in my run).
9. **Table data as CSV.** **Inferred** (download and upload exist in the group view, `Download a selection of member tables`).
   `cfg_udp_table_csv.sh get|put <table> <version> file.csv` for bulk edits of a table with many rows.
10. **Raw data file reader.** **Seen.** The tutorials read `Run_Run2_Raw.dat` with `hexdump -x` and explain the words by
    hand: the first word holds the type in the low byte and the sequence number in the high byte, then the counter or the
    samples. Proposed `read_raw_data.sh <file>`: decode packets and (for the multi-channel modes) the channels, print counts and
    a few samples. Also useful as a check after `emulator_sine_sweep.sh`.
11. **Make a MoM (macro of macros) file.** **Seen.** The MoM of the AI tutorial was written by hand as a JSON file under
    `$USER_DATA/ServiceData/MacroSequence/NO-USER/`. Proposed `mm_make_mom.sh name step...` with the same step syntax as
    `ots_mm_udp_test`.
12. **Wiz mode launch for an agent.** **Seen.** `ots -w` prints a URL with a one-time access code; every walkthrough parsed it
    from the output with `grep -o 'http[^ ]*Verify?code=[a-z0-9]*'`. Proposed `wiz_url.sh`, which starts Wiz if it is not
    running and prints the URL, plus a safe stop that kills only this installation's processes.
13. **Remove or hide a Desktop icon.** **Inferred.** `add_desktop_icon.sh` adds; nothing sets `Status` Off or deletes a row.
14. **Switch tutorial data without killing anything.** **Seen.** A variant of `reset_ots_tutorial.sh --tutorial <name>` that
    does not run `killall`, keeps the `.bak` copies and says what it moved.
15. **Iteration plans.** **Inferred.** Only the *generated* plan is scripted (`iteratePlayGenerated`). Creating real plans
    (`IterationPlanTable` and the command tables) from a short description is not.

## Missing skills

1. **First message and permissions for agents.** **Seen.** What a safe first message contains, and why agents stop. In the tutorial
   runs an agent refused Shutdown and Startup because the first message said "do not start, stop or kill any process", and it did
   not read the Macro Maker remote control because the message allowed only two ports. A short skill, `agent-first-message.md`,
   with the template of the tutorials: which skills to use, the remote-control addresses, which commands are allowed, and "name
   every table, group and alias you changed".
2. **Data path configuration.** **Seen** (partly in `gateway-udp-fsm.md` Troubleshooting). Listeners, data buffers, the raw
   data saver, the streamer, the Visualizer consumer: how they link, which tables, how to switch a consumer off (`Status=Off` on
   its `DataProcessorTable` row, needed in lite for the ROOT histogram consumer), where the files go (`$OTSDAQ_DATA`,
   `Run_RunN_Raw.dat`), run numbers (`ServiceData/RunNumber/<FSM>NextRunNumber.txt`).
3. **Application status monitoring.** **Seen.** What `Not Monitored` means, the two threads, the cost, what the status text
   `Failed:::` contains. Today it is one table row in `gateway-udp-fsm.md`.
4. **Table definitions, versions, groups and aliases: the model.** **Seen.** One page that says what an agent must know before it
   edits anything: table version and group key are different things; temporary versions are negative; a Configure resolves the alias
   through the active Backbone; which changes need which apply step (`gateway-udp-fsm.md` has the matrix for the Context group, but
   not the model). The tutorials now say it in words; the skill should too.
5. **Isolated agent sandbox.** **Seen.** How to run an installation next to another one on the same host without touching it:
   ports, environment, what to never run (`killall`, `reset_ots_tutorial.sh`, `ots -K`), how to stop only your own processes.
6. **Slow controls (EPICS) dashboard.** **Inferred.** No skill at all: the supervisor, the interface plugins, the pages and
   widgets, how to add a channel. The tutorials for it are still old drafts.
7. **Security and users.** **Inferred.** Accounts, permission levels, the access code of Wiz mode (the code-editor read-only rule
   with fewer than 8 characters is only in the Tour tutorial), CILogon, the HTTPS proxy. Nothing in `ai_skills/`.
8. **Writing plugins.** **Inferred.** A front-end interface (`FEVInterface`, FE Macros), a data processor (producer or
   consumer), a table plugin: the files, the registration, the build and test cycle. `development-workflow.md` covers the cycle but not the plugins.
9. **artdaq in otsdaq (full install).** **Inferred.** Board Reader, Event Builder, Data Logger supervisors, the artdaq Config
   Editor, MPI launches (the `ART` prefix rule of `ContextUID` is in the app's tooltip). Everything that needs the full build is out
   of reach of the lite skills.
10. **Databases: MongoDB.** **Inferred.** Replica sets, backups, switching an installation from files to MongoDB.
    `config-store-conversion.md` converts between files and text; MongoDB is only named.
11. **ROOT and the Visualizer on a full install.** **Seen.** The ROOT histogram path is the appendix of
    *Taking a Run*, and was not re-checked on a full install. A skill for the DQM consumer, `LIVE_DQM.root`, and the file list.
12. **Logbook, chat and console.** **Inferred.** The Console is reachable by UDP? Unknown; a skill should say how an agent reads
    the web console messages without a browser.

## Bugs and rough edges of existing tools

- **`ots_cfg_udp_test` exits 1 for a harmless `<Error>` reply.** **Seen.** Every script wraps it with `|| true` and reads the
  reply itself. The tool should not fail on a "value is the same as the current value" reply, or should exit with a distinct code.
- **`ots_drive_udp_test` and `gateway_udp_cmd.sh` cannot tell who moved the Gateway.** **Seen.** When another client drives
  the same Gateway, `gateway_udp_cmd.sh` printed "Reached Running" and exit 0 for a command that had been rejected, because
  someone else's Iterator plan took the state there. Proposed: compare the log (`Received a remote FSM Command`) or print a
  warning when the state changes without an acknowledged command.
- **A rejected FSM command is silent over UDP.** **Seen.** There is no reply (acknowledgements are off by default), the tool
  waits 10 s (`ots_drive_udp_test`) or the timeout (`gateway_udp_cmd.sh`) and says "probably rejected". The reason is only in the log.
  The tools should read the last error line of the Gateway log and print it. The text of the error for `Start` from `Halted` is also
  misleading: *Can only transition to Configured if the current state is Halted*.
- **The enable script and the edit script disagree about the active group.** **Seen.** `cfg_udp_edit_field.sh` edits the table
  version of the *active* group; after a tree edit in the browser changed the active Configuration group, the next scripted edit
  started from that group and carried its other changes. The script should print the group key it started from (it does) and
  offer `--from-key`.
- **`add_remote_subsystem.sh -n` writes a file.** **Seen.** The dry run copied `SubsystemUserDataPathsTableInfo.xml` into
  `$USER_DATA/TableInfo` and asked for a restart. Say so in the usage text, or make `-n` read-only.
- **Backbone aliases are not listed by the Gateway tools.** **Seen.** `ots_drive_udp_test` shows the FSM names but not the aliases
  that `Configure` accepts. It takes `ots_cfg_udp_test getGroupAliases` to find them.
- **The `TableEditWizBackbone` of the lite tutorial data.** **Seen.** The default lite install has the Backbone group named
  after the Wiz mode editor. Every alias change makes a new key of it. That is fine, but the skill never says why the name.

## Review of what exists (against all the otsdaq features)

| Area | Skills | Scripts | Verdict |
|------|--------|---------|---------|
| State machine, status, iterator | `gateway-udp-fsm.md` | `gateway_udp_cmd.sh`, `ots_drive_udp_test` | good; add the apply script and the log digest |
| Configuration: read, edit, groups | `configgui-udp.md` | `cfg_udp_*`, `enable_gateway_udp_transitions.sh` | good; add aliases, diff, repair, CSV |
| Hardware access, macros | `macromaker-udp.md`, `emulator-registers.md` | `emulator_sine_sweep.sh`, `read_emulator_registers.sh` | good for the emulator; no skill for a real board |
| Data path and files | `tutorial-to-visualizer-waveforms.md` | none | weak: nothing on listeners, buffers, saver, raw files, run numbers |
| Subsystems | `ots-subsystems.md` | `add_remote_subsystem.sh`, `setup_ots_subsystems.sh` | written and partly verified; the file says what is not verified (relaunch, the satellite folder, Configure and Start of a satellite from the top) |
| Build, install, architecture | `ots-lite-architecture.md`, `ots-maxlite-architecture.md`, `development-workflow.md` | `maxlite_cross_build.sh`, `check_maxlite_libraries.sh` | good; no skill for the macOS build of lite |
| Logs | `trace-and-logging.md` | `check_ots_status.sh` | partial; no digest of the Gateway log |
| Slow controls, security, plugins, artdaq, MongoDB, logbook | none | none | missing |

## Notes for the agent that takes this

- Tutorials that depend on a gap: *Using AI/SI LLM Agents to Control Configuration* (items 1, 5, 7 of the scripts), the planned multi-subsystem
  tutorial (script 4, skill 5), *Saving With Errors* (script 6, being rewritten now).
- Verify every new script on a running installation of your own, and write the date and the result at the end of the skill, as the
  existing skills do. `ai_scripts/README.md` and `ai_skills/README.md` are the indexes: add a row for each.
- Never test on the installation of a user: use a copy with its ports moved (script 4).

## Found while writing the Iterator tutorial (2026-11-15)

- **Table definitions fall behind the code (root cause fixed in the otsdaq checkout of dev-ots-web, local commits `06e3ec52`, `1186893d`).** **Seen.**
  Creating an iteration plan failed with `Table not found with name: IterationCommandIncludedSubsystemsTable`. `CoreTableInfoNames.dat`
  is filled, and has missing names appended, by the `ConfigurationManagerRW` constructor (Wiz mode, Configure app) from
  `ConfigurationManager::iterateMemberNames_`, and that set lacked the new table; fixed by adding it. Verified in Wiz mode: with the line deleted,
  the next Wiz start appends it. A second bug turned up in the same code: the last line of the file has no newline, so its last character was
  chopped when comparing, it never matched, and it was appended again at **every** start (three entries were already listed twice); fixed by stripping only a real newline.
  Verified: three further Wiz starts leave one copy. Still open: `UpdateOTS.sh --tables` rewrites **all** listed definitions without checking that saved
  versions still match them (it broke the Context group: `StateMachineTable` and five others, the Gateway exited at launch three times). The script
  does save `TableInfo.updateots.bk.<date_time>`, the right place to restore from. A guard would load the active groups against the refreshed definitions and offer to restore the backup.
- **A skill for the Iterator.** **Seen.** `gateway-udp-fsm.md` covers `iteratePlayGenerated` only. Creating and editing a real plan
  (`createIterationPlan`, the command editor, the 19 command types, saving to the Iterate group) is browser-only; a script like
  `iterate_make_plan.sh` is still missing (see the iteration plans item above).
- **Headless browser limits.** **Seen.** The Iterator asks for a plan name with a native `prompt()`, which blocks a headless run; the tutorial
  helpers call `createIterationPlan(true,false)` after setting `_targetPlan`. A UDP or script path would avoid this.

## Found while writing the Basic Features and Hotkeys tutorial (2026-11-15)

- **A supervisor that fails to construct takes the whole launch down.** **Seen.** Turning the `ControlsDashboardSupervisor` On without a
  `SlowControlsInterfacePluginType` made `ots_lite_main` throw ("Could not find property named ControlsInterfacePluginType"); the Gateway context
  exited at launch three times and `ots` gave up. A missing plugin type should switch that one application off with an error, not stop the system.
- **Enabling the Controls Dashboard needs a recipe.** **Seen.** Three coordinated edits (icon Status, application Status, a `SlowControlsDashboardSupervisorTable`
  row with `SlowControlsInterfacePluginType=OtsSlowControlsInterface`, and the link from the application's `LinkToSupervisorTable`); done here with
  `cfg_udp_edit_group.sh -w ... +SlowControlsDashboardSupervisorTable/ControlsSupervisor0 ... @link-UID=...`. Needs `enable_controls_dashboard.sh` and the slow controls skill.
- **`revert_active_table_group.sh` on a stopped system leaves a stale launch file.** **Seen.** After an offline revert from a Context key that broke the launch,
  the next `ots` copied `XDAQConfigurations/otsConfiguration_CMake_Run.xml` (written by the previous Gateway, still describing the broken key) to `ots.xml` and failed again;
  the second start worked, because the failed launch had rewritten the file. The script should delete or regenerate that file.
- **The dashboard hotkeys are untestable headless.** **Seen.** Synthetic `KeyboardEvent`s did not move a selected widget. Needs trusted key input (`ff_screenshot.py --steps` has it, in video mode only).

