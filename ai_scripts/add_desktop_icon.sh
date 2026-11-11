#!/bin/bash
# Add a Desktop icon to the active Context group via ConfigGUI UDP, then save a new
# Context group and activate it. Does what the Configuration GUI's "add desktop icon"
# actions do (e.g. the App Status icon), which cannot be sent over UDP because the
# request (addDesktopIcon) is handled by the GatewaySupervisor, not the ConfigGUI.
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running with OTS_CONFIG_UDP_PORT and OTS_CONFIG_UDP_IP set
#   - a Kerberos ticket (kinit): the author of the edit is taken from it; the tool asks
#     before proceeding anonymously, and this script never auto-confirms that
#
# Usage:
#   ./add_desktop_icon.sh [-n] appstatus
#   ./add_desktop_icon.sh [-n] <caption> <altText> <windowURL> <imageURL> [permission] [linkedApp] [unique] [folder]
#
#   -n          dry run: make the temporary table edits (visible only to this UDP user),
#               print the new row, and stop before saving anything
#   appstatus   preset: "App Status", STAT, /WebPath/html/SystemStatus.html,
#               icon-SystemStatus.png, permission 1, linked to the Gateway (LID 200), unique
#   permission  RequiredPermissionLevel (default 1)
#   linkedApp   XDAQApplicationTable app UID or numeric LID the window talks to; the
#               Gateway is LID 200 (default 200); use NONE for no link
#   unique      1 = only one window instance (default 1), 0 = many
#   folder      desktop folder name (default: none)
#
# The icon record UID is generatedIcon<caption letters and digits>, like the GUI. An icon
# with that UID already in the group is refused (no duplicates).
#
# Icons are read by the Gateway at startup: restart ots afterwards. Check with
#   gateway_udp_cmd.sh <ip> <gateway UDP port> GetRemoteDesktopIcons | grep "<caption>"

set -e

DRY=0
if [ "$1" = "-n" ]; then DRY=1; shift; fi

if [ "$1" = "appstatus" ]; then
    CAPTION="App Status"; ALT="STAT"; URL="/WebPath/html/SystemStatus.html"
    IMAGE="icon-SystemStatus.png"; PERM=1; LINKED=200; UNIQUE=1; FOLDER=""
