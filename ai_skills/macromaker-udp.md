# MacroMaker UDP Remote Control

## Overview

MacroMaker exposes FE macro execution and universal register read/write
over UDP. Enabled by setting environment variables before launching `ots`:

```bash
export OTS_MACROMAKER_UDP_PORT=2011
export OTS_MACROMAKER_UDP_IP=127.0.0.1
```

## CLI Tool: ots_mm_udp_test

### List front-ends
```bash
ots_mm_udp_test 127.0.0.1 2011
```

### List macros for a front-end
```bash
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0
```

### Run an FE macro
```bash
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 "varTest" <input_args...>
```

### Universal register read
```bash
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 r 0x1004
```

### Universal register write
```bash
ots_mm_udp_test 127.0.0.1 2011 ExampleInterface0 w 0x1004 0x2
```

### Run a MoM sequence
```bash
# List sequences (shown with front-end list)
ots_mm_udp_test 127.0.0.1 2011

# Inspect a sequence
ots_mm_udp_test 127.0.0.1 2011 "MySequence"

# Run with variable overrides
ots_mm_udp_test 127.0.0.1 2011 "MySequence" 1 [num_of_packets,200]
```

## UDP Protocol (Server Side)

The UDP handler is in `MacroMakerSupervisor::RemoteControlWorkLoop()`.
Recognized commands:

| UDP Message | Description |
|-------------|-------------|
| `GetFrontendMacroInfo` | Returns XML with all FE UIDs, types, macros |
| `RunFrontendMacro;feClass;feUID;macroType;macroName;inputs;outputs;save` | Runs an FE macro |
| `UniversalRead;feUID;address` | Reads a register via SOAP to FE Supervisor |
| `UniversalWrite;feUID;address;data` | Writes a register via SOAP to FE Supervisor |
| `GetSequenceList` | Returns XML with available sequences |
| `GetSequenceInfo;sequenceName` | Returns sequence definition JSON |

All responses are XML, sent via chunked acknowledge (1500-byte chunks).

## Client Library: ots_mm_udp_interface

Header: `test/ots_mm_udp_interface.h`
Implementation: `test/ots_mm_udp_interface.cpp`

Key methods:
- `getFrontendList()` — returns `;`-separated FE UIDs
- `getCommandList(feUID)` — returns `;`-separated macro names
- `runCommand(feUID, macroName, inputs)` — runs a macro, returns outputs
- `universalRead(feUID, address)` — reads a register
- `universalWrite(feUID, address, data)` — writes a register
- `getSequenceList()` / `getSequenceInfo()` / `runSequence()` — sequence ops

## Architecture

```
ots_mm_udp_test (CLI)
    |  UDP
    v
MacroMakerSupervisor::RemoteControlWorkLoop (UDP listener thread)
    |  SOAP (MacroMakerSupervisorRequest)
    v
FESupervisor → FEVInterface::universalRead/Write()
    |  OTS UDP protocol
    v
ots_udp_hw_emulator (or real hardware)
```

## Troubleshooting

- "No Front-ends found": System must be Initialized+Configured first
- Port not bound: Check `ss -ulnp | grep <port>` and env vars
- FE UID not found: Run without args to list available UIDs
- The MacroMaker UDP listener only starts if both env vars are set
