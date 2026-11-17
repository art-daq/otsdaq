# Slow controls to EPICS: from a polled register to the Controls Dashboard (Mu2e)

How a value read by a front-end (FE) slow-controls channel becomes an Experimental Physics and Industrial Control
System (EPICS) process variable (PV) that the Slow Controls Dashboard (plugin `EpicsInterface`) shows, at Mu2e.
Written 2026-11-16 from the code, the Mu2e calo configuration and the DCS files. **Status (2026-11-16 run in HWDev):
confirmed Configure with alias `tutorial_calo14_emulated` (after one Failed Initialize, cleared with Halt), the
`tutorialTemperature` record in `hwdev_otsdaq_dtc-ai.dbg` (387 records), and the fake temperature changing on
Macro Maker reads of 0x3E8 on `caloDTC26`. Not confirmed: the value updating live in the dashboard (headless
screenshots of the dashboard fail, see below).** Tutorial: otsdaq-mu2e/doc/mu2e-tutorials.

Related: `configgui-udp.md` (editing tables and groups), `gateway-udp-fsm.md` (Configure over UDP),
`../ai_scripts/slow_controls_epics_check.sh` (read-only check of the EPICS-side files).

## Two paths that meet at the PV name

1. **Values** (every sample): `FEVInterface::slowControlsRunning()` (otsdaq/FECore/FEVInterface.cc:318) ticks once
   per second and reads a channel when `timeCounter % DelayBetweenSamplesInSeconds == 0` (:484; minimum 1 s,
   FESlowControlsChannel.cc:64). It applies `Transformation` (a TFormula, e.g. `x/100`) and calls
   `metricMan->sendMetric(<interfaceUID>:<ChannelName>)` (:612/628). The artdaq `epics` metric plugin
   (artdaq-epics-plugin epics_metric.cc) turns `.` into `:`, prepends its `channel_name_prefix` parameter and does
   `ca_put`. **If no IOC serves that PV, the value is dropped after one warning.**
   Configured in `FESupervisorTable`: `SlowControlsMetricManagerChannelNamePreamble` (calo: `calo`) and
   `daqMetricsLink` -> `epicsMetric` (plugin `epics`) -> parameter `channel_name_prefix` (calo: `Mu2e`).
2. **The channel list** (on every Configure of the dashboard supervisor): `EpicsInterface::configure()`
   (otsdaq-epics EpicsInterface_slowcontrols.cc:1844-2199), for each table in
   `SlowControlsChannelSourceTableList` (Mu2e: `DTCInterfaceTable`):
   - regenerates `$OTSDAQ_EPICS_DATA/${MU2E_OWNER}_otsdaq_dtc-ai.dbg` (and `.csv`)
     (DTCInterfaceTable_table.cc:11-15, SlowControlsTableBase.cc:261-405); if the text changed, rewrites the file
     and writes `1` to `dirtyFlag.txt`;
   - inserts or updates the PV in the archive database tables `channel` and `num_metadata` (only adds/updates;
     never deletes), and subscribes to it. The dashboard's channel list is `SELECT name FROM channel`.
3. **DCS side (outside otsdaq):** the OTSReader soft IOC loads the `.dbg` files listed in
   `/home/mu2edcs/mu2e-dcs/apps/OTSReader/ioc/OTSData.iocsh` with `detector=Mu2e`. A DCS cron job (`run-dcs.sh`, every
   2 minutes) restarts the IOC when `db/dirtyFlag.txt` is `1`. Only then does the new PV exist and step 1's `ca_put`
   succeed.

## The PV name rule (all three must match)

| Where | Name |
|---|---|
| IOC record (`subst_ai.dbt`) | `$(detector):$(Subsystem):$(loc):$(pvar)` = `Mu2e:<preamble>:<DTC or ROC UID>:<ChannelName>` |
| Archive DB / dashboard list | `<OTS_EPICS_OWNER, else OTS_OWNER>:<preamble>:<UID>:<ChannelName>` (SlowControlsTableBase.cc:171-215) |
| FE metric (`ca_put`) | `<channel_name_prefix>:<preamble>:<UID>:<ChannelName>` |

