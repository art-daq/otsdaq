# otsdaq ai_scripts index

Helper scripts for AI agents (and people) working with a running otsdaq lite system. All
expect `source setup_ots.sh` first and discover group names, keys and versions at run time.
The recipes behind them are in `../ai_skills/README.md`. Run any script without arguments (or
read its header) for usage.

| Script | Needs | Does |
|--------|-------|------|
| [check_ots_status.sh](check_ots_status.sh) | `setup_ots.sh` | quick health check: processes, UDP ports, logs, TRACE, UDP remote controls |
| [ots_mode.sh](ots_mode.sh) | `setup_ots.sh` | `status`: which mode this installation runs (normal, Wiz, Macro Maker, none) and the URL to open (Wiz: with the access code); `wiz`/`normal` start it (refuse to switch a running mode without `-f`), `stop` = `ots -k` (this USER_DATA only) |
| [gateway_log_events.sh](gateway_log_events.sh) | `setup_ots.sh` | digest of the Gateway log: remote commands, transitions, the Configure group and key, rejections (`Error - `), failures, crashes; `-s` since the last start, `-a` all ERROR/WARNING lines |
| [gateway_udp_cmd.sh](gateway_udp_cmd.sh) | Gateway UDP enabled (`gateway-udp-fsm.md`) | send a Gateway UDP message (FSM command or query) and poll the Gateway status until a target state or failure; prints the Gateway log's error lines on failure/timeout, and exits 4 with a warning when the state was reached but no remote command of ours was logged (someone else moved it) |
| [apply_config_reload.sh](apply_config_reload.sh) | Gateway UDP, `ots_drive_udp_test` | make a running system use a changed Backbone/Context group: Halt, Shutdown, Startup, Initialize, then Configure `<alias>`; stops at the first failure and prints the Configure log line as proof; `-n` dry run |
| [mm_macro.py](mm_macro.py) | MacroMaker UDP | `list` FEs with their FE macros (inputs -> outputs) and the saved Macro Maker macros; `run <fe> <macro> name=value...` any macro type (fe, public, private) with name/count checks and the outputs; `new <name> 'w <addr> <data|$var>' 'r <addr|$var> [$out]' 'd <ms>'` writes a Macro Maker macro file (refuses hex-only variable names); `show`, `delete`. `ots_mm_udp_test` runs FE macros only |
| [mm_make_mom.py](mm_make_mom.py) | MacroMaker UDP (to check names) | write a MoM sequence file (`$USER_DATA/ServiceData/MacroSequence/NO-USER/<name>.dat`) from steps `"<feUID> <macro> input=value..."`, `"delay <s>"`, `"mom <name> [n]"` and `--var name=value:step:max`; FE class, input and output names checked against the running Macro Maker |
| [emulator_sine_sweep.sh](emulator_sine_sweep.sh) | MacroMaker UDP, system Configured (Running to see it) | stream N channels of 16-bit sine waves from the emulator and sweep the amplitude for S seconds; the "two channels, amplitude varying slowly" request from the AI tutorial as a script that costs no tokens |
| [read_emulator_registers.sh](read_emulator_registers.sh) | MacroMaker UDP, system Configured | read the emulator's data-config registers |
| [new_data_processor.py](new_data_processor.py) | none (python 3) | scaffold a data processor plugin (`producer`/`consumer <Name>`): header and `_processor.cc` in the structure of the real plugins (generated code compiles against the installed headers), and the next steps (CMake line, settings table with `tableinfo.py`, configuration rows). Skill `data-processor-plugins.md` |
| [new_table_plugin.py](new_table_plugin.py) | none (python 3) | scaffold a table plugin from a table definition (`<Table>`): header with a typed `Row` per data column and `_table.cc` with an `init()` that fills it (generated code compiles with the project flags, syntax only), and the next steps (CMake line, build, how to check it loaded). Skill `table-plugins.md` |
| [read_raw_data.py](read_raw_data.py) | Python 3 | decode a `Run_RunN_Raw.dat` of the emulator: packet length (found from the headers), burst types, sequence gaps, counter values (mode 0) or per-channel samples with min/max/mean (`--channels N --bits B`), `--csv` |
| [emulator_ctl.sh](emulator_ctl.sh) | `setup_ots.sh` | start, stop, status of the `ots_udp_hw_emulator` of one UDP port (pid file, log in `$USER_DATA`); refuses a busy port and never touches another user's or program's process (no `killall`) |
| [cfg_udp_browse_tree.sh](cfg_udp_browse_tree.sh) | ConfigGUI UDP | read-only tree browse of the active Context group |
| [cfg_udp_edit_field.sh](cfg_udp_edit_field.sh) | ConfigGUI UDP, Kerberos | edit one field, save the table version and a new group key, and activate it (it does not move the alias: run `cfg_udp_set_alias.sh` next); `--from-key <key>` starts from that key instead of the active one |
| [cfg_udp_set_alias.sh](cfg_udp_set_alias.sh) | ConfigGUI UDP, Kerberos | point a group alias (e.g. `defaultSystemAlias`) at a group key: new GroupAliasesTable version, new active Backbone key. The last step after `cfg_udp_edit_field.sh`; Configure resolves the alias through the active Backbone |
| [cfg_udp_set_table_alias.sh](cfg_udp_set_table_alias.sh) | ConfigGUI UDP, Kerberos | point a table version alias at a table version: new VersionAliasesTable version, new active Backbone key (`setTableAliasInActiveBackbone`) |
| [cfg_udp_list_aliases.sh](cfg_udp_list_aliases.sh) | ConfigGUI UDP | read-only: the group aliases (what Configure accepts) with group and key, and the table version aliases, of the active Backbone; `-c` prints only the Configuration aliases, one `<alias> <group> <key>` per line, for use in a Configure command |
| [iteration_plan.py](iteration_plan.py) | ConfigGUI UDP, Kerberos | create, `show`, change (`set`, `add --at`, `remove`, `move`, `enable`/`disable`, `delete`) Iterator plans: steps fsm, configure, run, label/repeat, start/stop/..., modify (value scan), fe-macro/mm-macro with targets and dimensional loops; one new Iterate key each time; `-n` dry run. Skill `iterator-plans.md` |
| [cfg_udp_diff_groups.py](cfg_udp_diff_groups.py) | ConfigGUI UDP | read-only: what differs between two group keys (`<group> <keyA> <keyB>`, or `group:key`, `alias:<name>`, `active:<type>`): tables added/removed, rows added/removed, every changed cell with old and new value |
| [cfg_udp_table_csv.py](cfg_udp_table_csv.py) | ConfigGUI UDP, Kerberos for `put` | `get <table> <version> f.csv` / `put <table> <version> f.csv`: bulk-edit a table as CSV through the running GUI; `put` saves a new version (an identical file saves nothing) and prints the `add:` edit for the group. Without ots running: `ots_table_download_csv`/`ots_table_upload_csv` |
| [tableinfo.py](tableinfo.py) | `setup_ots.sh` (ConfigGUI UDP to refresh) | table definitions without the Wiz Table Editor: `new <Table> Name:Type[:DataType[:Default[:Choices]]]...` (UID first and Comment/Author/Timestamp last added), `edit add|rename|remove`, `check`; backup first; `rename` keeps the StorageName (old versions keep loading) unless `--storage`; the running ConfigGUI re-reads definitions at once |
| [cfg_udp_repair_table.py](cfg_udp_repair_table.py) | ConfigGUI UDP, Kerberos | after a definition change: load `<table> <version>` with loose column matching and save it as a new version in the current format (`-n` to look first) |
| [enable_gateway_udp_transitions.sh](enable_gateway_udp_transitions.sh) | ConfigGUI UDP, Kerberos | enable Gateway UDP state changes and application status monitoring (IP, port and flags together) |
| [add_desktop_icon.sh](add_desktop_icon.sh) | ConfigGUI UDP, Kerberos | adds a Desktop icon (presets `appstatus`, `subsystemlaunch`) to the active Context group; `-n` dry run, `-w` accept tree warnings |
| [desktop_icon_ctl.py](desktop_icon_ctl.py) | ConfigGUI UDP, Kerberos | `list` the Desktop icons of the active Context; `off`/`on <IconName or Caption>` hides/shows one (Status), `delete` removes the row; new Context key, activated; `-n` dry run, `-w` accept tree warnings |
| [cfg_udp_edit_group.sh](cfg_udp_edit_group.sh) | ConfigGUI UDP, Kerberos | many edits across a group's tables (cells, links, new rows, row deletes `-T/uid` after `--`, `new:` first table version, `add:` existing table), one new group (optionally a new name), activate; `-n` dry run, `-w` accept tree warnings; `-k <key>` (or `--from-key`) starts from that key instead of the active one |
| [add_remote_subsystem.sh](add_remote_subsystem.sh) | ConfigGUI UDP of the top level, Kerberos | make a satellite ots a Remote Gateway subsystem of the top level: `SubsystemUserDataPathsTable` row + `ots:` Desktop icon in one Context group; `-n` dry run |
| [isolate_ots_ports.py](isolate_ots_ports.py) | ConfigGUI UDP, Kerberos for `--apply` | list every port of the active Context and Configuration groups and the `OTS_*PORT*` environment, flag ports held by processes outside this installation; `--offset N` previews moved ports and their conflicts, `--apply` saves them (one new key per group, activated; `--accept-warnings` for Contexts with existing tree warnings) and prints the `export` lines for `setup_ots.sh` |
| [setup_ots_subsystems.sh](setup_ots_subsystems.sh) | write access to `setup_ots.sh` | make `source setup_ots.sh <subsystem>` select `Data_<subsystem>` and its ports (shared database); rerun after `mz_uc` |
| [convert_config_store.sh](convert_config_store.sh) | `setup_ots.sh` of an install with both store plugins (lite/full) | `all`: copy a whole configuration store between `filesystemdb://` and `textdb://` keeping numbers; `active`: export the active groups and aliases of one store + USER_DATA and import them into another (merge, or `--fresh`); `-n` dry run |
| [maxlite_cross_build.sh](maxlite_cross_build.sh) | podman, network | cross-compile ots maxlite for aarch64 or arm32 in a Debian 12 container (`ots-quick-maxlite-start.sh --arch`), then check the libraries; works with rootless podman without `/etc/subuid` and with SELinux enforcing |
| [check_maxlite_libraries.sh](check_maxlite_libraries.sh) | readelf | list the shared libraries an ots maxlite install needs and flag ones maxlite must not need (TRACE, messagefacility, cetlib, fhiclcpp, Boost, artdaq, ZeroMQ, CURL, ICU...); works on cross-compiled installs |
| [switch_ots_tutorial.sh](switch_ots_tutorial.sh) | `setup_ots.sh`, `filesystemdb://` store, network | variant of `otsdaq-demo/tools/reset_ots_tutorial.sh` that kills nothing: refuses while this installation runs, installs `--tutorial <name>` data, prints the `.bak` copies it kept; `--launch` starts ots and the emulator (only if UDP 4000 is free) |
| [revert_active_table_group.sh](revert_active_table_group.sh) | file access to `Data/ServiceData` | revert an active group key when `ots` cannot start |

