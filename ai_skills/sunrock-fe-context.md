# Sunrock FE in its own Context and FE Supervisor

A placeholder front-end plugin (`FESunrockInterface`) for the Sunrock hardware, run
in a separate XDAQ context with its own FE Supervisor so it can be started, stopped
and failed independently of the UDP emulator front-end. Written to be improved
incrementally: each step says what is **done**, and what is still **unverified**
(verified end to end on 2026-11-10, see Checks).

Related skills: `configgui-udp.md` (all config edits below go through it),
`gateway-udp-fsm.md` (Configure/Start/Halt), `macromaker-udp.md` (FE macros and
registers), `ots-lite-architecture.md`.

## What was added

| Repo | File | Purpose |
|------|------|---------|
| otsdaq-td-ims-asic (new) | `otsdaq-td-ims-asic/FEInterfaces/FESunrockInterface.h`, `FESunrockInterfaceImpl.cc`, `FESunrockInterface_interface.cc`, `CMakeLists.txt` | the plugin (library + loadable plugin) |
| otsdaq-td-ims-asic (new) | `CoreTables/FESunrockInterfaceTableInfo.xml` | config table `FESunrockInterfaceTable` |
| otsdaq | `otsdaq/XdaqLite/ots_lite_main.cc` | lite executive: parse *all* contexts, serve the one on its own `-p` port, register the others' applications as remote |
| otsdaq | `otsdaq/XdaqLite/XdaqLite.cc` | `postSOAP` to a remote context over HTTP (was "not implemented yet") |

The plugin and table live in their own repo, `otsdaq-td-ims-asic`, not in otsdaq-components
or otsdaq data-core: the hardware is project specific. The repo was created locally in
`dev-srcs/otsdaq-td-ims-asic` (branch `develop`, no remote yet). Its top-level
`CMakeLists.txt` is derived from `otsdaq-demo`'s; it depends only on `otsdaq`.

Why the lite changes: before them `ots_lite_main` used only the **first**
`<xc:Context>` in the partition XML and could not send SOAP to another context, so a
second context would have loaded the main context's apps and the Gateway could not
drive its FE Supervisor. `ots` already launches extra local non-gateway contexts
(`xdaq.exe -h <host> -p <port> -e ...`), so only the executive needed work.

## The plugin (placeholders only)

Transport is a placeholder: every Zynq access goes through
`FESunrockInterface::runZynqCommand()`, which only logs the ssh command line it
*would* run (`zynqSshCommandLine()`) and returns `PLACEHOLDER`. The end goal is to
read the Zynq FPGA over ssh; replace that one function (popen of the ssh line, with
`CommandTimeoutSeconds`, throw on failure) and the rest follows.

| Function | Placeholder |
|----------|-------------|
| Monitoring | `pollStatus()` (simulated status), FE macro `sunrockStatus`, `running()` loop polling every `StatusPollPeriodSeconds` |
| Control | FE macro `sunrockSetValue` (inputs `name`, `value`), `universalRead`/`universalWrite` on a simulated 8-byte register map |
| Streaming | `startStreaming()`/`stopStreaming()` only log; no data path. A real stream needs a producer-type FE (see `FEOtsUDPProducerTemplateInterface`) and a data buffer in this context |

`FESunrockInterfaceTable` columns: `InterfaceID` (UID), `ZynqHostName`,
`ZynqSshUser`, `ZynqSshPort`, `CommandTimeoutSeconds`, `StatusPollPeriodSeconds`.

## Configuration to add (after the build, via ConfigGUI UDP)

Names below are the plan; ports and ids must be unused in your system (main context is
2015, UDP remote controls 2010/2011/2014, FE supervisor 210, apps 200-310).

Context group (`testContext`):
- `XDAQContextTable`: record `sunrockContext`: `Status=On`, `Id=1`,
  `Address=http://${HOSTNAME}`, `Port=2016`, `LinkToApplicationTable=XDAQApplicationTable`,
  `LinkToApplicationGroupID=sunrockContextApps`.
- `XDAQApplicationTable`: record `SunrockFESupervisor` in group `sunrockContextApps`:
  `LinkToSupervisorTable=FESupervisorTable`, `LinkToSupervisorUID=SunrockFESupervisor0`,
  `Status=On`, `Class=ots::FESupervisor`, `Id=211` (must be unique), `Instance=1`,
  `Network=local`, `Group=daq`, `Module=${OTSDAQ_LIB}/libCoreSupervisors.so`,
  priorities 0, `LinkToPropertyTable=NO_LINK`.

Configuration group (`defaultConfig`):
- `FESupervisorTable`: record `SunrockFESupervisor0` with
  `LinkToFEInterfaceTable=FEInterfaceTable`, `LinkToFEInterfaceGroupID=SunrockFEGroup`.
- `FEInterfaceTable`: record `SunrockInterface0` in group `SunrockFEGroup`:
  `Status=On`, `FEInterfacePluginName=FESunrockInterface`,
  `LinkToFETypeTable=FESunrockInterfaceTable`, `LinkToFETypeUID=SunrockInterface0`.
- `FESunrockInterfaceTable` (new member of the group): record `SunrockInterface0` with
  `ZynqHostName` (placeholder until the real host is known), `ZynqSshUser`,
  `ZynqSshPort=22`, `CommandTimeoutSeconds=5`, `StatusPollPeriodSeconds=5`.

