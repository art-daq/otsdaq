# otsdaq ai_skills index

Knowledge for AI agents (and people) working on otsdaq, mostly the lite build (`OTS_LITE`).
Each file says what was verified on a real run and what is still unverified. Scripts that
automate these recipes are indexed in `../ai_scripts/README.md`.
Knowledge about maintaining the project itself (README, documentation publishing, CI) is in
`../ai_dev_skills/README.md`.

| Skill | Use it to |
|-------|-----------|
| [ots-lite-architecture.md](ots-lite-architecture.md) | understand OTS_LITE: repos, XdaqLite, what is not built (artdaq), multi-context support, `mb`/`mz`/`mz_uc`, scripted `ots` start/stop, `#ifdef OTS_LITE` guards |
| [ots-maxlite-architecture.md](ots-maxlite-architecture.md) | understand OTS_MAXLITE (embedded/SoC build): install, run-time dependencies, what replaces TRACE/messagefacility/cetlib/fhiclcpp/Boost, the `textdb://` configuration store and `otsdaq_convert_config_db`, open items |
| [development-workflow.md](development-workflow.md) | follow the edit/commit/build/test cycle and the rules for AI agents; helper-script pitfalls |
| [configgui-udp.md](configgui-udp.md) | change configuration remotely, without DB access: read tables/tree, edit values, links and rows, create a table's first version, new groups, activate, Backbone aliases, Desktop icons |
| [gateway-udp-fsm.md](gateway-udp-fsm.md) | drive the Gateway state machine (Configure/Start/Halt/Shutdown/Startup) and read status/icons over UDP with `ots_drive_udp_test`; enabling it; alias/Backbone pitfalls; **which restart a Context change needs** (total restart vs Shutdown/Startup vs App Status relaunch) and what each renews |
| [macromaker-udp.md](macromaker-udp.md) | run FE macros and universal register read/write through MacroMaker UDP |
| [emulator-registers.md](emulator-registers.md) | program `ots_udp_hw_emulator`: register map, data modes (counter/sine/multi-channel/sawtooth), the stream path to the Visualizer |
| [tutorial-to-visualizer-waveforms.md](tutorial-to-visualizer-waveforms.md) | go from the first demo tutorial to live waveforms in the Visualizer window |
| [sunrock-fe-context.md](sunrock-fe-context.md) | the Sunrock placeholder FE (repo `otsdaq-td-ims-asic`) in its own context and FE Supervisor: config rows, install steps, checks |
| [ots-subsystems.md](ots-subsystems.md) | run a top-level ots and its satellite subsystems (Remote Gateways) from one lite area: `source setup_ots.sh <name>`, `Data_<name>`, shared database, the icon + `SubsystemUserDataPathsTable` pair, Subsystem Launch page, `RemoteSubsystems.txt`, per-instance ports, recipe, log checks, pitfalls |
| [trace-and-logging.md](trace-and-logging.md) | use TRACE and MessageFacility; where logs go in lite |

## Keeping this index current

Add a row when you add a skill and update the row when a skill's scope changes. Skills should
state what was verified and when, and link to related skills instead of repeating them.
