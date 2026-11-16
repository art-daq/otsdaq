#!/usr/bin/env python3
"""cfg_udp_diff_groups.py - show what really differs between two table groups (read-only).

Compares two group keys (of the same or different group names) member by member: tables added or
removed, tables whose version differs, and for those, rows added or removed and every changed cell
with its old and new value. Use it before activating or aliasing a key, to show what a change did.

Prerequisites: source setup_ots.sh; ots running with OTS_CONFIG_UDP_IP/OTS_CONFIG_UDP_PORT set.

Usage:
  cfg_udp_diff_groups.py <group> <keyA> <keyB>
  cfg_udp_diff_groups.py <A> <B>
     where A and B are each  <group>:<key>  |  alias:<groupAlias>  |  active:<Context|Configuration|Backbone|Iterate>
Examples:
  cfg_udp_diff_groups.py defaultConfig 3 5
  cfg_udp_diff_groups.py alias:defaultSystemAlias active:Configuration    # what Configure loads vs the active key
  cfg_udp_diff_groups.py testContext:13 testContext:15
Options: --ignore-meta  ignore the Author and RecordInsertionTime columns (default on; --all-columns to show)

Exit status: 0 identical, 1 error, 3 different.
"""
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import unquote

CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")
META = {"Author", "RecordInsertionTime"}


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


def resolve(spec):
    """'<group>:<key>' | 'alias:<name>' | 'active:<type>' -> (group, key)"""
    kind, _, val = spec.partition(":")
    if kind == "alias":
        d = cfg("getGroupAliases")
        for a, n, k in zip(d.iter("GroupAlias"), d.iter("GroupName"), d.iter("GroupKey")):
            if a.get("value") == val:
                return n.get("value"), k.get("value")
        sys.exit("No group alias '%s' in the active Backbone (cfg_udp_list_aliases.sh lists them)." % val)
    if kind == "active":
        d = cfg("getActiveTableGroups")
        n, k = d.find("%s-ActiveGroupName" % val), d.find("%s-ActiveGroupKey" % val)
        if n is None or k.get("value") == "-1":
            sys.exit("No active %s group." % val)
        return n.get("value"), k.get("value")
    if val and val.lstrip("-").isdigit():
        return kind, val
    sys.exit("Cannot read '%s': use <group>:<key>, alias:<name> or active:<type>." % spec)


def members(group, key):
    d = cfg("getSpecificTableGroup", "groupName=" + group, "groupKey=" + key)
    return dict(zip((e.get("value") for e in d.iter("MemberName")), (e.get("value") for e in d.iter("MemberVersion"))))


def table(name, version):
    d = cfg("getSpecificTable", "tableName=" + name, "version=" + version)
    hdr = d.find("CurrentVersionColumnHeaders")
    headers = [e.get("value") for e in hdr.iter("ColumnHeader")]
    types = [e.get("value") for e in hdr.iter("ColumnType")]
    uid = headers[types.index("UID")] if "UID" in types else headers[0]
    cols = [[unquote(v) for v in e.get("value").split(",")] if e.get("value") else []
            for e in d.find("CurrentVersionRows").iter("ColCSV")]
    n = max((len(c) for c in cols), default=0)
    rows = {}
    for r in range(n):
        row = {h: (cols[i][r] if r < len(cols[i]) else "") for i, h in enumerate(headers)}
        rows[row[uid]] = row
    return headers, uid, rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_meta = "--all-columns" in sys.argv
    if len(args) == 3 and args[1].lstrip("-").isdigit():
        A, B = (args[0], args[1]), (args[0], args[2])
    elif len(args) == 2:
        A, B = resolve(args[0]), resolve(args[1])
    else:
        print(__doc__)
        return 1
    print("A: %s key %s\nB: %s key %s" % (A[0], A[1], B[0], B[1]))
    ma, mb = members(*A), members(*B)
    ndiff = 0
    for t in sorted(set(ma) - set(mb)):
        print("\n%s v%s: only in A" % (t, ma[t])); ndiff += 1
    for t in sorted(set(mb) - set(ma)):
        print("\n%s v%s: only in B" % (t, mb[t])); ndiff += 1
    for t in sorted(set(ma) & set(mb)):
        if ma[t] == mb[t]:
            continue
        ndiff += 1
        ha, ua, ra = table(t, ma[t])
        hb, ub, rb = table(t, mb[t])
        print("\n%s: v%s -> v%s" % (t, ma[t], mb[t]))
        changed = 0
        for c in hb:
            if c not in ha:
                print("  column added: %s" % c); changed += 1
        for c in ha:
            if c not in hb:
                print("  column removed: %s" % c); changed += 1
        for u in ra:
            if u not in rb:
                print("  row removed: %s" % u); changed += 1
        for u in rb:
            if u not in ra:
                print("  row added:   %s" % u); changed += 1
        for u in ra:
            if u not in rb:
                continue
            for c in hb:
                if c in ha and (show_meta or c not in META) and ra[u].get(c) != rb[u].get(c):
                    print("  %s / %s: %r -> %r" % (u, c, ra[u].get(c), rb[u].get(c)))
                    changed += 1
        if not changed and not show_meta:
            print("  (no data change: only Author/RecordInsertionTime, or a re-save; --all-columns shows them)")
    same = sum(1 for t in set(ma) & set(mb) if ma[t] == mb[t])
    print("\n%d table(s) differ, %d identical." % (ndiff, same))
    return 3 if ndiff else 0


if __name__ == "__main__":
    sys.exit(main())
