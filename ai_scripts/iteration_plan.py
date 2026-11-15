#!/usr/bin/env python3
"""iteration_plan.py - create, show and modify Iterator plans through the Configuration GUI (UDP).

A plan is rows in IterateTable (the plan), IterationPlanTable (its steps, in order), one row per step in a
command table (the step's parameters), and for macro and modify steps IterationTargetTable (which front ends)
and the dimensional-loop tables. This script edits all of them together and saves one new Iterate group key,
activated. Playing a plan is separate (gateway-udp-fsm.md, "Iterator over Gateway UDP").

  iteration_plan.py list                                  plans of the active Iterate group
  iteration_plan.py show <Plan>                           numbered steps with all their values
  iteration_plan.py create [-f] <Plan> <step>...          new plan (-f replaces a plan of that name)
  iteration_plan.py add <Plan> [--at N] <step>...         insert steps before step N (default: append)
  iteration_plan.py remove <Plan> <N>...                  remove steps (and their parameter rows)
  iteration_plan.py move <Plan> <N> <M>                   move step N to position M
  iteration_plan.py set <Plan> <N> <Column>=<value>...    change cells of step N: its command-table columns
                                                          (DurationInSeconds, SystemAlias, FieldStartValue, MacroName, ...)
                                                          or Status / TargetSubsystem of the step
  iteration_plan.py enable|disable <Plan> [<N>...]        Status of the plan, or of steps
  iteration_plan.py delete <Plan>                         remove the plan and all its rows
  options before the command word's arguments: -n dry run (show the result, save nothing)

Steps (each ONE quoted argument; words split like a shell, so quote names that have spaces inside the step):
  fsm <StateMachineName>               CHOOSE_FSM            configure <alias>              CONFIGURE_ALIAS
  configure-group <group> <key>        CONFIGURE_GROUP       configure-active               CONFIGURE_ACTIVE_GROUP
  run <seconds> [wait]                 RUN (with wait: ends when the front ends' running threads end)
  start | stop | pause | resume | halt FSM transitions (no parameters)
  label <name> | repeat <label> <n>    BEGIN_LABEL | REPEAT_LABEL (loop: run the steps between again n times)
  subsystems Set|Add|Remove <a,b,..>   SETUP_INCLUDED_SUBSYSTEMS
  modify <Table>/<UID>[,<Table>/<UID>...] <relative/path/to/field> <start> [<step> [track]]
        MODIFY_ACTIVE_GROUP: set the field of every target to start + step * (pass number of the innermost
        label loop, 0 on the first pass), then activate; 'track' saves a new group key per pass, but with the
        changed table as the scratch version, so the keys do not keep per-pass values (the Gateway log does)
  fe-macro | mm-macro <Table>/<UID>[,...] <macro name> [out=<path>[:<radix>]] [loop=<n>[@<label>]:<name>=<init>:<step>[:...]]...
        EXECUTE_FE_MACRO (a front-end macro) | EXECUTE_MM_MACRO (a Macro Maker macro) on the targets, with
        dimensional loops: each loop=... is one dimension of <n> iterations whose named inputs start at <init>
        and change by <step> (e.g. loop=3:myOtherArg=5:2); out= saves the outputs to a file
Not covered: WAIT, ACTIVATE_ALIAS/GROUP, and TargetSubsystem for remote subsystems (set it with `set <N> TargetSubsystem=<name>`).

Example, a scan: five runs of 20 s, the amplitude register of the emulator is not reachable from a plan, so scan
an FE macro input instead:
  iteration_plan.py create AmpScan "fsm PhysicsRuns0" "configure defaultSystemAlias" "label scan" \\
      "fe-macro FEInterfaceTable/ExampleInterface0 varTest loop=5@scan:myOtherArg=1:1" "run 20" "repeat scan 4"

When a change takes effect: a NEW plan is found after apply_config_reload.sh <fsm>; a plan the Gateway has already played
keeps playing its OLD version until ots is restarted (ots_mode.sh stop, then ots), verified 2026-11-15.
Play: ots_drive_udp_test <ip> <gateway udp port> raw "Iterator,iteratePlay,<Plan>".
Prerequisites: source setup_ots.sh; ots running with the Configuration remote control; Kerberos ticket.
"""
import copy
import os
import re
import shlex
import subprocess
import sys
import xml.etree.ElementTree as ET
from urllib.parse import quote, unquote

