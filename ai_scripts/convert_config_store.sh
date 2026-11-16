#!/bin/bash
# Move otsdaq configurations between configuration store types:
#   filesystemdb://<path>   artdaq-database (Database plugin; ots lite and full)
#   textdb://<path>         plain text store (Text plugin; ots lite, full and maxlite)
# Recipes and background: ../ai_skills/config-store-conversion.md
#
# Prerequisites:
#   - source setup_ots.sh of an install that has the plugins for BOTH stores: ots lite or full
#     for filesystemdb <-> textdb; ots maxlite only for textdb -> textdb
#   - never point the destination at the store of a running ots
#
# Usage:
#   ./convert_config_store.sh [-n] all <fromURI> <toURI>
#       copy every table version and group key, keeping their numbers
#       (otsdaq_convert_config_db --verify); entries already in the destination are skipped
#
#   ./convert_config_store.sh [-n] active <fromURI> <fromUSER_DATA> <toURI> <toUSER_DATA> [--fresh] [--prepend <label>]
#       export the active groups and system aliases of <from> (otsdaq_export_system_aliases)
#       and import them into <to> (otsdaq_import_groups_from_export_path); <toUSER_DATA> gets
#       the new active groups. Without --fresh they are merged into <to>'s valid active groups;
#       with --fresh <toUSER_DATA>/ServiceData/ActiveTableGroups.cfg is moved aside first, so
#       the import starts from a new empty Backbone (use for an empty destination store)
#
#   -n   dry run: print what would be done (all: list the copies; active: export only)
#
# Examples:
#   ./convert_config_store.sh all filesystemdb://$HOME/ots/databases/filesystemdb/test_db \
#       textdb://$HOME/ots-maxlite/databases/textdb/test_db
#   ./convert_config_store.sh active textdb:///board/db /board/Data \
#       filesystemdb://$HOME/ots/databases/filesystemdb/test_db $HOME/ots/Data --prepend board

DRY=0
if [ "$1" = "-n" ]; then DRY=1; shift; fi
MODE="$1"; shift || true

usage() { sed -n 2,33p "$0" | sed 's/^# \{0,1\}//'; exit 1; }
plugin_for() { case "$1" in textdb://*) echo Text;; filesystemdb://*|mongodb://*) echo Database;; *) echo "";; esac; }
require_plugins() {
    local uri p
    for uri in "$@"; do
        p=$(plugin_for "$uri")
        [ -n "$p" ] || { echo "ERROR: unknown store type in '$uri' (expected textdb:// or filesystemdb://)."; exit 1; }
        if [ "$p" = Database ] && [ -n "$OTS_MAXLITE" ]; then
            echo "ERROR: '$uri' needs the artdaq-database plugin, which ots maxlite does not build."
            echo "       Run this from an ots lite (or full) install: source its setup_ots.sh first."
            exit 1
        fi
    done
}
[ -n "$USER_DATA" ] || { echo "ERROR: source setup_ots.sh first (USER_DATA is not set)."; exit 1; }

case "$MODE" in
all)
    [ $# -eq 2 ] || usage
    require_plugins "$1" "$2"
    command -v otsdaq_convert_config_db >/dev/null || { echo "ERROR: otsdaq_convert_config_db not found (otsdaq too old?)."; exit 1; }
    if [ $DRY = 1 ]; then
        otsdaq_convert_config_db "$1" "$2" --dry-run
    else
        otsdaq_convert_config_db "$1" "$2" --verify
    fi
    exit $?
    ;;
active)
    [ $# -ge 4 ] || usage
    FROM_URI="$1"; FROM_UD="$2"; TO_URI="$3"; TO_UD="$4"; shift 4
    FRESH=0; PREPEND=""
    while [ $# -gt 0 ]; do
        case "$1" in --fresh) FRESH=1;; --prepend) PREPEND="$2"; shift;; *) usage;; esac; shift
    done
    require_plugins "$FROM_URI" "$TO_URI"
    [ -d "$FROM_UD/ServiceData" ] || { echo "ERROR: $FROM_UD is not a USER_DATA area (no ServiceData)."; exit 1; }
    [ -d "$TO_UD/ServiceData" ]   || { echo "ERROR: $TO_UD is not a USER_DATA area (no ServiceData)."; exit 1; }

    WORK=$(mktemp -d /tmp/convert_config_store.XXXXXX)
    echo "Exporting active groups and aliases of $FROM_URI ($FROM_UD) to $WORK/export ..."
    ( export ARTDAQ_DATABASE_URI="$FROM_URI" USER_DATA="$FROM_UD"; cd "$WORK" && \
      otsdaq_export_system_aliases "$WORK/export" > "$WORK/export.log" 2>&1 ) \
      || { echo "ERROR: export failed, see $WORK/export.log"; exit 1; }
    grep -a "NO ERRORS\|Errors\|ERROR" "$WORK/export.log" | tail -2
    echo "Exported groups: $(ls "$WORK/export" | tr '\n' ' ')"
    if [ $DRY = 1 ]; then echo "Dry run: nothing imported (export kept in $WORK/export)."; exit 0; fi

    if [ $FRESH = 1 ] && [ -e "$TO_UD/ServiceData/ActiveTableGroups.cfg" ]; then
        BK="$TO_UD/ServiceData/ActiveTableGroups.cfg.bak-$(date +%Y%m%d-%H%M%S)"
        mv "$TO_UD/ServiceData/ActiveTableGroups.cfg" "$BK"
        echo "--fresh: moved the destination's active groups file to $BK"
    fi
    echo "Importing into $TO_URI ($TO_UD)${PREPEND:+ with prepend '$PREPEND'} ..."
    ( export ARTDAQ_DATABASE_URI="$TO_URI" USER_DATA="$TO_UD"; cd "$WORK" && \
      otsdaq_import_groups_from_export_path "$WORK/export" $PREPEND > "$WORK/import.log" 2>&1 )
    rc=$?
    grep -aE "Did not find valid active groups|End of Importing" "$WORK/import.log" | cut -c1-200 | tail -2
    [ $rc -eq 0 ] || { echo "ERROR: import failed (exit $rc), see $WORK/import.log"; exit 1; }
    echo "Active groups now in $TO_UD/ServiceData/ActiveTableGroups.cfg:"
    paste - - < "$TO_UD/ServiceData/ActiveTableGroups.cfg" | sed 's/^/  /'
    echo "Logs: $WORK/{export,import}.log"
    ;;
*) usage;;
esac
