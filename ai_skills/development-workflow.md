# otsdaq Development Workflow

Indexes: `ai_skills/README.md` (skills) and `ai_scripts/README.md` (scripts). Start there.

## Repository Layout

- `dev-srcs/` — SSH-cloned repos (the otsdaq repos plus `otsdaq-td-ims-asic`). All edits go here. Check each repo's branch (`git branch --show-current`); it changes over time.
- `srcs/` — Disposable copies used by the build. Populated by `UpdateOTS.sh --pullall`.
- `ots-lite-test/` — The lite install area (build/, local/install/, Data/, databases/).

## Edit → Build → Test Cycle

1. **Edit** only in `dev-srcs/<repo>/`
2. **Commit** on the repo's current working branch (push only when the user asks)
3. User runs in the lite area:
   ```bash
   cd ots-lite-test
   source setup_ots.sh
   UpdateOTS.sh --pullall   # pulls into srcs/
   mb                       # incremental build
   ots                      # launch
   ```

## Rules for AI Agents

- **Never build** — only edit files in `dev-srcs/`. User handles pulls, copies, and builds.
- **Never run sudo** — if a package is missing, report it and the install command.
- **Commit on the current working branch** — local commits are OK; always announce each commit; never push unless asked.
- **Push when needed** — so user can `UpdateOTS.sh --pullall` in their lite area.
- **Derive, don't rewrite** — when creating script variants, copy + minimal edits. Show the diff.
- **No invented jargon** — use actual file/program names, not coined nicknames.
- **Build commands** — tell user `mb`, `mz`, `mz_uc`, or `UpdateOTS.sh`. Never raw cmake.

## Commit Message Style

Short first line describing the change. Optional body paragraph.
End with attribution:

```
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

## Key Files

| Purpose | File |
|---------|------|
| Lite executive | `otsdaq/XdaqLite/ots_lite_main.cc` |
| MF configuration (lite) | `otsdaq/XdaqLite/ArtdaqLite.cc` |
| `ots` launcher script | `otsdaq/tools/ots` |
| Lite installer | `otsdaq-demo/tools/ots-quick-lite-start.sh` |
| Setup script (generated) | `ots-lite-test/setup_ots.sh` |
| Gateway log | `Data/Logs/otsdaq_quiet_run-gateway-*.txt` |
| MF config | `Data/MessageFacilityConfigurations/MessageFacilityGen.fcl` |

## Debugging

- Check the gateway quiet-run log for startup errors
- Use `tlvls` / `tshow` for TRACE diagnostics
- Use `ss -ulnp` to verify UDP ports are bound
- Use `pgrep -fa xdaq` to check running processes
- `ots_mm_udp_test` to test FE communication (see `macromaker-udp.md`)
- `ots_cfg_udp_test` to inspect/modify configuration tables (see `configgui-udp.md`;
  use `-y` for read-only queries when no Kerberos ticket is available)

## Writing Helper Scripts (lessons)

- A `python3 - <<'EOF'` heredoc occupies python's stdin with the script. Piping data
  into that same command delivers nothing and fails silently (empty output). Pass
  data as an argument (`python3 - "$DATA" <<'EOF'`, read `sys.argv[1]`) or write the
  script to a temp file.
- UDP replies from otsdaq supervisors are chunked into 1500-byte datagrams. Loop on
  `recvfrom` until the XML root closes; a single read returns a fragment.
- Test a script's parsing and polling against a throwaway local fake server on a
  spare port (replying in 1500-byte chunks and changing state on a command) instead
  of the live system; run `bash -n` for syntax.
- Do not assume element order or fixed IDs in status XML; select by `<class>`.

