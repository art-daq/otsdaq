#ifndef _ots_Cout_Macros_h_
#define _ots_Cout_Macros_h_

// clang-format off

#include <string.h>  //for strstr (not the same as <string>)
#include <iostream>  //for cout
#include <sstream>   //for stringstream, std::stringbuf

#define TRACEMF_USE_VERBATIM 1 //for trace longer path filenames
#include "TRACE/tracemf.h"

// take filename only after srcs/ (this gives by repo name)
// use 'builtin' to try to define at compile time
#define __SHORTFILE__ 		(__builtin_strstr(&__FILE__[0], "/srcs/") ? __builtin_strstr(&__FILE__[0], "/srcs/") + 6 : __FILE__)

// take only file name
#define __FILENAME__ 		(__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define __E__ 				std::endl

#define __THROW__(X) 		throw std::runtime_error(X)

//un-comment __COUT_TO_STD__ to send __COUT__ directly to std::cout, rather than through message facility
//#define __COUT_TO_STD__		1

//////// ==============================================================
//////// Use __MOUT__ for Message Facility use (easy to switch to cout for debugging):
////////

#define __MF_SUBJECT__ __FILENAME__  // default subject.. others can #undef and re-#define
// Note: to turn off MF everywhere, just replace with std::cout here at __MF_TYPE__(X)!

#define QQQQ(X) #X
#define QUOTE(X) QQQQ(X)
//#define __MF_TYPE__(X)	FIXME ?? how to do this ...(__ENV__("OTSDAQ_USING_MF")=="1"?
// mf::X (__MF_SUBJECT__) : std::cout << QUOTE(X) << ":" << __MF_SUBJECT__ << ":")

#define __COUT_HDR_F__ 		__SHORTFILE__ << ""
#define __COUT_HDR_L__ 		":" << std::dec        << __LINE__ << " |\t"
#define __COUT_HDR_P__ 		__PRETTY_FUNCTION__    << "\t"
#define __COUT_HDR_FL__ 	__SHORTFILE__ << ""  << __COUT_HDR_L__
#define __COUT_HDR_FP__ 	__SHORTFILE__ << ":" << __COUT_HDR_P__
#define __COUT_HDR__ 		""//__COUT_HDR_FL__
#define __MF_HDR__ 			""//__COUT_HDR__

//////// ==============================================================
#define __MF_DECOR__		(__MF_SUBJECT__)
#define __MF_TYPE__(X) 		TLOG(X, __MF_DECOR__)

#ifndef __COUT_TO_STD__
#define __COUT_TYPE__(X) 	__MF_TYPE__(X)
#else
#define __COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __MF_DECOR__ << ": " << __COUT_HDR_FL__
#endif

#define __MOUT_ERR__ 		__MF_TYPE__(TLVL_ERROR) 	<< __MF_HDR__
#define __MOUT_WARN__ 		__MF_TYPE__(TLVL_WARNING) 	<< __MF_HDR__
#define __MOUT_INFO__ 		__MF_TYPE__(TLVL_INFO) 		<< __MF_HDR__
#define __MOUT__ 			__MF_TYPE__(TLVL_DEBUG) 	<< __MF_HDR__
#define __MOUTV__(X) 		__MOUT__ << QUOTE(X) << " = " << X

#define __COUT_ERR__ 		__COUT_TYPE__(TLVL_ERROR) 	<< __COUT_HDR__
#define __COUT_WARN__ 		__COUT_TYPE__(TLVL_WARNING) << __COUT_HDR__
#define __COUT_INFO__ 		__COUT_TYPE__(TLVL_INFO) 	<< __COUT_HDR__
#define __COUT__ 			__COUT_TYPE__(TLVL_DEBUG) 	<< __COUT_HDR__
#define __COUTT__           __COUT_TYPE__(TLVL_TRACE) 	<< __COUT_HDR__
#define __COUTV__(X) 		__COUT__ << QUOTE(X) << " = " << X << __E__
#define __COUTTV__(X) 		__COUTT__ << QUOTE(X) << " = " << X << __E__


