# ots maxlite (OTS_MAXLITE)

ots maxlite is ots lite (see [ots-lite-architecture.md](ots-lite-architecture.md)) reduced for
FPGA SoC embedded deployments: the whole of ots (Gateway, Configuration GUI, MacroMaker, FE
Supervisors, web desktop) without TRACE, messagefacility, cetlib, fhiclcpp, Boost,
artdaq-database, ZeroMQ, CURL or Python.

Branch: `feature/ots-maxlite` in otsdaq, otsdaq-utilities, otsdaq-components, otsdaq-demo
(otsdaq-td-ims-asic: `develop`). As of 2026-11-12 the branch exists only in the developer's
local clones (not on GitHub), so installs use `--repo-base <local dir with the repos>`.

Status 2026-11-12, all on x86_64 (first_demo tutorial): install and compile, Wiz mode, normal
mode, ConfigGUI/MacroMaker/Gateway UDP remote controls, configuration edits saved to the text
store, Configure/Start, emulator data to the Visualizer (`getWaveformData` live samples), web
Console receiving. State machine: Configure/Start/Pause/Resume/Stop/Halt/Shutdown/Startup, and Startup
applying a Gateway UDP port change (see `gateway-udp-fsm.md`). Two contexts in two processes (Sunrock FE,
`sunrock-fe-context.md`); the Iterator (generated plan over Gateway UDP). ARM: cross-compiled for aarch64 and 32-bit ARM (armhf), see "Cross compiling
for ARM". Not yet: running on an ARM board.

## Install

```
mkdir ots-maxlite && cd ots-maxlite
cp <otsdaq-demo>/tools/ots-quick-maxlite-start.sh . && chmod 755 ots-quick-maxlite-start.sh
./ots-quick-maxlite-start.sh --branch feature/ots-maxlite --repo-base <dir with the repos, or omit for GitHub>
source setup_ots.sh
```

`setup_ots.sh` exports `OTS_LITE=1` and `OTS_MAXLITE=1`; `mb`, `mz` and `mz_uc` work as in lite
(`mz_uc` reruns `ots-quick-maxlite-start.sh`). The installer:

- checks only for OpenSSL (`openssl/sha.h`), Threads and Xerces-C (builds Xerces-C without ICU
  if the OS has none) and the shell tools; it downloads cmake 4.4.4 if the system cmake is
  older than 3.29;
- builds cetmodules (CMake functions only) and the otsdaq repos; no cet libraries, TRACE,
  artdaq-database, ZeroMQ or Catch2;
- installs the first_demo tutorial `Data/` and its database in `databases/filesystemdb/`
  (artdaq-database format), and creates an empty text store `databases/textdb/test_db`, which
  `ARTDAQ_DATABASE_URI` points at (see "Configuration: text store" to fill it).

## First run, step by step (verified 2026-11-12)

On a host where another otsdaq may run, give maxlite its own ports first; `ots -k` only stops
processes of this area (`$USER_DATA/` in their command line), but the emulator port and the
data ports of the tutorial configuration (4000, 4001, 4005, 4100, 4101) are fixed in the
database, so only one system can use them at a time.

```bash
cd ots-maxlite
# in setup_ots.sh (user's choice, off by default): own main port and UDP remote controls
#   export OTS_MAIN_PORT=5015
#   export OTS_CONFIG_UDP_IP=127.0.0.1;     export OTS_CONFIG_UDP_PORT=5010
#   export OTS_MACROMAKER_UDP_IP=127.0.0.1; export OTS_MACROMAKER_UDP_PORT=5011
# fill the text store from the tutorial database (from any ots lite install of this branch):
( cd <lite area> && source setup_ots.sh && otsdaq_convert_config_db \
    filesystemdb://$PWD/../ots-maxlite/databases/filesystemdb/test_db \
    textdb://$PWD/../ots-maxlite/databases/textdb/test_db --verify )
source setup_ots.sh
ots -w                                   # Wiz mode: prints the URL with the access code
ots_cfg_udp_test -y 127.0.0.1 5010 getActiveTableGroups   # reads the text store
echo Y | ots -k
ots                                      # normal mode
ots_udp_hw_emulator 4000 &
```

