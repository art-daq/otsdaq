#ifndef _ots_Utilities_ProgressBar_h_
#define _ots_Utilities_ProgressBar_h_

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <mutex>
#include <string>

namespace ots
{
// ProgressBar
// class by Ryan Rivera ( rrivera @ fnal.gov ), July 2013
//
// The are 4 public member function that should matter to the user:
//
//		resetProgressBar(int id) ~~
//			Resets progress bar to 0% complete, id does not have to be unique in your
// code,  read NOTE 2.
//
//		step() ~~
//			Marks that incremental progress has been made. user should place step()
//			freely throughout the code each time a jump in the progress bar is desired.
//			Thread safe.
//
//		complete() ~~
//			Indicate the progress bar is to be 100% at this point.
//
//		read() ~~
//			Returns the % complete. Thread safe.
//
//	USE: User places resetProgressBar(id) at the start of sequence of events, as many
// step()'s as desired 		within the sequence, and complete() at the end of the sequence
// of events. 		The first time the code is run ever the class determines the total
// number of steps, so the
//		progress % will be defined to be 0% with no steps complete, 50% with at least one
// step complete, 		and 100% once complete. Each run thereafter will always use the
// number of steps 		in the previous sequence to calculate the proper %.
//
//	NOTE 1: Since the code uses the previous execution's number of steps to calculate the
//%, avoid
//		placing step() calls in code that is run conditionally. It is best practice to
// place  them 		at checkpoints that must always be reached.
//
//	NOTE 2: The class uses the filename and line # of the resetProgressBar(id) call to
// store the baseline number 		of steps for a given sequence. So if using a single
// resetProgressBar(id) call to govern more than one 		sequence of events, you must
// assign an integer to the sequence you want to reset. Below is 		an example for two
// sequences sharing a resetProgressBar(id):
//
//				ProgressBar myProgressBar;
//				void setupProgress(int i)
//				{
//					myProgressBar.resetProgressBar(i);		//this reset is shared by
// sequence  one  and
//															//two so must identify with
// integer
//
//					if(i==0) sequenceOne();
//					else if(i==1) sequenceTwo();
//					myProgressBar.complete();
//				}
//
//				void sequenceOne()
//				{
//					//do things
//					myProgressBar.step();
//					//do more things
//					myProgressBar.step();
//				}
//
//				void sequenceTwo()
//				{
//					//do other things
//					myProgressBar.step();
//				}
//

class ProgressBar
{
  public:
	ProgressBar();

//********************************************************************//
// NOTE!!! don't call reset. Call resetProgressBar(id) as though
// the function was this:
// void resetProgressBar(int id)
//
// then the pre-compiler directive:
#define resetProgressBar(x) reset(__FILE__, std::to_string(__LINE__), x)
	// will call this reset:
	void reset(std::string file, std::string lineNumber, int id = 0);
	//********************************************************************//

	// remaining member functions are called normal

	// set functions
	void step();      // thread safe
	void complete();  // declare complete, thread safe

	// get functions
	bool isComplete();  // thread safe
	int  read();  // if stepsToComplete==0, then define any progress as 50%, thread safe
	std::string readPercentageString();  // if stepsToComplete==0, then define any
	                                     // progress as 50%, thread safe

  private:
	const std::string cProgressBarFilePath_;
	const std::string cProgressBarFileExtension_;
	std::string       totalStepsFileName_;
	int               stepCount_;
	int               stepsToComplete_;
	bool              started_;
	std::mutex        theMutex_;
};

}  // namespace ots

#endif
