#!/usr/bin/sh
# This script returns user's name in all caps from the phone book.
# It can be called remotely like so:
#	ssh <host> bash tele_lookup.sh <email>

NO_TELE=0
#TODO setup product deps source <setup> || NO_TELE=1
if [ $NO_TELE == 0 ]; then 
	setup telephone >/dev/null 2>&1 || NO_TELE=1
fi

if [ $NO_TELE == 1 ]; then 
	AUTHOR_NAME="UNKNOWN"
else
        EMAIL_ADDR=$(echo $1 | tr '[:upper:]' '[:lower:]')
        #get from FNAL telephone book up to first double space
        AUTHOR_NAME=`tele -match=email ${EMAIL_ADDR} | sed 's/  /;/g' | cut -d ';' -f1 | xargs`        #use xargs to trim whitespace (when error response)
fi


# echo -e "tele_lookup.sh [${LINENO}]  \t      Found name as ${AUTHOR_NAME}"
if [[ "x${AUTHOR_NAME:0:16}" == "xNo Results Found" ]]; then
        AUTHOR_NAME=`echo $EMAIL_ADDR | cut -d '@' -f1`
        AUTHOR_NAME=`wget -O- http://www-giduid.fnal.gov/cd/FUE/uidgid/uid_id.lis 2>/dev/null | grep -i $AUTHOR_NAME | xargs`   #use xargs to trim whitespace
        FIRST_NAME=`echo ${AUTHOR_NAME} | cut -d ' ' -f4`
        LAST_NAME=`echo ${AUTHOR_NAME} | cut -d ' ' -f3`
        AUTHOR_NAME="$FIRST_NAME $LAST_NAME"
        # echo -e "tele_lookup.sh [${LINENO}]  \t      Found FIRST_NAME as ${FIRST_NAME}"
        # echo -e "tele_lookup.sh [${LINENO}]  \t      Found LAST_NAME as ${LAST_NAME}"
        # echo -e "tele_lookup.sh [${LINENO}]  \t      Found name as ${AUTHOR_NAME}"
fi

echo ${AUTHOR_NAME}