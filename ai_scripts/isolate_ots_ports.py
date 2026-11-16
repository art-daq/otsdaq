#!/usr/bin/env python3
"""isolate_ots_ports.py - list the ports of an installation, find conflicts, move them by an offset.

Every fixed port of an otsdaq installation is either a cell of a table in an active group (context
ports, Gateway UDP, front-end interface and host ports, data listener and streamer ports, ...) or
an environment variable of setup_ots.sh (OTS_MAIN_PORT, OTS_CONFIG_UDP_PORT, ...). A second
installation on the same host needs all of them moved. This script:

  1. lists the port cells of the active Context and Configuration groups (any column whose name
     contains "Port"), and the OTS_*PORT* environment variables,
  2. marks each port that is in use on this host by a process that is NOT this installation
     (the process command line does not contain $USER_DATA) or by another user: a conflict,
  3. with --offset N: shows the new value of every literal port (cells that hold ${VAR} follow
     the environment), checks the new ports for conflicts, and with --apply saves them with
     cfg_udp_edit_group.sh (one new group key per group, activated), then prints the
     environment variables to change in setup_ots.sh (this script does not edit that file).

Prerequisites: source setup_ots.sh; ots running with OTS_CONFIG_UDP_IP/OTS_CONFIG_UDP_PORT set
(the Configuration GUI remote control); a Kerberos ticket for --apply.

Usage:
  isolate_ots_ports.py                     list ports and conflicts (read only)
  isolate_ots_ports.py --offset 100        also show the moved ports and their conflicts
  isolate_ots_ports.py --offset 100 --apply
  options: --groups Context,Configuration  group types to scan (default both)
           --include-off                   also move ports of rows whose Status is Off
           --accept-warnings               save groups despite tree warnings (cfg_udp_edit_group.sh -w);
                                           a Context often has some already (links to tables of the
                                           Configuration group), which the browser asks you to accept

After --apply the new ports take effect only after a total restart, twice for context ports
(configuration-model.md, "Which step makes a change take effect"): echo Y | ots -k; ots.
Exit status: 0 ok (no conflict), 1 error, 2 conflicts found.
"""
import argparse
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import unquote

HERE = os.path.dirname(os.path.abspath(__file__))
CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")
USER_DATA = os.environ.get("USER_DATA", "")


def cfg(*args):
    """read-only ConfigGUI request; returns the <DATA> element"""
    r = subprocess.run(["ots_cfg_udp_test", "-y", CFG_IP, CFG_PORT] + list(args),
                       capture_output=True, text=True)
    text = r.stdout + r.stderr
    i = text.find("<ROOT>")
    if i < 0:
        sys.exit("No reply from the Configuration GUI at %s:%s: %s" % (CFG_IP, CFG_PORT, text.strip()[:300]))
    root = ET.fromstring(text[i:text.rindex("</ROOT>") + 7])
    data = root.find("DATA")
    err = data.find("Error")
    if err is not None:
        raise RuntimeError(err.get("value"))
    return data


def active_groups():
    d = cfg("getActiveTableGroups")
    out = {}
    for t in ("Context", "Configuration", "Backbone", "Iterate"):
        n = d.find("%s-ActiveGroupName" % t)
        k = d.find("%s-ActiveGroupKey" % t)
        if n is not None and n.get("value") and k.get("value") != "-1":
            out[t] = (n.get("value"), k.get("value"))
    return out


def group_members(name, key):
    d = cfg("getSpecificTableGroup", "groupName=" + name, "groupKey=" + key)
    names = [e.get("value") for e in d.iter("MemberName")]
    vers = [e.get("value") for e in d.iter("MemberVersion")]
    return list(zip(names, vers))


def table(name, version):
    """returns (headers, uid column, rows as dicts)"""
    d = cfg("getSpecificTable", "tableName=" + name, "version=" + version)
    hdr = d.find("CurrentVersionColumnHeaders")
    headers = [e.get("value") for e in hdr.iter("ColumnHeader")]
    types = [e.get("value") for e in hdr.iter("ColumnType")]
    uidcol = headers[types.index("UID")] if "UID" in types else headers[0]
    cols = [[unquote(v) for v in (e.get("value") or "").split(",")] if e.get("value") else []
            for e in d.find("CurrentVersionRows").iter("ColCSV")]
    nrows = max((len(c) for c in cols), default=0)
    rows = [{h: (cols[i][r] if r < len(cols[i]) else "") for i, h in enumerate(headers)} for r in range(nrows)]
    return headers, uidcol, rows