CFG_IP = os.environ.get("OTS_CONFIG_UDP_IP", "127.0.0.1")
CFG_PORT = os.environ.get("OTS_CONFIG_UDP_PORT", "2010")
# step keyword -> (CommandType, command table or None)
KINDS = {"fsm": ("CHOOSE_FSM", "IterationCommandChooseFSMTable"),
         "configure": ("CONFIGURE_ALIAS", "IterationCommandConfigureAliasTable"),
         "configure-group": ("CONFIGURE_GROUP", "IterationCommandConfigureGroupTable"),
         "configure-active": ("CONFIGURE_ACTIVE_GROUP", None),
         "run": ("RUN", "IterationCommandRunTable"),
         "label": ("BEGIN_LABEL", "IterationCommandBeginLabelTable"),
         "repeat": ("REPEAT_LABEL", "IterationCommandRepeatLabelTable"),
         "start": ("START", None), "stop": ("STOP", None), "pause": ("PAUSE", None), "resume": ("RESUME", None),
         "halt": ("HALT", None),
         "subsystems": ("SETUP_INCLUDED_SUBSYSTEMS", "IterationCommandIncludedSubsystemsTable"),
         "modify": ("MODIFY_ACTIVE_GROUP", "IterationCommandModifyGroupTable"),
         "fe-macro": ("EXECUTE_FE_MACRO", "IterationCommandExecuteFEMacroTable"),
         "mm-macro": ("EXECUTE_MM_MACRO", "IterationCommandExecuteMacroTable")}
T_PLAN, T_ITER, T_TARGET = "IterationPlanTable", "IterateTable", "IterationTargetTable"
T_LOOP, T_PARAM = "IterationCommandMacroDimensionalLoopTable", "IterationCommandMacroDimensionalLoopParameterTable"


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


class Table:
    def __init__(self, name, version):
        d = cfg(True, "getSpecificTable", "tableName=" + name, "version=" + str(version))
        self.name, self.base = name, str(version)
        self.h = [e.get("value") for e in d.find("CurrentVersionColumnHeaders").iter("ColumnHeader")]
        self.defaults = [unquote(e.get("value")) for e in d.iter("DefaultRowValue")]
        cols = [[unquote(v) for v in e.get("value").split(",")] if e.get("value") else []
                for e in d.find("CurrentVersionRows").iter("ColCSV")]
        n = max((len(c) for c in cols), default=0) if version != "-1" else 0
        self.rows = [[(cols[i][r] if r < len(cols[i]) else "") for i in range(len(self.h))] for r in range(n)]
        self.orig = copy.deepcopy(self.rows)
        self.names = [e.get("value") for e in d.iter("ExistingTableNames")]

    def new_row(self, **v):
        # the reply lists a default for every column but the last (RecordInsertionTime)
        r = (list(self.defaults) + [""] * len(self.h))[:len(self.h)]
        for k, val in v.items():
            if k in self.h:
                r[self.h.index(k)] = str(val)
        self.rows.append(r)
        return r

    def get(self, row, col):
        return row[self.h.index(col)] if col in self.h else ""

    def put(self, row, col, val):
        row[self.h.index(col)] = str(val)

    def drop(self, pred):
        self.rows = [r for r in self.rows if not pred(r)]

    def save(self):
        if self.rows == self.orig and self.base != "-1":
            return self.base
        data = "".join("".join(quote(c, safe="") + "," for c in r[:-2]) + ";" for r in self.rows)
        d = cfg(False, "saveSpecificTable", "tableName=" + self.name, "version=" + self.base, "temporary=0",
                "dataOffset=0", "chunkSize=0", "sourceTableAsIs=0", "lookForEquivalent=1", "data=" + data,
                "tableComment=" + quote("iteration_plan.py", safe=""))
        e = err_of(d)
        if e and "No rows were modified" in e:
            return self.base
        if e:
            sys.exit("ERROR saving %s: %s" % (self.name, e[:300]))
        return d.find("savedVersion").get("value")


def active_iterate():
    d = cfg(True, "getActiveTableGroups")
    n, k = d.find("Iterate-ActiveGroupName"), d.find("Iterate-ActiveGroupKey")
    if n is None or not n.get("value") or k.get("value") == "-1":
        return None
    return n.get("value"), k.get("value")


