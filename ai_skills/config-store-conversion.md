# Moving configurations between store types

otsdaq reads its configuration through a `ConfigurationInterface` plugin chosen from
`ARTDAQ_DATABASE_URI`:

| URI | Plugin | Format on disk | Built in |
|-----|--------|----------------|----------|
| `filesystemdb://<path>` | `Database` (artdaq-database) | per-table `index.json` + one document per version, `GroupCache_*` | full, lite |
| `mongodb://...` | `Database` (artdaq-database) | MongoDB | full |
| `textdb://<path>` | `Text` (`otsdaq/ConfigurationInterface/Text_configInterface.cc`) | `tables/<T>/v<N>.json`, `groups/<G>/k<K>.txt`, no index files | full, lite, maxlite |

The script `../ai_scripts/convert_config_store.sh` wraps both ways below. Run it (and the tools)
from an install that has the plugins for both stores: ots lite or full for
`filesystemdb` <-> `textdb`; ots maxlite has only `Text`, and the script says so. Never write into
the store of a running ots; read-only exports from a running one are fine.

## Way 1: the whole store, numbers kept

Every table version and group key, with the same version and key numbers, so active group files,
aliases and logs stay valid. Use it to switch an installation to another store type, or to seed
an embedded target.

```bash
source <lite area>/setup_ots.sh
ai_scripts/convert_config_store.sh all filesystemdb://<path>/filesystemdb/test_db textdb://<path>/textdb/test_db
# same as: otsdaq_convert_config_db <from> <to> --verify   (-n / --dry-run lists only)
```

- Table text is copied as stored (`ConfigurationInterface::loadRawTableJSON`/`saveRawTableJSON`),
  no table schema needed. Entries already in the destination are skipped, so it can be rerun.
- `GroupCache_*` (artdaq-database lookup caches) are not copied; the `Database` plugin rebuilds
  them as groups are saved. Reading groups from an artdaq-database source can add cache entries
  to the source.
- Directories that are not table names (the tutorial data has a stray `test_db/test_db`) are
  reported and skipped.
- Then set `ARTDAQ_DATABASE_URI` to the new store in `setup_ots.sh` (the `USER_DATA` area is
  unchanged: same keys).

Verified 2026-11-12: tutorial store 35 table versions + 7 keys, and a working store 65 + 20;
round trip text -> artdaq-database -> text byte-identical.

## Way 2: the active groups and system aliases, renumbered

The export/import tools take what is active (Context, Configuration, Backbone and the groups the
aliases point to) from one store + `USER_DATA`, and import it into another as new keys. Use it to
bring a running system's configuration into another system, merging, or into an empty store.

```bash
source <lite area>/setup_ots.sh
# merge into the destination's valid active groups (aliases from the source are added):
ai_scripts/convert_config_store.sh active <fromURI> <fromUSER_DATA> <toURI> <toUSER_DATA> [--prepend <label>]
# empty destination store: start from a new empty Backbone
ai_scripts/convert_config_store.sh active <fromURI> <fromUSER_DATA> <toURI> <toUSER_DATA> --fresh
```

Underneath: `otsdaq_export_system_aliases <dir>` with the source's `ARTDAQ_DATABASE_URI`/`USER_DATA`
writes `<dir>/<Group>_<key>/<Table>_v<N>.json` (and `groupIsBackbone.txt`); then
`otsdaq_import_groups_from_export_path <dir> [prepend]` with the destination's. The export is
format-neutral. `otsdaq/tools/otsdaq_import_system_aliases.sh <fromURI> <fromUSER_DATA> [prepend]`
does both steps into the current setup (`EXPORT_ONLY` as first argument to export only; it
writes `./tmp/export_system_aliases`).

- The importer refuses a destination whose `ActiveTableGroups.cfg` names groups that are not in
  its store ("Did not find valid active groups ..."); `--fresh` moves that file aside (backup
  kept) so the importer creates an empty Backbone first.
- After a fresh import only the Backbone is active (as in lite); the aliases point at the new keys
  (e.g. `defaultSystemAlias` -> `defaultConfig` key 0). Activate Context/Configuration in the
  Configuration GUI or with `ai_scripts/cfg_udp_edit_group.sh`, or start `ots --wiz` once.

Verified 2026-11-12: maxlite text store -> fresh text store (22 tables identical to the export),
-> copy of itself (merge), -> fresh artdaq-database store (lite tools, 3 runs), and lite
artdaq-database -> fresh maxlite text store.

## Way 3: one table, as CSV

```bash
ots_table_download_csv <Table> <version> <file.csv>
export OTS_KCACHE_USER=$(klist | grep 'Default principal' | cut -d: -f2 | sed 's/ //g' | cut -d@ -f1)
ots_table_upload_csv <Table> <file.csv>     # identical content -> existing version reused
```

Works for every store type (it goes through the active plugin). The upload refuses to run without
`OTS_KCACHE_USER`; the saved AUTHOR is the tool's `export_admin`.

## Pitfall fixed on the way

`XDAQContextTable.cc` used to be compiled twice (its library, and the implementation library
`cet_build_plugin` makes from `<name>.cc`), and the importer sometimes ended with `double free or
corruption` after finishing its work (lite and maxlite). Since commit 56d4b759 the source is
`XDAQContextTableImpl.cc`. An install updated with `mb`/`mz` keeps the old
`lib..._XDAQContextTable_XDAQContextTable.so` in `local/install/lib`: it is no longer loaded, but
delete it (or use `mz_uc`).

Related: [ots-maxlite-architecture.md](ots-maxlite-architecture.md) (text store layout),
[configgui-udp.md](configgui-udp.md) (editing over UDP).
