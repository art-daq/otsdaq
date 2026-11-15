# Security, users and permissions

What decides who may do what in otsdaq: the security mode, the user accounts, permission levels and groups, the
per-request thresholds, the user lock, the Wiz mode access code, IP lists. Read it before telling an agent which
requests it may send, or before exposing an installation beyond localhost. Written from `WebUsers.{h,cc}`,
`WizardSupervisor.cc` and the supervisors' property defaults; the facts marked *seen* were read on the running
ots maxlite test install (2026-11-15). Nothing here was changed on a running system.

## The UDP remote controls skip all of this

The Gateway, Configuration and Macro Maker UDP remote controls (`gateway-udp-fsm.md`, `configgui-udp.md`,
`macromaker-udp.md`) have **no login**: whoever can send a UDP packet to the port acts as an automated user
(Configuration edits are attributed to the Kerberos user of `ots_cfg_udp_test`). Treat the port as the access
control: keep them on `127.0.0.1` unless you mean otherwise, never publish the port, and name the allowed addresses
in the agent's first message (`agent-first-message.md`). Everything below governs the **web** interface (browser).

## Security mode

`$USER_DATA/ServiceData/OtsWizardData/security.dat` holds one word, read at start:

| Value | Meaning |
|---|---|
| `NoSecurity` (default; also when the file is missing or unreadable) | no login: every browser user is the account `admin`; cookie codes are not checked. *Seen:* log line "The current security type is NoSecurity" |
| `DigestAccessAuthentication` | accounts with passwords (salted SHA-512), cookie codes on every request, login failure counting |

With NoSecurity, requests listed as requiring security (below) are refused in normal mode (everyone is `admin`).
Switching the mode is a Wiz mode action in the browser; the file is plain text, so an agent could write
it, but the effect after a restart was **not tested here**: do not switch a shared installation.

## Accounts and permission levels

Accounts live in `$USER_DATA/ServiceData/LoginData/UsersData/users.xml` (history and preferences next to it, session
and hash data in `HashesData`). *Seen:* a fresh install has one account, `admin`, display name `Administrator`,
permissions `allUsers:255`, **no password yet** (empty salt): in DigestAccessAuthentication mode the Gateway prints
a one-time **new account code** (NAC) for 10 s at start, which the person opening the page uses to set the password.

A user has a permission level **per group**: `<group>:<level>` pairs, always including the group `allUsers`.

| Level | Meaning |
|---|---|
| 0 | inactive: can not log in (also the state after too many failed logins) |
| 1 | novice |
| 10 | user (experienced: edits the configuration) |
| 100 | expert |
| 255 | admin of the group |

Admins create, change and delete accounts in the Desktop's Settings (request `accountSettings`: createAccount,
updateAccount, deleteAccount). Rules from the code: only an admin may; the first account (`admin`) can not be modified
except that its **password can be reset by itself** (clears its salt, so a new NAC is issued); user names and display
names have minimum lengths and display names must be unique; setting an inactive account active again resets its
failure count and password (that is the unlock mechanism). Limits: 20 failed logins deactivate an account, a login
session allows 5 attempts and lasts 5 minutes, an active session cookie 120 minutes (renewed during use), an IP is
blacklisted after 200 failures.

## What a request needs: thresholds per supervisor

Each supervisor has properties (Supervisor Properties = rows of `XDAQApplicationPropertyTable`, defaults in its
C++), read with `ots_cfg_udp_test`/`configgui-udp.md` like any table:

| Property | Meaning |
|---|---|
| `UserPermissionsThreshold` | `*=<level> \| <requestType>=<level> \| ...`: the user's highest level among the allowed groups must reach it. Example (Configuration GUI default): `*=10 \| deleteTreeNodeRecords=255 \| saveTableInfo=255 \| deleteTableInfo=255`: editing needs level 10, deleting needs an admin |
| `UserGroupsAllowed` / `UserGroupsDisallowed` | restrict by group (wildcards): if allowed groups are given the user must be in one, else a disallowed group refuses |
| `RequireUserLockRequestTypes` / `CheckUserLockRequestTypes` | request types that need the **user lock** (only the user holding it may edit; Configuration GUI: all, except read-only `get*`) |
| `AutomatedRequestTypes` | request types without a user (read-only access, e.g. Configuration `get*`) |
| `AllowNoLoginRequestTypes` | request types that need no login |
| `RequireSecurityRequestTypes` | request types refused unless security is "sufficient" (below). Code Editor: `codeEditor` (write access) |

*Seen:* the tutorial Context sets none of these properties, so the C++ defaults apply.

**Sufficient security**, for a request whose threshold is above 1 and is listed in `RequireSecurityRequestTypes`:
in **Wiz mode** the access code must have **8 or more characters** (otherwise the request is refused with
`NoPermission` and the Code Editor opens read-only); in **normal mode** the user must not be `admin` (nor the
Iterator or state-changer pseudo users): individual named accounts, i.e. DigestAccessAuthentication, for personal
accountability of edits.

## The user lock

One user at a time holds the lock for lock-protected requests (e.g. Configuration edits). A user takes it for
themselves (an admin may take it for another user); only the holder can release it, which leaves it unheld; 30
minutes of inactivity release it automatically. The holder is saved in `ServiceData/LoginData/user_with_lock.dat`
(no file: `admin`); *seen:* `admin` on the test install. A request needing the lock from another user fails until
the holder releases it.

## Wiz mode access code

`ots -w` prints a URL with a one-time code (`.../Verify?code=<code>`, also in `ServiceData/OtsWizardData/sequence.out`;
`ots_mode.sh status` prints it). The length comes from `OtsWizardData/sequence.dat` (a number, minimum 4):

| `sequence.dat` | Code |
|---|---|
| missing | 4 characters from a **fixed seed**: the same code at every start. Convenient, not a secret |
| number N >= 4 | N random letters and digits, new at every start; N >= 8 unlocks the security-required requests above |

Anyone with the URL has admin rights in Wiz mode: do not expose the Wiz port.

## IP lists and the network

`$USER_DATA/ServiceData/LoginData/ip_accept.dat` (wildcard patterns, one per line: accepted at once), `ip_reject.dat`
(refused) and the generated `ip_generated_blacklist.dat` (after repeated failures) apply to the web server.
Checked in that order: accept, reject, blacklist; an IP in none is accepted; the files are re-read every 10 minutes.
Beyond that otsdaq expects to sit behind your network's access control or a
reverse proxy that does the TLS; the web server itself speaks plain HTTP. The README mentions CILogon certificate
access for accounts; its set-up is not covered here and was not checked.

## For agents

- Ask the person for the mode and the account you may use; never change `security.dat`, `sequence.dat`, `users.xml`,
  the IP lists or the user lock on an installation you do not own.
- A `NoPermission` reply to a web request is a threshold, group, lock or security-mode refusal: the Gateway log has
  the reason ("has insufficient permissions for requestType ...: user level is X, Y required").
- To see the effective thresholds of a supervisor, its start-up log prints each property
  (`CorePropertySupervisorBase.cc` lines in the context's quiet-run log).

Not tested: DigestAccessAuthentication logins, account creation, the lock hand-over, IP lists, CILogon.
