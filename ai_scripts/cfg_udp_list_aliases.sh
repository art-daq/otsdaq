#!/bin/bash
# List the aliases of the active Backbone group (read-only):
#   - group aliases (GroupAliasesTable): alias -> group name and key; these are the names
#     that a Configure transition accepts (e.g. defaultSystemAlias)
#   - table version aliases (VersionAliasesTable): alias -> table and version
#
# Prerequisites: source setup_ots.sh; ots running with OTS_CONFIG_UDP_IP/OTS_CONFIG_UDP_PORT set.
#
# Usage: cfg_udp_list_aliases.sh [-c]
#   -c  only the aliases to give to Configure: the group aliases of type Configuration, one per
#       line as "<alias> <group> <key>" (nothing else printed; exit 1 if there is none), e.g.
#         gateway_udp_cmd.sh <ip> <port> PhysicsRuns0,Configure,$(cfg_udp_list_aliases.sh -c | head -1 | cut -d' ' -f1)
#       The list is the Configuration GUI's active Backbone. The Gateway resolves an alias through
#       the Backbone it read at Startup: after an alias change, apply_config_reload.sh first.
#   The group key an alias points at is what Configure loads; after editing a table and saving a
#   new group key, move the alias with cfg_udp_set_alias.sh (or a table alias with
#   cfg_udp_set_table_alias.sh).

CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"
[ "$1" = "-h" ] && { sed -n 2,17p "$0"; exit 0; }
MODE=all; [ "$1" = "-c" ] && MODE=configure
command -v ots_cfg_udp_test >/dev/null || { echo "ots_cfg_udp_test not in PATH: source setup_ots.sh"; exit 1; }

cfg_ro() { ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }   # exit 1 on a harmless <Error>: inspect the reply

GROUPS_XML=$(cfg_ro getGroupAliases)
VERS_XML=$(cfg_ro getVersionAliases)
if ! echo "$GROUPS_XML" | grep -q "<ROOT>"; then
	echo "No reply from the Configuration GUI at $CFG_IP:$CFG_PORT:"; echo "$GROUPS_XML" | head -5; exit 1
fi

python3 -I - "$GROUPS_XML" "$VERS_XML" "$MODE" <<'EOF'
import re, sys, html
from urllib.parse import unquote
g, v = sys.argv[1], sys.argv[2]
def vals(text, tag): return [unquote(html.unescape(x)) for x in re.findall(r"<%s value='([^']*)'/>" % tag, text)]
def one(text, tag):
    x = vals(text, tag); return x[0] if x else ""
if sys.argv[3] == "configure":
    rows = [r for r in zip(vals(g, "GroupAlias"), vals(g, "GroupName"), vals(g, "GroupKey"), vals(g, "GroupType")) if r[3] == "Configuration"]
    for a, n, k, t in rows: print(a, n, k)
    if not rows:
        for err in vals(g, "Error"): print("ERROR from the Configuration GUI:", err.splitlines()[0] if err else err, file=sys.stderr)
        print("No group alias of type Configuration in the active Backbone.", file=sys.stderr)
    sys.exit(0 if rows else 1)
for err in vals(g, "Error") + vals(v, "Error"):
    print("ERROR from the Configuration GUI:", err.splitlines()[0] if err else err)
print("Active Backbone: %s key %s" % (one(g, "Backbone-ActiveGroupName"), one(g, "Backbone-ActiveGroupKey")))
print("Active Configuration group: %s key %s (active alias: %s)" % (
    one(g, "Configuration-ActiveGroupName"), one(g, "Configuration-ActiveGroupKey"),
    one(g, "Configuration-ActiveGroupAlias") or "none"))
print()
print("Group aliases (GroupAliasesTable v%s):" % one(g, "GroupAliasesTableVersion"))
rows = list(zip(vals(g, "GroupAlias"), vals(g, "GroupName"), vals(g, "GroupKey"), vals(g, "GroupType"), vals(g, "AliasComment")))
if not rows: print("  (none)")
for a, n, k, t, c in rows:
    print("  %-28s -> %s key %-5s %-14s %s" % (a, n, k, "(" + t + ")", c))
print()
print("Table version aliases (VersionAliasesTable v%s):" % one(v, "VersionAliasesVersion"))
rows = list(zip(vals(v, "VersionAlias"), vals(v, "TableName"), vals(v, "Version"), vals(v, "Comment")))
if not rows: print("  (none)")
for a, t, ver, c in rows:
    print("  %-28s -> %s version %-5s %s" % (a, t, ver, c))
EOF
