# From the First Demo Tutorial to Streaming Waveforms in the Visualizer

End-to-end recipe for the lite build: start from the stock demo system and end with
a burst of 16-bit sine samples from the hardware emulator shown by the Visualizer.
Written to be improved incrementally. Each step says what was **verified** on a real
run (lite build, 2026-11-02) and what is still **unverified**; update it as you learn.

Related skills (do not duplicate them here): `configgui-udp.md` (editing config over
UDP), `gateway-udp-fsm.md` (Configure/Start/Halt and status over UDP),
`macromaker-udp.md` and `emulator-registers.md` (register read/write),
`ots-lite-architecture.md` (start/stop, env vars), `development-workflow.md`.

## Data path (demo configuration)

```
ots_udp_hw_emulator :4000  --burst-->  UDPListener0 :4005   (FE supervisor's data manager, DBGroup0)
   -> RAWSaver0 (saves raw file)
   -> UDPStreamer0 :4100  --stream-->  UDPListener1 :4101   (VisualSupervisor's data manager, DBGroup1)
        -> DB1Consumer1 = RawDataVisualizerConsumer  <-- VisualSupervisor getWaveformData
```

The emulator streams to the IP/port written to its stream-destination registers; the
FE writes them from `FEOtsUDPTemplateInterfaceTable` (`StreamToIPAddress`,
`StreamToPort`) during Configure. **`StreamToPort` must equal UDPListener0's
`HostPort` (4005)**, and UDPStreamer0's `StreamToPort` must equal UDPListener1's
`HostPort` (4101). Check with
`ots_cfg_udp_test -y <ip> <port> getSpecificTable tableName=<table>`.

## Prerequisites

1. Lite install from `ots-quick-lite-start.sh` and the demo database (groups
   `testContext` Context, `defaultConfig` Configuration, `TableEditWizBackbone`
   Backbone; alias `defaultSystemAlias` -> `defaultConfig`). The first tutorial
   configuration already contains the whole data path above.
2. Build with the Visualizer live-data change in `otsdaq-utilities`
   (`VisualSupervisor.cc` reads from `RawDataVisualizerConsumer` in lite;
   `libRawDataVisualizerConsumer` is linked in `Visualization/CMakeLists.txt`).
   Without it the Visualizer shows simulated sine waves regardless of the data.
   Standard sequence: `UpdateOTS.sh --pullall; mb`.
