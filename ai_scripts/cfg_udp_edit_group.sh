#!/bin/bash
# Apply many edits across the tables of one active group via ConfigGUI UDP, save each
# edited table once, save ONE new group (optionally under a new group name) and activate it.
# Generalizes cfg_udp_edit_field.sh (one field) for multi-table changes such as deriving a
# top-level Context from an existing one.
#
# Prerequisites:
#   - source setup_ots.sh <subsystem>   (OTS_CONFIG_UDP_IP/PORT of the instance to edit)
#   - ots running with ConfigGUI UDP enabled; a Kerberos ticket (the edit author)
#
# Usage:
#   ./cfg_udp_edit_group.sh [-n] [-t <groupType>] [-g <newGroupName>] [-c <comment>] <edit> [<edit> ...]
#
#   -n   dry run: make the temporary edits, print each edited table, save nothing
#   -t   group type to start from: Context (default), Configuration, Backbone, Iterate
#   -g   save the result under this group name instead of the active group's name
#   -c   table/group comment (default: "Edited by cfg_udp_edit_group.sh")
#   -w   ignore tree warnings when saving/activating the group (ignoreWarnings=1, what the
#        GUI does after you accept them), e.g. links from Off apps to tables not in the group
#
# Edits, applied in order (values raw; the script URI-encodes them):
#   <Table>/<UID>/<Column>=<value>          set a cell (editNodeType=value)
#   <Table>/<UID>/<Column>@<type>=<value>   other editNodeType, e.g. link-UID with value
#                                           <tableName>,<uid> or link-GroupID <table>,<groupID>
#   +<Table>/<UID>                          add a row with that UID (table-newRow); set its
#                                           cells with following edits
#   new:<Table>=<c1>,<c2>,...[;<row2>...]   create the FIRST version of a table that has none
#                                           (columns in order incl. CommentDescription; Author
#                                           and RecordInsertionTime are filled in) and add it
#                                           to the group; edit it further with later edits
#   -<Table>/<UID>                          delete a row (put `--` before the edits); deleteTreeNodeRecords
#                                           names no version in its reply, so the newest temporary
#                                           version is looked up
#   add:<Table>[=<version>]                 add an existing table version (default: latest) to
#                                           the group, e.g. a table created by an earlier run
#
# Example (top-level Context from the active one, gateway UDP on 2114, FE off):
#   ./cfg_udp_edit_group.sh -g topContext \
#       GatewaySupervisorTable/Supervisor0/PortForStateChangesOverUDP=2114 \
#       XDAQApplicationTable/FESupervisor/Status=Off
#
# Activation writes $USER_DATA/ServiceData/ActiveTableGroups.cfg of the instance edited only.
# Restart ots afterwards for Context/Backbone changes. If the result equals an existing
# group, that group's key is activated instead (no duplicate is saved).

set -e

DRY=0; IGNW=0; GROUP_TYPE=Context; NEW_GROUP=""; COMMENT="Edited by cfg_udp_edit_group.sh"
while getopts "nwt:g:c:" o; do
    case $o in n) DRY=1;; w) IGNW=1;; t) GROUP_TYPE="$OPTARG";; g) NEW_GROUP="$OPTARG";; c) COMMENT="$OPTARG";;
    *) sed -n 2,45p "$0" | sed 's/^# \{0,1\}//'; exit 1;; esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { sed -n 2,45p "$0" | sed 's/^# \{0,1\}//'; exit 1; }

CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"

