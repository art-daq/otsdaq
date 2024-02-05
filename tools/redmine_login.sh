
####################################### start redmine login code
####################################### start redmine login code
####################################### start redmine login code
####################################### start redmine login code
#
# urlencode -- encode special characters for post/get arguments
#
urlencode() {
   perl -pe 'chomp(); s{\W}{sprintf("%%%02x",ord($&))}ge;' "$@"
}

if [ "x$SKIP_REDMINE_LOGIN" != "x1" ]; then
	export REDMINE_LOGIN_WORKED=0
	export REDMINE_LOGIN_SITE=https://cdcvs.fnal.gov/redmine
	export REDMINE_LOGIN_LISTF=/tmp/redmine_list_p$$
	export REDMINE_LOGIN_COOKIEF=/tmp/redmine_cookies_p$$
	export REDMINE_LOGIN_RLVERBOSEF=${REDMINE_LOGIN_RLVERBOSEF:=false}
	trap 'echo -e "redmine_login.sh [${LINENO}]  \t Exit detected. Cleaning up..."; rm -f /tmp/postdata_p$$ /tmp/at_p$$ $REDMINE_LOGIN_COOKIEF $REDMINE_LOGIN_LISTF*; unset SKIP_REDMINE_LOGIN' EXIT
fi

#
# login form
#
do_login() {
	get_passwords
	get_auth_token "${REDMINE_LOGIN_SITE}/login"
	post_url  \
       "${REDMINE_LOGIN_SITE}/login" \
       "back_url=$REDMINE_LOGIN_SITE" \
       "authenticity_token=$authenticity_token" \
       "username=`echo $user | urlencode`" \
       "password=`echo $pass | urlencode`" \
       "login=Login ?" 
	if grep '>Sign in' $REDMINE_LOGIN_LISTF > /dev/null;then

		echo
        echo -e "redmine_login.sh [${LINENO}]  \t Login failed."
		unset user #force new login attempt
		unset pass
		export REDMINE_LOGIN_WORKED=0
        false
	else
		export REDMINE_LOGIN_WORKED=1
        true
	fi
}
get_passwords() {
	
   	case "x${user-}y${pass-}" in
   	xy)
       	if [ -r   ${REDMINE_AUTHDIR:-.}/.redmine_lib_passfile ];then 
	   		read -r user pass < ${REDMINE_AUTHDIR:-.}/.redmine_lib_passfile
       	else
	   
			printf "Enter your Services username: "
			read user

			#user=$USER
			stty -echo
			printf "Services password for $user: "
			read pass
			stty echo
       	fi;;
    esac
}
get_auth_token() {
    authenticity_token=`fetch_url "${1}" |
                  tee /tmp/at_p$$ |
                  grep 'name="authenticity_token"' |
                  head -1 |
                  sed -e 's/.*value="//' -e 's/".*//' | 
                  urlencode `
}
#
# fetch_url -- GET a url from a REDMINE_LOGIN_SITE, maintaining cookies, etc.
#
fetch_url() {
     wget \
        --no-check-certificate \
	--load-cookies=${REDMINE_LOGIN_COOKIEF} \
        --referer="${lastpage-}" \
	--save-cookies=${REDMINE_LOGIN_COOKIEF} \
	--keep-session-cookies \
	-o ${debugout:-/dev/null} \
	-O - \
	"$1"  | ${debugfilter:-cat}
     lastpage="$1"
}
#
# post_url POST to a url maintaining cookies, etc.
#    takes a url and multiple form data arguments
#    which are joined with "&" signs
#
post_url() {
     url="$1"
     extra=""
     if  [ "$url" == "-b" ];then
         extra="--remote-encoding application/octet-stream"
         shift
         url=$1
     fi
     shift
     the_data=""
     sep=""
     df=/tmp/postdata_p$$
     :>$df
     for d in "$@";do
        printf "%s" "$sep$d" >> $df
        sep="&"
     done
     wget -O $REDMINE_LOGIN_LISTF \
        -o $REDMINE_LOGIN_LISTF.log \
        --debug \
        --verbose \
        $extra \
        --no-check-certificate \
	--load-cookies=${REDMINE_LOGIN_COOKIEF} \
	--save-cookies=${REDMINE_LOGIN_COOKIEF} \
        --referer="${lastpage-}" \
	--keep-session-cookies \
        --post-file="$df"  $url
     if grep '<div.*id=.errorExplanation' $REDMINE_LOGIN_LISTF > /dev/null;then
        echo "Failed: error was:"
        cat $REDMINE_LOGIN_LISTF | sed -e '1,/<div.*id=.errorExplanation/d' | sed -e '/<.div>/,$d'
        return 1
     fi
     if grep '<div.*id=.flash_notice.*Success' $REDMINE_LOGIN_LISTF > /dev/null;then
        $REDMINE_LOGIN_RLVERBOSEF && echo "Succeeded"
        return 0
     fi
     # not sure if it worked... 
     $REDMINE_LOGIN_RLVERBOSEF && echo "Unknown -- detagged output:"
     $REDMINE_LOGIN_RLVERBOSEF && cat $REDMINE_LOGIN_LISTF | sed -e 's/<[^>]*>//g'
     $REDMINE_LOGIN_RLVERBOSEF && echo "-----"
     $REDMINE_LOGIN_RLVERBOSEF && cat $REDMINE_LOGIN_LISTF.log
     $REDMINE_LOGIN_RLVERBOSEF && echo "-----"
     return 0
} # post_url

echo
echo -e "redmine_login.sh [${LINENO}]  \t Attempting login... $SKIP_REDMINE_LOGIN"

if [ "x$SKIP_REDMINE_LOGIN" != "x1" ]; then
	do_login $REDMINE_LOGIN_SITE
	export SKIP_REDMINE_LOGIN=1 
fi 

if [ $REDMINE_LOGIN_WORKED == 0 ]; then
	echo
	echo -e "redmine_login.sh [${LINENO}]  \t !!!!!!!!!!"
	echo -e "redmine_login.sh [${LINENO}]  \t Check your Fermilab Services name and password!"
	echo -e "redmine_login.sh [${LINENO}]  \t !!!!!!!!!!"
	echo
	exit 1 
fi

echo
echo -e "redmine_login.sh [${LINENO}]  \t Login successful."
echo

		
####################################### end redmine login code
####################################### end redmine login code
####################################### end redmine login code
####################################### end redmine login code