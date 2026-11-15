# Reading the web Console without a browser

The Console window shows messages that every app sends to the Console Supervisor. An agent has
no browser, and the Console has **no UDP remote control** (unlike Configuration, Macro Maker and the
Gateway): its requests (`GetConsoleMsgs`, `SetTraceLevels`, ...) are HTTP only, behind the web login.
`ConsoleSupervisor.cc` handles them in `request()`. So an agent reads the same messages from the logs.

| Want | Use |
|---|---|
| state machine events, Configure group/key, rejections | `ai_scripts/gateway_log_events.sh [-n N] [-s] [-a]` |
| every ERROR/WARNING of the Gateway | `gateway_log_events.sh -a` |
| messages of another context | `$USER_DATA/Logs/otsdaq_quiet_run-<host>-<port>.txt` (grep ERROR/WARNING) |
| why a context died at launch | the crash block `ots` prints, same file (`ots` reports exits up to 30 s after launch) |
| status and last error text of every app | `ots_drive_udp_test <ip> <port> status`, `gateway_udp_cmd.sh ... GetRemoteAppStatus` (`detail`, `Failed:::`) |

Every app's console output lands in its context's quiet-run log even when the Console is not shown.
Log lines: lite/full `|date time:LEVEL:app: file:line |<TAB>text`; maxlite `HH:MM:SS.mmm LEVEL app file:line |<TAB>text`.
Messages the Console receives over UDP use `UDPMFMESSAGE|time|seq|host|addr|Severity|...`
(`ai_skills/ots-maxlite-architecture.md`); an agent with a login can use the HTTP request
`GetConsoleMsgs` instead (needs the web user, not covered by the UDP tools).

Note: TRACE level controls of the Console page are not available in maxlite; `OTS_COUT_LEVEL` sets
the threshold there (`ots-maxlite-architecture.md`).

Derived from the source and the logs of 2026-11-14; the HTTP route was not exercised.