//////// ==============================================================
//////// Use __MCOUT__ for cout and Message Facility use in one line (that compiler
/// expands to two)
////////
#define __MCOUT_ERR__(X)   	{ __MOUT_ERR__  << X;} 	//causes duplicate with mf improvements: { __MOUT_ERR__  << X; __COUT_ERR__ 	<< X; }
#define __MCOUT_WARN__(X)   { __MOUT_WARN__  << X;} //causes duplicate with mf improvements: { __MOUT_WARN__ << X; __COUT_WARN__ << X; }
#define __MCOUT_INFO__(X)   { __MOUT_INFO__  << X;} //causes duplicate with mf improvements: { __MOUT_INFO__ << X; __COUT_INFO__ << X; }
#define __MCOUT__(X)   		{ __MOUT__  << X;} 		//causes duplicate with mf improvements: { __MOUT__      << X; __COUT__ 		<< X; }
#define __MCOUTV__(X) 		{ __MOUTV__(X);} 		//causes duplicate with mf improvements: { __MOUTV__(X); __COUTV__(X); }

//////// ==============================================================

#define __SS__            	std::stringstream ss; ss << "|" << __MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#define __SS_THROW__        { __COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str()); } //put in {}'s to prevent surprises, e.g. if ... else __SS_THROW__;
#define __SS_ONLY_THROW__ 	throw std::runtime_error(ss.str())
#define __SSV__(X) 			__SS__ << QUOTE(X) << " = " << X

//////// ==============================================================
// for configurable objects, add name to subject
#define __CFG_MF_DECOR__		(theConfigurationRecordName_ + "\t<> ")
#define __CFG_MF_TYPE__(X) 		TLOG(X, __MF_SUBJECT__) << __CFG_MF_DECOR__

#ifndef __COUT_TO_STD__
#define __CFG_COUT_TYPE__(X) 	__CFG_MF_TYPE__(X)
#else
#define __CFG_COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __CFG_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#endif

#define __CFG_MOUT_ERR__ 		__CFG_MF_TYPE__(TLVL_ERROR) 	<< __COUT_HDR__
#define __CFG_MOUT_WARN__ 		__CFG_MF_TYPE__(TLVL_WARNING) 	<< __COUT_HDR__
#define __CFG_MOUT_INFO__ 		__CFG_MF_TYPE__(TLVL_INFO) 		<< __COUT_HDR__
#define __CFG_MOUT__ 			__CFG_MF_TYPE__(TLVL_DEBUG) 	<< __COUT_HDR__
#define __CFG_MOUTV__(X) 		__CFG_MOUT__ << QUOTE(X) << " = " << X
#define __CFG_COUT_ERR__ 		__CFG_COUT_TYPE__(TLVL_ERROR) 	<< __COUT_HDR__
#define __CFG_COUT_WARN__ 		__CFG_COUT_TYPE__(TLVL_WARNING) << __COUT_HDR__
#define __CFG_COUT_INFO__ 		__CFG_COUT_TYPE__(TLVL_INFO) 	<< __COUT_HDR__
#define __CFG_COUT__ 			__CFG_COUT_TYPE__(TLVL_DEBUG) 	<< __COUT_HDR__
#define __CFG_COUTV__(X) 		__CFG_COUT__ << QUOTE(X) << " = " << X << __E__

#define __CFG_MCOUT_ERR__(X)   	{ __CFG_MOUT_ERR__  << X;} 	//causes duplicate with mf improvements: { __CFG_MOUT_ERR__ 	<< X; __CFG_COUT_ERR__ 	<< X; }
#define __CFG_MCOUT_WARN__(X)  	{ __CFG_MOUT_WARN__  << X;} //causes duplicate with mf improvements: { __CFG_MOUT_WARN__ << X; __CFG_COUT_WARN__ << X; }
#define __CFG_MCOUT_INFO__(X)  	{ __CFG_MOUT_INFO__  << X;} //causes duplicate with mf improvements: { __CFG_MOUT_INFO__ << X; __CFG_COUT_INFO__ << X; }
#define __CFG_MCOUT__(X)   		{ __CFG_MOUT__  << X;} 		//causes duplicate with mf improvements: { __CFG_MOUT__ 		<< X; __CFG_COUT__ 		<< X; }
#define __CFG_MCOUTV__(X) 		{ __CFG_MOUTV__(X);} 		//causes duplicate with mf improvements: { __CFG_MOUTV__(X); __CFG_COUTV__(X); }

