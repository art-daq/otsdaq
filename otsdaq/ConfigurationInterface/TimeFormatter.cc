#include "otsdaq/ConfigurationInterface/TimeFormatter.h"

#include <sys/time.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#define USE_TIMER 0

using namespace ots;

//==============================================================================
TimeFormatter::TimeFormatter(std::string source)
{
	if(!USE_TIMER)
		return;
	origin_ = source;
	std::cout << __COUT_HDR_FL__ << "[TimeFormatter::TimeFormatter()]\t\t    Time counter started for " << origin_ << std::endl << std::endl;
	startTime_ = getImSecTime();
}

//==============================================================================
void TimeFormatter::stopTimer(void)
{
	if(!USE_TIMER)
		return;
	endTime_     = getImSecTime();
	double start = startTime_.tv_sec + startTime_.tv_usec / 1000000.;
	double stop  = endTime_.tv_sec + endTime_.tv_usec / 1000000.;
	std::cout << __COUT_HDR_FL__ << "[TimeFormatter::stopTimer()]\t\t\t    Elapsed time: " << stop - start << " seconds for " << origin_ << std::endl
	          << std::endl;
}

//==============================================================================
std::string TimeFormatter::getTime(void)
{
	char        theDate[72];
	struct tm*  thisTime;
	time_t      aclock;
	std::string date;
	time(&aclock);
	thisTime = localtime(&aclock);

	sprintf(theDate,
	        "%d-%02d-%02d %02d:%02d:%02d",
	        thisTime->tm_year + 1900,
	        thisTime->tm_mon + 1,
	        thisTime->tm_mday,
	        thisTime->tm_hour,
	        thisTime->tm_min,
	        thisTime->tm_sec);
	date = theDate;
	// std::cout << __COUT_HDR_FL__ << "[TimeFormatter::getTime()]\t\t\t\t    Time: " <<
	// date << std::endl  << std::endl;
	return date;
}

//==============================================================================
struct tm* TimeFormatter::getITime(void)
{
	struct tm* thisTime;
	time_t     aclock;
	time(&aclock);
	thisTime = localtime(&aclock);
	return thisTime;
}

//==============================================================================
std::string getmSecTime(void)
{
	char           theDate[20];
	struct timeval msecTime;
	gettimeofday(&msecTime, (struct timezone*)0);

	sprintf(theDate, "%d-%d", (unsigned int)msecTime.tv_sec, (unsigned int)msecTime.tv_usec);
	return std::string(theDate);
}

//==============================================================================
struct timeval TimeFormatter::getImSecTime(void)
{
	struct timeval msecTime;
	gettimeofday(&msecTime, (struct timezone*)0);

	return msecTime;
}
