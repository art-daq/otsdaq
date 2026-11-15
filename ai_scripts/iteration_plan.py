#!/usr/bin/env python3
"""iteration_plan.py - create an Iterator plan from a short description, through the Configuration GUI
(UDP): the rows of IterateTable, IterationPlanTable and the command tables, one new Iterate group key,
activated. (Playing a plan is separate: gateway-udp-fsm.md, "Iterator over Gateway UDP".)

  iteration_plan.py create [-n] [-f] <PlanName> <step> [<step> ...]
  iteration_plan.py list

Steps (each ONE quoted argument), run in order by the Iterator:
  "fsm <StateMachineName>"            choose the state machine to drive (CHOOSE_FSM), e.g. PhysicsRuns0
  "configure <alias>"                 Configure with a group alias (CONFIGURE_ALIAS)
  "configure-group <group> <key>"     Configure with a group name and key (CONFIGURE_GROUP)
  "run <seconds> [wait]"              Start, run for <seconds>, Stop (RUN); with "wait" the duration is
                                      ignored and the run lasts until the front ends' running threads end
  "label <name>"                      mark a point to loop back to (BEGIN_LABEL)
  "repeat <label> <n>"                go back to the label n times (REPEAT_LABEL)
Not covered (they need Targets/macro tables): ACTIVATE_*, MODIFY_ACTIVE_GROUP, EXECUTE_FE_MACRO,
EXECUTE_MM_MACRO, WAIT, START/STOP/PAUSE/RESUME/HALT: use the browser Iterate window.

Example (two 30 s runs with the same configuration):
  iteration_plan.py create TwoRuns "fsm PhysicsRuns0" "configure defaultSystemAlias" \\
      "label again" "run 30" "repeat again 1"

Adds to the active Iterate group if there is one (existing plans are kept), else makes the group;
saves the Iterate member tables that have no version yet (empty). -n shows the rows, saves nothing;
-f replaces a plan of the same name. Play with:
  ots_drive_udp_test <ip> <gateway udp port> raw "Iterator,iteratePlay,<PlanName>"
The Gateway reads the Iterate group when it starts: after creating a plan restart ots or run
apply_config_reload.sh <fsm>. Prerequisites: source setup_ots.sh; ots running with the Configuration
remote control; Kerberos ticket.
"""
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import quote, unquote

CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")
MEMBERS = ["IterateTable", "IterationPlanTable", "IterationTargetTable", "IterationCommandBeginLabelTable",
           "IterationCommandChooseFSMTable", "IterationCommandConfigureAliasTable",
           "IterationCommandConfigureGroupTable", "IterationCommandExecuteFEMacroTable",
           "IterationCommandExecuteMacroTable", "IterationCommandMacroDimensionalLoopTable",
           "IterationCommandMacroDimensionalLoopParameterTable", "IterationCommandModifyGroupTable",
           "IterationCommandRepeatLabelTable", "IterationCommandRunTable"]
# step keyword -> (CommandType, command table)
KINDS = {"fsm": ("CHOOSE_FSM", "IterationCommandChooseFSMTable"),
         "configure": ("CONFIGURE_ALIAS", "IterationCommandConfigureAliasTable"),
         "configure-group": ("CONFIGURE_GROUP", "IterationCommandConfigureGroupTable"),
         "run": ("RUN", "IterationCommandRunTable"),
         "label": ("BEGIN_LABEL", "IterationCommandBeginLabelTable"),
         "repeat": ("REPEAT_LABEL", "IterationCommandRepeatLabelTable")}


def cfg(readonly, *args):
    r = subprocess.run(["ots_cfg_udp_test"] + (["-y"] if readonly else []) + [CFG_IP, CFG_PORT] + list(args),
                       capture_output=True, text=True)
    t = r.stdout + r.stderr
    i = t.find("<ROOT>")
    if i < 0:
        sys.exit("No reply from the Configuration GUI at %s:%s: %s" % (CFG_IP, CFG_PORT, t.strip()[:300]))
    return ET.fromstring(t[i:t.rindex("</ROOT>") + 7]).find("DATA")


def err_of(d):
    e = d.find("Error")
    return e.get("value").strip() if e is not None else None


