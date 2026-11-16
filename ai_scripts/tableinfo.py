#!/usr/bin/env python3
"""tableinfo.py - create, change and check table definitions ($USER_DATA/TableInfo/<Table>Info.xml),
the job of the Wiz mode Table Editor, from a script.

  new <Table> <column> [<column> ...] [--description TEXT]
        <column> = Name:Type[:DataType[:Default[:Choices]]]
          Type      UID, Data, UniqueData, MultilineData, FixedChoiceData, BitMap, OnOff, TrueFalse,
                    YesNo, ChildLink-<x>, ChildLinkUID-<x>, ChildLinkGroupID-<x>, GroupID-<x>
          DataType  STRING (default), NUMBER, or TIMESTAMP WITH TIMEZONE
          Choices   for FixedChoiceData/links, '|'-separated, e.g. arbitraryBool=0|Producer|Consumer
        A UID column is added first if none is given (<Table without "Table">UID), and
        CommentDescription, Author and RecordInsertionTime are added last, as every table has them.
  edit <Table> add <column> [--after <Name>]
  edit <Table> rename <OldName> <NewName> [--storage]
        keeps the column's StorageName by default: saved versions store columns by StorageName, so
        they keep loading. --storage renames it too: then old versions load with warnings and need
        cfg_udp_repair_table.py.
  edit <Table> remove <Name>
  check <Table>    validate the file (names, types, UID first, Author/RecordInsertionTime last)

Every change writes <file>.bak<timestamp> first, then validates. Names: letters and digits only
(the Table Editor's Save refuses others); table names end in "Table".

After a change, the running Configuration GUI re-reads all definitions on the next table read with
allowIllegalColumns=1: this script does that request when OTS_CONFIG_UDP_PORT is set and ots runs
(--no-refresh to skip). Other processes (front ends, data managers) read definitions at start:
restart ots before using a changed table in a run.
"""
import os
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

TYPES_FIXED = {"UID", "Data", "UniqueData", "UniqueGroupData", "MultilineData", "FixedChoiceData", "BitMap",
               "OnOff", "TrueFalse", "YesNo", "Comment", "Author", "Timestamp"}
TYPES_PREFIX = ("ChildLink-", "ChildLinkUID-", "ChildLinkGroupID-", "GroupID-")
DATATYPES = {"STRING", "NUMBER", "TIMESTAMP WITH TIMEZONE", "VARCHAR2"}
TAIL = [("Comment", "CommentDescription", "STRING", None), ("Author", "Author", "STRING", None),
        ("Timestamp", "RecordInsertionTime", "TIMESTAMP WITH TIMEZONE", None)]


def snake(n):
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", n)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s).upper()


def path_of(table):
    ud = os.environ.get("USER_DATA")
    if not ud:
        sys.exit("USER_DATA is not set: source setup_ots.sh first.")
    return os.path.join(ud, "TableInfo", table + "Info.xml")


def column(spec):
    p = spec.split(":")
    if len(p) < 2:
        sys.exit("Column '%s': use Name:Type[:DataType[:Default[:Choices]]]." % spec)
    name, typ = p[0], p[1]
    dtype = p[2] if len(p) > 2 and p[2] else "STRING"
    default = p[3] if len(p) > 3 and p[3] != "" else None
    choices = p[4].replace("|", ",") if len(p) > 4 else ""
    return {"Type": typ, "Name": name, "StorageName": snake(name), "DataType": dtype, "DefaultValue": default,
            "DataChoices": choices}


def problems(table, cols):
    out = []
    if not re.fullmatch(r"[A-Za-z0-9]+Table", table):
        out.append("table name '%s': letters and digits only, ending in 'Table'" % table)
    names = [c["Name"] for c in cols]
    for c in cols:
        if not re.fullmatch(r"[A-Za-z0-9]+", c["Name"]):
            out.append("column '%s': letters and digits only" % c["Name"])
        if c["Type"] not in TYPES_FIXED and not c["Type"].startswith(TYPES_PREFIX):
            out.append("column '%s': unknown type '%s'" % (c["Name"], c["Type"]))
        if c["DataType"] not in DATATYPES:
            out.append("column '%s': unknown data type '%s'" % (c["Name"], c["DataType"]))
    for n in set(names):
        if names.count(n) > 1:
            out.append("column name '%s' twice" % n)
    st = [c["StorageName"] for c in cols]
    for n in set(st):
        if st.count(n) > 1:
            out.append("storage name '%s' twice" % n)
    if not cols or (cols[0]["Type"] != "UID" and not cols[0]["Type"].startswith("GroupID-")):
        out.append("the first column must be the UID (or a GroupID)")
    if [c["Type"] for c in cols[-2:]] != ["Author", "Timestamp"]:
        out.append("the last two columns must be Author and Timestamp (RecordInsertionTime)")
    return out


def read(table):
    p = path_of(table)
    if not os.path.exists(p):
        sys.exit("No definition %s." % p)
    root = ET.parse(p).getroot()
    view = root.find("TABLE").find("VIEW")
    cols = [{k: c.get(k) for k in ("Type", "Name", "StorageName", "DataType", "DefaultValue", "DataChoices")}
            for c in view.iter("COLUMN")]
    return view.get("Description") or "", cols


