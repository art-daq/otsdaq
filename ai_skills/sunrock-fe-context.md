# Sunrock FE in its own Context and FE Supervisor

A placeholder front-end plugin (`FESunrockInterface`) for the Sunrock hardware, run
in a separate XDAQ context with its own FE Supervisor so it can be started, stopped
and failed independently of the UDP emulator front-end. Written to be improved
incrementally: each step says what is **done**, and what is still **unverified**
(none of this code had been compiled or run when it was written).

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

## Checks (all unverified)

- `ots` prints `Launching 1 additional local non-Gateway Context procesess`; a second
  `xdaq.exe ... -p 2016` runs and its log is `Data/Logs/otsdaq_quiet_run-<host>-2016.txt`.
- The main context log shows `registered N remote applications of context
  http://<host>:2016`, and the second context's log shows the main context's apps
  registered as remote.
- Gateway status (`GetRemoteAppStatus`) lists `SunrockFESupervisor`; Configure/Start/Halt
  over Gateway UDP take it through the states. Failure text names the remote context
  (`cannot connect to remote context ...`) if the second process is not up.
- `ots_mm_udp_test <ip> 2011` lists `SunrockInterface0`; its FE macros `sunrockStatus`
  and `sunrockSetValue` run; `r`/`w` on `SunrockInterface0` hit the simulated registers.
- Stop the emulator FE (or its context) and confirm the Sunrock FE still configures.

## Known risks / next steps

- Not compiled. Likely first-build problems: the new `postSOAP` socket code in
  `XdaqLite.cc`, and the `FEVInterface` macro usage in the plugin.
- Remote SOAP is a minimal blocking HTTP client (20 minute timeout, one connection per
  message); fine for FSM commands, not tuned for chatty traffic.
- Only the lite executive was changed; the full-XDAQ build already supports multiple
  contexts natively.
- Replace `runZynqCommand()` with real ssh, then replace the simulated register map and
  status with real Zynq reads; decide the streaming path (producer FE + buffer).
