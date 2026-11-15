# The data path: listeners, buffers, savers, streamers, the Visualizer

How data travels from a front end to files and to the Visualizer, which tables link it, how to switch
a consumer off, where the files go and how runs are numbered. The emulator side (data modes,
registers) is in `emulator-registers.md`; the Visualizer waveform view in
`tutorial-to-visualizer-waveforms.md`.

## The chain of tables (Configuration group)

```
DataManagerSupervisorTable        one row per data manager app (e.g. DataManagerSupervisor0, VisualSupervisor0)
  -> DataBufferTable              its buffers (LinkToDataBufferGroupID -> DataBufferGroupID), Status On/Off
     -> DataProcessorTable        the producers and consumers of each buffer (LinkToDataProcessorGroupID)
        ProcessorType             Producer | Consumer
        ProcessorPluginName       the C++ plugin, e.g. UDPDataListenerProducer, RawDataSaverConsumer,
                                  UDPDataStreamerConsumer, RawDataVisualizerConsumer, DQMHistosConsumer
        LinkToProcessorTable/UID  the plugin's own settings row, e.g. UDPDataListenerProducerTable/UDPListener0
        Status                    On | Off
```

The tutorial data (`first_demo`), as on the test system:

| Buffer (app) | Producer | Consumers |
|---|---|---|
| `DB0` (DataManagerSupervisor0) | `UDPDataListenerProducer` UDPListener0: listens on `HostPort` 4005 (the emulator streams there: FE `StreamToPort`) | `RawDataSaverConsumer` RAWSaver0 (files), `UDPDataStreamerConsumer` UDPStreamer0 (forwards to 4101) |
| `DB1` (VisualSupervisor0) | `UDPDataListenerProducer` UDPListener1: listens on 4101 | the Visualizer's consumer: `DQMHistosConsumer` (full build, ROOT) or `RawDataVisualizerConsumer` (lite/maxlite, no ROOT) |

So one stream of UDP packets is saved by one data manager and forwarded to the other, which feeds the
Visualizer. Producers and consumers are linked only through their buffer: a consumer reads what
every producer of its buffer writes.

## Switching a consumer off

Set `Status=Off` on its `DataProcessorTable` row (not on the plugin's settings row), then save, move
the alias, apply (`configuration-model.md`):
```bash
cfg_udp_edit_group.sh -t Configuration DataProcessorTable/DB1Consumer1/Status=Off
cfg_udp_set_alias.sh defaultSystemAlias
apply_config_reload.sh PhysicsRuns0 defaultSystemAlias
```
Needed in lite for `DQMHistosConsumer` (ROOT plugins are not built; Configure fails "plugin not
found"), or swap its plugin to `RawDataVisualizerConsumer` (`tutorial-to-visualizer-waveforms.md`).

## Where the files go, and run numbers

- `RawDataSaverConsumerTable`: `FilePath` (tutorial data: `${OTSDAQ_DATA}`, set by `setup_ots.sh`, e.g.
  `$USER_DATA/OutputData`), `RadixFileName` (`Run`), `MaxFileSize` (0 = one file per run; otherwise the
  file name gets a sub-run number `_<n>` and a new file starts when the file reaches `MaxFileSize`/1000
  bytes, as `RawDataSaverConsumerBase::save` computes it).
- File name: `<FilePath>/<Radix>_Run<run number>[_<subrun>]_Raw.dat`, opened at Start, closed at Stop.
  The content is the raw UDP packets back to back (`read_raw_data.py` decodes it). A file of 0 bytes
  means no packet reached the buffer during the run (emulator not streaming, wrong port, listener Off).
- Run numbers: `$USER_DATA/ServiceData/RunNumber/<FSM>NextRunNumber.txt` holds the next number per
  state machine; Start takes it and increments it (unless a run-info plugin of the state machine
  claims the number, e.g. from a database). `ots_drive_udp_test ... Start <n>` sets one.

## Checks

- Listener ports: `isolate_ots_ports.py` lists every `HostPort`/`StreamToPort` and who holds them.
- During a run: `ls -la $OTSDAQ_DATA` grows; after Stop, `read_raw_data.py <file>`.
- Visualizer: `tutorial-to-visualizer-waveforms.md` (getWaveformData check).

Verified 2026-11-14 on ots maxlite (tutorial data imported from lite): the table chain and values
above are read from the active Configuration group (`defaultConfig` key 4); run files `Run_Run7..9_Raw.dat`
in `OutputData` and `PhysicsRuns0NextRunNumber.txt` exist. File naming and sub-runs are from
`RawDataSaverConsumerBase.cc`. Not re-checked: a full-build `DQMHistosConsumer` (ROOT), sub-run splitting.