For the waveform demo (Gateway UDP, `RawDataVisualizerConsumer`, `mode=waveform`) follow
[tutorial-to-visualizer-waveforms.md](tutorial-to-visualizer-waveforms.md), "Doing Steps 1-5
with the helper scripts"; use a Gateway UDP port of your own (5014 was used).

Check the run-time libraries of an install with
`../ai_scripts/check_maxlite_libraries.sh <install>/local/install`.

Verified 2026-11-12 (x86_64, AlmaLinux 9, 24 cores, gcc-toolset-13): clean install including the
compile of all repos in **117-140 s** (xerces-c 16 s when the OS lacks it, cetmodules 0 s,
otsdaq 67-81 s, otsdaq-components 10 s, otsdaq-utilities 18-22 s, otsdaq-demo 3 s). ots lite
from the same branch on the same machine: 276 s. Installed size: 88 MB (`local/install`,
unstripped; libraries 23 MB).

## Cross compiling for ARM (aarch64, arm32)

`ots-quick-maxlite-start.sh --arch aarch64|arm32` compiles the otsdaq repos for the target with
the Debian/Ubuntu multiarch cross compilers (`aarch64-linux-gnu-g++`, `arm-linux-gnueabihf-g++`)
and the target's `libxerces-c-dev` and `libssl-dev` (`apt install libxerces-c-dev:arm64
libssl-dev:arm64`, `:armhf` for arm32). It writes `tmp/toolchain-<triplet>.cmake` and uses it for
the prerequisite probe, a Xerces-C built from source, the otsdaq repos and `mb`/`mz` in
`setup_ots.sh`; cetmodules stays a build-host package. `--toolchain <file>` uses another CMake
toolchain file instead (e.g. from a PetaLinux/Yocto SDK).

On an x86 host without those packages (e.g. AlmaLinux), do it in a Debian 12 container:

```bash
ai_scripts/maxlite_cross_build.sh aarch64 ~/ots-maxlite-arm64 --repo-base <dir with the repos>
ai_scripts/maxlite_cross_build.sh arm32   ~/ots-maxlite-armhf --repo-base <dir with the repos>
```

It builds the image from `otsdaq-demo/tools/ots-maxlite-cross.Containerfile` once (rootless
podman; works without `/etc/subuid` entries and with SELinux enforcing, see the script header),
runs the installer with `--arch`, runs `check_maxlite_libraries.sh`, and prints the command to run
`mb`/`mz` in the container later.

Verified 2026-11-12 (AlmaLinux 9 host, Debian 12 gcc 12.2 cross compilers): clean installs
including the build in ~165 s each (otsdaq ~110 s, utilities ~30 s); programs are
`ELF 64-bit ARM aarch64` / `ELF 32-bit ARM EABI5` and need only libc, libstdc++, libgcc_s, libm,
`libxerces-c-3.2.so` and `libcrypto.so.3`; installs 78 MB (aarch64) and 71 MB (armhf),
unstripped. Not yet run on ARM (no board or emulator was used).

What the ARM builds found (fixed in all builds, commits a8867487 and following):
- `printf`/`scanf` of `uint64_t` with `%lu`/`%ld`/`%lX` (`long` is 32-bit on armhf: wrong values,
  and `scanf` wrote 8 bytes into 4): now `PRIu64`/`SCNu64`/`PRIX64` from `<cinttypes>`; `size_t`
  with `%zu`. WebUsers (user and session files), FEVInterface, GatewaySupervisor, Iterator, Chat.
- `char` is unsigned on ARM: a 161..255 range test in `StringMacros` now compares `unsigned char`
  (on x86 its `%d` print gave negative numbers).
- The emulator's counter mode copied 8 bytes from a 4-byte `time_t` on 32-bit.
- gcc 12 (the Debian cross compilers): standard headers that gcc 13 pulled in indirectly
  (`<iomanip>`, `<algorithm>`, `<optional>`, `<functional>`) are now included where used; a
  maybe-uninitialized variable in `TableView`; gcc 12's false `-Wrestrict` on
  `"literal" + std::string` (GCC bug 105651) is kept a warning by the toolchain file
  (`-Wno-error=restrict`).

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
| Web Console messages | messagefacility `UDP` destination (lite's stand-in discards them) | `OtsLog` sends them (see "Web Console") |
| `ots -t*` TRACE options, `tonS`, `trace_cntl`, `TRACE_FILE` | TRACE shell functions | not available; the `ots` script skips them when absent |
| Database URI types accepted by `ots` | `filesystemdb`, `mongodb` | also `textdb` |
| Plugins of other repos (FE interfaces, tables) | link cetlib/fhiclcpp | link nothing extra; same `cet_build_plugin` CMake |

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

## Xerces-C thread safety (fixed 2026-11-12, all builds)

The Gateway crashed (SIGSEGV in `xercesc::XMLMutex::lock` from `XmlDocument::initDocument`)
after ~20 min of a browser polling the Visualizer. Every `XmlDocument` (one per HTTP reply),
`TableInfoReader` and `ConfigurationHandler` calls `XMLPlatformUtils::Initialize()`/`Terminate()`,
whose reference count Xerces does not protect, and XdaqLite answers each HTTP connection in its own
thread: concurrent replies corrupted the count, it reached zero and Xerces freed its global
mutexes. All calls now go through `ots::XercesPlatform::initialize()/terminate()`
(`otsdaq/XmlUtilities/XercesPlatform.h`, one process-wide lock). Verified: 152181
`getWaveformData` requests from 16 parallel clients in 60 s, all HTTP 200, Gateway alive.

## Configuration export and import

How-to between store types: [config-store-conversion.md](config-store-conversion.md) and
`../ai_scripts/convert_config_store.sh`. Verified 2026-11-12 in maxlite (scratch stores, never the running one):

- `otsdaq_import_system_aliases.sh EXPORT_ONLY <URI> <USER_DATA>` exports the active groups and
  system aliases to `./tmp/export_system_aliases/<Group>_<key>/<Table>_v<N>.json` (3 groups, 26
  files from the tutorial store). The export is format-neutral: a `textdb://` export imports
  into an artdaq-database store with lite tools, and a lite (`filesystemdb://`) export imports
  into a maxlite text store; both directions verified.
