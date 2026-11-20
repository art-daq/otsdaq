# FE macros and Macro Maker macros: run, write, edit

How an agent works with the two kinds of macro that run on a front end (an FE interface plugin): listing and running
them, creating Macro Maker macros without a browser, writing and changing compiled FE macros in C++, and chaining them
in sequences and Iterator plans. The UDP side (Macro Maker remote control, `ots_mm_udp_test`) is in `macromaker-udp.md`;
this page is the practical layer on top. Script: `ai_scripts/mm_macro.py`. *Verified* = run on ots maxlite
(2026-11-15, ExampleInterface0 on the hardware emulator, ProjectInterface0); the rest is read from the source.

## The two kinds, and the third thing people confuse them with

| | FE macro | Macro Maker macro | MoM sequence |
|---|---|---|---|
| What | a C++ function in the FE plugin, registered in its constructor | a short script of register reads, writes and delays, saved as a file | an ordered list of macros (of either kind), with variables and delays |
| Needs a build | yes (`mb`, then restart `ots`) | no: a file under `ServiceData/MacroData/` | no: a file under `ServiceData/MacroSequence/` |
| Can do | anything the FE can: logic, loops, other macros, plots, config tree access | `w`rite, `r`ead, `d`elay on the FE's registers (universal read/write) | run steps in order |
| Inputs / outputs | named, typed in C++, any strings | variables in write/read/delay fields -> inputs; read results -> outputs; all numbers | variables substituted into the steps' inputs |
| Run with | `mm_macro.py run`, `ots_mm_udp_test <fe> <macro> ...` | `mm_macro.py run` (type public/private) | `ots_mm_udp_test <name> <n>`, `mm_make_mom.py` writes them |
| Skill / tool | this page | this page | `macromaker-udp.md`, `ai_scripts/mm_make_mom.py` |

A Macro Maker macro runs *on the front end*: Macro Maker sends the macro text to the FE Supervisor, which interprets it with
the FE's `universalRead`/`universalWrite` (so it works for any FE that implements them, including the emulator).

## Rule: FE Macros are registered by the end of Configure

Rule for every FEVInterface class: when the Configure transition finishes, its FE Macros are fully registered. Before that
the list is not valid. Macro Maker returns an error for list, run, read and write requests (web and UDP) made while a
transition is in progress: "Front-end requests through Macro Maker are not available until the '<transition>' transition
finishes." Wait for the transition to end and ask again. Forwarding from one front end to another through Macro Maker
(and FE Macro steps of Iterator plans) stays open during a transition.

For people writing supervisors: before every transition the Gateway sends every supervisor a `SystemTransitionNotice`
"GetReady" (get ready for `<transition>` - are you ready?) and afterwards "StandDown" (`<transition>` completed/failed).
`CoreSupervisorBase` only stores it: `isSystemTransitionInProgress()` and `getSystemTransitionName()`. Each supervisor uses
that however it wants (e.g. to avoid lockups). Override `isReadyForSystemTransition()` to answer "not ready"; the Gateway
asks again each second and after 8 seconds refuses the transition, naming the supervisor. No answer counts as "not ready"
too, so a supervisor busy with a long request (e.g. Macro Maker running a macro) holds off transitions until it is done.

## Find and run

The system must be **Configured** (the front ends only exist then; otherwise Macro Maker lists none).

```bash
ai_scripts/mm_macro.py list [<feUID>]       # FE macros with inputs -> outputs, and the saved Macro Maker macros
ai_scripts/mm_macro.py show <MacroMakerMacro>
ai_scripts/mm_macro.py run ExampleInterface0 varTest myOtherArg=5
ai_scripts/mm_macro.py run ExampleInterface0 AgentOffset newoff=0x40 --type public
ai_scripts/mm_macro.py run ProjectInterface0 projectStatus            # FE macro without inputs
```
`run` checks the names against the macro's signature first (every input exactly once, by name) and prints the outputs.
`ots_mm_udp_test` runs FE macros only; `mm_macro.py` also runs public and private Macro Maker macros. Underneath, both send
`RunFrontendMacro;<feClass>;<feUID>;<fe|public|private>;<name>;<inputs>;<outputs>;<save 0/1>` to the Macro Maker UDP port;
inputs are `name,value;name,value`, outputs `name,name`, each URI-encoded (the script does it).

