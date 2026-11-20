# DQM histograms: from a consumer to the live view in the Visualizer

How ROOT histograms filled during a run reach the Visualizer window ("LIVE_DQM"), and what to check when the live view is
empty. Full build only: the ROOT plugins are not built in lite/maxlite (see `data-processor-plugins.md`). The table chain of
buffers and processors is in `data-path-configuration.md`.
*Verified* = run on the artdaq tutorial area (2026-11-18, full build); the rest is read from the source. Not yet verified:
the top-folder rule below (changed 2026-11-18, waiting for a build).

## The pieces

| Piece | File | What it does |
|---|---|---|
| `DQMHistosBase` | `otsdaq/RootUtilities/DQMHistosBase.h` | holds the ROOT file of the run (`theFile_`) and the folder to fill (`myDirectory_`); `openFile`, `save`, `closeFile`, `getFile()` |
| `DQMHistosConsumerBase` | `otsdaq/DataManager/DQMHistosConsumerBase.h` | `DQMHistosBase` + `DataConsumer`; owns `fillHistoMutex_` |
| `VisualDataManager` | `otsdaq/RootUtilities/VisualDataManager.cc` | the Data Manager inside the Visualizer supervisor; keeps the list of live DQM consumers |
| `VisualSupervisor` | `otsdaq-utilities/Visualization/VisualSupervisor.cc` | answers the browser: `getDirectoryContents` (file tree) and `getRoot` (one object or one folder) |

A DQM consumer is an ordinary consumer plugin (a `DataProcessorTable` row, `ProcessorType` Consumer) that derives from
`DQMHistosConsumerBase`. It must sit in a buffer of the **Visualizer supervisor's** Data Manager: only that Data Manager is a
`VisualDataManager`. At Configure it finds every consumer that is a `DQMHistosBase` (`dynamic_cast`), puts it in the list
returned by `getLiveDQMs()` and gives it a pointer to itself (`setDataManager`).

Its settings come from `DQMHistosConsumerTable`: `SaveDQMFile`, `DQMFilePath`, `DQMFileNamePrefix`.

## Life of a run

1. **Start**: the consumer's `startProcessingData(runNumber)` calls `DQMHistosBase::openFile(<DQMFilePath>/<Prefix>_Run<n>.root)`.
   The file is opened even when `SaveDQMFile` is No: the live view reads the histograms through this `TFile` pointer, in
   memory, not from disk. The consumer then books its histograms in `myDirectory_` (or leaves that to the first packet).
   After all consumers have started, `VisualDataManager::start` sets `ready_` true.
2. **Running**: the consumer's work loop reads packets from the buffer and fills histograms, holding `fillHistoMutex_`
   while it does. The Visualizer takes the same lock while it converts an object for the browser.
3. **Stop**: `ready_` goes false first (the Data Manager waits for a request in progress, `doNotStop_`), then the consumer's
   `stopProcessingData` saves the file if `SaveDQMFile` is Yes and closes it. The file is then a normal ROOT file that the
   Visualizer shows from disk, if `DQMFilePath` is under `ROOT_BROWSER_PATH`.

## What the browser asks

- `getDirectoryContents` with `Path=/`: the entry `LIVE_DQM.root` is added **only while `isReady()` is true**, that is
  between Start and Stop. Not Running means no live entry. *Verified.*
- `getRoot` with `RootPath=/LIVE_DQM.root/<path>`: for each consumer in `getLiveDQMs()`, the supervisor looks for `<path>`
  in that consumer's file. An object is returned as JSON (jsROOT draws it); otherwise the folder contents of all consumers
  are merged into one listing. Several DQM consumers therefore appear as one tree.
- By hand (send with `curl -d`, not `--data-urlencode`):
  `curl -s -d "RootPath=/LIVE_DQM.root/" "http://<host>:<port>/urn:xdaq-application:lid=<Visualizer id>/Request?RequestType=getRoot"`

### Top folder rule

If a consumer's file has **exactly one folder and nothing else** at its top, that level is skipped: `/LIVE_DQM.root/` lists
the content of that folder. A path that spells out the folder still works. A file with anything else at its top is shown as
it is. The rule is applied per consumer.

History: commit 83908747 (2026-09-23) made the Visualizer always put `/Mu2eHistos` in front of live paths, the top folder
that the Mu2e consumer writes. Any other consumer then showed an empty live tree (*verified*: histograms were in the DQM
file but the live view was empty). The rule above replaced the fixed name on 2026-11-18.

## Two ways to fill

- **In the consumer**, from raw packets: `DemoDQMHistosConsumer` (otsdaq-demo) passes each packet to a small histogram
  class (`DemoDQMHistos`: `book`, `fill`). Use this when the data comes straight from a front end or the emulator.
- **Somewhere else, then send the histograms**: an art module (for example reading from an artdaq Dispatcher) fills
  histograms and sends them with `HistoSender` (otsdaq-demo `ArtModules`) over TCP to a `TCPDataListenerProducer`
  (`DataType` Packet) in the Visualizer's buffer; `DemoDQMHistoReceiverConsumer` unpacks each packet with `HistoReceiver`
  into its file. The folder name in a packet may end in `:add` (add to what is there; the sender resets its copy after a
  successful send) or `:replace`. *Verified* up to the DQM file with the `UDPEmulatorDQM` art module, port 2031.

## Live view is empty: what to check

| Check | How |
|---|---|
| Is the run Running? | `LIVE_DQM.root` exists only between Start and Stop |
| Is the consumer in the Visualizer's Data Manager and On? | `DataBufferTable` row linked from the Visualizer supervisor, `DataProcessorTable` row `Status` On |
| Did the file open? | `<DQMFilePath>/<Prefix>_Run<n>.root` exists after Start; the Visualizer log prints `LIVE file name:` on each request |
| Are packets arriving? | the producer's status (`TCPDataListenerProducer` reports `clients`); for a sender, its log says when it connected |
| What does the supervisor look for? | the Visualizer log prints `rootDirectoryName` and `liveDirectoryName` for each `getRoot` |
| Is the data there at all? | Stop the run and open the saved file: histograms there but not live points at the path lookup, not the data path |
