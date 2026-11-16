# Table plugins: when you need one, how to write it, how to tell it loaded

A **table plugin** is the C++ class ots creates for a table when it loads a configuration group. Most tables do not
have one. Read this to decide whether yours needs one, to write it from the scaffold, and to diagnose the case
that fails silently: a plugin that did not load. The table definition itself (columns, types, choices) is
`configuration-model.md` and `ai_scripts/tableinfo.py`; processors that read their own settings table are
`data-processor-plugins.md`.

## What a plugin is, and when you do not need one

`ConfigurationInterface::get()` (`ConfigurationInterface.icc`) builds a table object by name:
1. try `makeTable(name)`: load the library `lib<repo>_<dir>_<Name>_table.so` and call its `make()`;
2. if that throws or finds nothing (the exception is swallowed), use a plain `TableBase(name)`.

Either way the constructor reads `TableInfo/<Name>Info.xml`; the plain object is a complete table. A lite install
holds five table plugins (`XDAQContextTable`, `DesktopIconTable`, `IterateTable`, `MessageFacilityTable`,
`FESlowControlsTable`); every other table (`DataBufferTable`, `DataProcessorTable`, `FEInterfaceTable`,
`RawDataSaverConsumerTable`, ...) runs on the plain object.

| You want | Plugin? |
|---|---|
| a front end, data processor or supervisor to read its settings | **No.** It reads them through the `ConfigurationTree` it is given (`getNode("Column").getValue(x)`), see `fe-macros-and-macromaker.md`, `data-processor-plugins.md` |
| a new table, rows, links to it | **No.** Definition file, then rows (below) |
| check the table when its group is activated and refuse a bad group | Yes: `init()` throws |
| write files or derive data from the table when the group is activated | Yes: `init()` (`MessageFacilityTable` writes the Message Facility `.fcl`/`.cfg` files) |
| give application code the table as a C++ object | Yes: `configManager->getTable<YourTable>("YourTable")` (`XDAQContextTable`, `DesktopIconTable`) |

A new table with no plugin, *verified 2026-11-15* on ots maxlite (text store):
```bash
ai_scripts/tableinfo.py new AgentDemoTable "Note:Data:STRING:hello" "Count:Data:NUMBER:3" "Enabled:TrueFalse"
ai_scripts/cfg_udp_edit_group.sh -w -t Configuration -g defaultConfig "new:AgentDemoTable=Demo0,world,7,True,first row"
ai_scripts/cfg_udp_set_alias.sh defaultSystemAlias defaultConfig <new key>      # then:
ai_scripts/apply_config_reload.sh PhysicsRuns0 defaultSystemAlias               # Halt, Shutdown, Startup, Configure
ai_scripts/cfg_udp_table_csv.py get AgentDemoTable 0 /tmp/t.csv                 # reads the row back
```
The Gateway Configured with the new group and the row read back as `Demo0,world,7,True,...`. `new:` creates the first
version of a table that has none and adds it to the group (`cfg_udp_edit_group.sh` header); `+Table/UID` adds a row to a
table that is already a member. The CSV and group steps are the same as in `configuration-model.md`.

## Writing a plugin

```bash
ai_scripts/new_table_plugin.py MyThingTable --into /tmp/scaffold            # needs $USER_DATA/TableInfo/MyThingTableInfo.xml
ai_scripts/new_table_plugin.py MyThingTable --include-dir otsdaq-components/UserTableDataFormats
```
It reads the definition and writes `<Name>.h` and `<Name>_table.cc`: a `Row` struct with one typed member per data
column, `init()` filling `rows_` from the active rows (skipping `Status` Off), `getRows()`/`getRow(uid)`, and a
commented validation example. The pieces, if you write one by hand (`TablePlugins/TemplateTable*` is the minimal example):

- **Class and name**: `class X : public TableBase`, constructor `X() : TableBase("X")`. The class name, the string, the
  `TABLE Name=` in the definition, the file `<X>Info.xml` and the CMake name are all the same.
- **Registration**: end the `.cc` with `DEFINE_OTS_TABLE(X)` (`TablePluginMacros.h`: `extern "C" TableBase* make()`).
- **Constructor**: do not read table values (no table is loaded yet). `__COUT__` there crashed TRACE in full builds
  (`TableBase.cc` comment); keep it empty.
- **`init(ConfigurationManager* configManager)`**: the only hook. `configManager->__SELF_NODE__` is this table's node
  (`getNode(getTableName())`); `.getChildren()` gives the rows as `(UID, ConfigurationTree)` pairs; `row.getNode("Col")`
  is a cell, `.getValue(x)` reads it (types below), `.isDefaultValue()` tells an untouched cell. Other tables:
  `configManager->getNode("OtherTable")`. Throw `__SS__ << ...; __SS_THROW__;` (a `std::runtime_error`) to refuse the group.
  Guard file writes with `if(!configManager->isOwnerFirstAppInContext()) return;` (once per context process).
- **Threads**: in a Configuration group on a machine with at least 4 hardware threads the members' `init()` calls run in
  parallel threads (`ConfigurationManager.cc`, `initTableThread`); Context, Backbone and Iterate groups run one after
  another. Do not depend on another table's `init()` having run; protect anything shared.
- **Errors in `init()`**: a `std::runtime_error` is reported as "Error detected calling `<Table>`.init()!" followed by your
  message (thrown, or collected as a warning when the caller accumulates warnings); any other exception type is logged
  as a warning ("Unknown Error ...") and ignored. Throw runtime_errors.

