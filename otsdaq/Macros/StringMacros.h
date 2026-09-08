#ifndef _ots_StringMacros_h_
#define _ots_StringMacros_h_

#include "otsdaq/Macros/CoutMacros.h"

#include <chrono>
#include <map>
#include <memory>  //shared_ptr
#include <set>
#include <typeinfo>  // operator typeid
#include <vector>

namespace ots
{
struct StringMacros
{
	// clang-format off

  private:  ///< private constructor because all static members, should never instantiate
			// this class
	StringMacros	(void);
	~StringMacros	(void);

  public:

	//========================================================================================================================
	/// Here is the list of static helper functions:
	///
	///		wildCardMatch
	///		inWildCardSet
	///		getWildCardMatchFromMap
	///
	///		decodeURIComponent
	///		encodeURIComponent
	///		sanitizeForSQL
	///		escapeString
	///		escapeJSONStringEntities
	///		restoreJSONStringEntities
	///		trim
	///		convertEnvironmentVariables
	///		isNumber
	///		getNumber
	///		getTimestampString
	///		getTimeDurationString
	///
	///		validateValueForDefaultStringDataType
	///
	///		getSetFromString
	///		getVectorFromString
	///		getMapFromString
	///
	///		setToString
	///		vectorToString
	///		mapToString
	///
	///		demangleTypeName
	///		getTypeName
	///		stackTrace
	///		exec
	///		otsGetEnvironmentVarable
	///		extractXmlField
	///		rextractXmlField
	///		coutSplit
	///
	/// End  list of static helper functions:
	//========================================================================================================================

	static bool 				wildCardMatch				(const std::string& needle, const std::string& haystack, unsigned int* priorityIndex = 0);
	static bool 				inWildCardSet				(const std::string& needle, const std::set<std::string>& haystack);

	//========================================================================================================================
	/// getWildCardMatchFromMap ~
	///	returns value if needle is in haystack otherwise throws exception
	///	(considering wildcards AND match priority as defined by
	/// StringMacros::wildCardMatch)
	template<class T>
	static T& 					getWildCardMatchFromMap		(
		const std::string&        								needle,
		std::map<std::string, T>& 								haystack,
		std::string*              								foundKey = 0);  ///< defined in included .icc source

	static std::string 			decodeURIComponent			(const std::string& data);
	static std::string        	encodeURIComponent			(const std::string& data);
	static void		        	sanitizeForSQL				(std::string& data);
	static std::string			escapeString				(std::string inString, bool allowWhiteSpace = false, bool forHtml = false);
	static std::string			escapeJSONStringEntities	(const std::string& str);
	static std::string			restoreJSONStringEntities	(const std::string& str);
	static const std::string&	trim						(std::string& s);
	static std::string 			convertEnvironmentVariables	(const std::string& data);
	static std::map<std::string /* system variable */,
		std::map<std::string /* property */,
		std::string /* value */>>  							systemVariables_;
	static const std::string								TBD; //for to-be-defined system variables (so there is a value in wiz mode, before configuration, etc.)
	static unsigned int				getConcurrencyCount		(void);
	static std::string			getPersistentSystemVariablesFilePath(void);
	static bool					loadPersistentSystemVariables(void); ///< loads persisted 'artdaq' namespace systemVariables_; returns false if no file found

	static bool        			isNumber					(const std::string& stringToCheck); ///< Note: before call consider use of stringToCheck = StringMacros::convertEnvironmentVariables(stringToCheck)
	static std::string  		getNumberType				(const std::string& stringToCheck); ///< Note: before call consider use of stringToCheck = StringMacros::convertEnvironmentVariables(stringToCheck)
	template<class T>
	static bool        			getNumber					(const std::string& s, T& retValue);  ///< defined in included .icc source // Note: before call consider use of stringToCheck = StringMacros::convertEnvironmentVariables(stringToCheck)
	///template<>
	static bool        			getNumber		 			(const std::string& s, bool& retValue);  ///< defined in included .icc source // Note: before call consider use of stringToCheck = StringMacros::convertEnvironmentVariables(stringToCheck)

