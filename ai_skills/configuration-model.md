# The configuration model: tables, versions, groups, keys, aliases

What an agent must know before it edits any otsdaq configuration. The recipes are in
`configgui-udp.md` (requests) and `../ai_scripts/README.md` (scripts); how a change reaches a
running system is in `gateway-udp-fsm.md`. This page is the model behind them.

## The pieces

| Piece | What it is | Where it lives | Numbered by |
|---|---|---|---|
| **Table definition** | the columns of a table (name, type, default, choices) | `$USER_DATA/TableInfo/<Name>Info.xml` (Wiz mode Table Editor) | not versioned: one file per table |
| **Table version** | the rows of one table, saved once and never changed | the configuration store (`ARTDAQ_DATABASE_URI`: `filesystemdb://` or `textdb://`) | **version**: 0, 1, 2, ... per table |
| **Temporary version** | an unsaved edit in one process's memory | memory of the Configuration GUI | negative numbers (`-2`, `-3`, ...); lost on restart |
| **Group** | a named set of `table version` pairs, saved once | the store | **key**: 0, 1, 2, ... per group name |
| **Active group** | the key of each group type that the processes load | `$USER_DATA/ServiceData/ActiveTableGroups.cfg` (name/key line pairs) | one per type |
| **Group alias** | a name for a group key, e.g. `defaultSystemAlias` | a row of `GroupAliasesTable`, inside the active Backbone group | - |
| **Version alias** | a name for a table version | a row of `VersionAliasesTable`, inside the active Backbone group | - |

A **version** belongs to a table, a **key** to a group: "defaultConfig key 4" and
"FEInterfaceTable version 4" are unrelated numbers. Nothing saved is ever overwritten: every
edit makes a new table version, and every change of membership a new group key.

A table needs only its definition to work. A C++ **table plugin** (`table-plugins.md`) is optional: it adds code that
runs when the group is activated, or a typed object for application code.

## Group types

The type of a group follows from its member tables (`ConfigurationManager::getTypeOfGroup`):

| Type | Members | Loaded | Used for |
|---|---|---|---|
| **Context** | exactly the fixed context tables: `XDAQContextTable`, `XDAQApplicationTable`, `XDAQApplicationPropertyTable`, `DesktopIconTable`, `MessageFacilityTable`, `GatewaySupervisorTable`, `StateMachineTable`, `DesktopWindowParameterTable`, `SlowControlsDashboardSupervisorTable` (+ optional `SubsystemUserDataPathsTable`) | when the processes start, and at Startup | which processes and apps exist, their ports, Gateway settings, Desktop icons |
| **Backbone** | exactly `GroupAliasesTable` and `VersionAliasesTable` | when a configuration manager starts (Gateway: at Startup) | the aliases |
| **Iterate** | the Iterator tables (`IterateTable`, `IterationPlanTable`, `IterationTargetTable`, the `IterationCommand...Table`s) | by the Iterator | iteration plans |
| **Configuration** | anything else (front ends, data processors, ...) | at **Configure**, by name and key | what a run does |

A group with some but not all members of a fixed type is an error ("incomplete match to a
Backbone group").

## How Configure finds its configuration

`Configure,<alias>` (browser, `ots_drive_udp_test ... Configure <alias>`, the Iterator)
names a **group alias**. The Gateway looks the alias up in the `GroupAliasesTable` version of
the **active Backbone group that it loaded**, which gives a Configuration group name and key,
and loads that key. The Gateway log shows the result:

```
Attempting Configure transition with Configuration table group name: defaultConfig, key: 4
```

So a table edit reaches a run only after **four** steps, in this order:

1. new **table version** (the edit), 
2. new **group key** with that version (`saveNewTableGroup`), activated,
3. the **alias** moved to the new key: new `GroupAliasesTable` version, new **Backbone key**,
   activated (`cfg_udp_set_alias.sh`),
4. the running Gateway made to read the new Backbone (next section).

`cfg_udp_edit_field.sh` / `cfg_udp_edit_group.sh` do 1 and 2; `cfg_udp_set_alias.sh` does 3;
`apply_config_reload.sh` does 4 (and the Configure). Skipping 3 is the common mistake: the
alias still points at the old key and Configure silently loads the old configuration.

## Which step makes a change take effect

| What changed | Needed | Script |
|---|---|---|
| Configuration group (through its alias), alias, version alias: a new **Backbone** key | Halt, Shutdown, Startup, Initialize, then Configure (or restart `ots`) | `apply_config_reload.sh <fsm> <alias>` |
| **Context** group: Gateway settings, permissions, icons, table values of existing apps | Shutdown, Startup (see the matrix in `gateway-udp-fsm.md`, "Applying Active Context changes") | `apply_config_reload.sh <fsm>` |
| **Context** layout: a new context or app, a changed port or LID | a total restart, and a **second** `ots` launch: the ots script starts contexts from its copy `ots.xml` of the run XML that the Gateway writes (`gateway-udp-fsm.md`; seen in `sunrock-fe-context.md`) | `echo Y \| ots -k; ots`, then once more |
| Table definition (`TableInfo/*.xml`) | the Configuration GUI re-reads it on a table read with `allowIllegalColumns=1` (no restart); other processes at their start (`ots -k; ots`). Saved versions store columns by **StorageName**: a rename that keeps it changes nothing for them; a changed StorageName needs a repair | `tableinfo.py`, `cfg_udp_repair_table.py` |
| Iterate group | nothing: the Iterator reads it when a plan is played | - |

## Who changed it

Every saved version and key records its author (the Kerberos user of `ots_cfg_udp_test`, or the
browser user) and a comment. Scripts in `../ai_scripts` never save anonymously. `getGroupAliases`
and `getSpecificTableGroup` show author, comment and time.

## Undo

Nothing is deleted, so undo is "point back": activate the old group key
(`activateTableGroup`, or `revert_active_table_group.sh` when `ots` cannot start), and move the
alias back (`cfg_udp_set_alias.sh <alias> <group> <old key>`), then apply as above.

Verified 2026-11-14 on ots maxlite (text store): the four steps with `cfg_udp_set_table_alias.sh`
and `cfg_udp_list_aliases.sh`, and the apply with `apply_config_reload.sh` (Halt, Shutdown,
Startup, Initialize, Configure in 32 s; the Configure log line names the group and key). The
group types and their member tables are from `ConfigurationManager.cc`.