3. UDP remote control exports in `setup_ots.sh` (user's choice, off by default):
   `OTS_CONFIG_UDP_IP/PORT` (e.g. 127.0.0.1/2010), `OTS_MACROMAKER_UDP_IP/PORT`
   (127.0.0.1/2011).
4. `kinit` so config edits are attributed to you (`klist`). Never edit anonymously
   without asking the user.

## Step 1 - Configuration changes for the lite build

Do over ConfigGUI UDP (`configgui-udp.md` has the exact edit, saveSpecificTable,
saveNewTableGroup, activateTableGroup sequence; tableList needs a trailing comma).

a. **Gateway UDP state changes** (Context group, GatewaySupervisorTable record
   `Supervisor0`): set `EnableStateChangesOverUDP=Yes`,
   `IPAddressForStateChangesOverUDP=127.0.0.1`, `PortForStateChangesOverUDP=2014`,
   and `EnableApplicationStatusMonitoring=Yes`. Never set the enable flag without
   IP and port: the gateway aborts at start. *Verified.*

b. **Replace the DQM consumer with the visualizer consumer** (Configuration group,
   `DataProcessorTable`, record `DB1Consumer1` in group `DBGroup1`): set
   `ProcessorPluginName=RawDataVisualizerConsumer` and `Status=On`. The stock record
   uses `DemoDQMHistosConsumer`, a ROOT plugin that is not built in lite and makes
   Configure fail (`UnknownPlugin ... DemoDQMHistosConsumer`). The record's link to
   `DQMHistosConsumerTable` is ignored by the visualizer plugin (it reads no config
   table; no `RawDataVisualizerConsumerTable` exists). *Verified.*

c. **Make the alias follow the new Configuration group**: `setGroupAliasInActiveBackbone`
   saves a new `GroupAliasesTable` version, then save and activate a **new Backbone
   group** containing it (`GroupAliasesTable,<ver>,VersionAliasesTable,<ver>,`).
   Without the new Backbone group `Configure,defaultSystemAlias` silently uses the
   old Configuration key. *Verified.*

d. Restart `ots` (groups load at startup). Confirm with the gateway log line
   `Attempting Configure transition with Configuration table group name: defaultConfig, key: <new key>`.

## Step 2 - Start the system

```bash
cd <lite-area> && . setup_ots.sh
ots_udp_hw_emulator 4000 &            # ots never starts it; `ots -k` leaves it running
echo Y | ots -k                       # only if ots is already running
nohup ots > /tmp/ots_run.log 2>&1 &
sleep 25; ss -ulnp | grep -E ':2010|:2011|:2014'   # all three bound = ready
```
*Verified.* (Emulator launch syntax: confirm it matches how the user starts it.)

## Step 3 - Configure and Start the run (Gateway UDP)

```bash
ai_scripts/gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Configure,defaultSystemAlias Configured 120
ai_scripts/gateway_udp_cmd.sh 127.0.0.1 2014 PhysicsRuns0,Start,1 Running 90
ss -ulnp | grep -E ':4005|:4100|:4101'   # the three data-path listeners are now bound
```
Reads/writes of emulator registers through MacroMaker need the system **Configured**
(the front-end does not exist in the list before that). *Verified.*

## Step 4 - Configure the emulator and fire a burst (MacroMaker UDP)

Front-end UID in the demo is `ExampleInterface0`. Values are hex; read-back prints
16 hex digits. Addresses and data are written without needing `0x`.

```bash
mm() { ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 "$@"; }
mm w 0x1004 0x1      # data mode 1 = sine
mm w 0x1005 0x10     # 0x10 = 16-bit samples
mm w 0x1007 0x1      # 1 channel
mm w 0x1001 0x64     # packet count 0x64 = 100 packets (read back counts down to 0)
mm w 0x100000009 0x1 # burst data enable
mm r 0x1001          # poll: remaining packets
```
Register meanings are in `emulator-registers.md`. Writing a new count to `0x1001`
starts another burst while burst-enable stays 1. *Verified:* registers read back what
was written; the counter went 0x64 -> 0 in ~25 s with the listeners receiving.

## Step 5 - Check the data arrived

Gateway log `Data/Logs/otsdaq_quiet_run-gateway-*.txt`:
- `UDPListener0 ... Received data` and `UDPListener1 ... Received data` counts match
  (68 and 68 in one check) => the streamer forwarded everything.
- `RawDataVisualizerConsumerImpl.cc ... DB1Consumer1 running` and per-packet
  `DB1Consumer1 UID: VisualSupervisor` => the consumer handled packets. *Verified.*

## Step 5b - Put the Visualizer window in waveform mode (Context group)

`Visualization.html` only launches the waveform viewer when its window parameter
`mode` is `waveform`. Add it as a Desktop window parameter on the Visualizer icon
(`DesktopIconTable` -> `DesktopWindowParameterTable`), using the "Editing links and
adding rows" section of `configgui-udp.md`. Then activate the new Context group and
restart ots. *Verified:* the tree read-back shows the Visualizer icon resolving to
`mode=waveform`; **unverified:** what the opened window shows.

## Step 6 - See it in the Visualizer window

Open `http://<host>:2015/urn:xdaq-application:lid=200/`, log in, and open the
Visualizer window from the desktop; fire a burst with a large count (e.g. `w 0x1001
0x3E8`) while it is open. **Unverified:** what the window displays.

**Server side verified** (ots maxlite, 2026-11-12): `getWaveformData` is in the
VisualSupervisor's no-login request list, so it can be checked without a browser while
a burst runs:

```bash
curl -s -X POST "http://localhost:<port>/urn:xdaq-application:lid=270/Request?RequestType=getWaveformData&numChannels=1&bitWidth=16" -d ""
```
Live data replies `<source value='live'/>` with `packetCount`/`packetSeq` rising between
calls and `<channel_0 value='...'/>` holding the samples (simulated data has no
`source=live`). With `0x1008=0x40` (period 64), `0x1009=0x32` and `0x100A=0x32`
(50% amplitude and offset) one packet gives 745 samples from 16383 to 49151 around 32767.
With offset 0 the negative half reads 0 (clipped), which is not an error.

The Desktop icon must resolve to `Visualization.html?urn=270&mode=waveform` (check with
`ots_drive_udp_test <ip> <port> raw GetRemoteDesktopIcons`). A Visualizer window opened
from a desktop loaded before the Context change still runs the ROOT viewer (it requests
`getDirectoryContents` and logs "Directory browsing with ROOT file support is not
available in the otsdaq lite build"): reload the desktop page and reopen the window.

## Doing Steps 1-5 with the helper scripts (verified in ots maxlite, 2026-11-12)

All edits as the Kerberos user with `ai_scripts/cfg_udp_edit_group.sh` (it chains the
temporary versions, saves each table once, saves and activates one group):

```bash
S=<otsdaq>/ai_scripts
$S/cfg_udp_edit_group.sh -c "Gateway UDP + Visualizer mode=waveform" \
  GatewaySupervisorTable/Supervisor0/IPAddressForStateChangesOverUDP=127.0.0.1 \
  GatewaySupervisorTable/Supervisor0/PortForStateChangesOverUDP=<gw udp port> \
  GatewaySupervisorTable/Supervisor0/EnableStateChangesOverUDP=Yes \
  GatewaySupervisorTable/Supervisor0/EnableApplicationStatusMonitoring=Yes \
  DesktopIconTable/Visualizer/LinkToParameterTable@link-GroupID=DesktopWindowParameterTable,Visualizer_params \
  DesktopWindowParameterTable/Visualizer_param0/windowParameterGroupID@table-newGroupRow=windowParameters,Visualizer_params,Visualizer_param0 \
  DesktopWindowParameterTable/Visualizer_param0/windowParameterKey=mode \
  DesktopWindowParameterTable/Visualizer_param0/windowParameterValue=waveform
$S/cfg_udp_edit_group.sh -t Configuration \
  DataProcessorTable/DB1Consumer1/ProcessorPluginName=RawDataVisualizerConsumer
$S/cfg_udp_edit_group.sh -t Backbone GroupAliasesTable/defaultSystemAlias/GroupKey=<new Configuration key>
# restart ots, then
ots_drive_udp_test 127.0.0.1 <gw udp port> PhysicsRuns0 Configure defaultSystemAlias
ots_drive_udp_test 127.0.0.1 <gw udp port> PhysicsRuns0 Start
```
The Backbone edit replaces Step 1c's `setGroupAliasInActiveBackbone` (same result: a
new `GroupAliasesTable` version in a new Backbone group). Then Step 4 with
`ots_mm_udp_test` and the check in Step 6.

## Failure quick reference

| Symptom | Cause / fix |
|---------|-------------|
| `No Front-ends found in FE list` (ots_mm_udp_test) | system not Configured, or ots restarted back to Initial |
| Configure fails, `Timeout ... 127.0.0.1 : 4000` | emulator not running |
| Configure fails, `UnknownPlugin DemoDQMHistosConsumer` | Step 1b not applied / not loaded (restart, check alias key) |
| Configure still uses the old group key | Step 1c Backbone group missing, or ots not restarted |
| `w` prints `Wrote` but read-back unchanged | seen once before a dev fix to how the write data is parsed; pull and rebuild (`UpdateOTS.sh --pullall; mb`), then always read back after writing |
| Gateway aborts at start `Invalid IP:Port ... DEFAULT:0` | Step 1a flag set without IP/port; revert the active Context key (`revert_active_table_group.sh`) |
| No status / `Not Monitored` | `EnableApplicationStatusMonitoring` off |

## Known gaps / ideas to improve this skill

- Step 6: the window display itself is still unverified (server side verified).
- `ots-lite-architecture.md` still says `RawDataVisualizerConsumer` needs full artdaq;
  that is out of date now that it is built and linked in lite.
- A single script that performs Steps 3-5 (Configure, Start, set registers, burst,
  count packets in the log) would make this repeatable.
- Document how to Stop/Halt cleanly between runs and how to change channel count /
  data mode (`0x1004=2` multi-channel) and the visible effect.
- Add the exact `ots_udp_hw_emulator` launch line used by the tutorial.