def listening():
    """port -> list of (proto, pid, user, command line) for every listening socket on this host"""
    out = {}
    r = subprocess.run(["ss", "-H", "-tulnp"], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        f = line.split()
        if len(f) < 5:
            continue
        port = f[4].rsplit(":", 1)[-1]
        if not port.isdigit():
            continue
        pids = re.findall(r"pid=(\d+)", line)
        if not pids:
            out.setdefault(int(port), []).append((f[0], None, "?", "(process of another user)", ""))
        for pid in sorted(set(pids)):
            try:
                cmd = open("/proc/%s/cmdline" % pid, "rb").read().replace(b"\0", b" ").decode(errors="replace").strip()
                cwd = os.readlink("/proc/%s/cwd" % pid)
                user = subprocess.run(["ps", "-o", "user=", "-p", pid], capture_output=True, text=True).stdout.strip()
            except OSError:
                cmd, cwd, user = "?", "", "?"
            out.setdefault(int(port), []).append((f[0], pid, user, cmd, cwd))
    return out


INSTALL_DIR = os.path.dirname(os.path.realpath(USER_DATA)) if USER_DATA else ""


def ours(sock):
    """a process of this installation: its command line names USER_DATA, or it runs inside the
    installation directory (e.g. ots_udp_hw_emulator started from there)"""
    return bool(USER_DATA) and (USER_DATA in sock[3] or (sock[4] + "/").startswith(INSTALL_DIR + "/"))


def owner_text(port, socks):
    """'' if free or held by this installation, else a conflict description"""
    others = [s for s in socks.get(port, []) if not ours(s)]
    if not others:
        return ""
    p = others[0]
    return "IN USE: %s pid %s user %s: %s" % (p[0], p[1] or "?", p[2], p[3][:90])


def state_text(port, socks):
    conflict = owner_text(port, socks)
    if conflict:
        return conflict
    mine = [s for s in socks.get(port, []) if ours(s)]
    if mine:
        return "this installation (%s pid %s %s)" % (mine[0][0], mine[0][1], os.path.basename(mine[0][3].split()[0]))
    return "free"


def resolve(value):
    """value of a cell: int, or (int, 'VAR') for ${VAR}, or None"""
    m = re.fullmatch(r"\$\{?(\w+)\}?", value.strip())
    if m:
        v = os.environ.get(m.group(1), "")
        return (int(v) if v.isdigit() else None, m.group(1))
    return (int(value), None) if value.strip().isdigit() else (None, None)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--offset", type=int, default=0)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--groups", default="Context,Configuration")
    ap.add_argument("--include-off", action="store_true")
    ap.add_argument("--accept-warnings", action="store_true")
    a = ap.parse_args()
    if a.apply and not a.offset:
        sys.exit("--apply needs --offset N")
    if not USER_DATA:
        sys.exit("USER_DATA is not set: source setup_ots.sh first.")

    socks = listening()
    groups = active_groups()
    cells = []  # (groupType, table, uid, column, raw value, port, var, status)
    for gtype in a.groups.split(","):
        if gtype not in groups:
            print("No active %s group." % gtype)
            continue
        gname, gkey = groups[gtype]
        print("Active %s group: %s key %s" % (gtype, gname, gkey))
        for tname, tver in group_members(gname, gkey):
            if tname in ("TableGroupMetadataTable",):
                continue
            headers, uidcol, rows = table(tname, tver)
            pcols = [h for h in headers if "port" in h.lower() and h != uidcol]
            if not pcols:
                continue
            for row in rows:
                uid = row[uidcol]
                status = row.get("Status", "On")
                for c in pcols:
                    port, var = resolve(row[c])
                    if port is None and var is None:
                        continue  # empty or not a number (e.g. a port name)
                    cells.append((gtype, tname, uid, c, row[c], port, var, status))

    env = sorted((k, int(v)) for k, v in os.environ.items() if k.startswith("OTS_") and "PORT" in k and v.isdigit())

    conflicts = 0
    print("\nPorts in the active groups (On rows first):")
    print("  %-14s %-34s %-24s %-28s %-7s %s" % ("group", "table", "row", "column", "port", "state on this host"))
    for c in sorted(cells, key=lambda c: (c[7] == "Off", c[1], c[2])):
        gtype, tname, uid, col, raw, port, var, status = c
        own = owner_text(port, socks) if port else ""
        conflicts += bool(own) and status != "Off"
        shown = "%s (%s)" % (port, "$" + var) if var else str(port)
        print("  %-14s %-34s %-24s %-28s %-7s %s%s" % (gtype, tname, uid[:24], col[:28], shown, state_text(port, socks) if port else "",
                                                       "  [row Off]" if status == "Off" else ""))
    print("\nPorts from the environment (setup_ots.sh):")
    for k, v in env:
        own = owner_text(v, socks)
        conflicts += bool(own)
        print("  %-28s %-7s %s" % (k, v, state_text(v, socks)))

    if a.offset:
        print("\nWith offset %+d:" % a.offset)
        edits = {}
        notes = []
        for gtype, tname, uid, col, raw, port, var, status in cells:
            if var or not port or (status == "Off" and not a.include_off):
                continue
            new = port + a.offset
            own = owner_text(new, socks)
            print("  %-34s %-24s %-28s %5d -> %-5d %s" % (tname, uid[:24], col[:28], port, new, own or "free"))
            edits.setdefault(gtype, []).append("%s/%s/%s=%d" % (tname, uid, col, new))
            if col == "InterfacePort" and any("ots_udp_hw_emulator" in sk[3] for sk in socks.get(port, [])):
                notes.append("the emulator listens on %d: after the restart run emulator_ctl.sh stop %d; emulator_ctl.sh start %d" % (port, port, new))
        for n in notes:
            print("  NOTE: " + n)
        print("\n  environment (edit setup_ots.sh yourself; cells written as ${VAR} follow it):")
        for k, v in env:
            own = owner_text(v + a.offset, socks)
            print("    export %s=%d   %s" % (k, v + a.offset, own or ""))
        if a.apply:
            for gtype, ed in edits.items():
                print("\n=== %s group: %d port edits" % (gtype, len(ed)), flush=True)
                r = subprocess.run([os.path.join(HERE, "cfg_udp_edit_group.sh"), "-t", gtype,
                                    "-c", "Ports moved by %+d (isolate_ots_ports.py)" % a.offset]
                                   + (["-w"] if a.accept_warnings else []) + ed)
                if r.returncode != 0:
                    sys.exit("cfg_udp_edit_group.sh failed for the %s group (tree warnings above? rerun with "
                             "--accept-warnings if they were there before)." % gtype)
            print("\nDone. Change the environment lines above in setup_ots.sh, then restart ots (twice for context ports).")
        else:
            print("\n(not applied: add --apply)")

    print("\n%d conflict(s) with processes outside this installation." % conflicts)
    return 2 if conflicts else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as e:
        sys.exit("ERROR from the Configuration GUI: %s" % str(e).strip()[:400])
