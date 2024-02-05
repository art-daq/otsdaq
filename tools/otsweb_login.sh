
echo "This is broken - never worked.. too complictated to navigate login procedure"
return
exit
####################################### start otsqeb login code
####################################### start otsqeb login code
####################################### start otsqeb login code
####################################### start otsqeb login code
#
# urlencode -- encode special characters for post/get arguments
#
urlencode() {
   perl -pe 'chomp(); s{\W}{sprintf("%%%02x",ord($&))}ge;' "$@"
}

if [ "x$SKIP_OTSWEB_LOGIN" != "x1" ]; then
	echo -e "otsqeb_login.sh [${LINENO}]  \t Setting up login."
	export OTSWEB_LOGIN_WORKED=0
	export OTSWEB_LOGIN_SITE="https://otsdaq.fnal.gov/mellon/login?ReturnTo=https%3A%2F%2Fotsdaq.fnal.gov%2F&IdP=https%3A%2F%2Fidp.fnal.gov%2Fidp%2Fshibboleth"
	export OTSWEB_LOGIN_LISTF=/tmp/otsweb_list_p$$
	export OTSWEB_LOGIN_COOKIEF=/tmp/otsweb_cookies_p$$
	export OTSWEB_LOGIN_RLVERBOSEF=${OTSWEB_LOGIN_RLVERBOSEF:=true}
	# trap 'rm -f /tmp/postdata$$ /tmp/at_p$$ $OTSWEB_LOGIN_COOKIEF $OTSWEB_LOGIN_LISTF*' EXIT

	echo -e "otsqeb_login.sh [${LINENO}]  \t OTSWEB_LOGIN_SITE=$OTSWEB_LOGIN_SITE"
fi

#
# login form
#
do_login() {
	get_passwords
	echo -e "otsqeb_login.sh [${LINENO}]  \t OTSWEB_LOGIN_SITE=$OTSWEB_LOGIN_SITE"
	get_auth_token "${OTSWEB_LOGIN_SITE}"
	echo -e "otsqeb_login.sh [${LINENO}]  \t OTSWEB_LOGIN_SITE=$OTSWEB_LOGIN_SITE"
	post_url  \
       "${OTSWEB_LOGIN_SITE}" \
       "back_url=$OTSWEB_LOGIN_SITE" \
       "authenticity_token=$authenticity_token" \
       "username=`echo $user | urlencode`" \
       "password=`echo $pass | urlencode`" \
       "login=Login ?" 

	if [ $OTSWEB_LOGIN_WORKED == 2 ]; then 
		echo
        echo -e "otsqeb_login.sh [${LINENO}]  \t Login failed."
		unset user #force new login attempt
		unset pass
		export OTSWEB_LOGIN_WORKED=0

		echo -e "otsqeb_login.sh [${LINENO}]  \t Check your Fermilab Services name and password!"
		exit 1 
        false
	fi

	if grep '>Sign in' $OTSWEB_LOGIN_LISTF > /dev/null;then
		echo
        echo -e "otsqeb_login.sh [${LINENO}]  \t Login failed."
		unset user #force new login attempt
		unset pass
		export OTSWEB_LOGIN_WORKED=0
        false
	else
		export OTSWEB_LOGIN_WORKED=1
        true
	fi
}
get_passwords() {
	
   	case "x${user-}y${pass-}" in
   	xy)
       	if [ -r   ${OTSWEB_AUTHDIR:-.}/.otsqeb_lib_passfile ];then 
	   		read -r user pass < ${OTSWEB_AUTHDIR:-.}/.otsqeb_lib_passfile
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
# fetch_url -- GET a url from a OTSWEB_LOGIN_SITE, maintaining cookies, etc.
#
fetch_url() {
     wget \
        --no-check-certificate \
	--load-cookies=${OTSWEB_LOGIN_COOKIEF} \
        --referer="${lastpage-}" \
	--save-cookies=${OTSWEB_LOGIN_COOKIEF} \
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
     df=/tmp/postdata$$
     :>$df
     for d in "$@";do
        printf "%s" "$sep$d" >> $df
        sep="&"
     done
     wget -O $OTSWEB_LOGIN_LISTF \
        -o $OTSWEB_LOGIN_LISTF.log \
        --debug \
        --verbose \
        $extra \
        --no-check-certificate \
	--load-cookies=${OTSWEB_LOGIN_COOKIEF} \
	--save-cookies=${OTSWEB_LOGIN_COOKIEF} \
        --referer="${lastpage-}" \
	--keep-session-cookies \
        --post-file="$df"  $url
     if grep '<div.*id=.errorExplanation' $OTSWEB_LOGIN_LISTF > /dev/null;then
        echo "Failed: error was:"
        cat $OTSWEB_LOGIN_LISTF | sed -e '1,/<div.*id=.errorExplanation/d' | sed -e '/<.div>/,$d'
        return 1
     fi
     if grep '<div.*id=.flash_notice.*Success' $OTSWEB_LOGIN_LISTF > /dev/null;then
        $OTSWEB_LOGIN_RLVERBOSEF && echo "Succeeded"
        return 0
     fi
     # not sure if it worked... 
     $OTSWEB_LOGIN_RLVERBOSEF && echo "Unknown -- detagged output:"
     $OTSWEB_LOGIN_RLVERBOSEF && cat $OTSWEB_LOGIN_LISTF | sed -e 's/<[^>]*>//g'
     $OTSWEB_LOGIN_RLVERBOSEF && echo "-----"
     $OTSWEB_LOGIN_RLVERBOSEF && cat $OTSWEB_LOGIN_LISTF.log
     $OTSWEB_LOGIN_RLVERBOSEF && echo "-----"
	 # if this post was a login attempt, then mark login worked as failure = 2
	 if [ $OTSWEB_LOGIN_WORKED == 0 ]; then 
	 	echo -e "otsqeb_login.sh [${LINENO}]  \t Login attempt web site was invalid"
		export OTSWEB_LOGIN_WORKED=2
	 fi
     return 1
} # post_url

echo
echo -e "otsqeb_login.sh [${LINENO}]  \t Attempting login... $SKIP_OTSWEB_LOGIN"

if [ "x$SKIP_OTSWEB_LOGIN" != "x1" ]; then
	do_login $OTSWEB_LOGIN_SITE
	export SKIP_OTSWEB_LOGIN=1 
fi 

echo "OTSWEB_LOGIN_COOKIEF=${OTSWEB_LOGIN_COOKIEF}"
echo "OTSWEB_LOGIN_WORKED=${OTSWEB_LOGIN_WORKED}"

if [ $OTSWEB_LOGIN_WORKED == 0 ]; then
	echo -e "otsqeb_login.sh [${LINENO}]  \t Check your Fermilab Services name and password!"
	exit 1 
fi

echo -e "otsqeb_login.sh [${LINENO}]  \t Login successful."

		
####################################### end otsqeb login code
####################################### end otsqeb login code
####################################### end otsqeb login code
####################################### end otsqeb login code