# ots_cfg_udp_test exits 1 on any <Error> reply; every call inspects its own reply instead.
cfg()    { ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }
cfg_ro() { ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }
urlencode() { python3 -c "import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=''))" "$1"; }
xml_error() {
    local m
    m=$(echo "$1" | grep -oP "Error value='\K[^']+" | sed 's/&apos;/'"'"'/g;s/&quot;/"/g' | head -1 || true)
    echo "${m:-$(echo "$1" | tail -3 | cut -c1-300)}"
}
print_table() {  # print_table <table> <version>
    cfg_ro getSpecificTable tableName="$1" version="$2" | python3 -c "
import sys,re,urllib.parse as u
x=sys.stdin.read(); h=re.findall(r\"<ColumnHeader value='([^']*)'\",x)
c=[[u.unquote(v) for v in s.split(',')] for s in re.findall(r\"<ColCSV value='([^']*)'\",x)]
k=[(a,b) for a,b in zip(h,c) if a not in ('Author','RecordInsertionTime','CommentDescription')]
print('   ', ' | '.join(a for a,_ in k))
for r in zip(*[b for _,b in k]): print('   ', ' | '.join(r))"
}
first_column() {  # the UID column name of a table version
    cfg_ro getSpecificTable tableName="$1" version="$2" | grep -oP "<ColumnHeader value='\K[^']+" | head -1
}

echo "=== cfg_udp_edit_group: ConfigGUI UDP $CFG_IP:$CFG_PORT ==="
ACTIVE=$(cfg_ro getActiveTableGroups)
GROUP_NAME=$(echo "$ACTIVE" | grep -oP "${GROUP_TYPE}-ActiveGroupName value='\K[^']+" || true)
GROUP_KEY=$(echo "$ACTIVE" | grep -oP "${GROUP_TYPE}-ActiveGroupKey value='\K[^']+" || true)
[ -n "$GROUP_NAME" ] && [ "$GROUP_KEY" != "-1" ] || { echo "ERROR: no active $GROUP_TYPE group (is ots running with ConfigGUI UDP?)."; exit 1; }
echo "Starting from active $GROUP_TYPE group $GROUP_NAME key $GROUP_KEY"
MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$GROUP_NAME" groupKey="$GROUP_KEY")

declare -A VER        # table -> current (possibly temporary) version
declare -A EDITED     # table -> 1 when it has a temporary version to persist
ORDER=()              # member order for the tableList
while read -r n v; do [ -n "$n" ] && { VER[$n]=$v; ORDER+=("$n"); }; done < <(
    echo "$MEMBERS" | grep -oP "Member(Name|Version) value='\K[^']+" | paste -d' ' - -)

ENC_COMMENT=$(urlencode "$COMMENT")

for e in "$@"; do
    if [[ "$e" == new:* ]]; then
        t="${e#new:}"; t="${t%%=*}"; rows="${e#*=}"
        [ -z "${VER[$t]}" ] || { echo "ERROR: $t is already in the group (v${VER[$t]})."; exit 1; }
        data=""
        IFS=';' read -ra R <<< "$rows"
        for row in "${R[@]}"; do
            IFS=',' read -ra CELLS <<< "$row"
            for c in "${CELLS[@]}"; do data+="$(urlencode "$c"),"; done
            data+=";"
        done
        echo "- create first version of $t"
        [ "$DRY" = 1 ] && { echo "  (dry run: not created)"; continue; }
        r=$(cfg saveSpecificTable tableName="$t" version=-1 temporary=0 dataOffset=0 \
            sourceTableAsIs=0 lookForEquivalent=0 tableComment="$ENC_COMMENT" "data=$data")
        v=$(echo "$r" | grep -oP "<savedVersion value='\K[^']+" || true)
        [ -n "$v" ] || { echo "ERROR creating $t: $(xml_error "$r")"; exit 1; }
        VER[$t]=$v; ORDER+=("$t"); echo "  $t v$v"
        continue
    fi

    if [[ "$e" == add:* ]]; then
        t="${e#add:}"; v=""; [[ "$t" == *=* ]] && { v="${t#*=}"; t="${t%%=*}"; }
        [ -z "${VER[$t]}" ] || { echo "ERROR: $t is already in the group (v${VER[$t]})."; exit 1; }
        if [ -z "$v" ]; then
            v=$(cfg_ro getSpecificTable tableName="$t" | grep -oP "TableVersion value='\K[^']+" | head -1 || true)
        fi
        [ -n "$v" ] && [ "$v" != "-1" ] || { echo "ERROR: $t has no saved version (use new:$t=...)."; exit 1; }
        VER[$t]=$v; ORDER+=("$t"); echo "- add $t v$v to the group"
        continue
    fi

    if [[ "$e" == -* ]]; then
        t="${e#-}"; uid="${t#*/}"; t="${t%%/*}"
        [ -n "${VER[$t]}" ] || { echo "ERROR: $t is not in the group."; exit 1; }
        mod=""; [ "${VER[$t]}" -lt 0 ] && mod="$t,${VER[$t]},"
        echo "- $t: delete row $uid"
        r=$(cfg deleteTreeNodeRecords tableGroup="$GROUP_NAME" tableGroupKey="$GROUP_KEY" startPath="/$t" \
            modifiedTables="$mod" recordList="$(urlencode "$uid")")
        echo "$r" | grep -q "<Error" && { echo "ERROR: $(xml_error "$r")"; exit 1; }
        v=-2; last=""   # temporary versions count down from -2; an unknown one reads back as -1
        while [ "$(cfg_ro getSpecificTable tableName="$t" version=$v | grep -oP "TableVersion value='\K[^']+" | head -1)" = "$v" ]; do last=$v; v=$((v - 1)); done
        [ -n "$last" ] || { echo "ERROR: no temporary version of $t found after the delete."; exit 1; }
        VER[$t]=$last; EDITED[$t]=1
        continue
    fi

    if [[ "$e" == +* ]]; then
        t="${e#+}"; uid="${t#*/}"; t="${t%%/*}"
        [ -n "${VER[$t]}" ] || { echo "ERROR: $t is not in the group."; exit 1; }
        col=$(first_column "$t" "${VER[$t]}")
        echo "- $t: add row $uid"
        r=$(cfg saveTreeNodeEdit targetTable="$t" targetTableVersion="${VER[$t]}" targetUID="$uid" \
            targetColumn="$col" newValue="$(urlencode "$uid")" editNodeType=table-newRow)
    else
        lhs="${e%%=*}"; val="${e#*=}"
        [ "$lhs" != "$e" ] || { echo "ERROR: bad edit '$e'."; exit 1; }
        type=value
        if [[ "$lhs" == *@* ]]; then type="${lhs#*@}"; lhs="${lhs%@*}"; fi
        IFS=/ read -r t uid col <<< "$lhs"
        [ -n "$t" ] && [ -n "$uid" ] && [ -n "$col" ] || { echo "ERROR: bad edit '$e' (Table/UID/Column=value)."; exit 1; }
        [ -n "${VER[$t]}" ] || { echo "ERROR: $t is not in the group."; exit 1; }
        if [ "$type" = value ]; then nv=$(urlencode "$val")
        else # link-UID/link-GroupID (<table>,<id>) and table-newGroupRow (<linkIndex>,<groupID>,<uid>):
             # the server splits on raw commas, so encode each field, not the commas
             nv=""; IFS=, read -r -a parts <<< "$val"
             for p in "${parts[@]}"; do nv+="${nv:+,}$(urlencode "$p")"; done; fi
        echo "- $t/$uid/$col = $val${type:+ ($type)}"
        r=$(cfg saveTreeNodeEdit targetTable="$t" targetTableVersion="${VER[$t]}" targetUID="$uid" \
            targetColumn="$col" newValue="$nv" editNodeType="$type")
    fi
    v=$(echo "$r" | grep -oP "savedVersion value='\K[^']+" || true)
    if [ -n "$v" ]; then VER[$t]=$v; EDITED[$t]=1
    elif echo "$r" | grep -q "is the same as the current value"; then echo "  (already that value)"
    else echo "ERROR: $(xml_error "$r")"; exit 1; fi
done

for t in "${!EDITED[@]}"; do echo; echo "$t (temporary v${VER[$t]}):"; print_table "$t" "${VER[$t]}"; done

if [ "$DRY" = 1 ]; then echo; echo "Dry run: nothing saved."; exit 0; fi

echo
for t in "${!EDITED[@]}"; do
    r=$(cfg saveSpecificTable tableName="$t" version="${VER[$t]}" temporary=0 dataOffset=0 chunkSize=0 \
        sourceTableAsIs=1 lookForEquivalent=1 tableComment="$ENC_COMMENT")
    v=$(echo "$r" | grep -oP "<savedVersion value='\K[^']+" || true)
    [ -n "$v" ] || { echo "ERROR saving $t: $(xml_error "$r")"; exit 1; }
    VER[$t]=$v; echo "Saved $t v$v"
done

TABLE_LIST=""; for t in "${ORDER[@]}"; do TABLE_LIST+="$t,${VER[$t]},"; done
TARGET="${NEW_GROUP:-$GROUP_NAME}"
r=$(cfg saveNewTableGroup groupName="$TARGET" groupComment="$ENC_COMMENT" tableList="$TABLE_LIST" ignoreWarnings=$IGNW)
TREE=$(echo "$r" | grep -oP "TreeErrors value='\K[^']+" | sed "s/&apos;/'/g" | tr '|' '\n' | grep -v '^ConfigurationManager:' || true)
if [ -n "$TREE" ] && ! echo "$r" | grep -q "TableGroupKey value="; then
    echo "Group not saved, tree warnings:"; echo "$TREE" | sed 's/^/  /'
    echo "Tables are saved; rerun with -w to accept the warnings (new: tables now exist: drop those edits"
    echo "and add the table with its version instead -- see the README)."; exit 1
fi
dup=$(echo "$r" | grep -oP "duplicate of an existing group key \(\K[0-9]+" || true)
[ -n "$dup" ] && { echo "Identical to existing $TARGET key $dup; activating that key."; r="<TableGroupKey value='$dup'/>"; }
KEY=$(echo "$r" | grep -oP "<TableGroupKey value='\K[^']+" | head -1 || true)
[ -n "$KEY" ] || { echo "ERROR saving group $TARGET: $(xml_error "$r")"; exit 1; }
echo "Saved group $TARGET key $KEY"

r=$(cfg activateTableGroup groupName="$TARGET" groupKey="$KEY" ignoreWarnings=$IGNW)
echo "Active $GROUP_TYPE: $(echo "$r" | grep -oP "${GROUP_TYPE}-ActiveGroupName value='\K[^']+" || true) key $(echo "$r" | grep -oP "${GROUP_TYPE}-ActiveGroupKey value='\K[^']+" || true)"
echo "=== Done. Restart ots for Context/Backbone changes. ==="
