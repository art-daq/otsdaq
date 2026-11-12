# ots maxlite (OTS_MAXLITE)

ots maxlite is ots lite (see [ots-lite-architecture.md](ots-lite-architecture.md)) reduced for
FPGA SoC embedded deployments: the whole of ots (Gateway, Configuration GUI, MacroMaker, FE
Supervisors, web desktop) without TRACE, messagefacility, cetlib, fhiclcpp, Boost,
artdaq-database, ZeroMQ, CURL or Python.

Branch: `feature/ots-maxlite` in otsdaq, otsdaq-utilities, otsdaq-components, otsdaq-demo
(otsdaq-td-ims-asic: `develop`).

## Install

```
mkdir ots-maxlite && cd ots-maxlite
cp <otsdaq-demo>/tools/ots-quick-maxlite-start.sh . && chmod 755 ots-quick-maxlite-start.sh
./ots-quick-maxlite-start.sh --branch feature/ots-maxlite --repo-base <dir with the repos, or omit for GitHub>
source setup_ots.sh
```

`setup_ots.sh` exports `OTS_LITE=1` and `OTS_MAXLITE=1`; `mb`, `mz` and `mz_uc` work as in lite
(`mz_uc` reruns `ots-quick-maxlite-start.sh`).

Verified 2026-11-12 (x86_64, AlmaLinux 9, 24 cores, gcc-toolset-13): clean install including the
compile of all repos in **117-140 s** (xerces-c 16 s when the OS lacks it, cetmodules 0 s,
otsdaq 67-81 s, otsdaq-components 10 s, otsdaq-utilities 18-22 s, otsdaq-demo 3 s). ots lite
from the same branch on the same machine: 276 s. Installed size: 88 MB (`local/install`,
unstripped; libraries 23 MB).

## What the programs need at run time

Verified with `readelf -d` on every installed program and library: libstdc++, libc/libm,
libgcc_s, **Xerces-C** and **OpenSSL libcrypto** (SHA-512 in WebUsers). Nothing else.
When the OS has no Xerces-C, the installer builds it with `-Dtranscoder=gnuiconv
-Dmessage-loader=inmemory`, so it does not need ICU either (verified with `readelf`).

Build host only: cmake >= 3.29, gcc >= 12, and cetmodules (CMake functions, built by the
installer into `local/install`).

## What changed, by area

| Area | lite | maxlite |
|------|------|---------|
| Logging macros (`otsdaq/Macros/CoutMacros.h`) | TRACE `TLOG` -> messagefacility | `otsdaq/Macros/OtsLog.h`: same `TLOG`, `TLOG_DEBUG(n)`, `TTEST(n)`, `TLVL_*` numbers; one line per message to stdout, `HH:MM:SS.mmm LEVEL subject file:line \| text` |
| Print threshold | `TRACE_LVLS`, `tonS`/`toffS` | `OTS_COUT_LEVEL` (number, default 8 = DEBUG; 9+ = __COUTT__/__COUTS__(n)); also settable from the Console page TRACE controls |
| `INIT_MF` | starts messagefacility | no-op |
| TRACE controller (Console page) | TRACE masks, trigger, buffer snapshot | one name `ots`, slow mask = OtsLog threshold; trigger/buffer report not available |
| Plugin loading | `cet::BasicPluginFactory` | `ots::PluginFactory` (`otsdaq/Macros/PluginFactory.h`): dlopen of `lib[<prefix>_]<Name>_<suffix>.so` from `CET_PLUGIN_PATH`, then `LD_LIBRARY_PATH` |
| Configuration store | `Database` plugin (artdaq-database, `filesystemdb://`) | `Text` plugin (`textdb://`), see below |
| Macro Maker mode from fcl (`CONFIGURATION_INIT_FCL`) | fhiclcpp | not available (throws) |
| FE Supervisor ZeroMQ data publishing | cppzmq | not available (`initDataPublishing` throws) |
| FE metric manager parameter set | fhicl (already disabled in lite) | compiled out |
| ECLWriter (electronic logbook) | built if CURL+XSD | not built |
| XdaqLite messagefacility destinations (`OTS`, `UDP`) | built | not built |

Pattern used in sources: `#ifdef OTS_MAXLITE` / `#ifndef OTS_MAXLITE`; `OTS_MAXLITE` always comes
with `OTS_LITE`. The `Macros` target carries `OTS_LITE OTS_MAXLITE` as public compile
definitions, so every repo that links otsdaq compiles the same header branches.

