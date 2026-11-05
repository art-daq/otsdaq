# TRACE and MessageFacility in otsdaq

## TRACE Overview

TRACE is the tracing/logging system. It has two output paths:

- **Memory (fast)**: Circular buffer in shared memory (`/tmp/trace_buffer_$USER.$k5user`)
- **Slow**: Routes through MessageFacility (MF) via `mftrace_user()` in `tracemf.h`

## Shell Functions

Defined in `trace_functions.sh` (installed to `local/install/bin/`).
Must be sourced — they wrap the `trace_cntl` binary.

| Function | Purpose |
|----------|---------|
| `tlvls` | Show TRACE levels for all names |
| `tshow` | Show TRACE memory buffer contents |
| `tonM <name> <levels>` | Enable memory tracing for name |
| `toffM <name> <levels>` | Disable memory tracing for name |
| `tonS <name> <levels>` | Enable slow (MF) tracing for name |
| `toffS <name> <levels>` | Disable slow (MF) tracing for name |

## TRACE Levels

Levels 0-63. Common named levels:
- 0: TLVL_ERROR
- 1: TLVL_WARNING  
- 2-4: TLVL_INFO
- 5-8: TLVL_DEBUG
- 9+: TLVL_TRACE

The `ots` script sets default slow-path mask: levels 0-8 enabled
(`trace_cntl lvlmskg 0xfff 0x1ff 0`).

## __COUT__ Macros

All `__COUT__` macros (defined in `CoutMacros.h`) use `TLOG()` from
`tracemf.h`, which routes the slow path through MF.

## MessageFacility Configuration

### Normal otsdaq
MF is configured by `OTSDAQ_LOG_FHICL` → `MessageFacilityGen.fcl`.
Destinations include UDP (port 30000 → otsConsoleFwd → Console webapp)
and OTS-type (console).

### Lite otsdaq (ArtdaqLite.cc)
`artdaq::configureMessageFacility()` in `ArtdaqLite.cc` **ignores**
`OTSDAQ_LOG_FHICL`. It only creates:
- "cout" destination (disabled — useConsole=false from INIT_MF)
- "file" destination (only if `ARTDAQ_LOG_ROOT` is set)

So in lite, MF messages go to a log file under `$ARTDAQ_LOG_ROOT` or
nowhere. The UDP/OTS destinations from the fcl file are never created.

## Where Logs Go

| Output Type | Where It Goes |
|------------|---------------|
| `ots_lite_main` stdout/stderr | `Data/Logs/otsdaq_quiet_run-gateway-*.txt` (via `&>` redirect) |
| MF slow-path messages | Wherever MF destinations are configured (see above) |
| TRACE memory buffer | Viewable with `tshow` |
| `TRACE_PRINT` (via TRACE_PRINT_FD=2) | stderr of the process |

## INIT_MF()

Called by every Supervisor constructor (`CorePropertySupervisorBase`).
Reads `OTSDAQ_LOG_FHICL`, sets `ARTDAQ_LOG_FHICL`, displays its content
in the log, then calls `artdaq::configureMessageFacility(name, false, true)`.

In lite, this call goes to the ArtdaqLite.cc stand-in which ignores the fcl.