def load(table, version):
    """(headers, default row, rows) of a version, or of the mock-up when version is -1"""
    d = cfg(True, "getSpecificTable", "tableName=" + table, "version=" + str(version))
    h = [e.get("value") for e in d.find("CurrentVersionColumnHeaders").iter("ColumnHeader")]
    cols = [[unquote(v) for v in e.get("value").split(",")] if e.get("value") else []
            for e in d.find("CurrentVersionRows").iter("ColCSV")]
    n = max((len(c) for c in cols), default=0)
    rows = [[(cols[i][r] if r < len(cols[i]) else "") for i in range(len(h))] for r in range(n)] if version != -1 else []
    defaults = [unquote(e.get("value")) for e in d.iter("DefaultRowValue")]
    return h, defaults, rows


def active_iterate():
    d = cfg(True, "getActiveTableGroups")
    n, k = d.find("Iterate-ActiveGroupName"), d.find("Iterate-ActiveGroupKey")
    if n is None or not n.get("value") or k.get("value") == "-1":
        return None
    return n.get("value"), k.get("value")


def members_of(group):
    if not group:
        return {}
    m = cfg(True, "getSpecificTableGroup", "groupName=" + group[0], "groupKey=" + group[1])
    return dict(zip((e.get("value") for e in m.iter("MemberName")), (e.get("value") for e in m.iter("MemberVersion"))))


def save(table, base_version, rows):
    data = "".join("".join(quote(c, safe="") + "," for c in r[:-2]) + ";" for r in rows)
    d = cfg(False, "saveSpecificTable", "tableName=" + table, "version=" + str(base_version), "temporary=0",
            "dataOffset=0", "chunkSize=0", "sourceTableAsIs=0", "lookForEquivalent=1", "data=" + data,
            "tableComment=" + quote("iteration_plan.py", safe=""))
    e = err_of(d)
    if e and "No rows were modified" in e:
        return str(base_version)  # nothing to change: keep the version
    if e:
        sys.exit("ERROR saving %s: %s" % (table, e[:300]))
    return d.find("savedVersion").get("value")


def row_from(headers, defaults, **values):
    r = list(defaults) if len(defaults) == len(headers) else [""] * len(headers)
    for k, v in values.items():
        r[headers.index(k)] = str(v)
    return r