Values:
- **FE macro inputs** arrive as strings. `__GET_ARG_IN__("x", T)` converts a number with `StringMacros::getNumber`: decimal,
  or hex with `0x`; the word `Default`/`DEFAULT` selects the default given in C++. A non-number throws "is not a number".
  Outputs are whatever the macro set (verified: `projectStatus` returns text and `40 (0x28)` for a number).
- **Macro Maker macro inputs** are numbers: decimal, or hex with `0x`. *Verified trap:* `regaddr=1005` is decimal 1005 = 0x3ED, not
  0x1005 (a read of address `1005` returned the unmapped address' own value); give `regaddr=0x1005`. Outputs read
  `decimal (0xhex)`.
- Counts and names must match exactly: a missing or extra input, or a wrong output name, fails with "mismatch in number/name of
  input/output arguments" (verified). An FE macro input whose name has spaces or symbols (`Pause Slow Controls (Default = false)`)
  runs fine as a single macro (verified); inside a MoM sequence it failed in `ots_mm_udp_test` builds before the
  2026-11-15 fix (`macromaker-udp.md`).
- Results of `--save` go to `$OTSDAQ_DATA/macroOutput_*.txt`.
- Over UDP the acting user is `NO-USER` with all-users permission 255: macro permission levels do not block it, and a *private*
  macro means one in `MacroData/NO-USER/`.

## Macro Maker macros: format, rules, creating them

A saved macro is `MacroData/publicMacros/<name>.dat` (everyone) or `MacroData/<user>/<name>.dat`. The FE parses the file **by
position**, so keep exactly this layout (what `createMacro` and `mm_macro.py new` write):
```
{
"name":"AgentOffset",
"sequence":"0:r:100A:old,1:w:100A:newoff,2:d:10,3:r:100A:cur",
"time":"Sun Nov 15 2026",
"notes":"",
"LSBF":"false"
}@
```
`sequence` is a comma-separated list of `index:type:field[:field]`:

| Step | Form | Meaning |
|---|---|---|
| write | `n:w:<addr>:<data>` | write `data` to `addr` |
| read | `n:r:<addr>:<out>` | read `addr`; `out` names the output (empty `n:r:<addr>:` keeps nothing) |
| delay | `n:d:<ms>` | wait that many milliseconds |

Rules, all from `FEVInterface::macroStruct_t` (verified where marked):
- **Addresses and data in the file are hex without `0x`** (`100A`). **The delay is decimal milliseconds** (`d:10` = 10 ms), as the browser
  writes it and as the export to C++ treats it. Until 2026-11-15 the runner read the delay as hex (a saved `d:100` ran for 256 ms and
  `d:1000` for 4096 ms: the front end logged `Delay ms: 256`, `4096`); `FEVInterface.cc` now reads a digits-only delay as decimal
  (verified: 10, 100, 500, 1000 ran as 10, 100, 500, 1000 ms). A delay with hex letters (`d:a`) can only be hand-written and is still hex.
  Older installs still misread decimal delays.
- **A field that is not a valid hex number is a variable.** A write/read address, write data and delay can all be variables;
  variables in those fields are the macro's **inputs**; names in a read's last field are its **outputs**.
- **Name trap:** a variable name made only of `0-9a-fA-F` (`add`, `cafe`, `12`) is read as a hex number, so the macro silently has
  no variable (verified: running with `add` fails with an input-count mismatch). `mm_macro.py new` refuses such names.
- `LSBF` true writes addresses/data least-significant-byte first.
- Numbers are 64-bit. No loops, conditions or arithmetic: for those write an FE macro.

Create, inspect, run, remove:
```bash
ai_scripts/mm_macro.py new AgentOffset 'r 0x100A $old' 'w 0x100A $newoff' 'd 10' 'r 0x100A $cur'     # public; --private for NO-USER
ai_scripts/mm_macro.py show AgentOffset
ai_scripts/mm_macro.py run ExampleInterface0 AgentOffset newoff=0x40
ai_scripts/mm_macro.py delete AgentOffset
```
Addresses and data in `new` are hex (`1004`, `0x1004`) or `$variables`; delays are decimal ms. Use real register
addresses of your FE (`emulator-registers.md` for the emulator; a read of an unmapped emulator address returns an unrelated value, not an error).

## Writing an FE macro (C++)

Three edits in the FE plugin (`<repo>/<FEInterfaces>/`): the header, the constructor, the body. The example project FE
(`otsdaq-<project>/FEInterfaces/FEProjectInterface*.{h,cc}`) is a small complete example; `FEOtsUDPTemplateInterfaceImpl.cc` shows
register access and nested macros.

```cpp
// 1. header (FEProjectInterface.h): declare it; the signature is fixed by __ARGS__
void projectSetValue(__ARGS__);

// 2. constructor, in <Plugin>Impl.cc: register it (name, function, input names, output names, [permission, callers, tooltip])
FEVInterface::registerFEMacroFunction(
    "projectSetValue",
    static_cast<FEVInterface::frontEndMacroFunction_t>(&FEProjectInterface::projectSetValue),
    std::vector<std::string>{"name", "value"},   // namesOfInputArgs
    std::vector<std::string>{"result"},          // namesOfOutputArgs
    1);                                          // required permission: 0 none, 1 user, 255 admin

// 3. body
void FEProjectInterface::projectSetValue(__ARGS__)
{
    const std::string name  = FEVInterface::getFEMacroConstArgument(argsIn, "name");   // string input
    uint64_t          count = __GET_ARG_IN__("count", uint64_t, 1);                    // number input, default 1 for "Default"
    __SET_PCT_DONE__(10);                                                              // progress 0-100 for the caller
    // register access: buffers of the interface's sizes
    char* address = new char[universalAddressSize_]{0};
    char* data    = new char[universalDataSize_]{0};
    uint64_t a = 0x1002;  memcpy(address, &a, 8);
    universalRead(address, data);
    uint64_t v;           memcpy(&v, data, 8);
    delete[] address; delete[] data;
    __SET_ARG_OUT__("result", v);                      // any type with operator<<
    // throw to fail the macro: the text reaches the caller
    if(!v) { __FE_SS__ << "register is zero" << __E__; __FE_SS_THROW__; }
}
```
Helpers (`FEVInterface.h`): `__GET_ARG_IN__(name, T[, default])`, `__GET_ARG_OUT__`, `__SET_ARG_OUT__`, `__SET_PCT_DONE__`,
`getFEMacroConstArgument` (string), `runSelfFrontEndMacro(name, argsIn, argsOut)` to call another macro of the same FE
(`__SET_ARG_IN__` builds `argsIn`), the output argument `PLOTLY_PLOT` (a Plotly JSON string is drawn by the browser's FE Macro
Test), `getFEMacroInputDefaults` (override to give per-target default values for the UI). Further constructor arguments of
`registerFEMacroFunction`: `allowedCallingFEs` (wild-card list of FEs allowed to call it) and a tooltip.
Use `__FE_COUT__` for logs (the context's quiet-run log).

**Editing** an existing macro: change the body (and the input/output name vectors if its signature changes: tutorials and saved
sequences that name the old arguments break, and MoM steps reconcile by name). **Rule of thumb for a signature change:** run
`mm_macro.py list` after the rebuild and fix every saved sequence/plan step that names the macro (`iteration_plan.py show`,
`mm_make_mom.py`).

**Build and test** (the user builds, `development-workflow.md`): edit in dev-srcs, then `mb` (or `mz_uc` after adding a
new file), restart `ots` (`ots_mode.sh stop`, `ots`), Initialize and Configure, then `mm_macro.py list` shows the new macro with
its argument names, `mm_macro.py run ...` calls it. Plugin sources live in the repo of the FE (otsdaq-components, your own
repo). The repository's FE plugins are laid out as `<Plugin>.h`, `<Plugin>Impl.cc` (constructor, macros) and
`<Plugin>_interface.cc` (only `DEFINE_OTS_INTERFACE`).

## Promoting a Macro Maker macro to an FE macro

The browser's Macro Maker exports a macro as FE macro code: right-click the macro, choose the FE export (with exactly one front end
selected in the FE list); request `exportFEMacro` with `MacroName`, `PluginName` and the macro's `sequence` (raw, not URL-encoded, as
the browser sends it). It writes into the plugin's source tree (`OTS_SOURCE`, so Macro Maker must run next to the sources):
the registration after the constructor's `{`, the function body, and a declaration in `<Plugin>.h`.
- **Fixed 2026-11-15**: the exporter used to look for the constructor in `<Plugin>_interface.cc`. Since 2022 most plugins keep the
  constructor and methods in `<Plugin>Impl.cc` (`_interface.cc` holds only `DEFINE_OTS_INTERFACE`), so the export failed with "Could not find
  the code insert position in the source file ..._interface.cc" (nothing was modified). It now uses `<Plugin>Impl.cc` if present (the function is
  appended at the end of that file) and falls back to the single-file `<Plugin>_interface.cc` (function inserted before `DEFINE_OTS_INTERFACE`).