#define __CFG_SS__  			std::stringstream ss; ss << "|" << __CFG_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#define __CFG_SS_THROW__        { __CFG_COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str()); }

//////// ==============================================================

// for front-end interface objects, add name to subject
#define __FE_MF_DECOR__			("FE:" + getInterfaceType() + std::string(":") + getInterfaceUID() + ":" + theConfigurationRecordName_ + "\t<> ")
#define __FE_MF_TYPE__(X)      	TLOG(X, __MF_SUBJECT__) << __FE_MF_DECOR__

#ifndef __COUT_TO_STD__
#define __FE_COUT_TYPE__(X) 	__FE_MF_TYPE__(X)
#else
#define __FE_COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __FE_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#endif

#define __FE_MOUT_ERR__ 		__FE_MF_TYPE__(TLVL_ERROR) 		<< __COUT_HDR__
#define __FE_MOUT_WARN__ 		__FE_MF_TYPE__(TLVL_WARNING)	<< __COUT_HDR__
#define __FE_MOUT_INFO__ 		__FE_MF_TYPE__(TLVL_INFO) 		<< __COUT_HDR__
#define __FE_MOUT__ 			__FE_MF_TYPE__(TLVL_DEBUG) 		<< __COUT_HDR__
#define __FE_MOUTV__(X) 		__FE_MOUT__ << QUOTE(X) << " = " << X
#define __FE_COUT_ERR__ 		__FE_COUT_TYPE__(TLVL_ERROR) 	<< __COUT_HDR__
#define __FE_COUT_WARN__ 		__FE_COUT_TYPE__(TLVL_WARNING)	<< __COUT_HDR__
#define __FE_COUT_INFO__ 		__FE_COUT_TYPE__(TLVL_INFO) 	<< __COUT_HDR__
#define __FE_COUT__ 			__FE_COUT_TYPE__(TLVL_DEBUG) 	<< __COUT_HDR__
#define __FE_COUTV__(X) 		__FE_COUT__ << QUOTE(X) << " = " << X << __E__

#define __FE_MCOUT_ERR__(X)   	{ __FE_MOUT_ERR__  << X;} 	//causes duplicate with mf improvements: { __FE_MOUT_ERR__ 	<< X; __FE_COUT_ERR__ 	<< X; }
#define __FE_MCOUT_WARN__(X)   	{ __FE_MOUT_WARN__  << X;} 	//causes duplicate with mf improvements: { __FE_MOUT_WARN__ 	<< X; __FE_COUT_WARN__ 	<< X; }
#define __FE_MCOUT_INFO__(X)   	{ __FE_MOUT_INFO__  << X;} 	//causes duplicate with mf improvements: { __FE_MOUT_INFO__ 	<< X; __FE_COUT_INFO__ 	<< X; }
#define __FE_MCOUT__(X)   		{ __FE_MOUT__  << X;} 		//causes duplicate with mf improvements: { __FE_MOUT__ 		<< X; __FE_COUT__ 		<< X; }
#define __FE_MCOUTV__(X) 		{ __FE_MOUTV__(X);} 		//causes duplicate with mf improvements: { __FE_MOUTV__(X); __FE_COUTV__(X); }

#define __FE_SS__          		std::stringstream ss; ss << "|" << __FE_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#define __FE_SS_THROW__         { __FE_COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str()); }

//////// ==============================================================

// for generic decoration override, just have mfSubject declared
#define __GEN_MF_DECOR__		(mfSubject_ + "\t<> ")
#define __GEN_MF_TYPE__(X) 		TLOG(X, __MF_SUBJECT__) << __GEN_MF_DECOR__