def write(table, description, cols):
    bad = problems(table, cols)
    if bad:
        sys.exit("Not written, the definition would be invalid:\n  " + "\n  ".join(bad))
    p = path_of(table)
    if os.path.exists(p):
        bak = p + ".bak" + time.strftime("%y%m%d%H%M%S")
        shutil.copy2(p, bak)
        print("Backup: %s" % bak)
    lines = ['<?xml version="1.0" encoding="UTF-8" standalone="no" ?>',
             '\t<ROOT xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="TableInfo.xsd">',
             '\t\t<TABLE Name="%s">' % table,
             '\t\t\t<VIEW Name="%s" Type="File,Database,DatabaseTest" Description="%s">' % (snake(table), description)]
    esc = lambda v: v.replace("&", "&amp;").replace('"', "&quot;").replace("<", "&lt;")
    for c in cols:
        attrs = '<COLUMN Type="%s" \t Name="%s" \t StorageName="%s" \t\tDataType="%s"' % (
            c["Type"], c["Name"], c["StorageName"], c["DataType"])
        if c.get("DefaultValue") is not None:
            attrs += ' \t\tDefaultValue="%s"' % esc(c["DefaultValue"])
        attrs += ' \t\tDataChoices="%s"/>' % esc(c.get("DataChoices") or "")
        lines.append("\t\t\t\t" + attrs)
    lines += ["\t\t\t</VIEW>", "\t\t</TABLE>", "\t</ROOT>", ""]
    open(p, "w").write("\n".join(lines))
    ET.parse(p)  # well-formed
    print("Wrote %s: %d columns (%s)" % (p, len(cols), ", ".join(c["Name"] for c in cols)))


def refresh(table):
    """make the running Configuration GUI re-read the definitions, and show its warnings"""
    port = os.environ.get("OTS_CONFIG_UDP_PORT")
    if not port:
        print("(OTS_CONFIG_UDP_PORT not set: the Configuration GUI re-reads definitions at its next start)")
        return
    r = subprocess.run(["ots_cfg_udp_test", "-y", os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1"), port,
                        "getSpecificTable", "tableName=" + table, "version=-1", "allowIllegalColumns=1"],
                       capture_output=True, text=True)
    t = r.stdout + r.stderr
    if "<ROOT>" not in t:
        print("(no Configuration GUI reply: it re-reads definitions at its next start)")
        return
    heads = re.findall(r"<ColumnHeader value='([^']*)'/>", t)
    warns = re.findall(r"<(?:TableWarnings|Error) value='([^']*)'", t)
    print("Configuration GUI re-read the definitions: %s has columns %s" % (table, ", ".join(heads)))
    for w in warns:
        print("  warning: " + w.strip()[:300])


def main():
    a = sys.argv[1:]
    norefresh = "--no-refresh" in a
    a = [x for x in a if x != "--no-refresh"]
    if len(a) < 2 or a[0] not in ("new", "edit", "check"):
        print(__doc__)
        return 1
    op, table = a[0], a[1]
    if op == "check":
        desc, cols = read(table)
        bad = problems(table, cols)
        print("%s: %d columns; %s" % (path_of(table), len(cols), "OK" if not bad else "problems:\n  " + "\n  ".join(bad)))
        return 1 if bad else 0
    if op == "new":
        if os.path.exists(path_of(table)):
            sys.exit("%s exists: use edit." % path_of(table))
        desc = ""
        if "--description" in a:
            i = a.index("--description"); desc = a[i + 1]; a = a[:i] + a[i + 2:]
        cols = [column(s) for s in a[2:]]
        if not cols or cols[0]["Type"] != "UID":
            cols.insert(0, column("%sUID:UID" % table[:-5] if table.endswith("Table") else table + "UID"))
        names = {c["Name"] for c in cols}
        cols += [{"Type": t, "Name": n, "StorageName": snake(n), "DataType": d, "DefaultValue": None, "DataChoices": ""}
                 for t, n, d, _ in TAIL if n not in names]
        from urllib.parse import quote
        write(table, quote(desc), cols)
    else:
        desc, cols = read(table)
        sub = a[2] if len(a) > 2 else ""
        names = [c["Name"] for c in cols]
        if sub == "add" and len(a) >= 4:
            c = column(a[3])
            pos = len(cols) - 3  # before CommentDescription, Author, RecordInsertionTime
            if "--after" in a:
                after = a[a.index("--after") + 1]
                if after not in names:
                    sys.exit("No column %s (columns: %s)." % (after, ", ".join(names)))
                pos = names.index(after) + 1
            cols.insert(pos, c)
        elif sub == "rename" and len(a) >= 5:
            old, new = a[3], a[4]
            if old not in names:
                sys.exit("No column %s (columns: %s)." % (old, ", ".join(names)))
            c = cols[names.index(old)]
            c["Name"] = new
            if "--storage" in a:
                c["StorageName"] = snake(new)
                print("StorageName renamed too: saved versions of %s will load with warnings; "
                      "repair them with cfg_udp_repair_table.py." % table)
            else:
                print("StorageName kept (%s): saved versions keep loading." % c["StorageName"])
        elif sub == "remove" and len(a) >= 4:
            if a[3] not in names:
                sys.exit("No column %s (columns: %s)." % (a[3], ", ".join(names)))
            cols.pop(names.index(a[3]))
        else:
            print(__doc__)
            return 1
        write(table, desc, cols)
    if not norefresh:
        refresh(table)
    return 0


if __name__ == "__main__":
    sys.exit(main())
