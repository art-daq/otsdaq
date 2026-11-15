# otsdaq ai_scripts index

Helper scripts for AI agents (and people) working with a running otsdaq lite system. All
expect `source setup_ots.sh` first and discover group names, keys and versions at run time.
The recipes behind them are in `../ai_skills/README.md`. Run any script without arguments (or
read its header) for usage.

| Script | Needs | Does |
|--------|-------|------|
| [check_ots_status.sh](check_ots_status.sh) | `setup_ots.sh` | quick health check: processes, UDP ports, logs, TRACE, UDP remote controls |
| [gateway_log_events.sh](gateway_log_events.sh) | `setup_ots.sh` | digest of the Gateway log: remote commands, transitions, the Configure group and key, rejections (`Error - `), failures, crashes; `-s` since the last start, `-a` all ERROR/WARNING lines |
| [gateway_udp_cmd.sh](gateway_udp_cmd.sh) | Gateway UDP enabled (`gateway-udp-fsm.md`) | send a Gateway UDP message (FSM command or query) and poll the Gateway status until a target state or failure |
| [apply_config_reload.sh](apply_config_reload.sh) | Gateway UDP, `ots_drive_udp_test` | make a running system use a changed Backbone/Context group: Halt, Shutdown, Startup, Initialize, then Configure `<alias>`; stops at the first failure and prints the Configure log line as proof; `-n` dry run |
| [emulator_sine_sweep.sh](emulator_sine_sweep.sh) | MacroMaker UDP, system Configured (Running to see it) | stream N channels of 16-bit sine waves from the emulator and sweep the amplitude for S seconds; the "two channels, amplitude varying slowly" request from the AI tutorial as a script that costs no tokens |
| [read_emulator_registers.sh](read_emulator_registers.sh) | MacroMaker UDP, system Configured | read the emulator's data-config registers |
| [emulator_ctl.sh](emulator_ctl.sh) | `setup_ots.sh` | start, stop, status of the `ots_udp_hw_emulator` of one UDP port (pid file, log in `$USER_DATA`); refuses a busy port and never touches another user's or program's process (no `killall`) |
| [cfg_udp_browse_tree.sh](cfg_udp_browse_tree.sh) | ConfigGUI UDP | read-only tree browse of the active Context group |
| [cfg_udp_edit_field.sh](cfg_udp_edit_field.sh) | ConfigGUI UDP, Kerberos | edit one field, save the table version and a new group key, and activate it (it does not move the alias: run `cfg_udp_set_alias.sh` next) |
| [cfg_udp_set_alias.sh](cfg_udp_set_alias.sh) | ConfigGUI UDP, Kerberos | point a group alias (e.g. `defaultSystemAlias`) at a group key: new GroupAliasesTable version, new active Backbone key. The last step after `cfg_udp_edit_field.sh`; Configure resolves the alias through the active Backbone |
| [cfg_udp_set_table_alias.sh](cfg_udp_set_table_alias.sh) | ConfigGUI UDP, Kerberos | point a table version alias at a table version: new VersionAliasesTable version, new active Backbone key (`setTableAliasInActiveBackbone`) |
| [cfg_udp_list_aliases.sh](cfg_udp_list_aliases.sh) | ConfigGUI UDP | read-only: the group aliases (what Configure accepts) with group and key, and the table version aliases, of the active Backbone |
| [enable_gateway_udp_transitions.sh](enable_gateway_udp_transitions.sh) | ConfigGUI UDP, Kerberos | enable Gateway UDP state changes and application status monitoring (IP, port and flags together) |
| [add_desktop_icon.sh](add_desktop_icon.sh) | ConfigGUI UDP, Kerberos | adds a Desktop icon (presets `appstatus`, `subsystemlaunch`) to the active Context group; `-n` dry run, `-w` accept tree warnings |
| [cfg_udp_edit_group.sh](cfg_udp_edit_group.sh) | ConfigGUI UDP, Kerberos | many edits across a group's tables (cells, links, new rows, row deletes `-T/uid` after `--`, `new:` first table version, `add:` existing table), one new group (optionally a new name), activate; `-n` dry run, `-w` accept tree warnings |
| [add_remote_subsystem.sh](add_remote_subsystem.sh) | ConfigGUI UDP of the top level, Kerberos | make a satellite ots a Remote Gateway subsystem of the top level: `SubsystemUserDataPathsTable` row + `ots:` Desktop icon in one Context group; `-n` dry run |
| [isolate_ots_ports.py](isolate_ots_ports.py) | ConfigGUI UDP, Kerberos for `--apply` | list every port of the active Context and Configuration groups and the `OTS_*PORT*` environment, flag ports held by processes outside this installation; `--offset N` previews moved ports and their conflicts, `--apply` saves them (one new key per group, activated; `--accept-warnings` for Contexts with existing tree warnings) and prints the `export` lines for `setup_ots.sh` |
| [setup_ots_subsystems.sh](setup_ots_subsystems.sh) | write access to `setup_ots.sh` | make `source setup_ots.sh <subsystem>` select `Data_<subsystem>` and its ports (shared database); rerun after `mz_uc` |
| [convert_config_store.sh](convert_config_store.sh) | `setup_ots.sh` of an install with both store plugins (lite/full) | `all`: copy a whole configuration store between `filesystemdb://` and `textdb://` keeping numbers; `active`: export the active groups and aliases of one store + USER_DATA and import them into another (merge, or `--fresh`); `-n` dry run |
| [maxlite_cross_build.sh](maxlite_cross_build.sh) | podman, network | cross-compile ots maxlite for aarch64 or arm32 in a Debian 12 container (`ots-quick-maxlite-start.sh --arch`), then check the libraries; works with rootless podman without `/etc/subuid` and with SELinux enforcing |
| [check_maxlite_libraries.sh](check_maxlite_libraries.sh) | readelf | list the shared libraries an ots maxlite install needs and flag ones maxlite must not need (TRACE, messagefacility, cetlib, fhiclcpp, Boost, artdaq, ZeroMQ, CURL, ICU...); works on cross-compiled installs |
| [revert_active_table_group.sh](revert_active_table_group.sh) | file access to `Data/ServiceData` | revert an active group key when `ots` cannot start |

Config-changing scripts act as the Kerberos user and never proceed anonymously unless the user
confirms. Context and Backbone changes need an `ots` restart to take effect.

## Compiled tools (otsdaq-utilities/test, installed with the build)

These are the first choice when built; the scripts above do similar jobs without a build.

| Tool | Talks to | Does |
|------|----------|------|
| `ots_drive_udp_test <ip> <port> [...]` | Gateway UDP (address printed by `ots`) | drive the FSM (Configure/Start/Stop/Halt...) and wait for the result; status table; any Gateway query (`raw`) |
| `ots_mm_udp_test <ip> <port> [...]` | MacroMaker UDP (`$OTS_MACROMAKER_UDP_*`) | list FEs and FE macros, run macros, `r`/`w` registers |
| `ots_cfg_udp_test [-y] <ip> <port> <RequestType> [...]` | ConfigGUI UDP (`$OTS_CONFIG_UDP_*`) | any ConfigurationGUI request: read and edit tables, groups, aliases |

## Keeping this index current

Add a row when you add a script (and its header comment: purpose, prerequisites, usage).
Prefer extending a script over a near-copy; test with a dry run or a throwaway instance.
