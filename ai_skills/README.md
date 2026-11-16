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
| [config-store-conversion.md](config-store-conversion.md) | move configurations between store types (`filesystemdb://` artdaq-database, `textdb://` text store): the whole store with numbers kept, the active groups and aliases renumbered (merge or fresh), one table as CSV |
| [configuration-model.md](configuration-model.md) | know the model before editing: table definition, version (temporary = negative), group and key, group types and their member tables, group and version aliases in the Backbone, how Configure resolves an alias, which step makes each kind of change take effect, undo |
| [agent-first-message.md](agent-first-message.md) | write the first message to an agent: the template, which remote-control addresses to name, what to allow (state machine commands), what to forbid on a shared host, why agents stop |
| [isolated-agent-sandbox.md](isolated-agent-sandbox.md) | run an installation next to others on one host without touching them: what separates installations (files, TCP/UDP ports, emulator, processes), moving ports with `isolate_ots_ports.py`, what never to run (`killall`, `ots -K`, `reset_ots_tutorial.sh`), stopping only your own |
| [development-workflow.md](development-workflow.md) | follow the edit/commit/build/test cycle and the rules for AI agents; helper-script pitfalls |
| [configgui-udp.md](configgui-udp.md) | change configuration remotely, without DB access: read tables/tree, edit values, links and rows, create a table's first version, new groups, activate, Backbone aliases, Desktop icons |
| [gateway-udp-fsm.md](gateway-udp-fsm.md) | drive the Gateway state machine (Configure/Start/Halt/Shutdown/Startup) and read status/icons over UDP with `ots_drive_udp_test`; enabling it; alias/Backbone pitfalls; **which restart a Context change needs** (total restart vs Shutdown/Startup vs App Status relaunch) and what each renews |
| [app-status-monitoring.md](app-status-monitoring.md) | understand `EnableApplicationStatusMonitoring`: `Not Monitored`, the two status threads, how a dead app makes the Gateway `Failed` (not during Shutdown/Startup), disk-space alarms, the status fields |
| [iterator-plans.md](iterator-plans.md) | the Iterator: plan/step/command/target/loop tables, every step type, `iteration_plan.py` to create and edit plans, applying (restart for edited plans), playing and checking over UDP |
| [macromaker-udp.md](macromaker-udp.md) | run FE macros and universal register read/write through MacroMaker UDP |
| [fe-macros-and-macromaker.md](fe-macros-and-macromaker.md) | work with FE macros (C++ in an FE plugin) and Macro Maker macros (register read/write/delay files): find and run them with `mm_macro.py`, argument/number rules, the macro file format and its hex/variable traps, writing and editing an FE macro (header, `registerFEMacroFunction`, body helpers), promoting a Macro Maker macro to C++, sequences and Iterator use, troubleshooting |
| [emulator-registers.md](emulator-registers.md) | program `ots_udp_hw_emulator`: register map, data modes (counter/sine/multi-channel/sawtooth), the stream path to the Visualizer |
| [data-path-configuration.md](data-path-configuration.md) | the data path: DataManagerSupervisor -> DataBuffer -> DataProcessor (producers, consumers, plugins), the tutorial's two buffers, switching a consumer off, where raw files go and their names, run numbers |
| [data-processor-plugins.md](data-processor-plugins.md) | the plugins in the data path: the producers and consumers that exist (columns of each settings table), Data Manager lifecycle (consumers start first, producers stop first), buffer rules (every consumer sees every packet, a slow consumer stalls the producer), adding a processor to a configuration as an agent, writing a plugin (`new_data_processor.py`), troubleshooting; the Pause/Resume saver bug |
| [table-plugins.md](table-plugins.md) | table plugins: what one is (the loader falls back silently to a plain table) and when you need none (most tables: reading settings, new tables, rows, links), writing one (`new_table_plugin.py`: `init()`, typed rows, `getValue` conversions, threads, errors), build and library naming, checking that it loaded (missing library is silent: verified), troubleshooting |
| [tutorial-to-visualizer-waveforms.md](tutorial-to-visualizer-waveforms.md) | go from the first demo tutorial to live waveforms in the Visualizer window |
| [sunrock-fe-context.md](sunrock-fe-context.md) | the Sunrock placeholder FE (repo `otsdaq-td-ims-asic`) in its own context and FE Supervisor: config rows, install steps, checks |
| [ots-subsystems.md](ots-subsystems.md) | run a top-level ots and its satellite subsystems (Remote Gateways) from one lite area: `source setup_ots.sh <name>`, `Data_<name>`, shared database, the icon + `SubsystemUserDataPathsTable` pair, Subsystem Launch page, `RemoteSubsystems.txt`, per-instance ports, recipe, log checks, pitfalls |
| [console-without-browser.md](console-without-browser.md) | read Console messages as an agent: no UDP remote control exists, use the Gateway/context logs, `gateway_log_events.sh`, app status |
| [security-and-users.md](security-and-users.md) | web security: `NoSecurity` vs `DigestAccessAuthentication`, accounts and permission levels (0/1/10/100/255 per group), per-supervisor thresholds (`UserPermissionsThreshold`, lock, security-required requests), the user lock, the Wiz mode access code (length, 8+ for write access), IP lists; and that the UDP remote controls have no login |
| [trace-and-logging.md](trace-and-logging.md) | use TRACE and MessageFacility; where logs go in lite |

## Keeping this index current

Add a row when you add a skill and update the row when a skill's scope changes. Skills should
state what was verified and when, and link to related skills instead of repeating them.