def load_all():
    group = active_iterate()
    mem = {}
    if group:
        m = cfg(True, "getSpecificTableGroup", "groupName=" + group[0], "groupKey=" + group[1])
        mem = dict(zip((e.get("value") for e in m.iter("MemberName")), (e.get("value") for e in m.iter("MemberVersion"))))
    probe = Table(T_ITER, "-1")  # its reply lists the table names the Configuration GUI knows
    members = sorted(n for n in probe.names if n == T_ITER or re.fullmatch(r"Iteration\w*Table", n))
    tabs = {n: Table(n, mem.get(n, "-1")) for n in members}
    return group, tabs


class Plans:
    def __init__(self, tabs):
        self.t = tabs

    def plan_row(self, plan):
        t = self.t[T_ITER]
        r = [x for x in t.rows if x[0] == plan]
        return r[0] if r else None

    def gid(self, plan):
        return self.t[T_ITER].get(self.plan_row(plan), "LinkToIterationPlanGroupID")

    def steps(self, plan):
        p = self.t[T_PLAN]
        return [r for r in p.rows if p.get(r, "IterationPlanGroupID") == self.gid(plan)]

    def cmd_row(self, step):
        p = self.t[T_PLAN]
        tab, uid = p.get(step, "LinkToCommandTable"), p.get(step, "LinkToCommandUID")
        if tab in self.t and uid not in ("", "NO_LINK", "DEFAULT"):
            c = self.t[tab]
            for r in c.rows:
                if r[0] == uid:
                    return c, r
        return None, None

    def cascade_drop_cmd(self, step):
        """remove the command row of a step with its targets and dimensional loops"""
        c, r = self.cmd_row(step)
        if not r:
            return
        tg = c.get(r, "LinkToTargetsGroupID")
        if tg and T_TARGET in self.t:
            self.t[T_TARGET].drop(lambda x: x[0] == tg)
        lg = c.get(r, "LinkToMacroDimensionalLoopGroupID")
        if lg and T_LOOP in self.t:
            lt, pt = self.t[T_LOOP], self.t[T_PARAM]
            for lr in [x for x in lt.rows if lt.get(x, "DimensionalLoopGroupID") == lg]:
                pg = lt.get(lr, "LinkToDimensionalLoopParameterGroupID")
                pt.drop(lambda x: pt.get(x, "ParameterGroupID") == pg)
            lt.drop(lambda x: lt.get(x, "DimensionalLoopGroupID") == lg)
        c.drop(lambda x: x is r)

    def remove_step(self, step):
        self.cascade_drop_cmd(step)
        self.t[T_PLAN].drop(lambda x: x is step)

    def delete_plan(self, plan):
        for s in self.steps(plan):
            self.remove_step(s)
        row = self.plan_row(plan)
        self.t[T_ITER].drop(lambda x: x is row)

    def next_id(self, plan):
        used = [int(m.group(1)) for r in self.t[T_PLAN].rows
                for m in [re.fullmatch(re.escape(plan) + r"_(\d+)_\w+", self.t[T_PLAN].get(r, "IterationPlanCommandID"))] if m]
        return max(used, default=-1) + 1

    def new_step(self, plan, spec):
        """append the rows of one step; returns its plan-table row"""
        tok = shlex.split(spec)
        if not tok or tok[0] not in KINDS:
            sys.exit("Step '%s': unknown step (use: %s)." % (spec, ", ".join(KINDS)))
        kw = tok[0]
        ctype, ctab = KINDS[kw]
        cid = "%s_%02d_%s" % (plan, self.next_id(plan), kw.replace("-", ""))
        v, bad = {"CommandID": cid}, False
        n = len(tok)
        if kw == "fsm" and n == 2:
            v["NameOfStateMachine"] = tok[1]
        elif kw == "configure" and n == 2:
            v["SystemAlias"] = tok[1]
        elif kw == "configure-group" and n == 3:
            v["GroupName"], v["GroupKey"] = tok[1], tok[2]
        elif kw == "run" and n in (2, 3) and re.fullmatch(r"\d+", tok[1]):
            v["DurationInSeconds"] = tok[1]
            v["WaitForAllFrontEndsRunningThread"] = "True" if n == 3 and tok[2] == "wait" else "False"
        elif kw == "label" and n == 2:
            v["Label"] = tok[1]
        elif kw == "repeat" and n == 3 and re.fullmatch(r"\d+", tok[2]):
            v["Label"], v["NumberOfRepetitions"] = tok[1], tok[2]
        elif kw in ("configure-active", "start", "stop", "pause", "resume", "halt") and n == 1:
            pass
        elif kw == "subsystems" and n == 3 and tok[1] in ("Set", "Add", "Remove"):
            v["Mode"], v["Subsystems"] = tok[1], tok[2]
        elif kw == "modify" and n in (4, 5, 6):
            v["RelativePathToField"], v["FieldStartValue"] = tok[2], tok[3]
            v["FieldIterationStepSize"] = tok[4] if n >= 5 else "0"
            v["DoTrackGroupChanges"] = "True" if n == 6 and tok[5] == "track" else "False"
            self.add_targets(v, cid, tok[1])
        elif kw in ("fe-macro", "mm-macro") and n >= 3:
            v["MacroName"] = tok[2]
            self.add_targets(v, cid, tok[1])
            loops = []
            for o in tok[3:]:
                if o.startswith("out="):
                    path, _, radix = o[4:].partition(":")
                    v["EnableSavingOutputsToFile"], v["OutputFilePath"] = "True", path
                    if radix:
                        v["OutputFileRadix"] = radix
                elif o.startswith("loop="):
                    loops.append(o[5:])
                else:
                    sys.exit("Step '%s': unknown option '%s' (out=..., loop=...)." % (spec, o))
            if loops:
                self.add_loops(v, cid, loops, spec)
        else:
            bad = True
        if bad:
            sys.exit("Step '%s': wrong arguments (see the usage)." % spec)
        if ctab:
            if ctab not in self.t:
                sys.exit("The table %s is not known to this Configuration GUI (older table definitions?)." % ctab)
            self.t[ctab].new_row(**v)
        p = self.t[T_PLAN]
        return p.new_row(IterationPlanGroupID="", IterationPlanCommandID=cid, Status="On", CommandType=ctype,
                         LinkToCommandTable=ctab or "NO_LINK", LinkToCommandUID=cid if ctab else "NO_LINK")

    def add_targets(self, v, cid, spec):
        tg = cid + "_targets"
        for i, tu in enumerate(spec.split(",")):
            if "/" not in tu:
                sys.exit("Target '%s': use <Table>/<UID>." % tu)
            tbl, uid = tu.split("/", 1)
            self.t[T_TARGET].new_row(IterationTargetGroupID=tg, IterationTargetGroupMemberUID="%s_t%d" % (cid, i),
                                     LinkToTarget=tbl, LinkToTargetUID=uid)
        v["LinkToTargets"], v["LinkToTargetsGroupID"] = T_TARGET, tg

    def add_loops(self, v, cid, loops, spec):
        lg = cid + "_loops"
        for k, lp in enumerate(loops):
            parts = lp.split(":")
            m = re.fullmatch(r"(\d+)(?:@(\w+))?", parts[0])
            if not m:
                sys.exit("Step '%s': loop=%s: use loop=<n>[@<label>]:<name>=<init>:<step>[:...]" % (spec, lp))
            pg = "%s_d%d_params" % (cid, k)
            self.t[T_LOOP].new_row(DimensionalLoopID="%s_d%d" % (cid, k), DimensionalLoopGroupID=lg,
                                   DimensionalLoopPriority=k + 1, NumberOfIterations=m.group(1),
                                   StepLabel=m.group(2) or "", LinkToDimensionalLoopParameterTable=T_PARAM,
                                   LinkToDimensionalLoopParameterGroupID=pg)
            rest = parts[1:]
            j = 0
            while rest:
                nm, eq, init = rest[0].partition("=")
                if not eq or len(rest) < 2:
                    sys.exit("Step '%s': loop=%s: each input is <name>=<init>:<step>." % (spec, lp))
                self.t[T_PARAM].new_row(ParameterID="%s_d%d_p%d" % (cid, k, j), ParameterGroupID=pg,
                                        ParameterName=nm, ParameterInitialValue=init, ParameterStepSize=rest[1])
                rest, j = rest[2:], j + 1
        v["LinkToMacroDimensionalLoopTable"], v["LinkToMacroDimensionalLoopGroupID"] = T_LOOP, lg

    def place(self, plan, rows, at):
        """put freshly appended plan-table rows into the plan's steps, before step <at> (1-based; None = end)"""
        p = self.t[T_PLAN]
        gid = self.gid(plan)
        for r in rows:
            p.put(r, "IterationPlanGroupID", gid)
            p.rows.remove(r)
        steps = self.steps(plan)
        idx = p.rows.index(steps[at - 1]) if at and 0 < at <= len(steps) else (
            p.rows.index(steps[-1]) + 1 if steps else len(p.rows))
        p.rows[idx:idx] = rows

    def describe(self, step, n):
        p = self.t[T_PLAN]
        c, r = self.cmd_row(step)
        out = "  %2d  %-26s %-3s" % (n, p.get(step, "CommandType"), "" if p.get(step, "Status") in ("On", "1", "True") else "OFF")
        if r:
            vals = ["%s=%s" % (h, r[i]) for i, h in enumerate(c.h[1:-3], 1)
                    if r[i] not in ("", "DEFAULT", "NO_LINK") and not h.startswith("LinkTo")]
            out += " " + " ".join(vals)
            tg = c.get(r, "LinkToTargetsGroupID")
            if tg:
                ts = ["%s/%s" % (self.t[T_TARGET].get(x, "LinkToTarget"), self.t[T_TARGET].get(x, "LinkToTargetUID"))
                      for x in self.t[T_TARGET].rows if x[0] == tg]
                out += " targets=" + ",".join(ts)
            lg = c.get(r, "LinkToMacroDimensionalLoopGroupID")
            if lg:
                lt, pt = self.t[T_LOOP], self.t[T_PARAM]
                for lr in [x for x in lt.rows if lt.get(x, "DimensionalLoopGroupID") == lg]:
                    pg = lt.get(lr, "LinkToDimensionalLoopParameterGroupID")
                    ps = ",".join("%s=%s:%s" % (pt.get(x, "ParameterName"), pt.get(x, "ParameterInitialValue"),
                                                pt.get(x, "ParameterStepSize")) for x in pt.rows if pt.get(x, "ParameterGroupID") == pg)
                    out += " loop[prio %s]=%sx(%s%s)" % (lt.get(lr, "DimensionalLoopPriority"), lt.get(lr, "NumberOfIterations"), ps,
                                                       " @" + lt.get(lr, "StepLabel") if lt.get(lr, "StepLabel") not in ("", "DEFAULT") else "")
        tsub = p.get(step, "TargetSubsystem")
        if tsub not in ("", "DEFAULT"):
            out += " subsystem=" + tsub
        return out

    def show(self, plan):
        row = self.plan_row(plan)
        st = self.t[T_ITER].get(row, "Status")
        print("Plan %s (%s), %d step(s):" % (plan, "enabled" if st in ("On", "1", "True") else "DISABLED", len(self.steps(plan))))
        for n, s in enumerate(self.steps(plan), 1):
            print(self.describe(s, n))


