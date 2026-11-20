# How the list of apps is set up: at construction and at Startup

Which apps exist, which apps the Gateway commands in a transition, and when that knowledge is
taken from the active Context group. Read this before changing a Context (adding or removing a
context or an app, moving one to another host or port) on a running system, or when a transition
reports `could not be found in AllSupervisorInfo` or `looks to have been ignored by <host>`.

Status: sections "At construction" and "Rules before 2026-11-17" describe code that has run for a
long time. Section "At Startup" describes code written 2026-11-17 that was **read and written but
not yet built or run**; see "Not verified" at the end.

Related: [gateway-udp-fsm.md](gateway-udp-fsm.md) (how to run Shutdown/Startup and what else
Startup renews), [app-status-monitoring.md](app-status-monitoring.md) (the status threads that read
the list), [configuration-model.md](configuration-model.md) (Context groups),
[ots-lite-architecture.md](ots-lite-architecture.md) (XdaqLite).

## The rule

- The list of apps is set at two moments only: when a process is constructed, and, for the
  Gateway, at every **Startup** transition. Startup is the Gateway's "constructor" for the app list.
- Activating a Context group does **not** change the list. A new active Context takes hold at the
  next Startup (or at a total restart).
- Between a Startup and the next Shutdown the list is fixed: every transition (Initialize,
  Configure, Start, Halt, ...) targets the same apps.
- Shutdown targets the hosts of the apps that are running now (the old list), not the hosts of the
  newly activated Context.

So the way to apply a Context layout change without relaunching ots is:
Halt -> Shutdown -> activate the new Context group -> Startup -> Initialize.
(Activating before the Shutdown also works; Shutdown uses the running list.)

## Three places hold the Context layout

| Where | What it holds | Taken at |
|-------|---------------|----------|
| ots launch script (`otsdaq/tools/ots`) | `XDAQConfigurations/ots.xml`, a copy of the run xml (`otsConfiguration_CMake_Run.xml`), parsed into host/port arrays to launch `xdaq.exe` per context | full launch (script copies it); every Startup and App Status relaunch-all (the Gateway copies it, see below) |
| XDAQ in each process | application descriptors in group `daq` of the default zone (one per app of every context in the xml the process was started with) | process start; the Gateway adds missing ones at Startup |
| `AllSupervisorInfo allSupervisorInfo_` (member of `CorePropertySupervisorBase`, so every supervisor has its own) | `SupervisorInfo` per app (LID, class, name and context name from the Context table, URL, hostname, status), per-type maps (FE, DM, Logbook, MacroMaker, TRACE controller per host), and the app set with transition priorities | supervisor construction; Gateway only: every Startup |

The run xml is rewritten from the active Context whenever a configuration manager that is "first
app in context" initializes (`XDAQContextTable::init`), e.g. the Gateway's `cfgMgr->init()`.

## At construction

`CorePropertySupervisorBase` constructor calls `allSupervisorInfo_.init(applicationContext)`
(`otsdaq/otsdaq/SupervisorInfo/AllSupervisorInfo.cc`):

1. `destroy()` clears everything.
2. `SupervisorDescriptorInfoBase::init` reads the XDAQ descriptors of group `daq` (throws on a
   duplicate LID).
3. First descriptor that is a Gateway or a Wizard supervisor decides normal or wizard mode.
4. In normal mode a read-only `ConfigurationManager` loads the active `XDAQContextTable`.
5. `organizeSupervisors(contextConfig)`: stores the app set for transitions
   (`stateMachineApplicationSet_`: app LID and per-command priority, from the enabled apps of the
   Context), creates one `SupervisorInfo` per descriptor (name/context name looked up by context
   URL and LID; empty name becomes `LID-<id>`), fills the per-type maps, checks there is exactly
   one Gateway (or Wizard), builds the TRACE controller map (one app per host, the artdaq
   supervisor first).

