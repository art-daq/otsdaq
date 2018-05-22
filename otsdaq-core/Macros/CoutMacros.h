#ifndef _ots_Cout_Macros_h_
#define _ots_Cout_Macros_h_


#include <string.h> //for strstr (not the same as <string>)
#include <iostream> //for cout
#include <sstream> //for stringstream, std::stringbuf

//take filename only after srcs/ (this gives by repo name)
#define __SHORTFILE__ 	(strstr(&__FILE__[0], "/srcs/") ? strstr(&__FILE__[0], "/srcs/") + 6 : __FILE__)

//take only file name
#define __FILENAME__ 	(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define __E__			std::endl

//#define __COUT_HDR__     __FILE__ << " : " << __PRETTY_FUNCTION__ << " [" << __LINE__ << "]\t"
#define __COUT_HDR_FL__  __SHORTFILE__ << " [" << std::dec << __LINE__ << "]\t"
#define __COUT_HDR_FP__  __SHORTFILE__ << " : " << __PRETTY_FUNCTION__ << "\t"
#define __COUT_HDR_PL__  __PRETTY_FUNCTION__ << " [" << std::dec << __LINE__ << "]\t"
#define __COUT_HDR_F__   __SHORTFILE__ << "\t"
#define __COUT_HDR_L__   std::dec << __LINE__ << "\t"
#define __COUT_HDR_P__   __PRETTY_FUNCTION__ << "\t"
#define __COUT_HDR__     __COUT_HDR_FL__


#define __COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __MF_SUBJECT__ << ":"

#define __COUT_ERR__  	__COUT_TYPE__(LogError) 	<< __COUT_HDR__
#define __COUT_WARN__  	__COUT_TYPE__(LogWarning) 	<< __COUT_HDR__
#define __COUT_INFO__  	__COUT_TYPE__(LogInfo) 		<< __COUT_HDR__
#define __COUT__  		__COUT_TYPE__(LogDebug)		<< __COUT_HDR__
#define __COUTV__(X) 	__COUT__ << QUOTE(X) << " = " << X << __E__


//////// ==============================================================
//////// Use __MOUT__ for Message Facility use (easy to switch to cout for debugging):
////////

#define	__MF_SUBJECT__	"ots" 	//default subject.. others can #undef and re-#define
//Note: to turn off MF everywhere, just replace with std::cout here at __MF_TYPE__(X)!

#define Q(X) #X
#define QUOTE(X) Q(X)
//#define __MF_TYPE__(X)	FIXME ?? how to do this ...(getenv("OTSDAQ_USING_MF")=="1"? mf::X (__MF_SUBJECT__) : std::cout << QUOTE(X) << ":" << __MF_SUBJECT__ << ":")
#define __MF_TYPE__(X)	mf::X (__MF_SUBJECT__)



#define __MF_HDR__		__COUT_HDR_FL__
#define __MOUT_ERR__  	__MF_TYPE__(LogError) 	<< __MF_HDR__
#define __MOUT_WARN__  	__MF_TYPE__(LogWarning) << __MF_HDR__
#define __MOUT_INFO__  	__MF_TYPE__(LogInfo) 	<< __MF_HDR__
#define __MOUT__  		__MF_TYPE__(LogDebug)	<< __MF_HDR__
#define __MOUTV__(X)	__MOUT__ << QUOTE(X) << " = " << X << _E__


//////// ==============================================================
//////// Use __MCOUT__ for cout and Message Facility use in one line (that compiler expands to two)
////////
#define __MCOUT_ERR__(X)  	{__MOUT_ERR__ << X; 	__COUT_ERR__ << X;}
#define __MCOUT_WARN__(X)  	{__MOUT_WARN__ << X; 	__COUT_WARN__ << X;}
#define __MCOUT_INFO__(X)  	{__MOUT_INFO__ << X; 	__COUT_INFO__ << X;}
#define __MCOUT__(X)  		{__MOUT__ << X; 		__COUT__ << X;}
#define __MCOUTV__(X)		{__MOUTV__(X); 			__COUTV__(X);}


//////// ==============================================================

#define __SS__			std::stringstream ss; ss <<  ":" << __MF_SUBJECT__ << ":" << __COUT_HDR__
#define __SS_THROW__	__COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str())

//////// ==============================================================

//for configurable objects, add name to subject
#define __CFG_COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __MF_SUBJECT__ << "-" << theConfigurationRecordName_ << ":"
#define __CFG_MF_TYPE__(X)		mf::X (std::string(__MF_SUBJECT__) + theConfigurationRecordName_)

