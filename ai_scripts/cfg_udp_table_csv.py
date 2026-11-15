#!/usr/bin/env python3
"""cfg_udp_table_csv.py - download a table version as CSV, or save a CSV as a new table version,
through the running Configuration GUI (UDP), for bulk edits of tables with many rows.

  get <table> <version> <file.csv>    write the rows of that version (header = column names)
  put <table> <version> <file.csv>    save the CSV as a NEW version of <table>, starting from
                                      <version> (its column layout); an identical table returns
                                      the existing version. Prints the saved version.

The CSV has one header line with the column names (any order; Author and RecordInsertionTime are
optional and ignored: the server fills them). Every other column of the table must be present.
Rows are saved in file order; rows missing from the file are deleted.

After `put`, the version is not in any group yet. Add it, e.g.:
  cfg_udp_edit_group.sh -t Configuration add:<table>=<saved version>
then move the alias (cfg_udp_set_alias.sh) and apply (apply_config_reload.sh).

Prerequisites: source setup_ots.sh; ots running with OTS_CONFIG_UDP_IP/OTS_CONFIG_UDP_PORT set; a
Kerberos ticket for put (the author). Without ots running, the compiled tools
ots_table_download_csv / ots_table_upload_csv work on the store directly (config-store-conversion.md).
Limit: one UDP message, about 60 kB of encoded table; larger tables: use ots_table_upload_csv.
"""
import csv
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import quote, unquote

CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")
SERVER_FILLED = ("Author", "RecordInsertionTime")


def cfg(readonly, *args):
    cmd = ["ots_cfg_udp_test"] + (["-y"] if readonly else []) + [CFG_IP, CFG_PORT] + list(args)
    r = subprocess.run(cmd, capture_output=True, text=True)
    text = r.stdout + r.stderr
    i = text.find("<ROOT>")
    if i < 0:
        sys.exit("No reply from the Configuration GUI at %s:%s: %s" % (CFG_IP, CFG_PORT, text.strip()[:300]))
    data = ET.fromstring(text[i:text.rindex("</ROOT>") + 7]).find("DATA")
    err = data.find("Error")
    if err is not None and "No rows were modified" in err.get("value"):
        print("The CSV is identical to %s version %s: nothing saved." % (args[1].split("=", 1)[1], args[2].split("=", 1)[1]))
        sys.exit(0)
    if err is not None:
        sys.exit("ERROR from the Configuration GUI: %s" % err.get("value").strip()[:500])
    return data


def read_table(name, version):
    d = cfg(True, "getSpecificTable", "tableName=" + name, "version=" + version)
    hdr = d.find("CurrentVersionColumnHeaders")
    if hdr is None:
        sys.exit("No table %s version %s." % (name, version))
    headers = [e.get("value") for e in hdr.iter("ColumnHeader")]
    cols = [[unquote(v) for v in e.get("value").split(",")] if e.get("value") else []
            for e in d.find("CurrentVersionRows").iter("ColCSV")]
    n = max((len(c) for c in cols), default=0)
    return headers, [[(cols[i][r] if r < len(cols[i]) else "") for i in range(len(headers))] for r in range(n)]


def get(name, version, path):
    headers, rows = read_table(name, version)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(headers)
        w.writerows(rows)
    print("Wrote %s version %s: %d rows, %d columns -> %s" % (name, version, len(rows), len(headers), path))


def put(name, version, path):
    headers, _ = read_table(name, version)  # the column layout of the starting version
    with open(path, newline="") as f:
        rd = csv.reader(f)
        fh = next(rd)
        rows = [r for r in rd if any(c.strip() for c in r)]
    data_cols = headers[:-2]  # the server skips the last two (Author, RecordInsertionTime)
    missing = [c for c in data_cols if c not in fh]
    if missing:
        sys.exit("The CSV lacks columns of %s: %s" % (name, ", ".join(missing)))
    extra = [c for c in fh if c not in headers]
    if extra:
        sys.exit("The CSV has columns that %s version %s does not have: %s" % (name, version, ", ".join(extra)))
    idx = [fh.index(c) for c in data_cols]
    # each cell URI-encoded and followed by ',', each row ended by ';' (TableView::fillFromEncodedCSV).
    # The UDP remote control passes parameters on undecoded, so the separators stay plain; a cell's
    # own ',', ';', '&' and '%' are protected by its encoding.
    enc = "".join("".join(quote(r[i] if i < len(r) else "", safe="") + "," for i in idx) + ";" for r in rows)
    if not rows:
        sys.exit("The CSV has no rows: refusing to save an empty table.")
    param = "data=" + enc
    if len(param) > 60000:
        sys.exit("The encoded table is %d bytes, too big for one UDP request: use ots_table_upload_csv." % len(param))
    d = cfg(False, "saveSpecificTable", "tableName=" + name, "version=" + version, "temporary=0",
            "dataOffset=0", "chunkSize=0", "sourceTableAsIs=0", "lookForEquivalent=1",
            "tableComment=" + quote("Uploaded from %s (cfg_udp_table_csv.py)" % os.path.basename(path), safe=""), param)
    saved = d.find("savedVersion")
    if saved is None:
        sys.exit("No savedVersion in the reply.")
    print("Saved %s version %s (%d rows) from %s" % (name, saved.get("value"), len(rows), path))
    print("Next: cfg_udp_edit_group.sh -t <group type> add:%s=%s" % (name, saved.get("value")))


def main():
    if len(sys.argv) != 5 or sys.argv[1] not in ("get", "put"):
        print(__doc__)
        return 1
    (get if sys.argv[1] == "get" else put)(sys.argv[2], sys.argv[3], sys.argv[4])
    return 0


if __name__ == "__main__":
    sys.exit(main())