- It also declared its helper variables unconditionally, so a macro without a literal write failed to compile under `-Werror`
  ("unused variable `macroData`"); they are now `[[maybe_unused]]`.
- **Verified** (rebuilt `dev-skills`, request sent with `curl` as the browser does): `FEOtsUDPTemplateInterface` (split plugin) export gave a
  registration, a declaration and a function; `mb` compiled it with the project's `-Werror`; the macro appeared in `mm_macro.py list`
  and ran on the emulator (`old` 50, `cur` 64, then restored). `myNewInterface` (single file) still exports into `_interface.cc`. The
  browser dialog itself was not clicked.
- The macro must have a valid C++ name (letters, digits, `_`); the generated function logs its arguments, so edit it for real use.

How each Macro Maker step becomes C++ (what the exporter writes; do it by hand the same way for a macro the exporter can not reach):

| Macro Maker step | C++ |
|---|---|
| `r <addr> <out>` | `a = <addr>; memcpy(address,&a,8); universalRead(address,data); memcpy(&out,data,8); __SET_ARG_OUT__("out", out);` |
| `w <addr> <data>` | `a = <addr>; memcpy(address,&a,8); d = <data or __GET_ARG_IN__("var", uint64_t)>; memcpy(data,&d,8); universalWrite(address,data);` |
| `d <ms>` | `usleep(<ms> * 1000);` |