#define __CFG_MOUT_ERR__  	__CFG_MF_TYPE__(LogError) 	<< __MF_HDR__
#define __CFG_MOUT_WARN__  	__CFG_MF_TYPE__(LogWarning) << __MF_HDR__
#define __CFG_MOUT_INFO__  	__CFG_MF_TYPE__(LogInfo) 	<< __MF_HDR__
#define __CFG_MOUT__  		__CFG_MF_TYPE__(LogDebug)	<< __MF_HDR__
#define __CFG_MOUTV__(X)	__CFG_MOUT__ << QUOTE(X) << " = " << X
#define __CFG_COUT_ERR__  	__CFG_COUT_TYPE__(LogError) 	<< __COUT_HDR__
#define __CFG_COUT_WARN__  	__CFG_COUT_TYPE__(LogWarning) 	<< __COUT_HDR__
#define __CFG_COUT_INFO__  	__CFG_COUT_TYPE__(LogInfo) 		<< __COUT_HDR__
#define __CFG_COUT__  		__CFG_COUT_TYPE__(LogDebug)		<< __COUT_HDR__
#define __CFG_COUTV__(X) 	__CFG_COUT__ << QUOTE(X) << " = " << X << __E__

#define __CFG_MCOUT_ERR__(X)  	{__CFG_MOUT_ERR__ << X; 	__CFG_COUT_ERR__ << X;}
#define __CFG_MCOUT_WARN__(X)  	{__CFG_MOUT_WARN__ << X; 	__CFG_COUT_WARN__ << X;}
#define __CFG_MCOUT_INFO__(X)  	{__CFG_MOUT_INFO__ << X; 	__CFG_COUT_INFO__ << X;}
#define __CFG_MCOUT__(X)  		{__CFG_MOUT__ << X; 		__CFG_COUT__ << X;}
#define __CFG_MCOUTV__(X)		{__CFG_MOUTV__(X); 			__CFG_COUTV__(X);}

#define __CFG_SS__			std::stringstream ss; ss << ":" << __MF_SUBJECT__ << "-" << theConfigurationRecordName_ << ":" << __COUT_HDR__
#define __CFG_SS_THROW__	__CFG_COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str())


//////// ==============================================================

//for core supervisor objects (with supervisorClassNoNamespace_ defined), add class to subject
#define __SUP_COUT_TYPE__(X) 	std::cout << QUOTE(X) << ":" << __MF_SUBJECT__ << "-" << supervisorClassNoNamespace_ << ":"
#define __SUP_MF_TYPE__(X)		mf::X (std::string(__MF_SUBJECT__) + supervisorClassNoNamespace_)

#define __SUP_MOUT_ERR__  	__SUP_MF_TYPE__(LogError) 	<< __MF_HDR__
#define __SUP_MOUT_WARN__  	__SUP_MF_TYPE__(LogWarning) << __MF_HDR__
#define __SUP_MOUT_INFO__  	__SUP_MF_TYPE__(LogInfo) 	<< __MF_HDR__
#define __SUP_MOUT__  		__SUP_MF_TYPE__(LogDebug)	<< __MF_HDR__
#define __SUP_MOUTV__(X)	__SUP_MOUT__ << QUOTE(X) << " = " << X
#define __SUP_COUT_ERR__  	__SUP_COUT_TYPE__(LogError) 	<< __COUT_HDR__
#define __SUP_COUT_WARN__  	__SUP_COUT_TYPE__(LogWarning) 	<< __COUT_HDR__
#define __SUP_COUT_INFO__  	__SUP_COUT_TYPE__(LogInfo) 		<< __COUT_HDR__
#define __SUP_COUT__  		__SUP_COUT_TYPE__(LogDebug)		<< __COUT_HDR__
#define __SUP_COUTV__(X) 	__SUP_COUT__ << QUOTE(X) << " = " << X << __E__

#define __SUP_MCOUT_ERR__(X)  	{__SUP_MOUT_ERR__ << X; 	__SUP_COUT_ERR__ << X;}
#define __SUP_MCOUT_WARN__(X)  	{__SUP_MOUT_WARN__ << X; 	__SUP_COUT_WARN__ << X;}
#define __SUP_MCOUT_INFO__(X)  	{__SUP_MOUT_INFO__ << X; 	__SUP_COUT_INFO__ << X;}
#define __SUP_MCOUT__(X)  		{__SUP_MOUT__ << X; 		__SUP_COUT__ << X;}
#define __SUP_MCOUTV__(X)		{__SUP_MOUTV__(X); 			__SUP_COUTV__(X);}

#define __SUP_SS__			std::stringstream ss; ss << ":" << __MF_SUBJECT__ << "-" << supervisorClassNoNamespace_ << ":" << __COUT_HDR__
#define __SUP_SS_THROW__	__SUP_COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str())



//========================================================================================================================
//const_cast away the const
//	so that otsdaq is compatible with slf6 and slf7 versions of xdaq
//	where they changed to const xdaq::ApplicationDescriptor* in slf7
#ifdef XDAQ_NOCONST
#define XDAQ_CONST_CALL
#else
#define XDAQ_CONST_CALL const
#endif
//========================================================================================================================




#endif