	static std::string 			getTimestampString			(const std::string& linuxTimeInSeconds);
	static std::string 			getTimestampString			(const time_t linuxTimeInSeconds = time(0));
	static std::string 			getTimeDurationString		(const time_t durationInSeconds = time(0));
	static uint64_t				nowEpochMs					(void);

	//========================================================================================================================
	/// validateValueForDefaultStringDataType ~
	/// 	special validation ignoring any table info - just assuming type string
	template<class T>
	static T 					validateValueForDefaultStringDataType	( ///< defined in included .icc source
		const std::string& 										value,
		bool 													doConvertEnvironmentVariables = true);
	static std::string 			validateValueForDefaultStringDataType	(
		const std::string& 										value,
		bool 													doConvertEnvironmentVariables = true);

	static void 				getSetFromString			(const std::string& inputString, std::set<std::string>& setToReturn, const std::set<char>&  delimiter  = {',', '|', '&'}, const std::set<char>&  whitespace = {' ', '\t', '\n', '\r'});

	//========================================================================================================================
	/// getMapFromString ~
	template<class T /*value type*/,
			 class S = std::string /*name string or const string*/>
	static void 				getMapFromString			( ///< defined in included .icc source
		const std::string&    									inputString,
		std::map<S, T>&       									mapToReturn,
		const std::set<char>& 									pairPairDelimiter  	= {',', '|', '&'},
		const std::set<char>& 									nameValueDelimiter 	= {'=', ':'},
		const std::set<char>& 									whitespace         	= {' ', '\t', '\n', '\r'});
	static void 				getMapFromString			(
		const std::string&                  					inputString,
		std::map<std::string, std::string>& 					mapToReturn,
		const std::set<char>&               					pairPairDelimiter  	= {',', '|', '&'},
		const std::set<char>&               					nameValueDelimiter 	= {'=', ':'},
		const std::set<char>&               					whitespace         	= {' ', '\t', '\n', '\r'});
	static void 				getVectorFromString			(
		const std::string&        								inputString,
		std::vector<std::string>& 								listToReturn,
		const std::set<char>&     								delimiter        	= {',', '|', '&'},
		const std::set<char>&     								whitespace       	= {' ', '\t', '\n', '\r'},
		std::vector<char>*        								listOfDelimiters 	= 0,
		bool													decodeURIComponents = false);
	static std::vector<std::string> getVectorFromString	 	(
		const std::string&        								inputString,
		const std::set<char>&     								delimiter        	= {',', '|', '&'},
		const std::set<char>&     								whitespace       	= {' ', '\t', '\n', '\r'},
		std::vector<char>*        								listOfDelimiters 	= 0,
		bool													decodeURIComponents = false);

	//========================================================================================================================
	/// mapToString ~
	///	*ToString declarations (template definitions are in included .icc source)
	template<class T>
	static std::string 			mapToString					( ///< defined in included .icc source
		const std::map<std::string, T>& 						mapToReturn,
		const std::string& 										primaryDelimeter   	= ", ",
		const std::string& 										secondaryDelimeter 	= ": ");
	template<class S, class T>
	static std::string 			mapToString					( ///< defined in included .icc source
		const std::map<S, T>& 									mapToReturn,
		const std::string& 										primaryDelimeter   	= ", ",
		const std::string& 										secondaryDelimeter 	= ": ");
	template<class T>
	static std::string 			mapToString					(
		const std::map<std::pair<std::string, std::string>, T>& mapToReturn,
		const std::string&                                      primaryDelimeter 	= ", ",
		const std::string& 										secondaryDelimeter 	= ": ");
	template<class T>
	static std::string 			mapToString					(
		const std::map<std::pair<std::string, std::pair<std::string, std::string>>, T>&
																mapToReturn,
		const std::string& 										primaryDelimeter   	= ", ",
		const std::string& 										secondaryDelimeter 	= ": ");
	template<class T>
	static std::string 			mapToString					(
		const std::map<std::string, std::pair<std::string, T>>& mapToReturn,
		const std::string&                                      primaryDelimeter 	= ", ",
		const std::string& 										secondaryDelimeter 	= ": ");
	template<class T>
	static std::string 			mapToString					(
		const std::map<std::string, std::map<std::string, T>>& 	mapToReturn,
		const std::string&                                     	primaryDelimeter   	= ", ",
		const std::string&                                     	secondaryDelimeter 	= ": ");
	template<class T>
	static std::string 			mapToString					(
		const std::map<std::string, std::set<T>>& 				mapToReturn,
		const std::string& 										primaryDelimeter   = ", ",
		const std::string& 										secondaryDelimeter = ": ");
	template<class T>
	static std::string 			mapToString					(
		const std::map<std::string, std::vector<T>>& 			mapToReturn,
		const std::string&                           			primaryDelimeter   = ", ",
		const std::string&                           			secondaryDelimeter = ": ");
	static std::string 			mapToString					(
		const std::map<std::string, uint8_t>& 					mapToReturn,
		const std::string& 										primaryDelimeter   = ", ",
		const std::string& 										secondaryDelimeter = ": ");


