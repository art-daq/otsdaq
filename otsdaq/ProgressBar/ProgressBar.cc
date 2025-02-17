#include "otsdaq/ProgressBar/ProgressBar.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <dirent.h>  //for DIR
#include <sys/stat.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace ots;

//==============================================================================
ProgressBar::ProgressBar()
    : cProgressBarFilePath_(std::string(__ENV__("SERVICE_DATA_PATH")) +
                            "/ProgressBarData/")
    , cProgressBarFileExtension_(".txt")
    , totalStepsFileName_("")
    , stepCount_(0)
    , stepsToComplete_(0)
    , started_(false)
{
	std::string path = cProgressBarFilePath_;
	DIR*        dir  = opendir(path.c_str());
	if(dir)
		closedir(dir);
	else if(-1 == mkdir(path.c_str(), 0755))
	{
		// lets create the service folder (for first time)
		__SS__ << "Service directory creation failed: " << path << std::endl;
		__SS_THROW__;
	}
}  //end constructor()

//==============================================================================
///		reset() ~~
///		Resets progress bar to 0% complete
void ProgressBar::reset(std::string file, std::string lineNumber, int id)
{
	std::lock_guard<std::mutex> lock(theMutex_);  // lock out for remainder of scope

	stepCount_       = 0;
	stepsToComplete_ = 0;

	// try to load stepsToComplete based on file, lineNumber and id
	char fn[1000];
	sprintf(fn, "%s_%s_%d", file.c_str(), lineNumber.c_str(), id);

	for(unsigned int c = 0; c < strlen(fn); ++c)
		if(!((fn[c] >= '0' && fn[c] <= '9') || (fn[c] >= 'a' && fn[c] <= 'z') ||
		     (fn[c] >= 'A' && fn[c] <= 'Z')))
			fn[c] = '_';
	totalStepsFileName_ = cProgressBarFilePath_ + fn + cProgressBarFileExtension_;
	__COUTVS__(10, totalStepsFileName_);

	FILE* fp = fopen(totalStepsFileName_.c_str(), "r");
	if(fp)
	{
		fscanf(fp, "%d", &stepsToComplete_);
		fclose(fp);
		__COUT_TYPE__(TLVL_DEBUG + 10)
		    << __COUT_HDR__ << "File Found - stepsToComplete = " << stepsToComplete_
		    << std::endl;
	}
	else
		__COUTT__ << "File Not there: " << totalStepsFileName_ << __E__;

	started_ = true;
}  //end reset()

//==============================================================================
void ProgressBar::step()
{
	std::lock_guard<std::mutex> lock(theMutex_);  // lock out for remainder of scope
	++stepCount_;

	// do not allow to get to 100% until complete (in case stepsToComplete is not constant for all time)
	if(stepsToComplete_ && stepCount_ >= stepsToComplete_)
		stepsToComplete_ = stepCount_ + 1;

	__COUT_TYPE__(TLVL_DEBUG + 10) << __COUT_HDR__ << totalStepsFileName_ << " "
	                               << readPercentageString() << "% complete" << std::endl;
}  //end step()

//==============================================================================
bool ProgressBar::isComplete()
{
	std::lock_guard<std::mutex> lock(theMutex_);  // lock out for remainder of scope
	return !started_;
}  //end isComplete()

//==============================================================================
void ProgressBar::complete()
{
	step();  // consider complete as a step

	std::lock_guard<std::mutex> lock(theMutex_);  // lock out for remainder of scope

	stepsToComplete_ = stepCount_;
	started_         = false;

	// done, save steps to file

	__COUT_TYPE__(TLVL_DEBUG + 10) << __COUT_HDR__ << totalStepsFileName_ << std::endl;

	FILE* fp = fopen(totalStepsFileName_.c_str(), "w");
	if(fp)
	{
		fprintf(fp, "%d", stepsToComplete_);
		fclose(fp);
	}
	else
		__COUT_ERR__ << "Critical ERROR!" << std::endl;
}  //end complete()

//==============================================================================
/// return percentage complete as integer
int ProgressBar::read()
{
	std::lock_guard<std::mutex> lock(theMutex_);  // lock out for remainder of scope

	if(!started_)
		return 100;  // if no progress started, always return 100% complete

	if(stepsToComplete_)
		return stepCount_ * 100.0 / stepsToComplete_;

	return stepCount_ ? 50 : 0;
}  //end read()

//==============================================================================
/// return percentage complete as std::string
std::string ProgressBar::readPercentageString()
{
	char pct[5];
	sprintf(pct, "%d", read());
	return pct;
}  //end readPercentageString()