Config-changing scripts act as the Kerberos user and never proceed anonymously unless the user
confirms. Context and Backbone changes need an `ots` restart to take effect.

## Compiled tools (otsdaq-utilities/test, installed with the build)

These are the first choice when built; the scripts above do similar jobs without a build.

| Tool | Talks to | Does |
|------|----------|------|
| `ots_drive_udp_test <ip> <port> [...]` | Gateway UDP (address printed by `ots`) | drive the FSM (Configure/Start/Stop/Halt...) and wait for the result; status table; any Gateway query (`raw`) |
| `ots_mm_udp_test <ip> <port> [...]` | MacroMaker UDP (`$OTS_MACROMAKER_UDP_*`) | list FEs and FE macros, run macros, `r`/`w` registers |
| `ots_cfg_udp_test [-y] <ip> <port> <RequestType> [...]` | ConfigGUI UDP (`$OTS_CONFIG_UDP_*`) | any ConfigurationGUI request: read and edit tables, groups, aliases; exit 1 on an error reply, 2 on a harmless one (same value as current: verified 2026-11-15; duplicate group key and no-rows-modified use the same check, not exercised) |

## Keeping this index current

Add a row when you add a script (and its header comment: purpose, prerequisites, usage).
Prefer extending a script over a near-copy; test with a dry run or a throwaway instance.