- Set `OTS_EPICS_OWNER=Mu2e` (Mu2e setups do). If it is unset, `OTS_OWNER` (e.g. `Mu2e-Calo`) is used and the list
  gets names that no IOC serves: the dashboard lists them but they never update (the old `Mu2e-Calo:calo:...` rows).
- `MU2E_OWNER` must be the lower-case name the IOC file loads (`hwdev_otsdaq_dtc-ai.dbg`, `calo_otsdaq_dtc-ai.dbg`):
  `HWDev` wrote `HWDev_otsdaq_dtc-ai.dbg`, which the IOC never loaded. The Mu2e setup now exports
  `MU2E_OWNER=${subsystem,,}`.
- Different subsystems use different preambles, so their PVs do not collide. Two configurations that generate the
  same records into two loaded files define the same record twice.

## What goes into the `.dbg` file (and what can drop out)

`DTCInterfaceTable::slowControlsHandlerConfig` (DTCInterfaceTable_table.cc:52-182) walks every `FESupervisorTable`
row (whether or not its context runs), takes each FE with **Status On** and plugin `DTCFrontEndInterface`, its
`LinkToSlowControlsChannelTable` channels, then **every** ROC under it (ROC Status is not checked) with the ROC's
channels. Only analog channels go to the archive list; `1b` channels go to the file only.
- Configuring with a configuration where a DTC is Off removes that DTC's and its ROCs' records from the file; after
  the IOC restart they are not served until a Configure with them On. Archive history stays.
- Polling is paused while Running (DTCFrontEndInterfaceImpl.cc:3955-3970 Start, :4188-4200 Stop): values update in
  Configured and Stopped only.
- `Mu2eGlobalsTable.GlobalSkipCFOandDTCConfigureSteps=Yes` makes `DTCFrontEndInterface::configureSlowControls`
  return at once (DTCFrontEndInterfaceImpl.cc:1786): **no DTC/ROC slow controls at all** with a "skipConfig" alias.

## Adding a ROC register as a channel

1. A row of `FESlowControlsTable`: `FEGroupID` (the group the ROC links to), `ChannelName` (UID, the last part of
   the PV), `Status On`, `ChannelDataType` (`16b` for a ROC register), `Units`, `UniversalInterfaceAddress`,
   `Transformation` (`x` or e.g. `x/100`), `ReadAccess Yes`, `DelayBetweenSamplesInSeconds` (e.g. `5`),
   `MonitoringEnabled Yes`.
2. The ROC row of `ROCInterfaceTable`: `LinkToSlowControlsChannelTable`/`GroupID` -> that group (calo ROCs share
   `SidetROC0`; a new channel there appears for every ROC, so use a new group for one ROC).
3. The DTC row of `DTCInterfaceTable`: `SlowControlsEnable On`.
4. Save table versions, a new **Configuration** group key, point an alias at it (`cfg_udp_set_alias.sh`), Configure.
   Every step at once with `cfg_udp_edit_group.sh -t Configuration` (dry run with `-n`).

## Software-emulated DTC and ROC (no hardware)

- `DTCInterfaceTable.EmulatorMode=True` -> `DTC_SimMode_Performance` -> the `mu2esim` simulator
  (CFOandDTCCoreVInterface.cc:95, DTCFrontEndInterfaceImpl.cc:6940/7120, mu2e-pcie-utils mu2edev.cpp:89).
  DTC registers read random values (`CFOandDTCCoreVInterface::universalRead`, :408-414); writes are ignored.
- The ROC: `ROCInterfaceTable` `Status On`, `EmulateInDTCHardware True` (otherwise configure waits for `0x1234`,
  ROCCalorimeterInterface_interface.cc:1222-1250). In `mu2esim` every 16-bit ROC address is a read/write register
  starting at 0 (mu2esim.cpp, `rocRegisters_`): write it with Macro Maker, read it back.
- **Fake temperature (added 2026-11-16, mu2e-pcie-utils):** ROC address **1000** (`fakeTemperatureRocAddress_`)
  starts at 2500 (25.00 degC in 0.01 degC) and moves by a random step of -5..+5 on every read, kept within
  1500..3500. A write sets it and the walk continues from there. Channel: address 1000, `16b`, `Transformation
  x/100`, `Units degC`.

