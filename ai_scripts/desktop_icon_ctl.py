#!/usr/bin/env python3
"""desktop_icon_ctl.py - list, hide, show or delete Desktop icons of the active Context group
(the counterpart of add_desktop_icon.sh).

  list                          icons of the active Context: IconName, Status, Caption, window URL
  off <icon> | on <icon>        set Status (hide/show); <icon> is the IconName or the Caption
  delete <icon>                 remove the row (prefer off: it keeps the row and its window
                                parameters, and is undone with on)
  options for off/on/delete:    -n dry run, -w accept tree warnings (passed to cfg_udp_edit_group.sh)

Changes save a new DesktopIconTable version and a new Context group key, and activate it. The
Gateway reads the icons at start: apply with apply_config_reload.sh <fsm> (Shutdown/Startup) or
restart ots. Check with: gateway_udp_cmd.sh <ip> <gateway UDP port> GetRemoteDesktopIcons

Prerequisites: source setup_ots.sh; ots running with OTS_CONFIG_UDP_IP/OTS_CONFIG_UDP_PORT; a
Kerberos ticket for changes.
"""
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import unquote

HERE = os.path.dirname(os.path.abspath(__file__))
CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")


def cfg(*args):
    r = subprocess.run(["ots_cfg_udp_test", "-y", CFG_IP, CFG_PORT] + list(args), capture_output=True, text=True)
    text = r.stdout + r.stderr
    i = text.find("<ROOT>")
    if i < 0:
        sys.exit("No reply from the Configuration GUI at %s:%s: %s" % (CFG_IP, CFG_PORT, text.strip()[:300]))
    data = ET.fromstring(text[i:text.rindex("</ROOT>") + 7]).find("DATA")
    err = data.find("Error")
    if err is not None:
        sys.exit("ERROR from the Configuration GUI: %s" % err.get("value").strip()[:400])
    return data


def icons():
    d = cfg("getActiveTableGroups")
    g, k = d.find("Context-ActiveGroupName").get("value"), d.find("Context-ActiveGroupKey").get("value")
    m = cfg("getSpecificTableGroup", "groupName=" + g, "groupKey=" + k)
    ver = dict(zip((e.get("value") for e in m.iter("MemberName")), (e.get("value") for e in m.iter("MemberVersion"))))
    if "DesktopIconTable" not in ver:
        sys.exit("The active Context %s key %s has no DesktopIconTable." % (g, k))
    t = cfg("getSpecificTable", "tableName=DesktopIconTable", "version=" + ver["DesktopIconTable"])
    headers = [e.get("value") for e in t.find("CurrentVersionColumnHeaders").iter("ColumnHeader")]
    cols = [[unquote(v) for v in e.get("value").split(",")] if e.get("value") else []
            for e in t.find("CurrentVersionRows").iter("ColCSV")]
    rows = [dict(zip(headers, (c[r] for c in cols))) for r in range(max((len(c) for c in cols), default=0))]
    return g, k, ver["DesktopIconTable"], rows


def main():
    args = sys.argv[1:]
    flags = [a for a in args if a in ("-n", "-w")]
    args = [a for a in args if a not in ("-n", "-w")]
    if not args or args[0] not in ("list", "on", "off", "delete") or (args[0] != "list" and len(args) != 2):
        print(__doc__)
        return 1
    g, k, v, rows = icons()
    if args[0] == "list":
        print("Desktop icons of the active Context %s key %s (DesktopIconTable v%s):" % (g, k, v))
        for r in rows:
            print("  %-26s %-4s %-22s %s" % (r.get("IconName"), r.get("Status"), r.get("Caption"), r.get("WindowContentURL", "")))
        return 0
    want = args[1]
    match = [r for r in rows if want in (r.get("IconName"), r.get("Caption"))]
    if len(match) != 1:
        sys.exit("%s icon named '%s' (IconName or Caption). Run: %s list"
                 % ("No" if not match else "More than one", want, os.path.basename(sys.argv[0])))
    uid = match[0]["IconName"]
    if args[0] in ("on", "off"):
        status = "On" if args[0] == "on" else "Off"
        if match[0].get("Status") == status:
            print("Icon %s (%s) is already %s: nothing to change." % (uid, match[0].get("Caption"), status))
            return 0
        edit = ["DesktopIconTable/%s/Status=%s" % (uid, status)]
    else:
        edit = ["--", "-DesktopIconTable/%s" % uid]
    cmd = [os.path.join(HERE, "cfg_udp_edit_group.sh"), "-t", "Context",
           "-c", "Desktop icon %s %s (desktop_icon_ctl.py)" % (uid, args[0])] + flags + edit
    print("Running:", " ".join(cmd), flush=True)
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