### Reading cells (`getValue`, from `TableView.icc`)

| Column | `getValue(x)` with | Result |
|---|---|---|
| `DataType` NUMBER | any number type | `StringMacros::getNumber`: decimal, `0x` hex, and `+ - * /` between numbers (`2*1024`); `${VAR}` expanded first; otherwise throws "... is not a number!" |
| `FixedChoiceData`, STRING | `int`/`unsigned int` | the **index of the choice**: default value 0, the first choice 1, ... (the `arbitraryBool=` entry is skipped); not a choice: throws |
| `OnOff`, `TrueFalse`, `YesNo` (STRING) | `bool` | true only for exactly `On`, `True`, `Yes`; these types must have DataType STRING |
| other STRING | `std::string` | the text, `${VAR}` expanded |
| other STRING | `bool` | true if it starts with `1`, false with `0`, else throws "Invalid boolean value" |
| link columns | do not `getValue`: `row.getNode("LinkToX")` follows the link to the record, `.isDisconnected()` | the linked record's node |

Names are case sensitive; a name that is not a column throws. Defaults come from the definition's `DefaultValue`.

## Build and load

In the plugin directory's `CMakeLists.txt`, after `include(otsdaq::table)`:
```
cet_build_plugin(MyThingTable otsdaq::table)
```
It expects `MyThingTable_table.cc` next to it. The library is `lib<repo>_<dir>_MyThingTable_table.so` (for example
`libotsdaq_TablePlugins_MessageFacilityTable_table.so`); the loader finds it by the table name on `CET_PLUGIN_PATH`, then
`LD_LIBRARY_PATH` (`ots maxlite` uses `dlopen` with the same rule, `PluginFactory.h`). If other code must link against the
class (it calls `getTable<MyThingTable>`), put the logic in `MyThingTableImpl.cc`, build it with `cet_make_library(LIBRARY_NAME
MyThingTable SOURCE MyThingTableImpl.cc ...)` and give the plugin `LIBRARIES REG <that library>`: `DesktopIconTable` and
`IterateTable` in `otsdaq/TablePlugins/CMakeLists.txt` are the examples. The artdaq tables are not built in lite.
`otsdaq-components/UserTableDataFormats` holds older FE table stubs with commented-out code: copy the scaffold instead.

The cycle (user builds): edit in dev-srcs, `mz_uc` for a new file and `mb`, restart `ots`, make the rows (above), apply,
check `init()` ran.

## Did it load? (the silent failure)

A missing or unloadable plugin is **not an error**: *verified 2026-11-15* by moving
`libotsdaq_TablePlugins_MessageFacilityTable_table.so` out of `local/install/lib`, restarting and Configuring: the Gateway
reached Configured, the log held no line naming the table, `TableBase`, "plugin" or `init()` for it, and the files that plugin's `init()` writes
(`Data/MessageFacilityConfigurations/MessageFacilityGen.fcl`) were not written; with the library back they were.

| Check | How |
|---|---|
| the library exists | `ls $OTSDAQ_LIB \| grep MyThingTable_table` |
| `init()` ran | the line you log in `init()` (the scaffold logs "`MyThingTable` initialized with N active row(s)") in the Gateway log, `$USER_DATA/Logs/otsdaq_quiet_run-gateway-<host>-<port>.txt`, or the effect it has |
| group init happened | log lines "init start [for all plug-ins in Configuration group 'defaultConfig(4)']" and "init complete": seen once at Startup and once at Configure (20:31:29 and 20:32:06 in the run checked) |

| Symptom | Cause / fix |
|---|---|
| nothing happens, no error, `init()` never logs | the library is missing, not on the path, or its constructor threw (swallowed): check the library, then that `<Name>Info.xml` is readable (`tableinfo.py check <Name>`) |
| `Illegal cast of 'X' to type ots::X` from `getTable<X>` | the same: `X` was built as a plain table; `getTable` needs the plugin |
| "Failure reading table schema info for table 'X!' Perhaps the XML table definition has moved or been corrupted" | `TableInfo/XInfo.xml` is missing or malformed (the plugin constructor reads it too) |
| "Error detected calling X.init()!" + your text | `init()` threw: the message is yours; fix the data or the check |
| `... is not a number!`, `Invalid boolean value`, an unknown column | `getValue` type does not fit the column (table above), a typo in a name, or the plugin was written for an older definition: keep `Row`/`init()` in step with the Info file |
| old saved versions fail after a column rename | `configuration-model.md` (columns are stored by StorageName; `cfg_udp_repair_table.py`) |

## Verified and not run

*Run:* a table with no plugin through definition, first version, group, alias, reload, Configure and read-back; a missing plugin
library (silent; effect gone, back with the library); the `init start/complete` log lines; the scaffold for five
definitions (a throwaway one with every column kind, and the shipped `RawDataSaverConsumerTable`, `DataProcessorTable`,
`FEInterfaceTable`, `XDAQApplicationTable`) compiled with the project flags (`-Werror`, syntax only).
*From the source, not run:* building and loading a plugin written with the scaffold, a throwing `init()` and its message in
the Configuration GUI, the parallel `init()` threads, `getTable<T>` of your own class, `dlopen` loading in maxlite of a table
plugin (naming rule from `PluginFactory.h`), the artdaq tables.
