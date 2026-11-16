#!/usr/bin/env python3
"""mm_macro.py - list, run, create and delete FE macros and Macro Maker macros over the Macro Maker UDP remote control.

Two kinds of macro, both run on a front end (an FE interface plugin):
  FE macro          C++ function registered in the FE plugin (compiled; fe-macros-and-macromaker.md)
  Macro Maker macro a short script of register reads/writes/delays saved as a file by Macro Maker
                    ($USER_DATA/ServiceData/MacroData/publicMacros/<name>.dat for everyone, .../NO-USER/<name>.dat for
                    this UDP user); needs no compile. Steps use hex numbers; a name that is not a hex number is a variable.

  mm_macro.py list [<feUID>]                        front ends with their FE macros (inputs -> outputs) and the saved Macro Maker macros
  mm_macro.py show <name>                           a Macro Maker macro: steps, input and output variables
  mm_macro.py run <feUID> <macro> [name=value ...] [--type fe|public|private] [--save]
        runs a macro on that front end; --type defaults to fe if an FE macro of that name exists, else public, else private;
        every input must be given by name, outputs are all the macro's outputs; values are numbers: decimal, or hex with 0x
        (regaddr=1005 is 0x3ED, regaddr=0x1005 is 0x1005; literals inside a macro file are hex without 0x); prints the outputs
        (Macro Maker macro outputs read "decimal (0xhex)"); --save also writes them to $OTSDAQ_DATA/macroOutput_*.txt
  mm_macro.py new <name> [--private] [--lsbf] [-f] [--notes TEXT] <step> [<step> ...]
        writes a Macro Maker macro (public unless --private); steps, each one quoted argument:
          "w <addr> <data|$var>"   write (addr and data hex or 0x..., or a $variable for the data)
          "r <addr|$var> [$out]"   read; with $out the value is returned as an output of the macro
          "d <ms>"                 delay, milliseconds (decimal)
        --lsbf: addresses and data are written least-significant-byte first
  mm_macro.py delete <name> [--private]

Variable names: letters/digits/underscore, and NOT made only of the letters a-f and digits (a name like add, cafe or 12 is read as a
hex number, so the macro would silently have no variable): the script refuses such names.
The Macro Maker remote control: OTS_MACROMAKER_UDP_IP / OTS_MACROMAKER_UDP_PORT; the system must be Configured (front ends exist).
Exit status: 0 ok, 1 error.
"""
import os
import re
import socket
import sys
import time
import xml.etree.ElementTree as ET
from html import unescape
from urllib.parse import quote, unquote

MM_IP = os.environ.get("OTS_MACROMAKER_UDP_IP", "127.0.0.1")
MM_PORT = int(os.environ.get("OTS_MACROMAKER_UDP_PORT", "2011") or 2011)
HEX = set("0123456789abcdefABCDEF")


