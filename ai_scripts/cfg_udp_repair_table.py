#!/usr/bin/env python3
"""cfg_udp_repair_table.py - save an old table version again under the current table definition,
after the definition changed (columns added, removed or renamed with their StorageName).

The browser offers this as "Reload with loose column matching" then Save. This script:
  1. reads <table> <version> with allowIllegalColumns=1 (the Configuration GUI re-reads the
     definitions and fills the current columns from the old version: by storage name where they
     match, else by position), and prints the warnings,
  2. saves those rows as a NEW version, sent as data (not "as is": a copy made as is keeps the old
     source column names, so the duplicate check returns the old version and nothing is repaired in
     the store), and prints its number.

Check the result before using it: with loose matching, a column whose StorageName changed gets the
value of the old column in the same position. Then put the version in the group
(cfg_udp_edit_group.sh -t <type> add:<table>=<new version>), move the alias, apply.

Usage: cfg_udp_repair_table.py <table> <version> [-n]     (-n: show the loaded rows, save nothing)
Prerequisites: source setup_ots.sh; ots running with OTS_CONFIG_UDP_IP/OTS_CONFIG_UDP_PORT; Kerberos.
"""
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import quote, unquote

CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")


def cfg(readonly, *args):
    r = subprocess.run(["ots_cfg_udp_test"] + (["-y"] if readonly else []) + [CFG_IP, CFG_PORT] + list(args),
                       capture_output=True, text=True)
    text = r.stdout + r.stderr
    i = text.find("<ROOT>")
    if i < 0:
        sys.exit("No reply from the Configuration GUI at %s:%s: %s" % (CFG_IP, CFG_PORT, text.strip()[:300]))
    return ET.fromstring(text[i:text.rindex("</ROOT>") + 7]).find("DATA")


def rows_of(d):
    headers = [e.get("value") for e in d.find("CurrentVersionColumnHeaders").iter("ColumnHeader")]
    cols = [[unquote(v) for v in e.get("value").split(",")] if e.get("value") else []
            for e in d.find("CurrentVersionRows").iter("ColCSV")]
    n = max((len(c) for c in cols), default=0)
    return headers, [[(cols[i][r] if r < len(cols[i]) else "") for i in range(len(headers))] for r in range(n)]


def main():
    a = [x for x in sys.argv[1:] if x != "-n"]
    dry = "-n" in sys.argv
    if len(a) != 2:
        print(__doc__)
        return 1
    table, version = a
    d = cfg(True, "getSpecificTable", "tableName=" + table, "version=" + version, "allowIllegalColumns=1")
    err = d.find("Error")
    if err is not None:
        sys.exit("ERROR loading %s v%s: %s" % (table, version, err.get("value").strip()[:400]))
    for w in d.iter("TableWarnings"):
        print("warning: " + unquote(w.get("value") or "").strip()[:400])
    headers, rows = rows_of(d)
    print("%s v%s loaded with the current definition: %d rows, columns %s" % (table, version, len(rows), ", ".join(headers)))
    for r in rows[:10]:
        print("  " + " | ".join(r[:-2]))
    if dry:
        print("(dry run: nothing saved)")
        return 0
    # rows as encoded CSV (TableView::fillFromEncodedCSV): every cell URI-encoded and followed by ',',
    # each row ended by ';'; the server skips the last two columns (Author, RecordInsertionTime)
    data = "".join("".join(quote(c, safe="") + "," for c in r[:-2]) + ";" for r in rows)
    s = cfg(False, "saveSpecificTable", "tableName=" + table, "version=" + version, "temporary=0", "dataOffset=0",
            "chunkSize=0", "sourceTableAsIs=0", "lookForEquivalent=1", "data=" + data,
            "tableComment=" + quote("Repaired from v%s after a definition change (cfg_udp_repair_table.py)" % version, safe=""))
    err = s.find("Error")
    if err is not None:
        sys.exit("ERROR saving: %s" % err.get("value").strip()[:400])
    saved = s.find("savedVersion")
    print("Saved %s version %s." % (table, saved.get("value") if saved is not None else "?"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
