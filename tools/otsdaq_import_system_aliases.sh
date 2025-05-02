#!/bin/bash
# otsdaq/tools/otsdaq_import_system_aliases.sh

# usage:
# otsdaq_import_system_aliases.sh <path_to_import_database_folder> <path_to_import_user_data> <import_prepend_base_name (optional)>
#
# All group aliases and active groups are imported to current db and are prepended with a label if the optional arugment is provided.
# The steps of the import are as follows:
#	- delete directory content at tmp/export_system_aliases
#	- export (from import database URI and import USER_DATA args) group aliases and active groups to text/json version and store at tmp/export_system_aliases
#	- import (to current database URI and USER_DATA) all group directories found at tmp/export_system_aliases
#
# Developer note: must use terminal shell wrapper, because artdaq database URI is controlled by environment variable before running C++ main()


echo
echo "  |"
echo "  |"
echo "  |"
echo " _|_"
echo " \ /"
echo "  V "
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t ========================================================"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t usage: ./otsdaq_import_system_aliases.sh <import database URI> <import USER_DATA path> <import prepend base name (optional)>"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t for example..."
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t\t ./otsdaq_import_system_aliases.sh mongodb://user:pass@localhost:port/teststand_db?authSource=user /home/mu2eshift/ots_v4/Data_shift shift_v4"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t          or..."
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t\t ./otsdaq_import_system_aliases.sh filesystemdb:///home/mu2ehwdev/ots_dev/databases_HWDev/filesystemdb/test_db /home/mu2eshift/ots_v4/Data_shift shift_v4"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t All system aliases (specified by Backbone in the active groups file) are imported to current db, and imported Group Names and System Aliases are prepended with the optional label." 
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t To export only to tmp/ and not import..."
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t\t\t ./otsdaq_import_system_aliases.sh EXPORT_ONLY <import database URI> <import USER_DATA path>"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t"

#return  >/dev/null 2>&1 #return is used if script is sourced


echo
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Extracting parameters..."
echo

EXPORT_ONLY=0

if [[ "x$1" == "xEXPORT_ONLY" ]]; then
	EXPORT_ONLY=1
	echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Exporting only to tmp/"
	shift
fi

if [[ "x$1" == "x" || "x$2" == "x" ]]; then

	echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Illegal parameters.. See above for usage."
	return  >/dev/null 2>&1 #return is used if script is sourced
	exit  #exit is used if script is run ./reset...
fi

CURRENT_URI=$ARTDAQ_DATABASE_URI
CURRENT_USER_DATA=$USER_DATA
IMPORT_URI=$1
IMPORT_USER_DATA_PATH=$2
IMPORT_PREPEND=$3

echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t CURRENT_URI=$CURRENT_URI"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t CURRENT_USER_DATA=$CURRENT_USER_DATA"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t IMPORT_URI=$IMPORT_URI"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t IMPORT_USER_DATA_PATH=$IMPORT_USER_DATA_PATH"
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t IMPORT_PREPEND=$IMPORT_PREPEND"

#####################
#setup databases and USER_DATA for export of the import location to tmp/export_system_aliases
ARTDAQ_DATABASE_URI=$IMPORT_URI
USER_DATA=$IMPORT_USER_DATA_PATH
rm -rf tmp/export_system_aliases
mkdir tmp/export_system_aliases
otsdaq_export_system_aliases tmp/export_system_aliases 

if [[ $EXPORT_ONLY = 0 ]]; then
	#####################
	#setup databases and USER_DATA for import into current location from tmp/export_system_aliases
	#to debug: ARTDAQ_DATABASE_URI=filesystemdb:///home/mu2eshift/ots_ops/databases_HWDev/filesystemdb/test2_db #$CURRENT_URI
	#to debug: USER_DATA=$IMPORT_USER_DATA_PATH #$CURRENT_USER_DATA
	ARTDAQ_DATABASE_URI=$CURRENT_URI
	USER_DATA=$CURRENT_USER_DATA
	otsdaq_import_groups_from_export_path tmp/export_system_aliases $IMPORT_PREPEND

	echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Completed Import from = $IMPORT_URI"
	echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Completed Import to = $CURRENT_URI"
else 
	echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Exporting only, skipping import"
	echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Completed Export from = $IMPORT_URI"
fi

echo
echo
echo -e `date +"%h%y %T"` "otsdaq_import_system_aliases.sh [${LINENO}]  \t Import of System Aliases end of script!"