After a total launch the FSM is at `Initial` without a Startup having run; construction did the
same work.

## At Startup (Gateway only, since 2026-11-17)

`GatewaySupervisor::transitionStartingUp`, in order:

1. `stopBackgroundWorkLoops()`: the UDP state changer and both app status threads are stopped and
   joined, so nothing else reads the list.
2. Remember the hostnames of the running list (`getLiveContextHostnames()`).
3. `launchStartOTSCommand("OTS_APP_STARTUP", cfgMgr, &ignoredHostnames)`:
   - `cfgMgr->init()` re-reads the active groups and rewrites the run xml;
   - the Gateway copies the run xml to `ots.xml` **before** any script is commanded (so no host
     parses the file while it is written, and the scripts launch the active Context's contexts);
   - writes `OTS_APP_STARTUP` to `ServiceData/StartOTS_action_<host>.cmd` for every host of the
     active Context and checks after 2 s that each script took it.
4. A host that did not take the command: if it was in the running list, Startup fails
   ("looks to have been ignored by ..."). If it is new to the Context it has no script yet: its
   command file is cleared, a warning is logged, and the launch script on the Gateway host starts
   it by ssh (next section).
5. Under `stateMachineAccessMutex_` (blocks web requests and state machine commands):
   `localAllSupervisorInfo_.clear()` and `allSupervisorInfo_.rebuildFromActiveContext(...)`:
   - **register**: every enabled app of the active Context with no descriptor of the same LID and
     context URL in this process is registered with XDAQ (`ContextTable::createContextDescriptor`
     if the context URL is new, `ApplicationDescriptorFactory::createApplicationDescriptor` into
     group `daq`, `setInstance`; in OTS_LITE `ApplicationContext::addRemoteApplication`). These are
     the calls XDAQ itself makes when it loads the xml at process start;
   - **leave out**: XDAQ cannot remove a descriptor, so descriptors whose (URL, LID) is not an
     enabled app of the active Context are skipped. Apps of the Gateway's own process are always
     kept;
   - **rebuild**: `destroy()`, then `organizeSupervisors` on the remaining descriptors; this also
     stores the app set for transitions. If the rebuild throws, the previous list is put back and
     Startup fails.
6. `startBackgroundWorkLoops()` and `reloadSupervisorProperties()`.
7. Wait 5 s (for the launch scripts to take the command), start the wait for all apps to answer
   (next section), then broadcast Startup to the Gateway-context apps of the new list.

### Waiting for all apps to answer (since 2026-11-18, not yet built or run)

After a full launch (`init()`) and at Startup, `startWaitForAllAppsToAnswer()` starts a thread
(`AppsAnswerWaitWorkLoop`) that sends every app the `SystemTransitionNotice` with
`Notice = AreYouReady` (2 s reply timeout) and asks the ones that gave no answer again every 2 s.
There is no time limit. `CoreSupervisorBase::systemTransitionNotice` only answers it (it does not
mark a transition as in progress, so MacroMaker requests are not blocked).

While the wait runs the state machine **is** at `Initial`, but `Initial` is not shown:

- the Gateway's own status in App Status is `Starting Up`, progress = percent of apps that
  answered, detail = `Waiting for N of M apps to answer 'are you ready?' ...: <names>`
  (this is also what a top-level Gateway sees for the subsystem);
- `addStateMachineStatusToXML` gives `in_transition=1`, `current_transition=Starting Up`,
  `transition_progress` and `active_fsmStatus` with the same text; `current_state` stays `Initial`;
- commands other than Halt/Abort/Shutdown/Error/Fail are refused with
  `Can not accept '<command>' yet because the system is still starting up. Waiting for ...`.

The wait ends when all apps have answered (`All N app(s) answered 'are you ready?' after S
seconds`), at Shutdown, at the next Startup, or when the user cancels the transition from the
state machine page (`cancelStateMachineTransition`). An app that never comes up is named in a
warning once per minute; the way out is Shutdown (fix, then Startup) or the cancel.

### What the launch script does on `OTS_APP_STARTUP`

`otsActionHandler` in `otsdaq/tools/ots` runs `launchOTS nongateway`: kills non-gateway contexts,
parses `ots.xml`, launches the contexts whose host is this host. On the Gateway host it then also
starts, by ssh (`ots_remote_start`), each host that it has not started before
(`REMOTELAUNCH_NEW_HOSTS_ONLY=1` keeps the `remoteLaunchHosts` list instead of clearing it). Hosts
started at the full launch got the same command themselves and are not ssh-started again. If
remote launch was disabled for that script (`ots -s`, or the script is a remote one), a new host
must be started by hand (`ots -x` there, or `ots_remote_start`).

## What a Startup does not cover (total restart still needed)

- An app added to, removed from, or changed inside the **Gateway's own context** (its process is
  not relaunched, so the app object cannot be created). A warning is logged and the app is absent.