## In sequences and plans

- MoM sequence steps: `mm_make_mom.py` (FE macros; checks names against the running Macro Maker). FE macro inputs with
  spaces/symbols in their names work in a sequence since the 2026-11-15 `ots_mm_udp_test` fix; an older build fails them
  with "mismatch in a name of an input argument" (`macromaker-udp.md`).
- Iterator plan steps `fe-macro` / `mm-macro` with targets and loops: `iterator-plans.md` (`iteration_plan.py`).
- Macro Maker macros (files) in a MoM sequence use `macroType` `public`/`private`; `mm_make_mom.py` writes FE-macro steps only.

## Troubleshooting

| Message | Cause / fix |
|---|---|
| "Macro Maker lists no front ends" / "No Front-ends found" | system not Configured |
| "mismatch in number of input arguments" / "name of an input argument" | give every input by its exact name; for a Macro Maker macro the inputs are its variables (`mm_macro.py show`) |
| "Requested argument not found with name 'X'" | the macro body asks for an argument that is not in its registered input/output names. Seen: `varTest` run as an Iterator `fe-macro` step failed with `Plotly_Plot` (its direct run works); use another macro such as `test` for an Iterator test |
| "... is not a number" | an input of a numeric `__GET_ARG_IN__` or a Macro Maker variable got text |
| "Unable to locate file for macro" | no such public/private macro file (`list`); UDP users are `NO-USER` |
| "Invalid command type ..." | malformed `sequence` in the macro file: a step needs `index:type:field[:field]`, a read needs its 4th field (possibly empty) |
| FE macro missing from `list` after editing | not rebuilt/restarted, or not registered in the constructor |

*Verified 2026-11-15*: the export (above); list; running FE macros (`test`, `projectStatus`, one with a spaced input name) and public/private Macro Maker
macros with hex addresses, delay and variables; input-name/count errors; refusal of a hex-only variable name; decimal-vs-hex input
values. *Not run*: writing a new FE macro by hand, the browser's export dialog, `LSBF`, `--save`, FE macro permissions, `allowedCallingFEs`.
