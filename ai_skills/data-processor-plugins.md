# Data processor plugins: producers and consumers

A **data processor** is a plugin that takes part in the data path of a Data Manager: a **Producer** puts packets into a Data
Buffer (from a socket, hardware, a file, a generator), a **Consumer** takes them out (saves, forwards, decodes, shows). This page
covers what exists, the buffer rules that decide behaviour, how to configure processors as an agent, and how to write a new
one. The table chain that links managers, buffers and processors, file names and run numbers are in
`data-path-configuration.md`; the emulator that feeds the chain in `emulator-registers.md`. Script: `ai_scripts/new_data_processor.py`.
*Verified* = run on ots maxlite (2026-11-15); the rest is read from the source.

## The plugins that exist

| Plugin (`ProcessorPluginName`) | Role | Settings table (columns) | Notes |
|---|---|---|---|
| `UDPDataListenerProducer` | producer | `UDPDataListenerProducerTable`: `BufferSize`, `HostIPAddress`, `HostPort` (+ optional `SocketReceiveBufferSize`, default 256 MB) | one UDP packet = one buffer entry; header map gets `IPAddress`, `Port`. The hardware emulator streams to it |
| `TCPDataListenerProducer`, `TCPDataReceiverProducer` | producer | `TCPDataListenerProducerTable`, ... | TCP listener (accepts clients; reports `clients`, `socketBacklogBytes`) / receiver |
| `RawDataSaverConsumer` | consumer | `RawDataSaverConsumerTable`: `FilePath`, `RadixFileName`, `MaxFileSize` | writes packets back to back to `<FilePath>/<Radix>_Run<n>[_<sub>]_Raw.dat`; opens at Start, closes at Stop |
| `OtsDataSaverConsumer` | consumer | same table | as above plus one byte (quad-word count) before each packet and a sequence-gap warning |
| `UDPDataStreamerConsumer`, `TCPDataStreamerConsumer` | consumer | `UDPDataStreamerConsumerTable`: `HostIPAddress`, `HostPort`, `StreamToIPAddress`, `StreamToPort` | forwards every packet to another address: this is how the second Data Manager (the Visualizer's) gets data |
| `RawDataVisualizerConsumer` | consumer | `DQMHistosConsumerTable` row (no own table) | keeps the latest packet for the Visualizer's waveform view (lite/maxlite: no ROOT) |
| `DemoDQMHistosConsumer` | consumer | `DQMHistosConsumerTable` | ROOT histograms: full build only |
| `DataDecoderConsumer` | consumer | | decodes packets (template for decoders) |
| `ARTDAQProducer`, `ARTDAQConsumer` | both | | one artdaq Board Reader on the buffer's write/read side: full build only |

The plugin name is the C++ class name; `DataProcessorTable` lists suggestions but accepts any name (`arbitraryBool=1`).

## What the Data Manager does with them

From `DataManager::configure()` and the buffer functions:
- A **buffer** (`DataBufferTable` row) must have `Status` On; unless the application has front ends, it needs **at least one
  active Producer**, else Configure fails ("there must be at least 1 Producer"). Rows with `Status` Off are skipped; a
  `ProcessorType` other than `Producer`/`Consumer` throws.
- Each processor is built by name through the plugin loader (`MakeDataProcessor`), is registered to its buffer, and gets
  `configure()` at the Configure transition.
- **Start** starts every consumer of the buffer first, then the producers; **Stop** stops the producers first, then the consumers.
  **Pause** calls `pauseProcessingData()` (by default `stopProcessingData()`: the work loops end) and **Resume** calls
  `startProcessingData("")`, a start with an *empty run number*, which a plugin must treat as "continue the run, do not reset".
  The buffers are destroyed and rebuilt by the next Configure (`DataManager::configure()` begins with `destroyBuffers()`).
- **Bug found and fixed (needs `mb`):** `RawDataSaverConsumerBase` closed its file in `stopProcessingData()` and so on Pause, and a
  Resume (empty run number) did not reopen it: *verified* on the running install, with the emulator still streaming the file stayed
  at 671,990 bytes after Resume until Stop, i.e. **everything after a Resume was lost**. The fix overrides `pauseProcessingData()` to stop
  the loop but keep the file open. **Verified after a rebuild (2026-11-15, `dev-skills`)**: the same scenario grew the file from
  35,760 to 65,560 bytes after Resume and the packets decode. Until your install has it, do not Pause a run that must be
  saved; if you write a saver, do the same.
- Each producer owns a ring of `BufferSize` **sub-buffers**; a packet is a `std::string` plus a header `map<string,string>`.
  **Every consumer of the buffer receives every packet** (own read position; *verified*: two `RawDataSaverConsumer`s on one buffer
  wrote byte-identical files of 673,480 bytes).
- A sub-buffer can be reused by the producer only after **all** consumers have released it. So a **slow consumer stalls the
  producer**: the producer logs "There are no available buffers! Retrying...", the UDP listener waits 10 ms, and the kernel's
  socket buffer then overflows and drops packets. Gaps show up as jumps in the 1-byte packet sequence
  (`read_raw_data.py` prints "Sequence gaps"). `ConsumerPriority` (`Low`/`High`) exists, but `CircularBufferBase::registerConsumer`
  registers every consumer as high priority: do not rely on a low-priority consumer being skipped.
- Counters: `DataProcessor` keeps packets, bytes, errors and busy time (`countPacket`, `countError`); the Visualizer's
  `getDataStatus` request reports them, but it is compiled out of lite and maxlite builds. There, check the data path by file
  growth, `read_raw_data.py`, and the Visualizer's `getWaveformData` (`tutorial-to-visualizer-waveforms.md`).

## Using a plugin (an agent's workflow)

A new processor instance is two rows in the **Configuration** group: a settings row in the plugin's table, and a
`DataProcessorTable` row that names the plugin, its buffer group and links to the settings row. *Verified* sequence (adds a second
saver to buffer `DB0`):
```bash
# 0. edit from the Configuration key the alias uses (aliases: cfg_udp_list_aliases.sh): if it is not the active key,
#    add `-k <that key>` to the command of step 1 (no need to activate it first)
# 1. both rows, one new group key (-n first shows the result; -w accepts the tree warnings a Context/Configuration may have)
ai_scripts/cfg_udp_edit_group.sh -w -t Configuration -g defaultConfig \
  "+RawDataSaverConsumerTable/RAWSaver1" "RawDataSaverConsumerTable/RAWSaver1/FilePath=\${OTSDAQ_DATA}" \
  "RawDataSaverConsumerTable/RAWSaver1/RadixFileName=AgentCopy" "RawDataSaverConsumerTable/RAWSaver1/MaxFileSize=0" \
  "+DataProcessorTable/DB0Consumer2" "DataProcessorTable/DB0Consumer2/DataProcessorGroupID=DBGroup0" \
  "DataProcessorTable/DB0Consumer2/ProcessorType=Consumer" \
  "DataProcessorTable/DB0Consumer2/ProcessorPluginName=RawDataSaverConsumer" \
  "DataProcessorTable/DB0Consumer2/LinkToProcessorTable@link-UID=RawDataSaverConsumerTable,RAWSaver1"
# 2. alias to the new key, then a reload and Configure (the Backbone is read at Startup)
ai_scripts/cfg_udp_set_alias.sh <alias> defaultConfig <new key>
ai_scripts/apply_config_reload.sh PhysicsRuns0 <alias>
# 3. run and check (emulator streaming: emulator_sine_sweep.sh), then the files
ots_drive_udp_test <ip> <port> PhysicsRuns0 Start ; ... ; ... Stop ; ls -la $OTSDAQ_DATA
```
`DataProcessorGroupID` must equal the buffer's `LinkToDataProcessorGroupID` (`DBGroup0` for `DB0`). Use a different
`RadixFileName` for each saver, or both write the same file. To remove one: set `Status=Off` on its `DataProcessorTable` row (or
`-DataProcessorTable/<UID>` after `--`). Afterwards put the alias back: `cfg_udp_set_alias.sh <alias> <group> <old key>`.
Changing only a settings value (a port, a file path) is `cfg_udp_edit_field.sh`/`cfg_udp_edit_group.sh` on the settings row, then
the same alias/reload steps (`configuration-model.md`).

## Writing a plugin

```bash
ai_scripts/new_data_processor.py producer SyntheticPacketProducer --include-dir otsdaq-components/DataProcessorPlugins --into /tmp/scaffold
ai_scripts/new_data_processor.py consumer PacketCountConsumer
```
It writes `<Name>.h` and `<Name>_processor.cc` with the structure of the real plugins and prints the next steps. *Verified:* the
generated producer and consumer compile (syntax-only, project warning flags with `-Werror`) against the installed headers; they
were not linked or run (the build is the user's). The shape of every plugin:

- **Class**: `DataProducer` (or `DataConsumer`) plus `Configurable`; the constructor takes
  `(supervisorApplicationUID, bufferUID, processorUID, const ConfigurationTree&, const std::string& configurationPath)`,
  initializes `WorkLoop(processorUID)` first (virtual base), then the processor base, then `Configurable`. Settings come from the
  tree: `theXDAQContextConfigTree.getNode(configurationPath).getNode("Column").getValue<T>()`; the column must exist in the
  settings table (producers also read `BufferSize`, the number of sub-buffers).
- **Work loop**: override `workLoopThread(toolbox::task::WorkLoop*)`; it is called repeatedly while the run goes, returns
  `WorkLoop::continueWorkLoop_`. Producer: `attachToEmptySubBuffer(dataP, headerP)` (negative: no free sub-buffer, sleep and
  retry), fill the string and header map, `setWrittenSubBuffer<std::string, std::map<std::string,std::string>>()`. Consumer:
  `read(dataP, headerP)` (negative: nothing yet, sleep), use the data, then `setReadSubBuffer<...>()` so the producer can reuse
  the slot. `write(data, header)`/`read(data, header)` are the copying versions.
- **Lifecycle**: `configure()` (Configure), `startProcessingData(runNumber)` / `stopProcessingData()` (an empty run number means
  resume after pause: do not reset); call the base class version, it starts and stops the work loop.
- **Statistics and status**: `countPacket(bytes)`, `countError()`; override `getExtraStatus()` for plugin-specific items.
- **Registration**: end the `.cc` with `DEFINE_OTS_PROCESSOR(ClassName)` (`ProcessorPluginMacros.h`).
- **Build**: in the plugin directory's `CMakeLists.txt`, after `include(otsdaq::dataProcessor)`:
  `cet_build_plugin(<Name> otsdaq::dataProcessor)`. The library is `lib<repo>_<dir>_<Name>_processor.so` (for example
  `libotsdaq_DataProcessorPlugins_RawDataSaverConsumer_processor.so`); the loader finds it by the class name through the plugin path.
  A plugin that other code links against (like `RawDataVisualizerConsumer`, used by the Visualizer) also needs a
  `cet_make_library(... <Name>Impl.cc ...)` with the logic and `LIBRARIES REG` on the plugin; a standalone one does not.
- **Settings table**: a table definition whose first column is the `ProcessorUID` (type UID) and whose other columns are what the
  constructor reads: `ai_scripts/tableinfo.py new <Name>Table "BufferSize:Data:NUMBER:100" ...` (the scaffold prints the command).
  Then the two rows of "Using a plugin".
- **Specializing a saver**: derive from `RawDataSaverConsumerBase` and override `writeHeader`, `writePacketHeader/Footer`
  (`OtsDataSaverConsumer` is the example) instead of writing a consumer from scratch.

The cycle (user builds): edit in dev-srcs, `mb` (`mz_uc` for a new file), restart `ots`, add the rows, apply, run, check the file.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Configure fails: buffer has 0 Producers | the buffer's producer row is Off or missing; check the `DataProcessorGroupID` of each row |
| Configure fails: plugin not found | `ProcessorPluginName` is not the class name, the library was not built or is not on `CET_PLUGIN_PATH`/`LD_LIBRARY_PATH` (`ls local/install/lib \| grep _processor`) |
| Configure fails naming a column | the settings table lacks a column the constructor reads (`getNode("X")`), or a link/group ID does not match |
| Configure fails in a ROOT consumer (lite/maxlite) | `DemoDQMHistosConsumer` is not built: set that row Off or use `RawDataVisualizerConsumer` |
| raw file is 0 bytes | nothing reached the buffer during the run: emulator not streaming, port of the listener not the emulator's `StreamToPort`, listener Off |
| the first packet of a raw file is odd | a burst still running when the run starts leaves its "last" packet (a different size): `read_raw_data.py` skips it (verified) |
| sequence gaps in the file | a consumer too slow (see above) or the UDP socket buffer overflowed: raise `SocketReceiveBufferSize` and `net.core.rmem_max`, lighten the consumer |
| "There are no available buffers!" in the log | the same: a consumer holds sub-buffers; raise `BufferSize` only if the consumer is bursty, else fix the consumer |

*Not run:* building or loading a new plugin, TCP and artdaq plugins, `getExtraStatus`, behaviour of a slow consumer under load.