def commit(group, tabs, plan_name, dry):
    if dry:
        print("(dry run: nothing saved)")
        return 0
    versions = "".join("%s,%s," % (n, t.save()) for n, t in tabs.items())
    gname = group[0] if group else "iterateGroup"
    r = cfg(False, "saveNewTableGroup", "groupName=" + gname, "groupComment=" + quote("iteration_plan.py: " + plan_name, safe=""),
            "tableList=" + versions)
    dup = re.search(r"duplicate of an existing group key \((\d+)\)", err_of(r) or "")
    key = dup.group(1) if dup else (r.find("TableGroupKey").get("value") if r.find("TableGroupKey") is not None else None)
    if not key:
        sys.exit("ERROR saving the group: %s" % ((err_of(r) or ET.tostring(r).decode())[:500]))
    for ign in ("0", "1"):
        act = cfg(False, "activateTableGroup", "groupName=" + gname, "groupKey=" + key, "ignoreWarnings=" + ign)
        k = act.find("Iterate-ActiveGroupKey")
        if k is not None and k.get("value") == key:
            break
    else:
        sys.exit("The group was saved (%s key %s) but not activated." % (gname, key))
    print("Saved and activated Iterate group %s key %s." % (gname, key))
    print("New plan: apply_config_reload.sh <fsm>. Edited plan already played: restart ots. Then: ots_drive_udp_test <ip> <port> raw \"Iterator,iteratePlay,%s\"" % plan_name)
    return 0


