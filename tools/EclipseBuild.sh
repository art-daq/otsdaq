#!/bin/bash
cd $OTSDAQ_BUILD/..
source setupARTDAQOTS
cd $OTSDAQ_BUILD
source $OTSDAQ_REPO/ups/setup_for_development -p
buildtool $@

