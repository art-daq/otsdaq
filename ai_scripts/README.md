# otsdaq ai_scripts index

Helper scripts for AI agents (and people) working with a running otsdaq lite system. All
expect `source setup_ots.sh` first and discover group names, keys and versions at run time.
The recipes behind them are in `../ai_skills/README.md`. Run any script without arguments (or
read its header) for usage.

| Script | Needs | Does |
|--------|-------|------|
| [check_ots_status.sh](check_ots_status.sh) | `setup_ots.sh` | quick health check: processes, UDP ports, logs, TRACE, UDP remote controls |
| [gateway_udp_cmd.sh](gateway_udp_cmd.sh) | Gateway UDP enabled (`gateway-udp-fsm.md`) | send a Gateway UDP message (FSM command or query) and poll the Gateway status until a target state or failure |
| [read_emulator_registers.sh](read_emulator_registers.sh) | MacroMaker UDP, system Configured | read the emulator's data-config registers |
| [cfg_udp_browse_tree.sh](cfg_udp_browse_tree.sh) | ConfigGUI UDP | read-only tree browse of the active Context group |
| [cfg_udp_edit_field.sh](cfg_udp_edit_field.sh) | ConfigGUI UDP, Kerberos | edit one field, save a new group and activate it |
| [enable_gateway_udp_transitions.sh](enable_gateway_udp_transitions.sh) | ConfigGUI UDP, Kerberos | enable Gateway UDP state changes (IP, port and flag together) |
| [add_desktop_icon.sh](add_desktop_icon.sh) | ConfigGUI UDP, Kerberos | adds a Desktop icon (presets `appstatus`, `subsystemlaunch`) to the active Context group; `-n` dry run, `-w` accept tree warnings |
| [cfg_udp_edit_group.sh](cfg_udp_edit_group.sh) | ConfigGUI UDP, Kerberos | many edits across a group's tables (cells, links, new rows, row deletes `-T/uid` after `--`, `new:` first table version, `add:` existing table), one new group (optionally a new name), activate; `-n` dry run, `-w` accept tree warnings |
| [add_remote_subsystem.sh](add_remote_subsystem.sh) | ConfigGUI UDP of the top level, Kerberos | make a satellite ots a Remote Gateway subsystem of the top level: `SubsystemUserDataPathsTable` row + `ots:` Desktop icon in one Context group; `-n` dry run |
| [setup_ots_subsystems.sh](setup_ots_subsystems.sh) | write access to `setup_ots.sh` | make `source setup_ots.sh <subsystem>` select `Data_<subsystem>` and its ports (shared database); rerun after `mz_uc` |
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
