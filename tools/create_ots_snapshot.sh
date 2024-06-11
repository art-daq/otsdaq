#!/bin/sh
#
# create_ots_snapshot.sh 
#	Creates snapshot of Data and databases by making zip files
#	After this script, others users can pull the snapshot to clone a 'golden' setup
# 	or experts can pull the sanpshot try to reproduce problems.
#
#
# NOTE!!! <snapshot name> must be unique, or snapshot will be overwritten with no warning!
#
# usage: --name <snapshot name> --data <path of user data to clone> --database <path of database to clone>
# 
#   snapshot 
#		e.g. a, b, or c
#
#	data 
#		is the full path to the $USER_DATA (NoGitData) folder for the snapshot
#	database
#		is the full path to the databases folder for the snapshot
#
#  example run: (if not compiled, use ./create_ots_snapshot.sh)
#	create_ots_snapshot.sh --name a --data /home/rrivera/ots/NoGitData --database /home/rrivera/databases
#

echo
echo
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Do not source this script, run it as create_ots_snapshot.sh"
return  >/dev/null 2>&1 #return is used if script is sourced
		
echo
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Extracting parameters..."
echo

SRC=${PWD}
UDATA=${USER_DATA}
UDATABASES=`echo ${ARTDAQ_DATABASE_URI}|sed 's|.*//|/|'`
	
env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
   usage: `basename $0` [options]
examples: `basename $0` --name demo --data Data --database databases
If the \"data\" or \"database\" optional parameters are not supplied, the user will be
prompted for them.
-h            This message
-d,--data        Data directory to snapshot
-b,--database    Database directory to snapshot
-n,--name        Name of the snapshot
"

# Process script arguments and options
eval env_opts=\${$env_opts_var-} # can be args too

eval "set -- $env_opts \"\$@\""
op1chr='rest=`expr "$op" : "[^-]\(.*\)"`   && set -- "-$rest" "$@"'
op1arg='rest=`expr "$op" : "[^-]\(.*\)"`   && set --  "$rest" "$@"'
reqarg="$op1arg;"'test -z "${1+1}" &&echo opt -$op requires arg. &&echo "$USAGE" &&exit'
while [ -n "${1-}" ];do
    if expr "x${1-}" : 'x-' >/dev/null;then
        op=`expr "x$1" : 'x-\(.*\)'`; shift   # done with $1
        leq=`expr "x$op" : 'x-[^=]*\(=\)'` lev=`expr "x$op" : 'x-[^=]*=\(.*\)'`
        test -n "$leq"&&eval "set -- \"\$lev\" \"\$@\""&&op=`expr "x$op" : 'x\([^=]*\)'`
        case "$op" in
            \?*|h*)     eval $op1chr; do_help=1;;
            d*)         eval $reqarg; UDATA=$1; shift;;
            b*)         eval $reqarg; UDATABASES=$1; shift;;
            n*)         eval $reqarg; SNAPSHOT=$1; shift;;
            -data)      eval $reqarg; UDATA=$1; shift;;
            -database)  eval $reqarg; UDATABASES=$1; shift;;
            -name)      eval $reqarg; SNAPSHOT=$1; shift;;
            *)          echo "Unknown option -$op"; do_help=1;;
        esac
    else
        aa=`echo "$1" | sed -e"s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa	

echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t SNAPSHOT \t= $SNAPSHOT"
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t DATA     \t= $UDATA"
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t DATABASE \t= $UDATABASES"

echo
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Creating UserData and UserDatabases zips..."
echo

echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t user data directory: ${UDATA}"
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t user databases directory: ${UDATABASES}"


if [ -f snapshot_${SNAPSHOT}_Data.zip ]; then
  echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Snapshot already exists, re-run with different name!"
  exit 2
fi
if [ -f snapshot_${SNAPSHOT}_database.zip ]; then
  echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Snapshot already exists, re-run with different name!"
  exit 2
fi

#copy folders to SRC location and cleanup
rm -rf ${SRC}/snapshot_${SNAPSHOT}_databases; #replace databases 
cp -r ${UDATABASES} ${SRC}/snapshot_${SNAPSHOT}_databases;
rm -rf ${SRC}/snapshot_${SNAPSHOT}_databases/filesystemdb/test_db.*; 
rm -rf ${SRC}/snapshot_${SNAPSHOT}_databases/filesystemdb/test_db_*;
rm -rf ${SRC}/snapshot_${SNAPSHOT}_databases/filesystemdb/test_dbb*; #remove backups or bkups

rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data; #replace data
cp -r ${UDATA} ${SRC}/snapshot_${SNAPSHOT}_Data; 
rm ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/ActiveConfigurationGroups.cf*;
rm ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/ActiveTableGroups.cfg.*; 
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/ConfigurationInfo.*
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/OutputData/*   #*/ fix comment text coloring
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/Logs/*   #*/ fix comment text coloring
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/RunNumber/*  #*/ fix comment text coloring
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/MacroHistory/* #*/ fix comment text coloring
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/ProgressBarData 
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/RunControlData
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data/ServiceData/LoginData/UsersData/TooltipData


#make demo zips from repo
rm snapshot_${SNAPSHOT}_Data.zip
mv NoGitData NoGitData.mv.bk.snapshot; #for safety attempt to move and then restore temporary folder
cp -r ${SRC}/snapshot_${SNAPSHOT}_Data NoGitData;
zip -r snapshot_${SNAPSHOT}_Data.zip NoGitData; 
rm -rf NoGitData; 
mv NoGitData.mv.bk.snapshot NoGitData; 

rm snapshot_${SNAPSHOT}_database.zip
mv databases databases.mv.bk.snapshot; #for safety attempt to move and then restore temporary folder
cp -r ${SRC}/snapshot_${SNAPSHOT}_databases databases; 
zip -r snapshot_${SNAPSHOT}_database.zip databases; 
rm -rf databases; 
mv databases.mv.bk.snapshot databases;

#remove clean copies
rm -rf ${SRC}/snapshot_${SNAPSHOT}_Data
rm -rf ${SRC}/snapshot_${SNAPSHOT}_databases




echo
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Done creating UserData and UserDatabases zips."
echo

echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t SNAPSHOT \t= $SNAPSHOT"
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t DATA     \t= $UDATA"
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t DATABASE \t= $UDATABASES"

echo
echo -e `date +"%h%y %T"` "create_ots_snapshot.sh [${LINENO}]  \t Done handling snapshot UserData and UserDatabases!"
echo



	