def main():
    a = sys.argv[1:]
    dry, force = "-n" in a, "-f" in a
    a = [x for x in a if x not in ("-n", "-f")]
    if not a or a[0] not in ("create", "list"):
        print(__doc__)
        return 1
    group = active_iterate()
    mem = members_of(group)
    if a[0] == "list":
        if "IterateTable" not in mem:
            print("No active Iterate group: no plans.")
            return 0
        print("Active Iterate group %s key %s. Plans:" % group)
        h, _, rows = load("IterateTable", mem["IterateTable"])
        ph, _, prows = load("IterationPlanTable", mem["IterationPlanTable"])
        for r in rows:
            gid = r[h.index("LinkToIterationPlanGroupID")]
            steps = [p for p in prows if p[ph.index("IterationPlanGroupID")] == gid]
            print("  %-24s %-4s %d command(s): %s" % (r[0], r[h.index("Status")], len(steps),
                  ", ".join(p[ph.index("CommandType")] for p in steps)))
        return 0
    if len(a) < 3:
        print(__doc__)
        return 1
    plan, steps = a[1], a[2:]
    if not re.fullmatch(r"[A-Za-z0-9]+", plan):
        sys.exit("Plan names: letters and digits only.")
    tabs = {}  # table -> [headers, defaults, rows, base version]
    for t in MEMBERS:
        v = mem.get(t, "-1")
        h, d, rows = load(t, v)
        if v == "-1":
            h, d, _ = load(t, -1)
        tabs[t] = [h, d, rows, v]
    ih = tabs["IterateTable"][0]
    existing = [r for r in tabs["IterateTable"][2] if r[0] == plan]
    if existing and not force:
        sys.exit("Plan %s exists: use -f to replace it." % plan)
    if existing:
        gid_old = existing[0][ih.index("LinkToIterationPlanGroupID")]
        tabs["IterateTable"][2] = [r for r in tabs["IterateTable"][2] if r[0] != plan]
        pg = tabs["IterationPlanTable"]
        pg[2] = [r for r in pg[2] if r[pg[0].index("IterationPlanGroupID")] != gid_old]
    gid = plan + "Commands"
    for n, step in enumerate(a[2:]):
        tok = step.split()
        if not tok or tok[0] not in KINDS:
            sys.exit("Step %d '%s': unknown step (use: %s)." % (n, step, ", ".join(KINDS)))
        ctype, ctab = KINDS[tok[0]]
        cid = "%s_%02d_%s" % (plan, n, tok[0].replace("-", ""))
        h, d, _, _ = tabs[ctab]
        vals = {"CommandID": cid}
        if tok[0] == "fsm" and len(tok) == 2:
            vals["NameOfStateMachine"] = tok[1]
        elif tok[0] == "configure" and len(tok) == 2:
            vals["SystemAlias"] = tok[1]
        elif tok[0] == "configure-group" and len(tok) == 3:
            vals["GroupName"], vals["GroupKey"] = tok[1], tok[2]
        elif tok[0] == "run" and len(tok) in (2, 3) and re.fullmatch(r"\d+", tok[1]):
            vals["DurationInSeconds"] = tok[1]
            vals["WaitForAllFrontEndsRunningThread"] = "True" if len(tok) == 3 and tok[2] == "wait" else "False"
        elif tok[0] == "label" and len(tok) == 2:
            vals["Label"] = tok[1]
        elif tok[0] == "repeat" and len(tok) == 3 and re.fullmatch(r"\d+", tok[2]):
            vals["Label"], vals["NumberOfRepetitions"] = tok[1], tok[2]
        else:
            sys.exit("Step %d '%s': wrong arguments (see the usage)." % (n, step))
        tabs[ctab][2].append(row_from(h, d, **vals))
        ph, pd, _, _ = tabs["IterationPlanTable"]
        tabs["IterationPlanTable"][2].append(row_from(ph, pd, IterationPlanGroupID=gid, IterationPlanCommandID=cid,
                                                      Status="On", CommandType=ctype, LinkToCommandTable=ctab,
                                                      LinkToCommandUID=cid))
    tabs["IterateTable"][2].append(row_from(ih, tabs["IterateTable"][1], IterationPlanName=plan, Status="On",
                                            LinkToIterationPlanTable="IterationPlanTable",
                                            LinkToIterationPlanGroupID=gid))
    print("Plan %s, %d step(s), command group %s:" % (plan, len(steps), gid))
    for r in tabs["IterationPlanTable"][2]:
        if r[tabs["IterationPlanTable"][0].index("IterationPlanGroupID")] == gid:
            print("  %s: %s" % (r[1], r[3]))
    if dry:
        print("(dry run: nothing saved)")
        return 0
    versions = []
    for t in MEMBERS:
        h, d, rows, base = tabs[t]
        versions.append(t + "," + save(t, base, rows) + ",")
    gname = group[0] if group else "iterateGroup"
    r = cfg(False, "saveNewTableGroup", "groupName=" + gname, "groupComment=" + quote("plan " + plan, safe=""),
            "tableList=" + "".join(versions))
    dup = re.search(r"duplicate of an existing group key \((\d+)\)", err_of(r) or "")
    key = dup.group(1) if dup else (r.find("TableGroupKey").get("value") if r.find("TableGroupKey") is not None else None)
    if not key:
        sys.exit("ERROR saving the group: %s" % ((err_of(r) or ET.tostring(r).decode())[:400]))
    for ign in ("0", "1"):
        act = cfg(False, "activateTableGroup", "groupName=" + gname, "groupKey=" + key, "ignoreWarnings=" + ign)
        k = act.find("Iterate-ActiveGroupKey")
        if k is not None and k.get("value") == key:
            break
    else:
        sys.exit("The group was saved (%s key %s) but not activated." % (gname, key))
    print("Saved and activated Iterate group %s key %s." % (gname, key))
    print("Restart ots (or apply_config_reload.sh <fsm>), then: ots_drive_udp_test <ip> <port> raw \"Iterator,iteratePlay,%s\"" % plan)
    return 0


if __name__ == "__main__":
    sys.exit(main())
