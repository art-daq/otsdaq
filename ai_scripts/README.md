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
| [add_desktop_icon.sh](add_desktop_icon.sh) | ConfigGUI UDP, Kerberos | add a Desktop icon (e.g. `appstatus`) to the active Context group; `-n` dry run |
| [revert_active_table_group.sh](revert_active_table_group.sh) | file access to `Data/ServiceData` | revert an active group key when `ots` cannot start |

Config-changing scripts act as the Kerberos user and never proceed anonymously unless the user
confirms. Context and Backbone changes need an `ots` restart to take effect.

## Keeping this index current

Add a row when you add a script (and its header comment: purpose, prerequisites, usage).
Prefer extending a script over a near-copy; test with a dry run or a throwaway instance.
