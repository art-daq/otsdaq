#!/bin/sh
# Source this to get color code variables to use during output

defineColors ()
{
  # Regular Colors
  Black=`printf '\033[0;30m'`        # Black
  Red=`printf '\033[0;31m'`          # Red
  Green=`printf '\033[0;32m'`        # Green
  Yellow=`printf '\033[0;32m'`       # \033[0;33m  Yellow -- too hard to see on white (so making green)
  Blue=`printf '\033[0;34m'`         # Blue
  Purple=`printf '\033[0;35m'`       # Purple
  Cyan=`printf '\033[0;36m'`         # Cyan
  White=`printf '\033[0;37m'`        # White

  # Bold
  BBlack=`printf '\033[1;30m'`       # Black
  BRed=`printf '\033[1;31m'`         # Red
  BGreen=`printf '\033[1;32m'`       # Green
  BYellow=`printf '\033[1;33m'`      # Yellow
  BBlue=`printf '\033[1;34m'`        # Blue
  BPurple=`printf '\033[1;35m'`      # Purple
  BCyan=`printf '\033[1;36m'`        # Cyan
  BWhite=`printf '\033[1;37m'`       # White

  # Underline
  UBlack=`printf '\033[4;30m'`       # Black
  URed=`printf '\033[4;31m'`         # Red
  UGreen=`printf '\033[4;32m'`       # Green
  UYellow=`printf '\033[4;33m'`      # Yellow
  UBlue=`printf '\033[4;34m'`        # Blue
  UPurple=`printf '\033[4;35m'`      # Purple
  UCyan=`printf '\033[4;36m'`        # Cyan
  UWhite=`printf '\033[4;37m'`       # White

  # Background
  On_Black=`printf '\033[40m'`       # Black
  On_Red=`printf '\033[41m'`         # Red
  On_Green=`printf '\033[42m'`       # Green
  On_Yellow=`printf '\033[43m'`      # Yellow
  On_Blue=`printf '\033[44m'`        # Blue
  On_Purple=`printf '\033[45m'`      # Purple
  On_Cyan=`printf '\033[46m'`        # Cyan
  On_White=`printf '\033[47m'`       # White

  # High Intensity
  IBlack=`printf '\033[0;90m'`       # Black
  IRed=`printf '\033[0;91m'`         # Red
  IGreen=`printf '\033[0;92m'`       # Green
  IYellow=`printf '\033[0;31m'`      #'\033[0;93m'      # Yellow  -- too hard to see on white (so making light red)
  IBlue=`printf '\033[0;94m'`        # Blue
  IPurple=`printf '\033[0;95m'`      # Purple
  ICyan=`printf '\033[0;96m'`        # Cyan
  IWhite=`printf '\033[0;97m'`       # White

  # Bold High Intensity
  BIBlack=`printf '\033[1;90m'`      # Black
  BIRed=`printf '\033[1;91m'`        # Red
  BIGreen=`printf '\033[1;92m'`      # Green
  BIYellow=`printf '\033[1;93m'`     # Yellow
  BIBlue=`printf '\033[1;94m'`       # Blue
  BIPurple=`printf '\033[1;95m'`     # Purple
  BICyan=`printf '\033[1;96m'`       # Cyan
  BIWhite=`printf '\033[1;97m'`      # White

  # High Intensity backgrounds
  On_IBlack=`printf '\033[0;100m'`   # Black
  On_IRed=`printf '\033[0;101m'`     # Red
  On_IGreen=`printf '\033[0;102m'`   # Green
  On_IYellow=`printf '\033[0;103m'`  # Yellow
  On_IBlue=`printf '\033[0;104m'`    # Blue
  On_IPurple=`printf '\033[0;105m'`  # Purple
  On_ICyan=`printf '\033[0;106m'`    # Cyan
  On_IWhite=`printf '\033[0;107m'`   # White

  RstClr=`printf '\033[0m'`         # Reset color
  Bold=`tput bold -T xterm`          # Select bold mode                  
  DIM=`tput dim -T xterm`            # Select dim (half-bright) mode     
  Blink=`tput blink -T xterm`        # Select dim (half-bright) mode 
  EUNDERLINE=`tput smul -T xterm`    # Enable underline mode             
  DUNDERLINE=`tput rmul -T xterm`    # Disable underline mode            
  REV=`tput rev -T xterm`            # Turn on reverse video mode        
#Reset=`tput init -T xterm`         # Reset all
  Reset=`tput init -T xterm 2>/dev/null;tput sgr0 -T xterm`         # Reset all
  EBold=`tput smso -T xterm`         # Enter standout (bold) mode        
  DBold=`tput rmso -T xterm`         # Exit standout mode              
}

defineColors

#SCRIPT_NAME=$1
out()     { part1="${RstClr}${IRed}${STARTTIME}${RstClr}-${Green}" part2="${IBlue}${THIS_HOST}${RstClr}" part3="|${IBlack}	${RstClr}";         do_out TLVL_LOG     "$*"; }
info()    { part1="${RstClr}${IRed}${STARTTIME}${RstClr}-${Green}" part2="${IBlue}${THIS_HOST}${RstClr}" part3="|${IBlack}	${RstClr}$IBlue";   do_out TLVL_INFO    "$*"; }
success() { part1="${RstClr}${IRed}${STARTTIME}${RstClr}-${Green}" part2="${IBlue}${THIS_HOST}${RstClr}" part3="|${IBlack}	${RstClr}$IGreen";  do_out TLVL_NOTICE  "$*"; }
warning() { part1="${RstClr}${IRed}${STARTTIME}${RstClr}-${Green}" part2="${IBlue}${THIS_HOST}${RstClr}" part3="|${IBlack}	${RstClr}$IYellow"; do_out TLVL_WARNING "$*" >&2; }
error()   { part1="${RstClr}${IRed}${STARTTIME}${RstClr}-${Green}" part2="${IBlue}${THIS_HOST}${RstClr}" part3="|${IBlack}	${RstClr}$IRed";    do_out TLVL_ERROR   "$*" >&2; }
die()     { part1="${RstClr}${IRed}${STARTTIME}${RstClr}-${Green}" part2="${IBlue}${THIS_HOST}${RstClr}" part3="|${IBlack}	${RstClr}$IRed";    do_out TLVL_FATAL   "$*"; exit 1; }

do_out() {
    tlvl=$1;shift
    if hash trace_cntl >/dev/null 2>&1;then
	TRACE_TIME_FMT=%d%h%y.%T TRACE_PRINT="${part1}%T $part2 %n:$Cyan${BASH_LINENO[1]}$RstClr $part3" \
	    trace_cntl -n`basename "${BASH_SOURCE[2]}"` -L${BASH_LINENO[1]} TRACE TLVL_LOG "$(echo -e "$*")${RstClr}"
    else
	echo -e "${part1}`date +%d%h%y.%T` $part2 `basename "${BASH_SOURCE[2]}"`:$Cyan${BASH_LINENO[0]}$RstClr ${part3}${*}${RstClr}"
    fi
}