Changes made for all builds (lite and full too): Boost regex/filesystem/tokenizer replaced by
`std::regex`/`std::filesystem`/a plain split (TCPClientBase, XmlDocument, WizardSupervisor,
ConsoleSupervisor.h); unused Boost includes removed; standard includes that used to arrive
through TRACE added (`<cstdint>`, `<sys/stat.h>`, `<unistd.h>`).

Lite check, 2026-11-12: a clean ots lite install from `feature/ots-maxlite` (lite installer,
`--repo-base` dev-srcs) builds all repos without errors.

## Web Console

In lite and full builds messagefacility's UDP destination sends each message to
`otsConsoleFwd`, which forwards it to the Console Supervisor (ports in
`Data/MessageFacilityConfigurations/QuietForwarder.cfg`, e.g. 30000 -> 30001). In maxlite
`OtsLog` sends the same line itself, to the `type: UDP` destination (host, port) of the fcl
file named by `OTSDAQ_LOG_FHICL` (the ots script sets it, e.g. `MessageFacilityGen.fcl`):

```
UDPMFMESSAGE|12-Nov-2026 14:39:19|1|<host>|<host IP>|Debug|<subject>|<application>|<pid>|||<file>|<line>|<text>
```

Severity names are messagefacility's (`Error`, `Warning`, `Info`, `Debug`); the application
is the first `INIT_MF` name of the process. Numbering and sending happen under one lock, so
the Console's per-process sequence check sees lines in order.

Verified 2026-11-12 (ots maxlite, first_demo, normal mode): the Console log shows "Console has
first message" and keeps receiving (lite builds show "No messages received ... Exiting",
because lite's `UDP` destination discards). Known: at the Configure burst the Console
reported `Console missed 117 packet(s)` once per run; the kernel caps socket receive buffers
at `net.core.rmem_max` (212992 here), so the forwarder drops a burst of several hundred
lines. Raising `net.core.rmem_max` (root) removes it; the same limit applies to
messagefacility's UDP destination.

## Visualizer waveforms

Verified 2026-11-12 with first_demo in maxlite: Configure/Start over Gateway UDP, emulator sine
burst over MacroMaker UDP, `getWaveformData` returns `source=live` samples (see
[tutorial-to-visualizer-waveforms.md](tutorial-to-visualizer-waveforms.md), "Doing Steps 1-5
with the helper scripts").

## Configuration: text store

`ARTDAQ_DATABASE_URI=textdb://<path>` selects the `Text` plugin
(`otsdaq/ConfigurationInterface/Text_configInterface.{h,cc}`); any other scheme selects the
`Database` plugin, which maxlite does not build.

```
<path>/tables/<TableName>/v<version>.json   table version as TableView::printJSON writes it
<path>/groups/<GroupName>/k<key>.txt        one "<TableName> <version>" line per member
```

No index files. Custom JSON documents are tables named `JSONDoc_<name>`.

`otsdaq_convert_config_db <from> <to> [--verify] [--dry-run]` copies between `filesystemdb://`
and `textdb://` keeping version and key numbers. It needs the `Database` plugin, so it runs in
an ots lite (or full) installation. Verified 2026-11-12 on the tutorial database: 65 table
versions and 20 group keys, round trip text -> artdaq-database -> text byte-identical.

The maxlite installer still installs the tutorial database in `databases/filesystemdb/` and
creates an **empty** `databases/textdb/test_db`; converting it is an open item. Until then,
convert it once from any lite install of this branch (2026-11-12: 35 table versions, 7 group
keys, verified):

```
cd <lite area> && source setup_ots.sh
otsdaq_convert_config_db filesystemdb://<maxlite>/databases/filesystemdb/test_db \
    textdb://<maxlite>/databases/textdb/test_db --verify
```

Do not use `reset_ots_tutorial.sh` in maxlite: its `get_tutorial_data.sh` accepts only
`filesystemdb://`, and it kills every otsdaq of the user on the host
(see `../ai_dev_skills/tutorial-improvement.md`).

## Open items

- Tutorial database for maxlite (convert once and ship, or convert during install).
- Running ots in maxlite (not yet tried): `tools/ots` still calls TRACE shell commands
  (`tonS`, `trace_cntl`, `setupTrace`) and starts `otsConsoleFwd`; these need maxlite guards.
- Cross compiling for aarch64 and 32-bit ARM (toolchain files, installer `--arch`).
