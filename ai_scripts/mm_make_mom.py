#!/usr/bin/env python3
"""mm_make_mom.py - write a Macro Maker MoM (macro of macros) sequence file, the file the browser's
"Save sequence" writes, so an agent can build sequences without the browser.

Usage: mm_make_mom.py [-f] [--user NO-USER] [--var NAME=VALUE[:STEP[:MAX]]]... <name> <step> [<step> ...]

Each <step> is ONE argument (quote it):
  "<feUID> <macroName> [input=value ...]"   an FE macro (macro names may contain spaces; inputs by
                                            name, as listed by: ots_mm_udp_test <ip> <port> <feUID>).
                                            Write the macro name unquoted and quote an input whose name
                                            has spaces:  "ExampleInterface0 Slow Controls Pause/Resume
                                            'Pause Slow Controls (Default = false)=true'"
  "delay <seconds>"                         wait
  "mom <sequenceName> [iterations]"         run another sequence (nested MoM)
Input values may contain variables, $name, defined with --var name=value[:step[:max]]: each
iteration adds step to value, up to max (same rules as ots_mm_udp_test [name,value,step,max]).

The FE class and the input and output names of each FE macro are read from the running Macro Maker
(OTS_MACROMAKER_UDP_IP/OTS_MACROMAKER_UDP_PORT), so names are checked before the file is written.
The file goes to $SERVICE_DATA_PATH/MacroSequence/<user>/<name>.dat (user NO-USER: the sequences the
UDP remote control and ots_mm_udp_test see). -f overwrites an existing sequence.

Run it:  ots_mm_udp_test <ip> <port> <name> <iterations> [[name,value,step,max] ...]
Example:
  mm_make_mom.py --var amp=10:10:100 SineSweep \\
      "ExampleInterface0 varTest myOtherArg=\\$amp" "delay 2" "mom ExampleMoM 1"
"""
import json
import os
import re
import shlex
import socket
import sys
from urllib.parse import quote, unquote

MM_IP = os.environ.get("OTS_MACROMAKER_UDP_IP", "127.0.0.1")
MM_PORT = int(os.environ.get("OTS_MACROMAKER_UDP_PORT", "2011") or 2011)