	//========================================================================================================================
	/// setToString ~
	template<class T>
	static std::string 			setToString					( ///< defined in included .icc source
		const std::set<T>& 										setToReturn,
		const std::string& 										delimeter 			= ", ");
	static std::string 			setToString					(
		const std::set<uint8_t>& 								setToReturn,
		const std::string&       								delimeter 			= ", ");
	template<class S, class T>
	static std::string 			setToString					(
		const std::set<std::pair<S, T>>& 						setToReturn,
		const std::string& 										primaryDelimeter   	= ", ",
		const std::string& 										secondaryDelimeter 	= ":");


	//========================================================================================================================
	/// vectorToString ~
	template<class T>
	static std::string 			vectorToString				( ///< defined in included .icc source
		const std::vector<T>& 									setToReturn,
		const std::string&    									delimeter 			= ", ");
	static std::string 			vectorToString				(
		const std::vector<uint8_t>& 							setToReturn,
		const std::string&          							delimeter 			= ", ");
	template<class S, class T>
	static std::string 			vectorToString				(
		const std::vector<std::pair<S, T>>& 					setToReturn,
		const std::string& 										primaryDelimeter   	= "; ",
		const std::string& 										secondaryDelimeter 	= ":");

	static bool 				extractCommonChunks			(const std::vector<std::string>& haystack, std::vector<std::string>& commonChunksToReturn, std::vector<std::string>& wildcardStrings, unsigned int& fixedWildcardLength);

	static std::string 			demangleTypeName			(const char* name);
	template<class T>
	static std::string          getTypeName					(void) {return StringMacros::demangleTypeName(typeid(T).name());}
	static std::string 			stackTrace					(void);
	static std::string 			exec						(const char* cmd);

	static char* 				otsGetEnvironmentVarable	(const char* name, const std::string&  location, const unsigned int& line);



	static std::string 			extractXmlField				(const std::string &xml,
															 const std::string &field,
															 uint32_t occurrence, size_t after,
															size_t *returnFindPos = nullptr,
															 const std::string &valueField = "value=",
															 const std::string &quoteType = "'");
	static std::string 			rextractXmlField			(const std::string &xml,
															 const std::string &field,
															 uint32_t occurrence,
															 size_t before,
															size_t *returnFindPos = nullptr,
															 const std::string &valueField = "value=",
															 const std::string &quoteType = "'");
	static void 				coutSplit					(const std::string &str,
															 uint8_t traceLevel = TLVL_DEBUG,
															 const std::set<char>& delimiter = {',', '\n', ';'});

	struct IgnoreCaseCompareStruct { ///<get string in order ignoring letter case
		bool 					operator() 					(const std::string& lhs, const std::string& rhs) const; ///<comparison handler
	}; //end IgnoreCaseCompareStruct

	// clang-format on
};  // end StringMarcos static class

#include "otsdaq/Macros/StringMacros.icc"  //define template functions

}  // namespace ots
#endif