## Calo and HWDev at Mu2e (2026-11-16)

- Calo: Gateway mu2e-cfo-01:3125, ConfigGUI UDP 3120, dashboard context `slowControl` (port main+4). calo-14 context
  `calo_14_FEContext`, supervisor `Calo14FESupervisor`, DTCs `caloDTC26` (ROCs `caloROC156`-`161`) and `caloDTC27`.
- HWDev (Gateway mu2e-cfo-01:3155, ConfigGUI UDP 3150 set in its `.rte`) shares the calo MongoDB, so it can
  activate calo's groups in its own `ActiveTableGroups.cfg`. Its IOC file is `hwdev_otsdaq_dtc-ai.dbg`. Use it for
  experiments: its generated records never touch the calo file. Done 2026-11-16:
  1. HWDev's `TableInfo` made identical to `Data_calo/TableInfo` (`UpdateOTS.sh --tables` gives only the
     `otsdaq-mu2e` definitions, not the calorimeter ones). **A running Configure app keeps the definitions it loaded
     at start**: restart `ots -w` after changing `TableInfo`, or saves fail with *Can't find column named ...*.
  2. In Wiz mode, activated calo's Context 65, Backbone 577, Configuration 350 through HWDev's port (Configuration
     with `ignoreWarnings=1`: calo's `SubsystemCalorimeterParametersTable` v16 has a repeated `DEFAULT` UID).
  3. `HWDevCalo14TutorialContext` key 0: only `MainContext`, `slowControl`, `calo_14_FEContext` On (calo's Context
     also launches FE contexts on calo-01..04, whose FE supervisors would open the DTC cards calo's ots uses).
  4. `HWDevCalo14TutorialConfig` key 0: all calo DTC FEs Off except `caloDTC26` (EmulatorMode, SlowControlsEnable
     On), `caloROC156` On + EmulateInDTCHardware with group `tutorialROCSlowControls` -> channel
     `tutorialTemperature` (address 1000, 16b, x/100, degC, 5 s), `Calo14FESupervisor` preamble `hwdev` (PVs
     `Mu2e:hwdev:...`), `GlobalSkipCFOandDTCConfigureSteps=No`, `GlobalOperatingMode=HardwareDevMode`.
  5. Alias `tutorial_calo14_emulated` in `MC2CaloBackbone` key 655 (active in HWDev only; calo stays on 577).
  Save and activate through the HWDev port only: `cfg_udp_edit_group.sh` activates what it saves, and `-g <name>`
  keeps HWDev groups apart from calo's.

## Checking each hop

| Hop | How |
|---|---|
| FE polls | FE context log (`otsdaq_quiet_run-<host>-<port>.txt`): slow-controls loop, channel reads |
| PV in the IOC file | `ai_scripts/slow_controls_epics_check.sh <owner> <channel>`: file, time, dirty flag, IOC loads it, matching records |
| IOC restarted | `$OTSDAQ_EPICS_DATA/dirtyFlag.txt` (shared by all owners) no longer `1` after the cron run (it was absent on 2026-11-16; when it is cleared is not confirmed) |
| Dashboard | **Edit Attributes** channel search; **Go Live!** (see the otsdaq-web EPICS tutorial) |

## Walking the GUI headless (ff_screenshot.py)

- Desktop icons open on `click` of `a.DesktopIcons-iconLink` (not dblclick).
- State Machine draws states on a canvas: send transitions with
  `StateMachineXgiHandler?StateMachine=<Initialize|Configure|Halt|Start>&fsmName=OtherRuns0`.
- Macro Maker: FE list items are `selbox-box1-option_N`; address field `addressInput`; read with the iframe's
  `callRead()` (buttons are `<button>`, text "Read").
- The dashboard is served by the `slowControl` context (another port): in a headless browser its iframe cannot get
  the session from the Desktop and the window closes in about 1 s ("Login has expired"). Take dashboard pictures by
  hand.
