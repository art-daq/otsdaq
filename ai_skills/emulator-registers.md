# ots_udp_hw_emulator Register Map

The hardware emulator (`ots_udp_hw_emulator`) simulates a UDP-based
front-end device. Source: `otsdaq-demo/test/ots_udp_hw_emulator.cpp`.

Default port: 4000 (launched by the `ots` script).

## Register Address Space

Values are bare hex (no `0x` prefix needed in the web GUI; `ots_mm_udp_test`
strips `0x` automatically). Reads return 16 hex digits (8 bytes, little-endian).

| Address | R/W | Size | Description | Default |
|---------|-----|------|-------------|---------|
| 0x1001 | W/R | 8 | Data packet count (countdown to 0) | 0 |
| 0x1002 | W/R | 8 | Data rate (loop iterations between sends) | 100 (0x64) |
| 0x1003 | W/R | 1 | LED register (8-bit bitmask) | 0 |
| 0x1004 | W/R | 1 | Data mode: 0=counter, 1=sine, 2=multi-ch waveforms | 0 |
| 0x1005 | W/R | 2 | Sample bit width (8, 12, 16, 32) | 16 (0x10) |
| 0x1006 | W/R | 2 | Samples per packet (0=auto-fill to max) | 1 |
| 0x1007 | W/R | 2 | Number of channels (min 1) | 1 |
| 0x100000006 | W/R | 4 | Stream destination IP (network byte order) | — |
| 0x100000008 | W/R | 4 | Stream destination port | — |
| 0x100000009 | W/R | 1 | Burst data enable (1=on, 0=off) | 0 |

The stream destination IP and port are set by the FE during Configure
(from `FEOtsUDPTemplateInterface::configure()`), not by the user.
Writing a new count to 0x1001 while burst enable is 1 starts another burst.

## Reading Registers via CLI

Requires MacroMaker UDP to be enabled and the system to be Configured:

```bash
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 r 0x1004
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 r 0x1005
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 r 0x1006
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 r 0x1007
```

## Writing Registers via CLI

```bash
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 w 0x1004 0x2
```

## Data Stream Path

During Configure, the FE (`FEOtsUDPTemplateInterface::configure()`)
writes `StreamToIPAddress` and `StreamToPort` from the config tree
into registers 0x100000006 and 0x100000008, telling the emulator
where to send burst data. During Start, it calls `startBurst()`.

The full-OTS data chain:

1. Emulator sends UDP packets to StreamToIPAddress:StreamToPort
2. `UDPDataListenerProducer` (a DataProcessor plugin) listens on that
   socket and writes packets into a `CircularBuffer`
3. `RawDataVisualizerConsumer` (another DataProcessor plugin) reads from
   the buffer and holds the latest packet in `getLastRawDataBuffer()`
4. `VisualSupervisor::request("getWaveformData")` finds the consumer,
   copies the latest raw packet, skips the 2-byte header (type byte,
   sequence byte), de-interleaves it using the `numChannels` and
   `bitWidth` request parameters, and returns CSV per channel as XML
5. `ViewerWaveform.js` renders the waveform in the browser; its
   "ch" and "-bit" drop-downs set those request parameters and must
   match emulator registers 0x1007 and 0x1005 (defaults 1 ch, 16-bit)

The emulator packet carries no channel-count or bit-width fields, so
the viewer cannot discover them; if the drop-downs disagree with the
registers the traces are split wrong but the viewer still goes live.
If no consumer has data yet (system not Running, or the packet is
shorter than one sample), the handler returns a DEMO sine wave.

Both the listener and the consumer must be configured in the
`DataProcessorTable` records linked to the supervisor's
`LinkToDataBufferTable`. The supervisor that hosts them can be
`FEDataManagerSupervisor` (combines FE + DataManager in one app)
or a standalone DataManager context.

## FE Interface

The FE plugin `FEOtsUDPTemplateInterface` (in otsdaq-components) talks
to the emulator. It implements `universalRead()` and `universalWrite()`
which send OTS UDP protocol read/write packets. It does NOT inherit
`DataProducerBase` — it is not a buffer producer. The emulator's data
stream bypasses the FE entirely and goes straight to the
`UDPDataListenerProducer`.

The producer variant `FEOtsUDPProducerTemplateInterface` inherits
`FEProducerVInterface` (which includes `DataProducerBase`) and writes
directly into the CircularBuffer from its `running()` method. In that
case no separate `UDPDataListenerProducer` is needed.

Registered FE macros (hardcoded, not register-generic):
- `test` — no inputs, reads fixed addresses, returns outArg1
- `varTest` — reads 0x1002 and 0x1001, writes 0x1002 and 0x1001
- `varTest2` — similar to varTest
- `Slow Controls Pause/Resume` — pauses/resumes the slow controls workloop
