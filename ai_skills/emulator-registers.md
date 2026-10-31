# ots_udp_hw_emulator Register Map

The hardware emulator (`ots_udp_hw_emulator`) simulates a UDP-based
front-end device. Source: `otsdaq-demo/test/ots_udp_hw_emulator.cpp`.

Default port: 4000 (launched by the `ots` script).

## Register Address Space

| Address | R/W | Description |
|---------|-----|-------------|
| 0x0000000000001001 | W/R | Firmware version |
| 0x0000000000001002 | W/R | Firmware type |
| 0x0000000000001003 | W/R | Number of destined packets |
| 0x0000000000001004 | W/R | Data mode (0=counter, 1=sine, 2=multi-ch waveforms) |
| 0x0000000000001005 | W/R | Sample bit width (8, 12, 16, 32) |
| 0x0000000000001006 | W/R | Samples per packet (0=max fit) |
| 0x0000000000001007 | W/R | Number of channels |
| 0x0000000000001008 | W/R | Trigger (0=off, >0=on) |

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

## FE Interface

The FE plugin `FEOtsUDPTemplateInterface` (in otsdaq-components) talks
to the emulator. It implements `universalRead()` and `universalWrite()`
which send OTS UDP protocol read/write packets.

Registered FE macros (hardcoded, not register-generic):
- `test` — no inputs, reads fixed addresses, returns outArg1
- `varTest` — reads 0x1002 and 0x1001, writes 0x1002 and 0x1001
- `varTest2` — similar to varTest
- `Slow Controls Pause/Resume` — pauses/resumes the slow controls workloop
