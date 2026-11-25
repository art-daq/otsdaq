# MacroMaker UDP Remote Control

## Overview

MacroMaker exposes FE macro execution and universal register read/write
over UDP. Enabled by setting environment variables before launching `ots`:

```bash
export OTS_MACROMAKER_UDP_PORT=2011
export OTS_MACROMAKER_UDP_IP=127.0.0.1
```

## Starting Macro Maker mode (no configuration, no State Machine)

`ots -mm <file.fcl>` (or `--macromaker`) stops the other ots processes of the install and starts Macro Maker mode: only the
Macro Maker, FE Macros and Code Editor apps, and the front-ends named in the FHiCL file. Set the two UDP variables above
**before** the launch, or only a warning is printed. Verified 2026-11-20 on a full install with the demo emulator on port 4000
(`ots_udp_hw_emulator 4000 &`):

```bash
cat > my_first_mm.fcl <<'EOF'
interface0: {
	FEInterfacePluginName: "FEOtsUDPTemplateInterface"
	LinkToFETypeTable_FEOtsUDPTemplateInterfaceTable: {
		OtsInterface0: {
			InterfaceIPAddress: "127.0.0.1"
			InterfacePort: 4000
			HostIPAddress: "127.0.0.1"
			HostPort: 4001
			StreamToIPAddress: "127.0.0.1"
			StreamToPort: 4005
		}
	}
}
EOF
export OTS_MACROMAKER_UDP_IP=127.0.0.1 OTS_MACROMAKER_UDP_PORT=2011
ots -mm my_first_mm.fcl
ots_mm_udp_test $OTS_MACROMAKER_UDP_IP $OTS_MACROMAKER_UDP_PORT interface0 r 0x1002
```

- The block name (`interface0`) is the front-end UID. The link block is `<link column>_<table name>`, then the record UID, then its
  fields: the same names as in the Tree-view of the Configure app. `FirmwareVersion` can be left out; **all six address fields
  must be there**, or the launch fails after 3 attempts with `Socket.cc ... Invalid IP:Port combination ... DEFAULT:0` (the log is
  `$USER_DATA/Logs/otsdaq_quiet_run-mm-<host>.txt`).
- **Every Configure** of a normal mode system writes `$USER_DATA/MacroMakerModeConfigurations/MacroMakerModeFhiclDump.fcl` with the
  front-ends that are On (`ConfigurationManager::dumpMacroMakerModeFhicl`, called by the Gateway). Use it with
  `ots -mm $USER_DATA/MacroMakerModeConfigurations/MacroMakerModeFhiclDump.fcl`; copy it first, the next Configure overwrites it.
- The path is not expanded by ots: in single quotes `$USER_DATA` stays as typed and the launch fails with
  `Failed to instantiate application class 'ots::FESupervisor'`. Read the line `CONFIGURATION_INIT_FCL = ...` of the output.
- Tutorial: *Macro Maker Mode* on https://otsdaq.fnal.gov/tutorials/macromaker-mode.html

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

Write a sequence without the browser: `ai_scripts/mm_make_mom.py` (file format: one JSON object per
step, `macroType` `fe`, `__delay__` or `__mom__`; inputs `name,value;name,value` URI-encoded; variables
under `__variables__` as `$name: {value, step, max}`). Verified 2026-11-14 on ots maxlite: an FE
macro with a `$amp` variable, a delay and a project FE status read ran over UDP. **Fixed 2026-11-15** (was a bug): an FE macro
input whose *name* has spaces or symbols (e.g. `Pause Slow Controls (Default = false)`) failed when the
sequence ran over UDP ("mismatch in a name of an input argument" from `FEVInterfacesManager.cc`, and the saved value was
replaced by `Default`). Cause: the FE Macro info carries the names URI-encoded; `ots_mm_udp_interface::resolveMacroSignature`
returned them still encoded and `runSequenceRecursive` encoded them again. It now decodes them where it builds the
signature (`ots_mm_udp_interface.cpp`). Only `ots_mm_udp_test` running a *sequence* was affected; a single macro run
(`ots_mm_udp_test <fe> <macro> ...`, `mm_macro.py`) and the FEMacroTest web app encode the names once and never did. An
install built before the fix needs a rebuild (`mb`) to get it. Verified on ots maxlite: the sequence with the spaced
name and a non-default value (`true`) reached the FE ("workloop is paused"); a plain-name sequence with a `$amp`
variable over 3 iterations behaves as before.

For running public/private Macro Maker macros, creating them, and writing FE macros in C++, see `fe-macros-and-macromaker.md` and `ai_scripts/mm_macro.py`.

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