def fe_macros():
    """{feUID: (feClass, {macroName: (inputs, outputs)})} from GetFrontendMacroInfo"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(5)
    s.sendto(b"GetFrontendMacroInfo", (MM_IP, MM_PORT))
    out = b""
    try:
        while True:
            out += s.recvfrom(65535)[0]
            if b"</ROOT>" in out[-10:]:
                break
            s.settimeout(1.0)
    except socket.timeout:
        pass
    if not out:
        sys.exit("No reply from Macro Maker at %s:%d (OTS_MACROMAKER_UDP_IP/PORT; is ots running?)." % (MM_IP, MM_PORT))
    fes = {}
    for v in re.findall(r"<FEMacros value='([^']*)'/>", out.decode(errors="replace")):
        f = v.split(";")
        cls, uid, i, macros = f[2], f[3], 4, {}
        # per macro: name; permission; tooltip; nIn; inputs...; nOut; outputs...
        while i + 3 < len(f):
            name = unquote(f[i]); nin = int(f[i + 3]); ins = [unquote(x) for x in f[i + 4:i + 4 + nin]]
            j = i + 4 + nin; nout = int(f[j]); outs = [unquote(x) for x in f[j + 1:j + 1 + nout]]
            macros[name] = (ins, outs)
            i = j + 1 + nout
        fes[uid] = (cls, macros)
    return fes


def main():
    args, force, user, variables = sys.argv[1:], False, "NO-USER", {}
    while args and args[0].startswith("-"):
        a = args.pop(0)
        if a == "-f":
            force = True
        elif a == "--user":
            user = args.pop(0)
        elif a == "--var":
            name, _, rest = args.pop(0).partition("=")
            parts = (rest.split(":") + ["0", "0"])[:3]
            variables["$" + name.lstrip("$")] = {"value": parts[0], "step": parts[1] or "0", "max": parts[2] or "0"}
        else:
            print(__doc__)
            return 1
    if len(args) < 2:
        print(__doc__)
        return 1
    name, steps = args[0], args[1:]
    if not re.fullmatch(r"[A-Za-z0-9 _-]+", name):
        sys.exit("Sequence names may hold only letters, digits, space, - and _ (Macro Maker's rule).")
    sdp = os.environ.get("SERVICE_DATA_PATH") or (os.environ.get("USER_DATA", "") + "/ServiceData")
    if not os.path.isdir(sdp):
        sys.exit("No ServiceData directory (%s): source setup_ots.sh first." % sdp)
    path = os.path.join(sdp, "MacroSequence", user, name + ".dat")
    if os.path.exists(path) and not force:
        sys.exit("%s exists: use -f to overwrite." % path)

    fes = None
    seq, used_vars = {}, set()
    for n, step in enumerate(steps):
        tok = shlex.split(step)
        if not tok:
            sys.exit("Empty step %d." % n)
        if tok[0] == "delay" and len(tok) == 2:
            seq[str(n)] = {"feClass": "__delay__", "feUID": "__delay__", "macroType": "__delay__", "macroName": "Delay",
                           "inputArgs": "Duration%20(s)," + quote(tok[1], safe=""), "outputArgs": "", "saveOutputs": "0"}
            continue
        if tok[0] == "mom" and len(tok) in (2, 3):
            seq[str(n)] = {"feClass": "__mom__", "feUID": "__mom__", "macroType": "__mom__", "macroName": tok[1],
                           "inputArgs": "Iterations," + quote(tok[2] if len(tok) == 3 else "1", safe=""),
                           "outputArgs": "", "saveOutputs": "0"}
            continue
        fes = fes or fe_macros()
        if tok[0] not in fes:
            sys.exit("Step %d: '%s' is not a front-end UID (known: %s)." % (n, tok[0], ", ".join(fes)))
        cls, macros = fes[tok[0]]
        rest = tok[1:]
        # the longest macro name that the words start with (names may contain spaces)
        macro = next((m for m in sorted(macros, key=len, reverse=True)
                      if rest[:len(m.split())] == m.split()), None)
        if macro is None:
            sys.exit("Step %d: no macro of %s matches '%s' (known: %s)." % (n, tok[0], " ".join(rest), "; ".join(macros)))
        ins, outs = macros[macro]
        given = {}
        for kv in rest[len(macro.split()):]:
            # input names may themselves contain '=' (e.g. "Pause Slow Controls (Default = false)")
            k = next((i for i in sorted(ins, key=len, reverse=True) if kv.startswith(i + "=")), None)
            eq, v = (True, kv[len(k) + 1:]) if k else (False, "")
            if not eq:
                sys.exit("Step %d: '%s' is not input=value for %s (inputs: %s)." % (n, kv, macro, ", ".join(ins) or "none"))
            given[k] = v
            used_vars.update(re.findall(r"\$[A-Za-z_]\w*", v))
        missing = [i for i in ins if i not in given]
        if missing:
            sys.exit("Step %d: %s needs inputs %s." % (n, macro, ", ".join(missing)))
        seq[str(n)] = {"feClass": cls, "feUID": tok[0], "macroType": "fe", "macroName": macro,
                       "inputArgs": ";".join(quote(i, safe="") + "," + quote(given[i], safe="") for i in ins),
                       "outputArgs": ",".join(quote(o, safe="") for o in outs), "saveOutputs": "0"}
    undefined = used_vars - set(variables)
    if undefined:
        sys.exit("Variables used but not defined with --var: %s" % ", ".join(sorted(undefined)))
    if variables:
        seq["__variables__"] = {k: v for k, v in variables.items() if k in used_vars} or variables
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(seq, f)
    print("Wrote %s: %d step(s)%s" % (path, len(steps), (", variables " + ", ".join(variables)) if variables else ""))
    print("Run:   ots_mm_udp_test %s %d '%s' 1" % (MM_IP, MM_PORT, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