- `otsdaq_import_groups_from_export_path <export dir> [prepend]` imports into the current
  `ARTDAQ_DATABASE_URI`/`USER_DATA`: either merging into valid active groups (new keys, aliases
  kept), or into a fresh store when `Data/ServiceData/ActiveTableGroups.cfg` is removed (it then
  creates an empty Backbone, imports, and remaps the aliases to the new keys). All 22 imported
  tables matched the exported rows. A target whose active groups file names groups that are not
  in its database is refused ("Did not find valid active groups ...") by design.
- `ots_table_download_csv <table> <version> <file>` / `ots_table_upload_csv <table> <file>`:
  an unchanged CSV is recognized as the existing version (no new version), a changed one is
  saved as the next version. The upload needs `OTS_KCACHE_USER` (set it from `klist`, as
  `otsdaq-utilities/tools/ots_table_update_cells.sh` does); the saved AUTHOR is the tool's
  `export_admin`, in all builds.

Fixed on the way (all builds): `XDAQContextTable.cc` was compiled twice (its library and, by
`cet_build_plugin`'s default `<name>.cc`, an implementation library of its plugin), and the two
copies of a static map were destroyed twice at exit (`double free or corruption` after a fresh
import in maxlite). The source is now `XDAQContextTableImpl.cc`, like `DesktopIconTableImpl.cc`.

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

- Tutorial database for maxlite (convert once and ship a text copy, or convert during install).
- Run the ARM builds on a board (copy `local/install`, `Data`, the text store and the
  `otsdaq-utilities/WebGUI` web files; a packaging script is planned), measure footprint/RAM.
- ots lite from this branch: builds (2026-11-12); not yet run in Wiz/normal mode. Full (Spack)
  build not compiled.
- Console burst loss at Configure (kernel `rmem_max`, see "Web Console").
- Console receive thread ended once (16:35:27 on 2026-11-12, during an Iterator Stop) with an
  exception that is not a `std::runtime_error` ("Unknown error caught at Console Supervisor
  thread"); the Console then receives nothing until ots restarts. Suspect the message parser
  (`ConsoleMessageStruct`, rewritten without Boost) on one line; not yet investigated.