#ifndef __COUT_TO_STD__
#define __GEN_COUT_TYPE__(X) 	__GEN_MF_TYPE__(X)
#else
#define __GEN_COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __GEN_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#endif

#define __GEN_MOUT_ERR__ 		__GEN_MF_TYPE__(TLVL_ERROR) 		<< __COUT_HDR__
#define __GEN_MOUT_WARN__ 		__GEN_MF_TYPE__(TLVL_WARNING) 		<< __COUT_HDR__
#define __GEN_MOUT_INFO__ 		__GEN_MF_TYPE__(TLVL_INFO) 			<< __COUT_HDR__
#define __GEN_MOUT__ 			__GEN_MF_TYPE__(TLVL_DEBUG) 		<< __COUT_HDR__
#define __GEN_MOUTV__(X) 		__GEN_MOUT__ << QUOTE(X) << " = " << X
#define __GEN_COUT_ERR__ 		__GEN_COUT_TYPE__(TLVL_ERROR) 		<< __COUT_HDR__
#define __GEN_COUT_WARN__ 		__GEN_COUT_TYPE__(TLVL_WARNING) 	<< __COUT_HDR__
#define __GEN_COUT_INFO__ 		__GEN_COUT_TYPE__(TLVL_INFO) 		<< __COUT_HDR__
#define __GEN_COUT__ 			__GEN_COUT_TYPE__(TLVL_DEBUG) 		<< __COUT_HDR__
#define __GEN_COUTV__(X) 		__GEN_COUT__ << QUOTE(X) << " = " << X << __E__

#define __GEN_MCOUT_ERR__(X)   	{ __GEN_MOUT_ERR__  << X;} 	//causes duplicate with mf improvements: { __GEN_MOUT_ERR__ 	<< X; __GEN_COUT_ERR__ 	<< X; }
#define __GEN_MCOUT_WARN__(X)   { __GEN_MOUT_WARN__  << X;} //causes duplicate with mf improvements: { __GEN_MOUT_WARN__ << X; __GEN_COUT_WARN__ << X; }
#define __GEN_MCOUT_INFO__(X)   { __GEN_MOUT_INFO__  << X;} //causes duplicate with mf improvements: { __GEN_MOUT_INFO__ << X; __GEN_COUT_INFO__ << X; }
#define __GEN_MCOUT__(X)   		{ __GEN_MOUT__  << X;} 		//causes duplicate with mf improvements: { __GEN_MOUT__ 		<< X; __GEN_COUT__ 		<< X; }
#define __GEN_MCOUTV__(X) 		{ __GEN_MOUTV__(X);} 		//causes duplicate with mf improvements: { __GEN_MOUTV__(X); __GEN_COUTV__(X); }

#define __GEN_SS__        		std::stringstream ss; ss << "|" << __GEN_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#define __GEN_SS_THROW__        { __GEN_COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str()); }

//////// ==============================================================

// for core supervisor objects (with supervisorClassNoNamespace_ defined), add class to
// subject
#define __SUP_MF_DECOR__		(supervisorClassNoNamespace_ + std::string(":") + CorePropertySupervisorBase::getSupervisorUID() + "\t<> ")
#define __SUP_MF_TYPE__(X)      TLOG(X, __MF_SUBJECT__) << __SUP_MF_DECOR__ //mf::X(supervisorClassNoNamespace_ + "-" + CorePropertySupervisorBase::getSupervisorUID())

#ifndef __COUT_TO_STD__
#define __SUP_COUT_TYPE__(X) 	__SUP_MF_TYPE__(X)
#else
#define __SUP_COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __SUP_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#endif