elif [ $# -ge 4 ]; then
    CAPTION="$1"; ALT="$2"; URL="$3"; IMAGE="$4"
    PERM="${5:-1}"; LINKED="${6:-200}"; UNIQUE="${7:-1}"; FOLDER="${8:-}"
else
    sed -n 2,34p "$0" | sed 's/^# \{0,1\}//'
    exit 1
fi

CFG_IP="${OTS_CONFIG_UDP_IP:-127.0.0.1}"
CFG_PORT="${OTS_CONFIG_UDP_PORT:-2010}"

# ots_cfg_udp_test exits 1 when the reply holds an <Error> (even a harmless "same value" one),
# which would end this script under set -e before it can read the reply; every call below
# inspects its own reply, so the exit status is deliberately ignored.
cfg()    { ots_cfg_udp_test "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }
cfg_ro() { ots_cfg_udp_test -y "$CFG_IP" "$CFG_PORT" "$@" 2>&1 || true; }

urlencode() {
    python3 -c "import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=''))" "$1"
}
xml_error() {
    local m
    m=$(echo "$1" | grep -oP "Error value='\K[^']+" | sed 's/&amp;lt;&amp;gt;//g;s/&apos;/'"'"'/g;s/&quot;/"/g' | grep -o '[^|]*$' | head -1 || true)
    echo "${m:-$(echo "$1" | tail -3 | cut -c1-200)}"
}

# edit_cell <column> <value> <editNodeType>  -- chains on TEMP_VERSION (each reply returns the next one)
edit_cell() {
    local col="$1" val="$2" type="$3" r v
    r=$(cfg saveTreeNodeEdit targetTable=DesktopIconTable targetTableVersion="$TEMP_VERSION" \
        targetUID="$ICON_UID" targetColumn="$col" newValue="$val" editNodeType="$type")
    v=$(echo "$r" | grep -oP "savedVersion value='\K[^']+" || true)
    if [ -n "$v" ]; then TEMP_VERSION="$v"; return 0; fi
    if echo "$r" | grep -q "is the same as the current value"; then return 0; fi  # already that value
    echo "ERROR editing $col: $(xml_error "$r")"; exit 1
}

echo "=== Add Desktop Icon ==="
echo "Caption: $CAPTION  Alt: $ALT  URL: $URL  Image: $IMAGE  Permission: $PERM  Unique: $UNIQUE  Folder: ${FOLDER:-<none>}  Linked: $LINKED"
echo "ConfigGUI UDP: $CFG_IP:$CFG_PORT"

ACTIVE=$(cfg_ro getActiveTableGroups)
GROUP_NAME=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupName value='\K[^']+" || true)
GROUP_KEY=$(echo "$ACTIVE" | grep -oP "Context-ActiveGroupKey value='\K[^']+" || true)
if [ -z "$GROUP_NAME" ] || [ "$GROUP_KEY" = "-1" ]; then echo "ERROR: no active Context group (is ots running with the UDP env vars?)."; exit 1; fi
echo "Active Context group: $GROUP_NAME key $GROUP_KEY"

MEMBERS=$(cfg_ro getSpecificTableGroup groupName="$GROUP_NAME" groupKey="$GROUP_KEY")
member_version() { echo "$MEMBERS" | grep -A1 "MemberName value='$1'" | grep -oP "MemberVersion value='\K[^']+" | head -1 || true; }
ICON_VERSION=$(member_version DesktopIconTable)
APP_VERSION=$(member_version XDAQApplicationTable)
[ -n "$ICON_VERSION" ] || { echo "ERROR: DesktopIconTable not in $GROUP_NAME."; exit 1; }
echo "DesktopIconTable v$ICON_VERSION, XDAQApplicationTable v$APP_VERSION"

# icon record UID, like the GUI: generatedIcon + letters/digits of the caption
ICON_UID="generatedIcon$(echo "$CAPTION" | tr -cd 'A-Za-z0-9')"
EXISTING=$(cfg_ro getSpecificTable tableName=DesktopIconTable version="$ICON_VERSION" | python3 -c "
import sys,re,urllib.parse as u
x=sys.stdin.read()
h=re.findall(r\"<ColumnHeader value='([^']*)'\",x); c=[u.unquote(v).split(',') for v in re.findall(r\"<ColCSV value='([^']*)'\",x)]
print(' '.join(dict(zip(h,c)).get('IconName',[])))")
case " $EXISTING " in *" $ICON_UID "*) echo "ERROR: an icon '$ICON_UID' already exists in $GROUP_NAME key $GROUP_KEY."; exit 1;; esac

# resolve the linked app to its UID (numeric = LID)
LINK_UID=""
if [ "$LINKED" != "NONE" ]; then
    if [[ "$LINKED" =~ ^[0-9]+$ ]]; then
        LINK_UID=$(cfg_ro getSpecificTable tableName=XDAQApplicationTable version="$APP_VERSION" | python3 -c "
import sys,re,urllib.parse as u
x=sys.stdin.read()
h=re.findall(r\"<ColumnHeader value='([^']*)'\",x); c=[u.unquote(v).split(',') for v in re.findall(r\"<ColCSV value='([^']*)'\",x)]
d=dict(zip(h,c))
for i,n in enumerate(d.get('Id',[])):
    if n==sys.argv[1]: print(d['ApplicationUID'][i]); break" "$LINKED")
        [ -n "$LINK_UID" ] || { echo "ERROR: no application with LID $LINKED in XDAQApplicationTable v$APP_VERSION."; exit 1; }
    else
        LINK_UID="$LINKED"
    fi
    echo "Linked application UID: $LINK_UID"
fi

echo
echo "Step 1: add record $ICON_UID and set its fields"
TEMP_VERSION="$ICON_VERSION"
r=$(cfg saveTreeNodeEdit targetTable=DesktopIconTable targetTableVersion="$ICON_VERSION" \
    targetUID="$ICON_UID" targetColumn=IconName newValue="$ICON_UID" editNodeType=table-newRow)
TEMP_VERSION=$(echo "$r" | grep -oP "savedVersion value='\K[^']+" || true)
[ -n "$TEMP_VERSION" ] || { echo "ERROR adding the record: $(xml_error "$r")"; exit 1; }

edit_cell Caption              "$(urlencode "$CAPTION")" value
edit_cell AlternateText        "$(urlencode "$ALT")"     value
edit_cell ForceOnlyOneInstance "$UNIQUE"                 value
edit_cell RequiredPermissionLevel "$PERM"                value
edit_cell ImageURL             "$(urlencode "$IMAGE")"   value
edit_cell WindowContentURL     "$(urlencode "$URL")"     value
[ -n "$FOLDER" ] && edit_cell FolderPath "$(urlencode "$FOLDER")" value
[ -n "$LINK_UID" ] && edit_cell LinkToApplicationTable "XDAQApplicationTable,$(urlencode "$LINK_UID")" link-UID
echo "Temporary version: $TEMP_VERSION"

echo
echo "New row:"
cfg_ro getSpecificTable tableName=DesktopIconTable version="$TEMP_VERSION" | python3 -c "
import sys,re,urllib.parse as u
x=sys.stdin.read()
h=re.findall(r\"<ColumnHeader value='([^']*)'\",x); c=[u.unquote(v).split(',') for v in re.findall(r\"<ColCSV value='([^']*)'\",x)]
for a,b in zip(h,c):
    if a not in ('Author','RecordInsertionTime','CommentDescription'): print('  %-26s %s' % (a, b[-1][:80]))"

if [ "$DRY" = 1 ]; then
    echo
    echo "Dry run: nothing saved (the temporary version exists only in this UDP user's session)."
    exit 0
fi

echo
echo "Step 2: persist the table"
ENC_COMMENT=$(urlencode "Add $CAPTION icon")
SAVE=$(cfg saveSpecificTable tableName=DesktopIconTable version="$TEMP_VERSION" temporary=0 \
    dataOffset=0 chunkSize=0 sourceTableAsIs=1 lookForEquivalent=1 tableComment="$ENC_COMMENT")
NEW_VERSION=$(echo "$SAVE" | grep -oP "<savedVersion value='\K[^']+" || true)
[ -n "$NEW_VERSION" ] || { echo "ERROR in saveSpecificTable: $(xml_error "$SAVE")"; exit 1; }
echo "DesktopIconTable v$NEW_VERSION"

echo
echo "Step 3: save a new $GROUP_NAME group"
TABLE_LIST=$(echo "$MEMBERS" | grep -A1 MemberName | grep -oP "(MemberName|MemberVersion) value='\K[^']+" | paste -d, - - | tr '\n' ',' \
    | sed "s/DesktopIconTable,[0-9]*,/DesktopIconTable,$NEW_VERSION,/")
GROUP=$(cfg saveNewTableGroup groupName="$GROUP_NAME" groupComment="$ENC_COMMENT" tableList="$TABLE_LIST")
NEW_KEY=$(echo "$GROUP" | grep -oP "<TableGroupKey value='\K[^']+" || true)
[ -n "$NEW_KEY" ] || { echo "ERROR in saveNewTableGroup: $(xml_error "$GROUP")"; exit 1; }
echo "New group: $GROUP_NAME key $NEW_KEY"

echo
echo "Step 4: activate $GROUP_NAME key $NEW_KEY"
ACT=$(cfg activateTableGroup groupName="$GROUP_NAME" groupKey="$NEW_KEY" ignoreWarnings=0)
echo "Active Context key: $(echo "$ACT" | grep -oP "Context-ActiveGroupKey value='\K[^']+" || true)"

echo
echo "=== Done: '$CAPTION' icon added in $GROUP_NAME key $NEW_KEY ==="
echo "Restart ots for the Desktop to show it."