def udp(msg, timeout=30.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(timeout)
    s.sendto(msg.encode(), (MM_IP, MM_PORT))
    out = b""
    try:
        while True:
            out += s.recvfrom(65535)[0]
            if b"</ROOT>" in out[-10:] or not out.lstrip().startswith(b"<"):
                break
            s.settimeout(1.0)
    except socket.timeout:
        pass
    if not out:
        sys.exit("No reply from Macro Maker at %s:%d (OTS_MACROMAKER_UDP_IP/PORT; is ots running and Configured?)." % (MM_IP, MM_PORT))
    return out.decode(errors="replace")


def macro_dir(private):
    sdp = os.environ.get("SERVICE_DATA_PATH") or (os.environ.get("USER_DATA", "") + "/ServiceData")
    if not os.path.isdir(sdp):
        sys.exit("No ServiceData directory (%s): source setup_ots.sh first." % sdp)
    return os.path.join(sdp, "MacroData", "NO-USER" if private else "publicMacros")


def fe_info():
    """{feUID: (feClass, {macro: (inputs, outputs, tooltip)})} from GetFrontendMacroInfo"""
    t = udp("GetFrontendMacroInfo")
    fes = {}
    for v in re.findall(r"<FEMacros value='([^']*)'/>", t):
        f = v.split(";")
        cls, uid, i, macros = f[2], f[3], 4, {}
        while i + 3 < len(f):  # name; permission; tooltip; nIn; inputs...; nOut; outputs...
            name, tip, nin = unquote(f[i]), unquote(f[i + 2]), int(f[i + 3])
            ins = [unquote(x) for x in f[i + 4:i + 4 + nin]]
            j = i + 4 + nin
            nout = int(f[j])
            outs = [unquote(x) for x in f[j + 1:j + 1 + nout]]
            macros[name] = (ins, outs, tip)
            i = j + 1 + nout
        fes[uid] = (cls, macros)
    if not fes:
        sys.exit("Macro Maker lists no front ends: the system must be Configured.")
    return fes


def is_var(field):
    return any(c not in HEX for c in field)


def parse_macro_file(path):
    """(sequence string, notes, lsbf) of a Macro Maker macro file"""
    t = open(path).read().replace("\n", "")
    m = re.search(r'"sequence":"([^"]*)"', t)
    if not m:
        sys.exit("%s is not a Macro Maker macro file (no sequence)." % path)
    n = re.search(r'"notes":"([^"]*)"', t)
    l = re.search(r'"LSBF":"([^"]*)"', t)
    return m.group(1), n.group(1) if n else "", (l.group(1) if l else "false")


def steps_of(seq):
    out = []
    for c in [x for x in seq.split(",") if x]:
        p = c.split(":")
        out.append(p)
    return out


def macro_vars(seq):
    ins, outs = [], []
    for p in steps_of(seq):
        kind = p[1] if len(p) > 1 else "?"
        fields = p[2:]
        if kind == "r":
            if fields and is_var(fields[0]) and fields[0] not in ins:
                ins.append(fields[0])
            if len(fields) > 1 and fields[1] and is_var(fields[1]) and fields[1] not in outs:
                outs.append(fields[1])
        elif kind in ("w", "d"):
            for fld in fields:
                if fld and is_var(fld) and fld not in ins:
                    ins.append(fld)
    return ins, outs


def find_macro_file(name, private=None):
    for priv in ([private] if private is not None else [False, True]):
        p = os.path.join(macro_dir(priv), name + ".dat")
        if os.path.isfile(p):
            return p, priv
    return None, None


def describe_step(p):
    k = p[1]
    if k == "w":
        return "write 0x%s <- %s" % (p[2], ("$" + p[3]) if is_var(p[3]) else "0x" + p[3])
    if k == "r":
        a = ("$" + p[2]) if is_var(p[2]) else "0x" + p[2]
        return "read  %s%s" % (a, (" -> $" + p[3]) if len(p) > 3 and p[3] else " (value not kept)")
    if k == "d":
        if is_var(p[2]):
            return "delay $%s ms" % p[2]
        return "delay %d ms" % (int(p[2]) if p[2].isdigit() else int(p[2], 16))  # digits: decimal (browser), hex letters: hand-written
    return "?" + ":".join(p)


def cmd_list(a):
    fes = fe_info()
    for uid, (cls, macros) in fes.items():
        if a and uid != a[0]:
            continue
        print("%s (%s)" % (uid, cls))
        for n, (i, o, tip) in macros.items():
            print("    FE macro %-30s in: %-40s out: %s" % (n, ", ".join(i) or "-", ", ".join(o) or "-"))
    for priv in (False, True):
        d = macro_dir(priv)
        names = sorted(x[:-4] for x in os.listdir(d) if x.endswith(".dat")) if os.path.isdir(d) else []
        print("%s Macro Maker macros (%s): %s" % ("Private (NO-USER)" if priv else "Public", d, ", ".join(names) or "none"))


def cmd_show(a):
    if not a:
        sys.exit(__doc__)
    p, priv = find_macro_file(a[0])
    if not p:
        sys.exit("No Macro Maker macro '%s' (list shows them)." % a[0])
    seq, notes, lsbf = parse_macro_file(p)
    ins, outs = macro_vars(seq)
    print("%s (%s) %s; LSBF=%s%s" % (a[0], "private" if priv else "public", p, lsbf, "; notes: " + notes if notes else ""))
    for p_ in steps_of(seq):
        print("  %s  %s" % (p_[0], describe_step(p_)))
    print("inputs: %s | outputs: %s" % (", ".join(ins) or "-", ", ".join(outs) or "-"))


def cmd_run(a):
    save, mtype, pos = False, None, []
    it = iter(a)
    for x in it:
        if x == "--save":
            save = True
        elif x == "--type":
            mtype = next(it, None)
        else:
            pos.append(x)
    if len(pos) < 2 or mtype not in (None, "fe", "public", "private"):
        sys.exit(__doc__)
    uid, name, kv = pos[0], pos[1], pos[2:]
    fes = fe_info()
    if uid not in fes:
        sys.exit("No front end '%s' (known: %s)." % (uid, ", ".join(fes)))
    cls, macros = fes[uid]
    if mtype is None:
        mtype = "fe" if name in macros else ("public" if find_macro_file(name, False)[0] else "private")
    if mtype == "fe":
        if name not in macros:
            sys.exit("%s has no FE macro '%s' (it has: %s)." % (uid, name, "; ".join(macros)))
        ins, outs = macros[name][0], macros[name][1]
    else:
        p, _ = find_macro_file(name, mtype == "private")
        if not p:
            sys.exit("No %s Macro Maker macro '%s'." % (mtype, name))
        ins, outs = macro_vars(parse_macro_file(p)[0])
    given = {}
    for s in kv:
        # input names may contain '=' (e.g. "Pause Slow Controls (Default = false)"): match the longest known name
        k = next((i for i in sorted(ins, key=len, reverse=True) if s.startswith(i + "=")), None)
        if not k:
            sys.exit("'%s' is not name=value for an input of %s (inputs: %s)." % (s, name, ", ".join(ins) or "none"))
        given[k] = s[len(k) + 1:]
    missing = [i for i in ins if i not in given]
    if missing:
        sys.exit("%s needs inputs: %s." % (name, ", ".join(missing)))
    inputs = ";".join(quote(i, safe="") + "," + quote(given[i], safe="") for i in ins)
    msg = ";".join(["RunFrontendMacro", cls, uid, mtype, quote(name, safe=""), quote(inputs, safe=""),
                    quote(",".join(outs), safe=""), "1" if save else "0"])
    t = udp(msg)
    err = re.search(r"<Error value='([^']*)'", t)
    if err or t.startswith("Error"):
        e = unescape(err.group(1) if err else t)
        e = re.sub(r"&#?\w+;", " ", unescape(e))
        # the cause is the last "error" text of the chain; show the first lines
        print("FAILED: %s %s on %s:" % (mtype, name, uid))
        for line in [x.strip() for x in re.split(r"\|", e) if x.strip()][:6]:
            print("   " + line[:240])
        return 1
    names = [unquote(unescape(x)) for x in re.findall(r"<outputArgs_name value='([^']*)'", t)]
    vals = [unquote(unescape(x)) for x in re.findall(r"<outputArgs_value value='([^']*)'", t)]
    print("%s %s on %s: ok" % (mtype, name, uid))
    for n, v in zip(names, vals):
        print("  %-24s %s" % (n, v))
    if not names:
        print("  (no outputs)")
    fn = re.search(r"Filename.*?<feMacroRunArgs_value value='([^']*)'", t, re.S)
    if save and fn:
        print("  output file: " + fn.group(1))
    return 0


def to_hex(tok, what):
    try:
        return "%x" % int(tok, 16)  # hex with or without 0x
    except ValueError:
        sys.exit("%s '%s' is not a hex number (use 1004, 0x1004, or a $variable)." % (what, tok))


def var_name(tok):
    n = tok[1:]
    if not re.fullmatch(r"[A-Za-z0-9_]+", n):
        sys.exit("Variable '%s': letters, digits and underscore only." % tok)
    if not is_var(n):
        sys.exit("Variable '%s' is made only of hex characters (a-f, 0-9): Macro Maker would read it as a number. Rename it (e.g. %s_)." % (tok, n))
    return n


def cmd_new(a):
    private = "--private" in a
    lsbf = "--lsbf" in a
    force = "-f" in a
    notes = ""
    rest = [x for x in a if x not in ("--private", "--lsbf", "-f")]
    if "--notes" in rest:
        i = rest.index("--notes")
        notes = rest[i + 1]
        rest = rest[:i] + rest[i + 2:]
    if len(rest) < 2:
        sys.exit(__doc__)
    name, steps = rest[0], rest[1:]
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
        sys.exit("Macro names: letters, digits, _ and - only.")
    out = []
    for n, s in enumerate(steps):
        t = s.split()
        if t and t[0] == "w" and len(t) == 3:
            addr = to_hex(t[1], "Address")
            data = var_name(t[2]) if t[2].startswith("$") else to_hex(t[2], "Data")
            out.append("%d:w:%s:%s" % (n, addr, data))
        elif t and t[0] == "r" and len(t) in (2, 3):
            addr = var_name(t[1]) if t[1].startswith("$") else to_hex(t[1], "Address")
            o = var_name(t[2]) if len(t) == 3 and t[2].startswith("$") else ""
            if len(t) == 3 and not t[2].startswith("$"):
                sys.exit("Step %d: the output of a read is a $variable." % n)
            out.append("%d:r:%s:%s" % (n, addr, o))
        elif t and t[0] == "d" and len(t) == 2 and t[1].isdigit():
            out.append("%d:d:%d" % (n, int(t[1])))  # decimal ms, as the browser writes it
        else:
            sys.exit("Step %d '%s': use 'w <addr> <data|$var>', 'r <addr|$var> [$out]' or 'd <ms>'." % (n, s))
    d = macro_dir(private)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, name + ".dat")
    if os.path.exists(path) and not force:
        sys.exit("%s exists: use -f to overwrite." % path)
    # the exact layout Macro Maker's createMacro writes (the FE parses it by position)
    with open(path, "w") as f:
        f.write('{\n"name":"%s",\n"sequence":"%s",\n"time":"%s",\n"notes":"%s",\n"LSBF":"%s"\n}@\n'
                % (name, ",".join(out), time.strftime("%a %b %d %Y"), notes.replace('"', "'"), "true" if lsbf else "false"))
    ins, outs = macro_vars(",".join(out))
    print("Wrote %s (%s macro): inputs %s, outputs %s" % (path, "private" if private else "public", ins or "-", outs or "-"))
    print("Run: mm_macro.py run <feUID> %s %s" % (name, " ".join(i + "=<value>" for i in ins)))
    return 0


def cmd_delete(a):
    private = "--private" in a
    rest = [x for x in a if x != "--private"]
    if len(rest) != 1:
        sys.exit(__doc__)
    p = os.path.join(macro_dir(private), rest[0] + ".dat")
    if not os.path.isfile(p):
        sys.exit("No such macro file %s." % p)
    os.remove(p)
    print("Deleted %s" % p)
    return 0


def main():
    a = sys.argv[1:]
    cmds = {"list": cmd_list, "show": cmd_show, "run": cmd_run, "new": cmd_new, "delete": cmd_delete}
    if not a or a[0] not in cmds:
        print(__doc__)
        return 1
    return cmds[a[0]](a[1:]) or 0


if __name__ == "__main__":
    sys.exit(main())