def step_numbers(P, plan, words):
    steps = P.steps(plan)
    out = []
    for w in words:
        if not re.fullmatch(r"\d+", w) or not 1 <= int(w) <= len(steps):
            sys.exit("Step number '%s': plan %s has steps 1..%d." % (w, plan, len(steps)))
        out.append(steps[int(w) - 1])
    return out


def main():
    a = sys.argv[1:]
    dry = "-n" in a
    a = [x for x in a if x != "-n"]
    force = "-f" in a
    a = [x for x in a if x != "-f"]
    if not a or a[0] not in ("list", "show", "create", "add", "remove", "move", "set", "enable", "disable", "delete"):
        print(__doc__)
        return 1
    cmd = a[0]
    group, tabs = load_all()
    if T_ITER not in tabs or T_PLAN not in tabs:
        sys.exit("The Configuration GUI does not know %s / %s (table definitions missing)." % (T_ITER, T_PLAN))
    P = Plans(tabs)
    if cmd == "list":
        if not P.t[T_ITER].rows:
            print("No plans" + ("" if group else " (no active Iterate group)") + ".")
            return 0
        print("Active Iterate group %s key %s. Plans:" % group)
        for r in P.t[T_ITER].rows:
            s = P.steps(r[0])
            print("  %-24s %-4s %d step(s): %s" % (r[0], P.t[T_ITER].get(r, "Status"), len(s),
                  ", ".join(P.t[T_PLAN].get(x, "CommandType") for x in s)))
        return 0
    if len(a) < 2:
        print(__doc__)
        return 1
    plan = a[1]
    if not re.fullmatch(r"[A-Za-z0-9]+", plan):
        sys.exit("Plan names: letters and digits only.")
    exists = P.plan_row(plan) is not None
    if cmd == "show":
        if not exists:
            sys.exit("No plan %s." % plan)
        P.show(plan)
        return 0
    if cmd == "create":
        if len(a) < 3:
            print(__doc__)
            return 1
        if exists and not force:
            sys.exit("Plan %s exists: use -f to replace it, or add/remove/set to change it." % plan)
        if exists:
            P.delete_plan(plan)
        P.t[T_ITER].new_row(**{"IterationPlanName": plan, "Status": "On", "LinkToIterationPlanTable": T_PLAN,
                               "LinkToIterationPlanGroupID": plan + "Commands"})
        rows = [P.new_step(plan, s) for s in a[2:]]
        P.place(plan, rows, None)
    else:
        if not exists:
            sys.exit("No plan %s (list shows the plans)." % plan)
        if cmd == "add":
            at, specs = None, a[2:]
            if specs[:1] == ["--at"]:
                at, specs = int(specs[1]), specs[2:]
            if not specs:
                print(__doc__)
                return 1
            P.place(plan, [P.new_step(plan, s) for s in specs], at)
        elif cmd == "remove":
            for s in step_numbers(P, plan, a[2:]):
                P.remove_step(s)
        elif cmd == "move" and len(a) == 4:
            (s,) = step_numbers(P, plan, [a[2]])
            step_numbers(P, plan, [a[3]])
            p = P.t[T_PLAN]
            p.rows.remove(s)
            P.place(plan, [], None)  # no-op, keeps the gid lookup consistent
            steps = P.steps(plan)
            idx = p.rows.index(steps[int(a[3]) - 1]) if int(a[3]) <= len(steps) else p.rows.index(steps[-1]) + 1
            p.rows.insert(idx, s)
        elif cmd == "set" and len(a) >= 4:
            (s,) = step_numbers(P, plan, [a[2]])
            c, r = P.cmd_row(s)
            for kv in a[3:]:
                k, eq, val = kv.partition("=")
                if not eq:
                    sys.exit("'%s': use Column=value." % kv)
                if r and k in c.h[1:-2]:
                    c.put(r, k, val)
                elif k in P.t[T_PLAN].h:
                    P.t[T_PLAN].put(s, k, val)
                else:
                    sys.exit("No column %s in this step (command table columns: %s; plan columns: Status, TargetSubsystem)." %
                             (k, ", ".join(c.h[1:-2]) if c else "none: this step has no parameters"))
        elif cmd in ("enable", "disable"):
            val = "On" if cmd == "enable" else "Off"
            if len(a) == 2:
                P.t[T_ITER].put(P.plan_row(plan), "Status", val)
            else:
                for s in step_numbers(P, plan, a[2:]):
                    P.t[T_PLAN].put(s, "Status", val)
        elif cmd == "delete":
            P.delete_plan(plan)
        else:
            print(__doc__)
            return 1
    if cmd != "delete":
        P.show(plan)
    return commit(group, tabs, plan, dry)


if __name__ == "__main__":
    sys.exit(main())