- A change of the Gateway context's own host/port, or the Gateway's LID.
- An app that keeps its LID and context URL but changes class: the Gateway keeps the old class in
  its descriptor (warning logged). The relaunched process itself has the new class.
- A new app reusing the LID of an app in the Gateway's own context: Startup fails with
  "Duplicate Application IDs".
- Other supervisors in the Gateway's process (ConfigGUI, MacroMaker, Console, ...) keep the list
  they built at construction; they do not see apps added at Startup. Supervisors in relaunched
  contexts build a fresh list when they start.
- Environment variables of new contexts come from the launch script's environment.

## Errors and what they mean

| Message | Meaning |
|---------|---------|
| `... could not be found in AllSupervisorInfo. Has the Context changed? Rebooting ots may fix this issue.` | The app set for transitions names a LID the list does not have. Since the list and the set are now built together at Startup this should only appear for the cases in the section above; a Shutdown -> Startup or total restart clears it. |
| `The command '...' looks to have been ignored by <host>` | No launch script is reading the action file on that host. On Startup, only for a host that was already running; run `ots -a` there. |
| `Host '<host>' is new to the active Context and has no ots launch script running yet` (warning) | Expected for a host added to the Context; it is started by ssh from the Gateway host's script. |
| `App '<uid>' ... was added to the Gateway's own context ... a total restart of ots is needed` (warning) | See limits above. |
| `Error! Duplicate Application IDs are not allowed` at Startup | Two enabled apps of the active Context (or one new app and a Gateway-process app) share a LID. |

## Rules before 2026-11-17 (for reading old logs)

- `allSupervisorInfo_` was built once per process; `ots.xml` was copied only at a full launch.
  Any layout change needed a total restart.
- Before 2026-11-16 the app set for transitions was read from the active Context at each
  transition, so activating a Context changed which apps Halt targeted, and Shutdown wrote to
  hosts of the new Context ("ignored by <new host>").

## Not verified (as of 2026-11-17)

Nothing in "At Startup" has been built or run yet. To check on a run:

1. No Context change: Shutdown -> Startup -> Initialize -> Configure behaves as before; the
   Gateway log shows `Rebuilt info from the active Context: N apps`.
2. Add a context on a host that already runs one: after Startup its process exists
   (`Registering app ...` in the Gateway log), it appears in App Status, and Initialize/Configure
   reach it. This is the test of SOAP to an app registered after process start.
3. Disable a context: after Startup it is not launched, is gone from App Status
   (`Leaving out app LID ...`), and transitions do not target it.
4. Add a context on a host new to the system: the warning appears, the Gateway host's action
   handler log shows the ssh remote launch, and `Starting Up` is shown until that app answers.
5. Lite build (`OTS_LITE`): the same with `addRemoteApplication`.
6. App Status "relaunch all" (`LAUNCH_OTS`) still works; it now also gets the renewed `ots.xml`
   copy from the Gateway before the script makes its own.