Then: new Context and Configuration groups, activate both, point the alias at the new
Configuration group via a **new Backbone group** (see `gateway-udp-fsm.md`), restart
`ots`. Use the link/new-row edit types in `configgui-udp.md` (`table-newGroupRow`,
`link-GroupID`, `link-UID`).

## Install and run (user steps)

The lite installer (`ots-quick-lite-start.sh`) hardcodes its four repos (clone loop, the
`mb`/`mz`/`mz_uc` aliases written to `setup_ots.sh`, and the build loop), and
`UpdateOTS.sh --pullall` only knows those four, so the fifth repo is added by hand:

1. Put the repo in the run area: `cp -r <dev-srcs>/otsdaq-td-ims-asic <lite-area>/srcs/`
   (or `git clone` it into `srcs/` once it has a remote).
2. Add `otsdaq-td-ims-asic` after `otsdaq-demo` in the `for p in ...` lists of the `mb` and
   `mz` aliases in `setup_ots.sh`.
3. First build, with the same cmake flags the `mz` alias uses for the other packages
   (after otsdaq has been built and installed):
   `cmake -S srcs/otsdaq-td-ims-asic -B build/otsdaq-td-ims-asic <flags>`, then
   `cmake --build build/otsdaq-td-ims-asic -j && cmake --install build/otsdaq-td-ims-asic`.
   Afterwards `mb` includes it.
4. Copy the new table definition into the run area:
   `cp srcs/otsdaq-td-ims-asic/CoreTables/FESunrockInterfaceTableInfo.xml Data/TableInfo/`
   (the run-area `Data/TableInfo` is a plain copy made at install time).
5. Apply the configuration above, restart `ots`.

An earlier build of the plugin from otsdaq-components may have left
`libFESunrockInterface.so` in `local/install/lib`; the new package installs over the same
file names, so no cleanup is needed.

## Checks (verified 2026-11-10, lite build)

Configuration as applied: Configuration `defaultConfig` key 5 (FEInterfaceTable v3,
FESupervisorTable v3, FESunrockInterfaceTable v0), Context `testContext` key 11
(XDAQApplicationTable v5, XDAQContextTable v4; application Id 211), Backbone key 7
(alias `defaultSystemAlias` -> `defaultConfig` 5). The first version of
FESunrockInterfaceTable was created with `saveSpecificTable` and CSV data (see
`configgui-udp.md`, "First version of a brand-new table").

- `ots` printed `Launching 1 additional local non-Gateway Context procesess` and
  `Verified successful ots non-gateway context <host>-2016 launch`; `xdaq.exe -p 2016` runs,
  log `Data/Logs/otsdaq_quiet_run-<host>-2016.txt`.
- Main context log: `registered 1 remote applications of context http://<host>:2016`;
  Sunrock context log: `registered 8 remote applications of context http://<host>:2015`.
- Gateway `GetRemoteAppStatus` lists `SunrockFESupervisor` (url :2016). Configure over
  Gateway UDP reached Configured for Supervisor, FESupervisor and SunrockFESupervisor
  (remote SOAP works); the 2016 log shows `Done Configuring interface SunrockInterface0`.
- MacroMaker lists `SunrockInterface0`; FE macro `sunrockStatus` returns the placeholder
  status; `w 0x40 0x1234` then `r 0x40` reads back `...1234`; each Zynq access logs the
  ssh command it would run (`would run: ssh ... root@<ZynqHostName> 'devmem 0x40'`).
- 2026-11-11: Configure, Start, Stop and Halt with `ots_drive_udp_test` all reached their
  states (the Gateway reports a state only once every supervisor, including
  `SunrockFESupervisor` on :2016, reached it).
- 2026-11-12, ots maxlite (otsdaq-td-ims-asic cloned into `srcs/`, built, table XML copied to
  `Data/TableInfo`): the configuration was not re-entered but imported from ots-lite-test with
  `ai_scripts/convert_config_store.sh active` (artdaq-database -> text store, `--prepend lite`;
  alias `litedefaultSystemAlias`). After activating the imported Context, the **second** `ots`
  launch started the 2016 context (the first launch still used the old `ots.xml` layout). Both
  contexts registered each other's apps; Configure/Start/Stop/Halt over Gateway UDP; MacroMaker
  lists `SunrockInterface0`, `sunrockStatus` returns the placeholders, `w 0x40 0x1234` reads back;
  an Iterator generated plan (2 runs) ran in both contexts.
- Not yet checked: the Sunrock FE surviving the emulator FE failing.

## Known risks / next steps

- `mz_uc` regenerates `setup_ots.sh` from the installer and drops hand-added exports
  (e.g. the `OTS_*_UDP_*` remote-control variables); re-add them afterwards.
- Remote SOAP is a minimal blocking HTTP client (20 minute timeout, one connection per
  message); fine for FSM commands, not tuned for chatty traffic.
- Only the lite executive was changed; the full-XDAQ build already supports multiple
  contexts natively.
- Replace `runZynqCommand()` with real ssh, then replace the simulated register map and
  status with real Zynq reads; decide the streaming path (producer FE + buffer).
