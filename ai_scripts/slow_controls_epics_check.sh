#!/bin/bash
# Read-only check of the EPICS side of otsdaq slow controls (Mu2e OTSReader IOC files).
# See ../ai_skills/slow-controls-to-epics.md.
#
# Shows, for one owner (MU2E_OWNER, lower case, e.g. calo, hwdev):
#   - the generated PV file $OTSDAQ_EPICS_DATA/<owner>_otsdaq_dtc-ai.dbg: time, number of records
#   - the dirty flag (1 = changed, the DCS cron restarts the IOC within about 2 minutes, then 0)
#   - whether the IOC start file (../ioc/OTSData.iocsh) loads that file
#   - the records whose line matches an optional text (e.g. a channel or ROC name)
# Reads files only: no EPICS channel access, no database queries.
#
# Usage:
#   slow_controls_epics_check.sh <owner> [match text]
#   e.g. slow_controls_epics_check.sh hwdev tutorialTemperature

owner=$1
matchText=$2
if [ -z "$owner" ]; then
	echo "Usage: $0 <owner, e.g. calo or hwdev> [match text]"
	exit 1
fi

epicsDataPath=${OTSDAQ_EPICS_DATA:-/home/mu2edcs/mu2e-dcs/apps/OTSReader/db/}
pvFile="${epicsDataPath%/}/${owner}_otsdaq_dtc-ai.dbg"
dirtyFlagFile="${epicsDataPath%/}/dirtyFlag.txt"
iocStartFile="${epicsDataPath%/}/../ioc/OTSData.iocsh"

echo "PV file:     $pvFile"
if [ -f "$pvFile" ]; then
	# a record is one data line: { "<Subsystem>", "<loc>", "<pvar>", ... }
	echo "             last written $(date -r "$pvFile" '+%F %T'), $(grep -c '^[[:space:]]*{ "' "$pvFile") records"
else
	echo "             MISSING: Configure with the dashboard on has not written it (check MU2E_OWNER and OTSDAQ_EPICS_DATA)"
fi

if [ -f "$dirtyFlagFile" ]; then
	echo "Dirty flag:  $(cat "$dirtyFlagFile") ($dirtyFlagFile, written $(date -r "$dirtyFlagFile" '+%F %T'))"
else
	echo "Dirty flag:  none ($dirtyFlagFile absent: no pending change; Configure writes 1 when the PV file changes)"
fi

if [ -r "$iocStartFile" ]; then
	loadLine=$(grep -n "dbLoadTemplate(\"${owner}_otsdaq_dtc-ai.dbg\"" "$iocStartFile")
	if [ -n "$loadLine" ]; then
		echo "IOC loads:   yes, $iocStartFile:$loadLine"
	else
		echo "IOC loads:   NO, ${owner}_otsdaq_dtc-ai.dbg is not in $iocStartFile (file names are case sensitive)"
	fi
else
	echo "IOC loads:   cannot read $iocStartFile"
fi

if [ -n "$matchText" ] && [ -f "$pvFile" ]; then
	echo "Records matching '$matchText':"
	grep -n -- "$matchText" "$pvFile" | head -20
fi