#define __SUP_MOUT_ERR__ 		__SUP_MF_TYPE__(TLVL_ERROR) 		<< __COUT_HDR__
#define __SUP_MOUT_WARN__ 		__SUP_MF_TYPE__(TLVL_WARNING) 		<< __COUT_HDR__
#define __SUP_MOUT_INFO__ 		__SUP_MF_TYPE__(TLVL_INFO) 			<< __COUT_HDR__
#define __SUP_MOUT__ 			__SUP_MF_TYPE__(TLVL_DEBUG) 		<< __COUT_HDR__
#define __SUP_MOUTV__(X) 		__SUP_MOUT__ << QUOTE(X) << " = " << X

#define __SUP_COUT_ERR__ 		__SUP_COUT_TYPE__(TLVL_ERROR) 		<< __COUT_HDR__
#define __SUP_COUT_WARN__ 		__SUP_COUT_TYPE__(TLVL_WARNING) 	<< __COUT_HDR__
#define __SUP_COUT_INFO__ 		__SUP_COUT_TYPE__(TLVL_INFO) 		<< __COUT_HDR__
#define __SUP_COUT__ 			__SUP_COUT_TYPE__(TLVL_DEBUG) 		<< __COUT_HDR__
#define __SUP_COUTV__(X) 		__SUP_COUT__ << QUOTE(X) << " = " << X << __E__

#define __SUP_MCOUT_ERR__(X)   	{ __SUP_MOUT_ERR__  << X;} 	//causes duplicate with mf improvements: { __SUP_MOUT_ERR__ << X; __SUP_COUT_ERR__ << X; }
#define __SUP_MCOUT_WARN__(X)  	{ __SUP_MOUT_WARN__  << X;} //causes duplicate with mf improvements: { __SUP_MOUT_WARN__ << X; __SUP_COUT_WARN__ << X; }
#define __SUP_MCOUT_INFO__(X)  	{ __SUP_MOUT_INFO__  << X;} //causes duplicate with mf improvements: { __SUP_MOUT_INFO__ << X; __SUP_COUT_INFO__ << X; }
#define __SUP_MCOUT__(X)   		{ __SUP_MOUT__  << X;} 		//causes duplicate with mf improvements: { __SUP_MOUT__ << X; __SUP_COUT__ << X; }
#define __SUP_MCOUTV__(X) 		{ __SUP_MOUTV__(X);} 		//causes duplicate with mf improvements: { __SUP_MOUTV__(X); __SUP_COUTV__(X); }

#define __SUP_SS__              std::stringstream ss; ss << "|" << __SUP_MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#define __SUP_SS_THROW__        { __SUP_COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str()); }

//To use __ENV__, will need to include #include "otsdaq/Macros/StringMacros.h"
#define __ENV__(X) 				StringMacros::otsGetEnvironmentVarable(X, std::string(__SHORTFILE__), __LINE__)

//========================================================================================================================
// const_cast away the const
//	so that otsdaq is compatible with slf6 and slf7 versions of xdaq
//	where they changed to const xdaq::ApplicationDescriptor* in slf7
#ifdef XDAQ_NOCONST
#define XDAQ_CONST_CALL
#else
#define XDAQ_CONST_CALL const
#endif
//========================================================================================================================

//========================================================================================================================
// declare special ots soft PAUSE exception
//	a PAUSE exception thrown during a running workloop by a state machine plugin will
//	PAUSE the global state machine and allow for manual intervention to resume a run.
namespace ots
{
struct __OTS_PAUSE_EXCEPTION__ : public std::exception
{
	__OTS_PAUSE_EXCEPTION__(const std::string& what) : what_(what) {}
	virtual char const* what() const throw() { return what_.c_str(); }
	std::string         what_;
};
}  // end namespace ots
//========================================================================================================================
// declare special ots harder STOP exception
//	a STOP exception thrown during a running workloop by a state machine plugin will
//	STOP the global state machine and allow for manual intervention to resume a run.
namespace ots
{
struct __OTS_STOP_EXCEPTION__ : public std::exception
{
	__OTS_STOP_EXCEPTION__(const std::string& what) : what_(what) {}
	virtual char const* what() const throw() { return what_.c_str(); }
	std::string         what_;
};
}  // end namespace ots

#endif

// clang-format on
