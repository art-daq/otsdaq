#include "otsdaq/Macros/StringMacros.h"

#include <sched.h>    // for sched_getaffinity
#include <algorithm>  // for find_if
#include <array>
#include <cstdint>  // for uintptr_t
#include <fstream>  // for loadPersistentSystemVariables
#include <mutex>    // for loadPersistentSystemVariables

using namespace ots;

std::map<std::string /* system variable */,
         std::map<std::string /* property */, std::string /* value */>>
                  StringMacros::systemVariables_;
const std::string StringMacros::TBD = "To-be-defined";

//==============================================================================
unsigned int StringMacros::getConcurrencyCount(void)
{
	try
	{
		return std::stoul(systemVariables_.at("System").at("logicalCores"));
	}
	catch(...)
	{
	}
	cpu_set_t    mask;
	unsigned int hw = 0;
	if(sched_getaffinity(0, sizeof(mask), &mask) == 0)
		hw = CPU_COUNT(&mask);
	systemVariables_["System"]["logicalCores"] = std::to_string(hw);
	return hw;
}  //end getConcurrencyCount()

//==============================================================================
// getPersistentSystemVariablesFilePath
//	Path of the file where 'artdaq' namespace system variables are persisted
//	(written by the ARTDAQ Supervisor, e.g. from web GUI setSystemVariable requests).
std::string StringMacros::getPersistentSystemVariablesFilePath(void)
{
	return std::string(__ENV__("USER_DATA")) + "/ServiceData/ArtdaqSystemVariables.dat";
}  // end getPersistentSystemVariablesFilePath()

//==============================================================================
// loadPersistentSystemVariables
//	Load persisted 'artdaq' namespace system variables so that
//	${OTS.artdaq.<property>} references resolve in every process, not only in
//	the ARTDAQ Supervisor that saved them. Returns false if no file was found.
bool StringMacros::loadPersistentSystemVariables(void)
try
{
	// serialize concurrent callers (e.g. the parallel table-init threads calling
	// ConfigurationManager::initPrereqsForARTDAQ() at configure time) - unguarded
	// concurrent insertion into the static systemVariables_ map corrupts the heap
	static std::mutex           loadMutex;
	std::lock_guard<std::mutex> lock(loadMutex);

	std::ifstream file(getPersistentSystemVariablesFilePath());
	if(!file.is_open())
		return false;

	auto&       ns = systemVariables_["artdaq"];
	std::string line;
	while(std::getline(file, line))
	{
		size_t eqPos = line.find('=');
		if(eqPos == std::string::npos)
			continue;
		ns[line.substr(0, eqPos)] = line.substr(eqPos + 1);
	}
	return true;
}  // end loadPersistentSystemVariables()
catch(...)
{
	return false;  // e.g. USER_DATA not defined in this process
}

#define TLVL_EscapeString 30  // = TLVL_DEBUG + 30
#define TLVL_EnvMath 49       // = TLVL_DEBUG + 49
#define TLVL_EnvSub 50        // = TLVL_DEBUG + 50

//==============================================================================
/// wildCardMatch
///	find needle in haystack
///		allow needle to have wildcard '*' anywhere
///		consider priority in matching, no matter the order in the haystack:
///			- 0: no match!
///			- 1: highest priority is exact match
///			- 2: next highest is partial TRAILING-wildcard match
///			- 3: next highest is partial LEADING-wildcard match
///			- 4: lowest priority is wildcard match (including internal '*')
///			- 5: wildcard-only match
///		return priority found by reference
bool StringMacros::wildCardMatch(const std::string& needle,
                                 const std::string& haystack,
                                 unsigned int*      priorityIndex)
try
{
	__COUTT__ << "\t\t wildCardMatch: " << needle << " =in= " << haystack << " ??? "
	          << std::endl;

	// empty needle
	if(needle.size() == 0)
	{
		if(priorityIndex)
			*priorityIndex = 1;  // consider an exact match, to stop higher level loops
		return true;             // if empty needle, always "found"
	}

	// only wildcard
	if(needle == "*")
	{
		if(priorityIndex)
			*priorityIndex = 5;  // only wildcard, is lowest priority
		return true;             // if empty needle, always "found"
	}

	// no wildcards
	if(needle == haystack)
	{
		if(priorityIndex)
			*priorityIndex = 1;  // an exact match
		return true;
	}

	const bool hasWildcard = (needle.find('*') != std::string::npos);
	if(!hasWildcard)
	{
		if(priorityIndex)
			*priorityIndex = 0;  // no wildcard and not exact => no match
		return false;
	}

	// trailing wildcard
	if(needle[needle.size() - 1] == '*' &&
	   needle.substr(0, needle.size() - 1) == haystack.substr(0, needle.size() - 1))
	{
		if(priorityIndex)
			*priorityIndex = 2;  // trailing wildcard match
		return true;
	}

	// leading wildcard
	if(needle[0] == '*' &&
	   needle.substr(1) == haystack.substr(haystack.size() - (needle.size() - 1)))
	{
		if(priorityIndex)
			*priorityIndex = 3;  // leading wildcard match
		return true;
	}

	// generic wildcard matching with '*' anywhere in needle
	// '*' matches any sequence (including empty)
	std::size_t patternPos      = 0;
	std::size_t textPos         = 0;
	std::size_t lastStarPattern = std::string::npos;
	std::size_t lastStarTextPos = std::string::npos;
	while(textPos < haystack.size())
	{
		if(patternPos < needle.size() && needle[patternPos] == haystack[textPos])
		{
			++patternPos;
			++textPos;
		}
		else if(patternPos < needle.size() && needle[patternPos] == '*')
		{
			lastStarPattern = patternPos++;
			lastStarTextPos = textPos;
		}
		else if(lastStarPattern != std::string::npos)
		{
			patternPos = lastStarPattern + 1;
			textPos    = ++lastStarTextPos;
		}
		else
		{
			if(priorityIndex)
				*priorityIndex = 0;  // no match
			return false;
		}
	}

	while(patternPos < needle.size() && needle[patternPos] == '*')
		++patternPos;

	if(patternPos == needle.size())
	{
		if(priorityIndex)
			*priorityIndex = 4;  // wildcard match
		return true;
	}

	// else no match
	if(priorityIndex)
		*priorityIndex = 0;  // no match
	return false;
}  //end wildCardMatch()
catch(...)
{
	if(priorityIndex)
		*priorityIndex = 0;  // no match
	return false;            // if out of range
}  //end wildCardMatch() catch

//==============================================================================
/// inWildCardSet ~
///	returns true if needle is in haystack (considering wildcards)
///	allow inverted haystack strings by first character being '!'
bool StringMacros::inWildCardSet(const std::string&           needle,
                                 const std::set<std::string>& haystack)
{
	for(const auto& haystackString : haystack)
	{
		// use wildcard match, flip needle parameter.. because we want haystack to have the wildcards
		if(haystackString.size() && haystackString[0] == '!')
		{
			//treat as inverted
			if(!StringMacros::wildCardMatch(haystackString.substr(1), needle))
				return true;
		}
		else if(StringMacros::wildCardMatch(haystackString, needle))
			return true;
	}
	return false;
}

//==============================================================================
/// decodeURIComponent
///	converts all %## to the ascii character
std::string StringMacros::decodeURIComponent(const std::string& data)
{
	std::string  decodeURIString(data.size(), 0);  // init to same size
	unsigned int j = 0;
	for(unsigned int i = 0; i < data.size(); ++i, ++j)
	{
		if(data[i] == '%')
		{
			// high order hex nibble digit
			if(data[i + 1] > '9')  // then ABCDEF
				decodeURIString[j] += (data[i + 1] - 55) * 16;
			else
				decodeURIString[j] += (data[i + 1] - 48) * 16;

			// low order hex nibble digit
			if(data[i + 2] > '9')  // then ABCDEF
				decodeURIString[j] += (data[i + 2] - 55);
			else
				decodeURIString[j] += (data[i + 2] - 48);

			i += 2;  // skip to next char
		}
		else
			decodeURIString[j] = data[i];
	}
	decodeURIString.resize(j);
	return decodeURIString;
}  // end decodeURIComponent()

//==============================================================================
std::string StringMacros::encodeURIComponent(const std::string& sourceStr)
{
	std::string retStr = "";
	char        encodeStr[4];
	for(const auto& c : sourceStr)
		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			retStr += c;
		else
		{
			sprintf(encodeStr, "%%%2.2X", (uint8_t)c);
			retStr += encodeStr;
		}
	return retStr;
}  // end encodeURIComponent()

//==============================================================================
/// StringMacros::sanitizeForSQL
void StringMacros::sanitizeForSQL(std::string& str)
{
	std::map<char, std::string> replacements = {
	    {'\'', "''"},   // Single quote becomes two single quotes
	    {'\\', "\\\\"}  //,  // Backslash becomes double backslash
	    // {';', "\\;"},    // Semicolon can be escaped (optional)
	    // {'-', "\\-"},    // Dash for comments (optional, context-specific)
	};

	size_t pos = 0;
	while(pos < str.size())
	{
		auto it = replacements.find(str[pos]);
		if(it != replacements.end())
		{
			str.replace(pos, 1, it->second);
			pos += it->second.size();  // Advance past the replacement
		}
		else
		{
			++pos;
		}
	}
}  //end sanitizeForSQL

//==============================================================================
/// StringMacros::escapeString
///	convert quotes to html quote characters &apos; = ' and &quot; = "
///	remove new line characters
///	and (if !allowWhiteSpace) remove white space (so that read from file white space
/// artifact removed)
///
///	convert &amp; = &
///	if(allowWhiteSpace) convert \t to 8 &#160; spaces and \n to <br>
///
/// if(forHtml) then double escape < > to &lt; and &gt; for better display in web browser
std::string StringMacros::escapeString(std::string inString,
                                       bool        allowWhiteSpace /* = false */,
                                       bool        forHtml /* = false */)
{
	unsigned int ws = -1;
	char         htmlTmp[10];

	__COUTVS__(TLVL_EscapeString, allowWhiteSpace);
	__COUTVS__(TLVL_EscapeString, forHtml);

	for(unsigned int i = 0; i < inString.length(); i++)
		if(inString[i] != ' ')
		{
			__COUTS__(TLVL_EscapeString)
			    << i << ". " << inString[i] << ":" << (int)inString[i] << std::endl;

			// remove new lines and unprintable characters
			if(inString[i] == '\r' || inString[i] == '\n' ||  // remove new line chars
			   inString[i] == '\t' ||                         // remove tabs
			   inString[i] < 32 ||  // remove un-printable characters (they mess up xml
			                        // interpretation)
			   (inString[i] > char(126) &&
			    inString[i] < char(161)))  // this is aggravated by the bug in
			                               // MFextensions (though Eric says he fixed on
			                               // 8/24/2016)  Note: greater than 255 should be
			                               // impossible if by byte (but there are html
			                               // chracters in 300s and 8000s)
			{
				//handle UTF-8 encoded characters
				if(i + 2 < inString.size() && inString[i] == char(0xE2) &&
				   inString[i + 1] == char(0x80) &&
				   inString[i + 2] ==
				       char(0x93))  // longer dash endash is 3-bytes 0xE2 0x80 0x93
				{
					//encode "--" as &#8211;
					inString.insert(i,
					                "&#82");  // insert HTML name before special character
					inString.replace(
					    i + 4, 1, 1, '1');  // replace special character-0 with s
					inString.replace(
					    i + 5, 1, 1, '1');  // replace special character-1 with h
					inString.replace(
					    i + 6, 1, 1, ';');  // replace special character-2 with ;
					i += 7;                 // skip to next char to check
					ws = i;                 // last non white space char
					--i;
					continue;
				}

				if(inString[i] == '\n')  // maintain new lines and tabs
				{
					if(allowWhiteSpace)
					{
						sprintf(htmlTmp, "&#%3.3d", inString[i]);
						inString.insert(
						    i, std::string(htmlTmp));  // insert html str sequence
						inString.replace(
						    i + 5, 1, 1, ';');  // replace special character with ;
						i += 6;                 // skip to next char to check
						--i;
					}
					else  // translate to ' '
						inString[i] = ' ';
				}
				else if(inString[i] == '\t')  // maintain new lines and tabs
				{
					if(allowWhiteSpace)
					{
						if(0)
						{
							// tab = 8 spaces
							sprintf(htmlTmp,
							        "&#160;&#160;&#160;&#160;&#160;&#160;&#160;&#160");
							inString.insert(
							    i, std::string(htmlTmp));  // insert html str sequence
							inString.replace(
							    i + 47, 1, 1, ';');  // replace special character with ;
							i += 48;                 // skip to next char to check
							--i;
						}
						else  // tab =  0x09
						{
							sprintf(htmlTmp, "&#009");
							inString.insert(
							    i, std::string(htmlTmp));  // insert html str sequence
							inString.replace(
							    i + 5, 1, 1, ';');  // replace special character with ;
							i += 6;                 // skip to next char to check
							--i;
						}
					}
					else  // translate to ' '
						inString[i] = ' ';
				}
				else
				{
					inString.erase(i, 1);  // erase character
					--i;                   // step back so next char to check is correct
				}
				__COUTS__(31) << inString << std::endl;
				continue;
			}

			__COUTS__(31) << inString << std::endl;

			// replace special characters
			if(inString[i] == '\"' || inString[i] == '\'')
			{
				//check for extra escaping of the quotes
				// a quote is escaped only when preceded by an odd number of backslashes
				{
					unsigned int backslashCount = 0;
					for(unsigned int j = i; j > 0 && inString[j - 1] == '\\'; --j)
						++backslashCount;

					if(backslashCount % 2 == 1)
					{
						//then this is an escaped quote, so remove the escape character and skip
						inString.erase(i - 1, 1);  // erase escape character
						--i;  // step back so next char to check is correct
					}
				}

				inString.insert(i,
				                (inString[i] == '\'')
				                    ? "&apos"
				                    : "&quot");      // insert HTML name before quotes
				inString.replace(i + 5, 1, 1, ';');  // replace special character with ;
				i += 5;                              // skip to next char to check
				                                     //__COUT__ <<  inString << std::endl;
			}
			else if(inString[i] == '&')
			{
				inString.insert(i, "&amp");  // insert HTML name before special character
				inString.replace(i + 4, 1, 1, ';');  // replace special character with ;
				i += 4;                              // skip to next char to check
			}
			else if(inString[i] == '<' || inString[i] == '>')
			{
				if(!forHtml)
				{
					inString.insert(
					    i,
					    (inString[i] == '<')
					        ? "&lt"
					        : "&gt");  // insert HTML name before special character
					inString.replace(
					    i + 3, 1, 1, ';');  // replace special character with ;
					i += 3;                 // skip to next char to check
				}
				else  //double escape
				{
					inString.insert(
					    i,
					    (inString[i] == '<')
					        ? "&amp;lt"
					        : "&amp;gt");  // insert HTML name before special character
					inString.replace(
					    i + 7, 1, 1, ';');  // replace special character with ;
					i += 7;                 // skip to next char to check
				}
			}
			else if(inString[i] >= char(161) &&
			        inString[i] <= char(255))  // printable special characters
			{
				sprintf(htmlTmp, "&#%3.3d", inString[i]);
				inString.insert(i, std::string(htmlTmp));  // insert html number sequence
				inString.replace(i + 5, 1, 1, ';');  // replace special character with ;
				i += 5;                              // skip to next char to check
			}

			__COUTS__(TLVL_EscapeString) << inString << std::endl;

			ws = i;  // last non white space char
		}
		else if(allowWhiteSpace)  // keep white space if allowed
		{
			if(i - 1 == ws)
				continue;  // dont do anything for first white space

			// for second white space add 2, and 1 from then
			if(0 && i - 2 == ws)
			{
				inString.insert(i, "&#160;");  // insert html space
				i += 6;                        // skip to point at space again
			}
			inString.insert(i, "&#160");         // insert html space
			inString.replace(i + 5, 1, 1, ';');  // replace special character with ;
			i += 5;                              // skip to next char to check
			                                     // ws = i;
		}

	__COUTS__(TLVL_EscapeString) << inString.size() << " " << ws << std::endl;

	// inString.substr(0,ws+1);

	__COUTS__(TLVL_EscapeString) << inString.size() << " " << inString << std::endl;

	if(allowWhiteSpace)  // keep all white space
		return inString;
	// else trim trailing white space

	if(ws == (unsigned int)-1)
		return "";                      // empty std::string since all white space
	return inString.substr(0, ws + 1);  // trim right white space
}  // end escapeString()

//==============================================================================
/// getEscapedValueAsString
///	  Returns string with special characters escaped for JSON
///	Note: this should be useful for any values placed in double quotes, i.e. JSON.
///  Reverse of restoreJSONStringEntities()
std::string StringMacros::escapeJSONStringEntities(const std::string& str)
{
	unsigned int sz = str.size();
	if(!sz)
		return "";  // empty string, returns empty string

	std::string retStr = "";
	retStr.reserve(str.size() * 2);  // reserve roughly right size
	for(unsigned int i = 0; i < sz; ++i)
	{
		switch(str[i])
		{
		case '\n':
			retStr += "\\n";
			break;
		case '"':
			retStr += "\\\"";
			break;
		case '\t':
			retStr += "\\t";
			break;
		case '\r':
			retStr += "\\r";
			break;
		case '\\':
			retStr += "\\\\";
			break;
		default:
			retStr += str[i];
		}
	}
	return retStr;
}  //end escapeJSONStringEntities

//==============================================================================
/// restoreJSONStringEntities
///	 Returns string with literals \n \t \" \r \\ replaced with char
///  Reverse of escapeJSONStringEntities()
std::string StringMacros::restoreJSONStringEntities(const std::string& str)
{
	unsigned int sz = str.size();
	if(!sz)
		return "";  // empty string, returns empty string

	std::string retStr = "";
	retStr.reserve(str.size());  // reserve roughly right size
	unsigned int i = 0;
	for(; i < sz - 1; ++i)
	{
		if(str[i] == '\\')  // if 2 char escape sequence, replace with char
			switch(str[i + 1])
			{
			case 'n':
				retStr += '\n';
				++i;
				break;
			case '"':
				retStr += '"';
				++i;
				break;
			case 't':
				retStr += '\t';
				++i;
				break;
			case 'r':
				retStr += '\r';
				++i;
				break;
			case '\\':
				retStr += '\\';
				++i;
				break;
			default:
				retStr += str[i];
			}
		else
			retStr += str[i];
	}
	if(i == sz - 1)
		retStr += str[sz - 1];  // output last character (which can't escape anything)

	return retStr;
}  // end restoreJSONStringEntities()

//==============================================================================
/// StringMacros::trim
///		Remove whitespace like JavaScript trim()
const std::string& StringMacros::trim(std::string& s)
{
	// remove leading whitespace
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		        return !std::isspace(ch);
	        }));

	// remove trailing whitespace
	s.erase(std::find_if(
	            s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
	            .base(),
	        s.end());

	return s;
}  // end trim()

//==============================================================================
/// convertEnvironmentVariables ~
///	static recursive function
///
///	Allows environment variables entered as $NAME or ${NAME}
///  or system variables entered as ${OTS.<variable>.<property>} (only bracket syntax allowed!)
///		e.g. ${OTS.ActiveStateMachine.name}
///		e.g. ${OTS.ActiveStateMachine.fileNameAlias}
///	System variable are read from the static StringMacros map,
/// 	which is generally filled by the host Supervisor.
std::string StringMacros::convertEnvironmentVariables(const std::string& data)
{
	size_t begin = data.find("$");
	if(begin != std::string::npos && begin + 1 < data.size())
	{
		size_t      end;
		std::string envVariable;
		std::string converted  = data;   // make copy to modify
		bool        usedBraces = false;  // track if braces were used

		while(begin && begin != std::string::npos &&
		      converted[begin - 1] ==
		          '\\')  //do not convert environment variables with escaped \$
		{
			converted.replace(begin - 1, 1, "");
			begin = converted.find("$", begin + 1);  //find next
			if(begin == std::string::npos)
			{
				__COUTS__(TLVL_EnvSub)
				    << "Only found escaped $'s that will not be converted: " << converted
				    << __E__;
				return converted;
			}
		}

		// check if using $(( )) arithmetic expansion syntax
		// First expand any $-based variables (including OTS system variables),
		// then evaluate the arithmetic expression using getNumber() (requires explicit $ for variables, e.g., $A-$B)
		if(begin + 2 < data.size() && data[begin + 1] == '(' && data[begin + 2] == '(')
		{
			end = data.find("))", begin + 3);
			if(end == std::string::npos)
			{
				__SS__ << "Arithmetic expansion '$((...)),' at pos " << begin
				       << " in value, is missing closing '))'! Here was the value: "
				       << data << __E__;
				__SS_THROW__;
			}

			std::string expression = data.substr(begin + 3, end - begin - 3);
			__COUTVS__(TLVL_EnvMath, expression);

			// Expand $VAR and ${OTS.*.*} inside the expression
			expression = convertEnvironmentVariables(expression);
			__COUTVS__(TLVL_EnvMath, expression);

			int64_t result;

			bool isNumber = getNumber(expression, result);
			if(!isNumber)
			{
				__SS__ << "Arithmetic expansion '$((...)),' at pos " << begin
				       << " in value, does not evaluate to a number! Here was the value: "
				       << data << __E__;
				__SS_THROW__;
			}

			__COUTS__(TLVL_EnvMath) << "Arithmetic result: " << result << __E__;

			// proceed recursively, replacing $((...)) with the result
			return convertEnvironmentVariables(
			    converted.replace(begin, end - begin + 2, std::to_string(result)));
		}
		else if(data[begin + 1] == '{')  // check if using ${NAME} syntax
		{
			end         = data.find("}", begin + 2);
			envVariable = data.substr(begin + 2, end - begin - 2);
			++end;  // replace the closing } too!
			usedBraces = true;
		}
		else  // else using $NAME syntax
		{
			// end is first non environment variable character
			for(end = begin + 1; end < data.size(); ++end)
				if(!((data[end] >= '0' && data[end] <= '9') ||
				     (data[end] >= 'A' && data[end] <= 'Z') ||
				     (data[end] >= 'a' && data[end] <= 'z') || data[end] == '-' ||
				     data[end] == '_' || data[end] == '.' || data[end] == ':'))
					break;  // found end
			envVariable = data.substr(begin + 1, end - begin - 1);
			usedBraces  = false;
		}
		__COUTVS__(TLVL_EnvSub, data);
		__COUTVS__(TLVL_EnvSub, envVariable);
		if(usedBraces && envVariable.starts_with("OTS."))
		{
			__COUTS__(TLVL_EnvSub) << "OTS system variable detected!" << __E__;
			auto sysVarSplit = StringMacros::getVectorFromString(envVariable, {'.'});
			__COUTVS__(TLVL_EnvSub, StringMacros::vectorToString(sysVarSplit));

			if(sysVarSplit.size() != 3 ||
			   systemVariables_.find(sysVarSplit[1]) == systemVariables_.end() ||
			   systemVariables_.at(sysVarSplit[1]).find(sysVarSplit[2]) ==
			       systemVariables_.at(sysVarSplit[1]).end())
			{
				__SS__
				    << "System variable ${" << envVariable
				    << "} is not valid or was not found!"
				    << "\n\n"
				    << "If you were trying to access an ots System Variable, the correct "
				       "syntax is "
				    << "${OTS.<variable>.<property>}, e.g. "
				       "${OTS.ActiveStateMachine.name}"
				    << "\n\n"
				    << "If you were trying to insert an arithmetic operation, the "
				       "correct "
				       "syntax is $((4 - 3)) or $(($ENVVAR1 - $ENVVAR2))"
				    << "\n\n"
				    << "Available system variables:" << __E__;

				// Print all available system variables
				for(const auto& varPair : systemVariables_)
				{
					ss << "\n  OTS." << varPair.first << ".*";
					for(const auto& propPair : varPair.second)
						ss << "\n    - OTS." << varPair.first << "." << propPair.first;
				}
				ss << __E__;
				__SS_THROW__;
			}
			//else successful
			// proceed recursively
			return convertEnvironmentVariables(converted.replace(
			    begin,
			    end - begin,
			    systemVariables_.at(sysVarSplit[1]).at(sysVarSplit[2])));
		}
		else
		{
			char* envResult = nullptr;
			try
			{
				envResult = __ENV__(envVariable.c_str());
			}
			catch(const std::runtime_error& e)
			{
				__SS__
				    << ("The environmental variable '" + envVariable +
				        "' is not set! Please make sure you set it before continuing!" +
				        "\n\n" +
				        "If you were trying to access an ots System Variable, the "
				        "correct syntax is " +
				        "${OTS.<variable>.<property>}, e.g. "
				        "${OTS.ActiveStateMachine.name}")
				    << __E__;
				ss << "\n" << e.what() << __E__;
				__SS_ONLY_THROW__;
			}

			// proceed recursively
			return convertEnvironmentVariables(
			    converted.replace(begin, end - begin, envResult));
		}
	}
	// else no environment variables found in string
	__COUTS__(TLVL_EnvSub) << "Result: " << data << __E__;
	return data;
}  //end convertEnvironmentVariables()

//==============================================================================
/// isNumber ~~
///	returns true if one or many numbers separated by operations (+,-,/,*) is
///		present in the string.
///	Numbers can be hex ("0x.."), binary("b..."), or base10.
bool StringMacros::isNumber(const std::string& s)
{
	// extract set of potential numbers and operators
	std::vector<std::string> numbers;
	std::vector<char>        ops;

	if(!s.size())
		return false;

	StringMacros::getVectorFromString(
	    s,
	    numbers,
	    /*delimiter*/ std::set<char>({'+', '-', '*', '/'}),
	    /*whitespace*/ std::set<char>({' ', '\t', '\n', '\r'}),
	    &ops);

	//__COUTV__(StringMacros::vectorToString(numbers));
	//__COUTV__(StringMacros::vectorToString(ops));

	for(const auto& number : numbers)
	{
		if(number.size() == 0)
			continue;  // skip empty numbers

		if(number.find("0x") == 0)  // indicates hex
		{
			//__COUT__ << "0x found" << std::endl;
			for(unsigned int i = 2; i < number.size(); ++i)
			{
				if(!((number[i] >= '0' && number[i] <= '9') ||
				     (number[i] >= 'A' && number[i] <= 'F') ||
				     (number[i] >= 'a' && number[i] <= 'f')))
				{
					//__COUT__ << "prob " << number[i] << std::endl;
					return false;
				}
			}
			// return std::regex_match(number.substr(2), std::regex("^[0-90-9a-fA-F]+"));
		}
		else if(number[0] == 'b')  // indicates binary
		{
			//__COUT__ << "b found" << std::endl;

			for(unsigned int i = 1; i < number.size(); ++i)
			{
				if(!((number[i] >= '0' && number[i] <= '1')))
				{
					//__COUT__ << "prob " << number[i] << std::endl;
					return false;
				}
			}
		}
		else
		{
			//__COUT__ << "base 10 " << std::endl;
			for(unsigned int i = 0; i < number.size(); ++i)
				if(!((number[i] >= '0' && number[i] <= '9') || number[i] == '.' ||
				     number[i] == '+' || number[i] == '-'))
					return false;
			// Note: std::regex crashes in unresolvable ways (says Ryan.. also, stop using
			// libraries)  return std::regex_match(s,
			// std::regex("^(\\-|\\+)?[0-9]*(\\.[0-9]+)?"));
		}
	}

	//__COUT__ << "yes " << std::endl;

	// all numbers are numbers
	return true;
}  // end isNumber()

//==============================================================================
/// getNumberType ~~
///	returns string of number type: "unsigned long long", "double"
///	or else "nan" for not-a-number
///
///	Numbers can be hex ("0x.."), binary("b..."), or base10.
std::string StringMacros::getNumberType(const std::string& s)
{
	// extract set of potential numbers and operators
	std::vector<std::string> numbers;
	std::vector<char>        ops;

	bool hasDecimal = false;

	StringMacros::getVectorFromString(
	    s,
	    numbers,
	    /*delimiter*/ std::set<char>({'+', '-', '*', '/'}),
	    /*whitespace*/ std::set<char>({' ', '\t', '\n', '\r'}),
	    &ops);

	//__COUTV__(StringMacros::vectorToString(numbers));
	//__COUTV__(StringMacros::vectorToString(ops));

	for(const auto& number : numbers)
	{
		if(number.size() == 0)
			continue;  // skip empty numbers

		if(number.find("0x") == 0)  // indicates hex
		{
			//__COUT__ << "0x found" << std::endl;
			for(unsigned int i = 2; i < number.size(); ++i)
			{
				if(!((number[i] >= '0' && number[i] <= '9') ||
				     (number[i] >= 'A' && number[i] <= 'F') ||
				     (number[i] >= 'a' && number[i] <= 'f')))
				{
					//__COUT__ << "prob " << number[i] << std::endl;
					return "nan";
				}
			}
			// return std::regex_match(number.substr(2), std::regex("^[0-90-9a-fA-F]+"));
		}
		else if(number[0] == 'b')  // indicates binary
		{
			//__COUT__ << "b found" << std::endl;

			for(unsigned int i = 1; i < number.size(); ++i)
			{
				if(!((number[i] >= '0' && number[i] <= '1')))
				{
					//__COUT__ << "prob " << number[i] << std::endl;
					return "nan";
				}
			}
		}
		else
		{
			//__COUT__ << "base 10 " << std::endl;
			for(unsigned int i = 0; i < number.size(); ++i)
				if(!((number[i] >= '0' && number[i] <= '9') || number[i] == '.' ||
				     number[i] == '+' || number[i] == '-'))
					return "nan";
				else if(number[i] == '.')
					hasDecimal = true;
			// Note: std::regex crashes in unresolvable ways (says Ryan.. also, stop using
			// libraries)  return std::regex_match(s,
			// std::regex("^(\\-|\\+)?[0-9]*(\\.[0-9]+)?"));
		}
	}

	//__COUT__ << "yes " << std::endl;

	// all numbers are numbers
	if(hasDecimal)
		return "double";
	return "unsigned long long";
}  // end getNumberType()

//==============================================================================
// static template function
///	for bool, but not all other number types
///	return false if string is not a bool
/// template<>
/// inline bool StringMacros::getNumber<bool>(const std::string& s, bool& retValue)
bool StringMacros::getNumber(const std::string& s, bool& retValue)
{
	if(s.size() < 1)
	{
		__COUT_ERR__ << "Invalid empty bool string " << s << __E__;
		return false;
	}

	// check true case
	if(s.find("1") != std::string::npos || s == "true" || s == "True" || s == "TRUE")
	{
		retValue = true;
		return true;
	}

	// check false case
	if(s.find("0") != std::string::npos || s == "false" || s == "False" || s == "FALSE")
	{
		retValue = false;
		return true;
	}

	__COUT_ERR__ << "Invalid bool string " << s << __E__;
	return false;

}  // end static getNumber<bool>

//==============================================================================
/// getTimestampString ~~
///	returns ots style timestamp string
///	of known fixed size: Thu Aug 23 14:55:02 2001 CST
std::string StringMacros::getTimestampString(const std::string& linuxTimeInSeconds)
{
	time_t timestamp(strtol(linuxTimeInSeconds.c_str(), 0, 10));
	return getTimestampString(timestamp);
}  // end getTimestampString()

//==============================================================================
/// getTimestampString ~~
///	returns ots style timestamp string
///	of known fixed size: Thu Aug 23 14:55:02 2001 CST
std::string StringMacros::getTimestampString(const time_t linuxTimeInSeconds)
{
	return ots::TimestampString().get(linuxTimeInSeconds);
}  // end getTimestampString()

//==============================================================================
/// getTimeDurationString
///	returns the duration HH:MM:SS with consideration for day(s)
std::string StringMacros::getTimeDurationString(time_t t)
{
	//e.g., used by CoreSupervisorBase::getStatusProgressDetail(void)

	std::stringstream ss;
	int               days = t / 60 / 60 / 24;
	if(days > 0)
	{
		ss << days << " day" << (days > 1 ? "s" : "") << ", ";
		t -= days * 60 * 60 * 24;
	}

	//HH:MM:SS
	ss << std::setw(2) << std::setfill('0') << (t / 60 / 60) << ":" << std::setw(2)
	   << std::setfill('0') << ((t % (60 * 60)) / 60) << ":" << std::setw(2)
	   << std::setfill('0') << (t % 60);
	return ss.str();
}  //end getTimeDurationString()

//==============================================================================
uint64_t StringMacros::nowEpochMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	           std::chrono::system_clock::now().time_since_epoch())
	    .count();
}  //end nowEpochMs()

//==============================================================================
/// validateValueForDefaultStringDataType
///
std::string StringMacros::validateValueForDefaultStringDataType(
    const std::string& value, bool doConvertEnvironmentVariables)
try
{
	return doConvertEnvironmentVariables
	           ? StringMacros::convertEnvironmentVariables(value)
	           : value;
}
catch(const std::runtime_error& e)
{
	__SS__ << "Failed to validate value for default string data type. " << __E__
	       << e.what() << __E__;
	__SS_THROW__;
}

//==============================================================================
/// getSetFromString
///	extracts the set of elements from string that uses a delimiter
///		ignoring whitespace
void StringMacros::getSetFromString(const std::string&     inputString,
                                    std::set<std::string>& setToReturn,
                                    const std::set<char>&  delimiter,
                                    const std::set<char>&  whitespace)
{
	unsigned int i = 0;
	unsigned int j = 0;

	// go through the full string extracting elements
	// add each found element to set
	for(; j < inputString.size(); ++j)
		if((whitespace.find(inputString[j]) !=
		        whitespace.end() ||  // ignore leading white space or delimiter
		    delimiter.find(inputString[j]) != delimiter.end()) &&
		   i == j)
			++i;
		else if((whitespace.find(inputString[j]) !=
		             whitespace
		                 .end() ||  // trailing white space or delimiter indicates end
		         delimiter.find(inputString[j]) != delimiter.end()) &&
		        i != j)  // assume end of element
		{
			//__COUT__ << "Set element found: " <<
			//		inputString.substr(i,j-i) << std::endl;

			setToReturn.emplace(inputString.substr(i, j - i));

			// setup i and j for next find
			i = j + 1;
		}

	if(i != j)  // last element check (for case when no concluding ' ' or delimiter)
		setToReturn.emplace(inputString.substr(i, j - i));
}  // end getSetFromString()

//==============================================================================
/// getVectorFromString
///	extracts the list of elements from string that uses a delimiter
///		ignoring whitespace
///	optionally returns the list of delimiters encountered, which may be useful
///		for extracting which operator was used.
///
///
///	Note: lists are returned as vectors
///	Note: the size() of delimiters will be one less than the size() of the returned values
///		unless there is a leading delimiter, in which case vectors will have the same
/// size.
void StringMacros::getVectorFromString(const std::string&        inputString,
                                       std::vector<std::string>& listToReturn,
                                       const std::set<char>&     delimiter,
                                       const std::set<char>&     whitespace,
                                       std::vector<char>*        listOfDelimiters,
                                       bool                      decodeURIComponents)
{
	unsigned int             i = 0;
	unsigned int             j = 0;
	unsigned int             c = 0;
	std::set<char>::iterator delimeterSearchIt;
	char                     lastDelimiter = 0;
	bool                     isDelimiter;
	// bool foundLeadingDelimiter = false;

	//__COUT__ << inputString << __E__;
	//__COUTV__(inputString.length());

	// go through the full string extracting elements
	// add each found element to set
	for(; c < inputString.size(); ++c)
	{
		//__COUT__ << (char)inputString[c] << __E__;

		delimeterSearchIt = delimiter.find(inputString[c]);
		isDelimiter       = delimeterSearchIt != delimiter.end();

		//__COUT__ << (char)inputString[c] << " " << isDelimiter <<
		//__E__;//char)lastDelimiter << __E__;

		if(whitespace.find(inputString[c]) !=
		       whitespace.end()  // ignore leading white space
		   && i == j)
		{
			++i;
			++j;
			// if(isDelimiter)
			//	foundLeadingDelimiter = true;
		}
		else if(whitespace.find(inputString[c]) != whitespace.end() &&
		        i != j)  // trailing white space, assume possible end of element
		{
			// do not change j or i
		}
		else if(isDelimiter)  // delimiter is end of element
		{
			//__COUT__ << "Set element found: " <<
			//		inputString.substr(i,j-i) << std::endl;

			if(listOfDelimiters && listToReturn.size())  // || foundLeadingDelimiter))
			                                             // //accept leading delimiter
			                                             // (especially for case of
			                                             // leading negative in math
			                                             // parsing)
			{
				//__COUTV__(lastDelimiter);
				listOfDelimiters->push_back(lastDelimiter);
			}
			listToReturn.push_back(decodeURIComponents ? StringMacros::decodeURIComponent(
			                                                 inputString.substr(i, j - i))
			                                           : inputString.substr(i, j - i));

			// setup i and j for next find
			i = c + 1;
			j = c + 1;
		}
		else  // part of element, so move j, not i
			j = c + 1;

		if(isDelimiter)
			lastDelimiter = *delimeterSearchIt;
		//__COUTV__(lastDelimiter);
	}

	if(1)  // i != j) //last element check (for case when no concluding ' ' or delimiter)
	{
		//__COUT__ << "Last element found: " <<
		//		inputString.substr(i,j-i) << std::endl;

		if(listOfDelimiters && listToReturn.size())  // || foundLeadingDelimiter))
		                                             // //accept leading delimiter
		                                             // (especially for case of leading
		                                             // negative in math parsing)
		{
			//__COUTV__(lastDelimiter);
			listOfDelimiters->push_back(lastDelimiter);
		}
		listToReturn.push_back(decodeURIComponents ? StringMacros::decodeURIComponent(
		                                                 inputString.substr(i, j - i))
		                                           : inputString.substr(i, j - i));
	}

	// assert that there is one less delimiter than values
	if(listOfDelimiters && listToReturn.size() - 1 != listOfDelimiters->size() &&
	   listToReturn.size() != listOfDelimiters->size())
	{
		__SS__ << "There is a mismatch in delimiters to entries (should be equal or one "
		          "less delimiter): "
		       << listOfDelimiters->size() << " vs " << listToReturn.size() << __E__
		       << "Entries: " << StringMacros::vectorToString(listToReturn) << __E__
		       << "Delimiters: " << StringMacros::vectorToString(*listOfDelimiters)
		       << __E__;
		__SS_THROW__;
	}

}  // end getVectorFromString()

//==============================================================================
/// getVectorFromString
///	extracts the list of elements from string that uses a delimiter
///		ignoring whitespace
///	optionally returns the list of delimiters encountered, which may be useful
///		for extracting which operator was used.
///
///
///	Note: lists are returned as vectors
///	Note: the size() of delimiters will be one less than the size() of the returned values
///		unless there is a leading delimiter, in which case vectors will have the same
/// size.
std::vector<std::string> StringMacros::getVectorFromString(
    const std::string&    inputString,
    const std::set<char>& delimiter,
    const std::set<char>& whitespace,
    std::vector<char>*    listOfDelimiters,
    bool                  decodeURIComponents)
{
	std::vector<std::string> listToReturn;

	StringMacros::getVectorFromString(inputString,
	                                  listToReturn,
	                                  delimiter,
	                                  whitespace,
	                                  listOfDelimiters,
	                                  decodeURIComponents);
	return listToReturn;
}  // end getVectorFromString()

//==============================================================================
/// getMapFromString
///	extracts the map of name-value pairs from string that uses two delimiters
///		ignoring whitespace
void StringMacros::getMapFromString(const std::string&                  inputString,
                                    std::map<std::string, std::string>& mapToReturn,
                                    const std::set<char>&               pairPairDelimiter,
                                    const std::set<char>& nameValueDelimiter,
                                    const std::set<char>& whitespace)
try
{
	unsigned int i = 0;
	unsigned int j = 0;
	std::string  name;
	bool         needValue = false;

	// go through the full string extracting map pairs
	// add each found pair to map
	for(; j < inputString.size(); ++j)
		if(!needValue)  // finding name
		{
			if((whitespace.find(inputString[j]) !=
			        whitespace.end() ||  // ignore leading white space or delimiter
			    pairPairDelimiter.find(inputString[j]) != pairPairDelimiter.end()) &&
			   i == j)
				++i;
			else if((whitespace.find(inputString[j]) !=
			             whitespace
			                 .end() ||  // trailing white space or delimiter indicates end
			         nameValueDelimiter.find(inputString[j]) !=
			             nameValueDelimiter.end()) &&
			        i != j)  // assume end of map name
			{
				//__COUT__ << "Map name found: " <<
				//		inputString.substr(i,j-i) << std::endl;

				name = inputString.substr(i, j - i);  // save name, for concluding pair

				needValue = true;  // need value now

				// setup i and j for next find
				i = j + 1;
			}
		}
		else  // finding value
		{
			if((whitespace.find(inputString[j]) !=
			        whitespace.end() ||  // ignore leading white space or delimiter
			    nameValueDelimiter.find(inputString[j]) != nameValueDelimiter.end()) &&
			   i == j)
				++i;
			else if(whitespace.find(inputString[j]) !=
			            whitespace
			                .end() ||  // trailing white space or delimiter indicates end
			        pairPairDelimiter.find(inputString[j]) !=
			            pairPairDelimiter.end())  // &&
			                                      //  i != j)  // assume end of value name
			{
				//__COUT__ << "Map value found: " <<
				//		inputString.substr(i,j-i) << std::endl;

				auto /*pair<it,success>*/ emplaceReturn =
				    mapToReturn.emplace(std::pair<std::string, std::string>(
				        name,
				        validateValueForDefaultStringDataType(
				            inputString.substr(i, j - i))  // value
				        ));

				if(!emplaceReturn.second)
				{
					__COUT__ << "Ignoring repetitive value ('"
					         << inputString.substr(i, j - i)
					         << "') and keeping current value ('"
					         << emplaceReturn.first->second << "'). " << __E__;
				}

				needValue = false;  // need name now

				// setup i and j for next find
				i = j + 1;
			}
		}

	if(i != j)  // last value (for case when no concluding ' ' or delimiter)
	{
		auto /*pair<it,success>*/ emplaceReturn =
		    mapToReturn.emplace(std::pair<std::string, std::string>(
		        name,
		        validateValueForDefaultStringDataType(
		            inputString.substr(i, j - i))  // value
		        ));

		if(!emplaceReturn.second)
		{
			__COUT__ << "Ignoring repetitive value ('" << inputString.substr(i, j - i)
			         << "') and keeping current value ('" << emplaceReturn.first->second
			         << "'). " << __E__;
		}
	}
}  // end getMapFromString()
catch(const std::runtime_error& e)
{
	__SS__ << "Error while extracting a map from the string '" << inputString
	       << "'... is it a valid map?" << __E__ << e.what() << __E__;
	__SS_THROW__;
}

//==============================================================================
/// mapToString
std::string StringMacros::mapToString(const std::map<std::string, uint8_t>& mapToReturn,
                                      const std::string& primaryDelimeter,
                                      const std::string& secondaryDelimeter)
{
	std::stringstream ss;
	bool              first = true;
	for(auto& mapPair : mapToReturn)
	{
		if(first)
			first = false;
		else
			ss << primaryDelimeter;
		ss << mapPair.first << secondaryDelimeter << (unsigned int)mapPair.second;
	}
	return ss.str();
}  // end mapToString()

//==============================================================================
/// setToString
std::string StringMacros::setToString(const std::set<uint8_t>& setToReturn,
                                      const std::string&       delimeter)
{
	std::stringstream ss;
	bool              first = true;
	for(auto& setValue : setToReturn)
	{
		if(first)
			first = false;
		else
			ss << delimeter;
		ss << (unsigned int)setValue;
	}
	return ss.str();
}  // end setToString()

//==============================================================================
/// vectorToString
std::string StringMacros::vectorToString(const std::vector<uint8_t>& setToReturn,
                                         const std::string&          delimeter)
{
	std::stringstream ss;
	bool              first = true;
	if(delimeter == "\n")
		ss << "\n";  //add initial new line if new line delimiting
	for(auto& setValue : setToReturn)
	{
		if(first)
			first = false;
		else
			ss << delimeter;
		ss << (unsigned int)setValue;
	}
	return ss.str();
}  // end vectorToString()

//==============================================================================
/// extractCommonChunks
///	return the common chunks from the vector of strings
///		e.g. if the strings were created from a template
///	string like reader*_east*, this function will return
///	a vector of size 3 := {"reader","_east",""} and
///	a vector of wildcards that would replace the *
///
///	Returns true if common chunks and wildcards found,
///	returns false if all inputs were the same (i.e. no wildcards needed)
bool StringMacros::extractCommonChunks(const std::vector<std::string>& haystack,
                                       std::vector<std::string>& commonChunksToReturn,
                                       std::vector<std::string>& wildcardStringsToReturn,
                                       unsigned int&             fixedWildcardLength)
{
	fixedWildcardLength = 0;  // init to default
	__COUTTV__(StringMacros::vectorToString(haystack));

	// Steps:
	//	- find start and end common chunks first in haystack strings
	//  - use start and end to determine if there is more than one *
	//	- decide if fixed width was specified (based on prepended 0s to numbers)
	//	- search for more instances of * value
	//
	//
	//	// Note: lambda recursive function to find chunks
	//	std::function<void(
	//			const std::vector<std::string>&,
	//			const std::string&,
	//			const unsigned int, const int)> localRecurse =
	//	    [&specialFolders, &specialMapTypes, &retMap, &localRecurse](
	//	    		const std::vector<std::string>& haystack,
	//			const std::string& offsetPath,
	//			const unsigned int depth,
	//			const int specialIndex)
	//			{
	//
	//		    //__COUTV__(path);
	//		    //__COUTV__(depth);
	//	}
	std::pair<unsigned int /*lo*/, unsigned int /*hi*/> wildcardBounds(
	    std::make_pair(-1, 0));  // initialize to illegal wildcard

	// look for starting matching segment
	for(unsigned int n = 1; n < haystack.size(); ++n)
		for(unsigned int i = 0, j = 0;
		    i < haystack[0].length() && j < haystack[n].length();
		    ++i, ++j)
		{
			if(i < wildcardBounds.first)
			{
				if(haystack[0][i] != haystack[n][j])
				{
					wildcardBounds.first = i;  // found lo side of wildcard
					break;
				}
			}
			else
				break;
		}
	__COUTS__(3) << "Low side = " << wildcardBounds.first << " "
	             << haystack[0].substr(0, wildcardBounds.first) << __E__;

	// look for end matching segment
	for(unsigned int n = 1; n < haystack.size(); ++n)
		for(int i = haystack[0].length() - 1, j = haystack[n].length() - 1;
		    i >= (int)wildcardBounds.first && j >= (int)wildcardBounds.first;
		    --i, --j)
		{
			if(i > (int)wildcardBounds.second)  // looking for hi side
			{
				if(haystack[0][i] != haystack[n][j])
				{
					wildcardBounds.second = i + 1;  // found hi side of wildcard
					break;
				}
			}
			else
				break;
		}

	__COUTS__(3) << "High side = " << wildcardBounds.second << " "
	             << haystack[0].substr(wildcardBounds.second) << __E__;

	// add first common chunk
	commonChunksToReturn.push_back(haystack[0].substr(0, wildcardBounds.first));

	if(wildcardBounds.first != (unsigned int)-1)  // potentially more chunks if not end
	{
		//  - use start and end to determine if there is more than one *
		for(int i = (wildcardBounds.first + wildcardBounds.second) / 2 + 1;
		    i < (int)wildcardBounds.second;
		    ++i)
			if(haystack[0][wildcardBounds.first] == haystack[0][i] &&
			   haystack[0].substr(wildcardBounds.first, wildcardBounds.second - i) ==
			       haystack[0].substr(i, wildcardBounds.second - i))
			{
				std::string multiWildcardString =
				    haystack[0].substr(i, wildcardBounds.second - i);
				__COUT__ << "Potential multi-wildcard found: " << multiWildcardString
				         << " at position i=" << i << __E__;

				std::vector<unsigned int /*lo index*/> wildCardInstances;
				// add front one now, and back one later
				wildCardInstances.push_back(wildcardBounds.first);

				unsigned int offset =
				    wildCardInstances[0] + multiWildcardString.size() + 1;
				std::string middleString = haystack[0].substr(offset, (i - 1) - offset);
				__COUTV__(middleString);

				// search for more wildcard instances in new common area
				size_t k;
				while((k = middleString.find(multiWildcardString)) != std::string::npos)
				{
					__COUT__ << "Multi-wildcard found at " << k << __E__;

					wildCardInstances.push_back(offset + k);

					middleString =
					    middleString.substr(k + multiWildcardString.size() + 1);
					offset += k + multiWildcardString.size() + 1;
					__COUTV__(middleString);
				}

				// add back one last
				wildCardInstances.push_back(i);

				for(unsigned int w = 0; w < wildCardInstances.size() - 1; ++w)
				{
					__COUTV__(wildCardInstances[w]);
					__COUTV__(wildCardInstances[w + 1]);
					__COUTV__(wildCardInstances.size());
					commonChunksToReturn.push_back(haystack[0].substr(
					    wildCardInstances[w] + multiWildcardString.size(),
					    wildCardInstances[w + 1] -
					        (wildCardInstances[w] + multiWildcardString.size())));
				}
			}

		__COUTTV__(StringMacros::vectorToString(commonChunksToReturn));
		//confirm valid multi-commonChunksToReturn for all haystack entries (only first can be certain at this point)
		for(unsigned int c = 1; c < commonChunksToReturn.size(); ++c)
		{
			__COUT__ << "Checking [" << c << "]: " << commonChunksToReturn[c] << __E__;
			for(unsigned int n = 1; n < haystack.size(); ++n)
			{
				__COUT__ << "Checking chunks work with haystack [" << n
				         << "]: " << haystack[n] << __E__;
				__COUTV__(commonChunksToReturn[0].size());
				std::string wildCardValue = haystack[n].substr(
				    commonChunksToReturn[0].size(),
				    haystack[n].find(commonChunksToReturn[1],
				                     commonChunksToReturn[0].size() + 1) -
				        commonChunksToReturn[0].size());
				__COUTTV__(wildCardValue);

				std::string builtString = "";
				for(unsigned int cc = 0; cc < commonChunksToReturn.size(); ++cc)
					builtString += commonChunksToReturn[cc] + wildCardValue;
				__COUTTV__(builtString);
				__COUTTV__(wildCardValue);

				if(haystack[n].find(builtString) != 0)
				{
					__COUT__ << "Dropping common chunk " << commonChunksToReturn[c]
					         << ", built '" << builtString << "' not found in "
					         << haystack[n] << __E__;
					commonChunksToReturn.erase(commonChunksToReturn.begin() + c);
					--c;    //rewind
					break;  //check next chunk
				}
				else
					__COUTT__ << "Found built '" << builtString << "' in " << haystack[n]
					          << __E__;
			}  //end haystack loop
		}      //end common chunk loop

		__COUTTV__(StringMacros::vectorToString(commonChunksToReturn));
		__COUTTV__(commonChunksToReturn[0].size());

		__COUTTV__(fixedWildcardLength);
		// check if all common chunks END in 0 to add fixed length
		for(unsigned int i = 0; i < commonChunksToReturn[0].size(); ++i)
			if(commonChunksToReturn[0][commonChunksToReturn[0].size() - 1 - i] == '0')
			{
				++fixedWildcardLength;
				__COUTT__ << "Trying for added fixed length +1 to " << fixedWildcardLength
				          << __E__;
			}
			else
				break;

		// bool allHave0 = true;
		for(unsigned int c = 0; c < commonChunksToReturn.size(); ++c)
		{
			unsigned int cnt = 0;
			for(unsigned int i = 0; i < commonChunksToReturn[c].size(); ++i)
				if(commonChunksToReturn[c][commonChunksToReturn[c].size() - 1 - i] == '0')
					++cnt;
				else
					break;

			if(fixedWildcardLength < cnt)
				fixedWildcardLength = cnt;
			else if(fixedWildcardLength > cnt)
			{
				__SS__ << "Invalid fixed length found, please simplify indexing between "
				          "these common chunks: "
				       << StringMacros::vectorToString(commonChunksToReturn) << __E__;
				__SS_THROW__;
			}
		}
		__COUTTV__(fixedWildcardLength);

		if(fixedWildcardLength)  // take trailing 0s out of common chunks
			for(unsigned int c = 0; c < commonChunksToReturn.size(); ++c)
				commonChunksToReturn[c] = commonChunksToReturn[c].substr(
				    0, commonChunksToReturn[c].size() - fixedWildcardLength);

		// add last common chunk
		commonChunksToReturn.push_back(haystack[0].substr(wildcardBounds.second));
	}  // end handling more chunks
	__COUTTV__(StringMacros::vectorToString(commonChunksToReturn));

	// now determine wildcard strings
	size_t       k;
	unsigned int i;
	unsigned int ioff                 = fixedWildcardLength;
	bool         wildcardsNeeded      = false;
	bool         someLeadingZeros     = false;
	bool         allWildcardsSameSize = true;

	for(unsigned int n = 0; n < haystack.size(); ++n)
	{
		std::string wildcard = "";
		k                    = 0;
		i                    = ioff + commonChunksToReturn[0].size();

		if(commonChunksToReturn.size() == 1)  // just get end
			wildcard = haystack[n].substr(i);
		else
			for(unsigned int c = 1; c < commonChunksToReturn.size(); ++c)
			{
				if(c == commonChunksToReturn.size() - 1)  // for last, do reverse find
					k = haystack[n].rfind(commonChunksToReturn[c]);
				else
					k = haystack[n].find(commonChunksToReturn[c], i + 1);

				if(wildcard == "")
				{
					// set wildcard for first time
					__COUTVS__(3, i);
					__COUTVS__(3, k);
					__COUTVS__(3, k - i);

					wildcard = haystack[n].substr(i, k - i);
					if(fixedWildcardLength && n == 0)
						fixedWildcardLength += wildcard.size();

					__COUTS__(3) << "name[" << n << "] = " << wildcard << " fixed @ "
					             << fixedWildcardLength << __E__;

					break;
				}
				else if(0 /*skip validation in favor of speed*/ &&
				        wildcard != haystack[n].substr(i, k - i))
				{
					__SS__ << "Invalid wildcard! for name[" << n << "] = " << haystack[n]
					       << " - the extraction algorithm is confused, please simplify "
					          "your naming convention."
					       << __E__;
					__SS_THROW__;
				}

				i = k;
			}  // end commonChunksToReturn loop

		if(wildcard.size())
		{
			wildcardsNeeded = true;

			//track if need for leading 0s in wildcards
			if(wildcard[0] == '0' && !fixedWildcardLength)
			{
				someLeadingZeros = true;
				if(wildcardStringsToReturn.size() &&
				   wildcard.size() != wildcardStringsToReturn[0].size())
					allWildcardsSameSize = false;
			}
		}
		wildcardStringsToReturn.push_back(wildcard);

	}  // end name loop

	__COUTTV__(StringMacros::vectorToString(commonChunksToReturn));
	__COUTTV__(StringMacros::vectorToString(wildcardStringsToReturn));

	if(someLeadingZeros && allWildcardsSameSize)
	{
		__COUTTV__(fixedWildcardLength);  //should be 0 in this case
		fixedWildcardLength = wildcardStringsToReturn[0].size();
		__COUT__ << "Enforce wildcard size of " << fixedWildcardLength << __E__;
	}

	if(wildcardStringsToReturn.size() != haystack.size())
	{
		__SS__ << "There was a problem during common chunk extraction!" << __E__;
		__SS_THROW__;
	}

	return wildcardsNeeded;

}  // end extractCommonChunks()

//==============================================================================
/// IgnoreCaseCompareStruct operator used to order
///	std::set, etc ignoring letter case
/// e.g. used here: void ConfigurationGUISupervisor::handleTablesXML
bool StringMacros::IgnoreCaseCompareStruct::operator()(const std::string& lhs,
                                                       const std::string& rhs) const
{
	//__COUTV__(lhs);
	//__COUTV__(rhs);
	// return true if lhs < rhs (lhs will be ordered first)

	for(unsigned int i = 0; i < lhs.size() && i < rhs.size(); ++i)
	{
		//__COUT__ << i << "\t" << lhs[i] << "\t" << rhs[i] << __E__;
		if((lhs[i] >= 'A' && lhs[i] <= 'Z' && rhs[i] >= 'A' && rhs[i] <= 'Z') ||
		   (lhs[i] >= 'a' && lhs[i] <= 'z' && rhs[i] >= 'a' && rhs[i] <= 'z'))
		{  // same case
			if(lhs[i] == rhs[i])
				continue;
			return (lhs[i] < rhs[i]);
			//{ retVal = false; break;} //return false;
		}
		else if(lhs[i] >= 'A' && lhs[i] <= 'Z')  // rhs lower case
		{
			if(lhs[i] + 32 == rhs[i])  // lower case is higher by 32
				return false;          // in tie return lower case first
			return (lhs[i] + 32 < rhs[i]);
		}
		else if(rhs[i] >= 'A' && rhs[i] <= 'Z')
		{
			if(lhs[i] == rhs[i] + 32)  // lower case is higher by 32
				return true;           // in tie return lower case first
			return (lhs[i] < rhs[i] + 32);
		}
		else  // not letters case (should only be for numbers)
		{
			if(lhs[i] == rhs[i])
				continue;
			return (lhs[i] < rhs[i]);
		}
	}  // end case insensitive compare loop

	// lhs and rhs are equivalent to character[i], so return false if rhs.size() was the limit reached
	return lhs.size() < rhs.size();
}  // end IgnoreCaseCompareStruct::operator() comparison handler

//==============================================================================
/// exec
///	run linux command and get result back in string
std::string StringMacros::exec(const char* cmd)
{
	__COUTTV__(cmd);

	std::array<char, 128> buffer;
	std::string           result;

	// For capturing both stdout and stderr, we need to redirect stderr to stdout
	// This is done by appending " 2>&1" to the command
	std::string cmdWithRedirect = std::string(cmd) + " 2>&1";
	FILE*       rawPipe         = popen(cmdWithRedirect.c_str(), "r");
	if(!rawPipe)
		__THROW__("popen() failed!");

	while(fgets(buffer.data(), buffer.size(), rawPipe) != nullptr)
		result += buffer.data();

	int status = pclose(rawPipe);
	if(status == -1)
		__COUT_WARN__ << "pclose() failed for command: " << cmd << __E__;

	__COUTTV__(result);
	return result;
}  // end exec()

// #include <iostream>
#include <fstream> /* for ifstream */
// #include <sstream>
// #include <string>
// #include <cstdlib>
//==============================================================================
uintptr_t find_library_base(const std::string& libname)
{
	std::ifstream maps("/proc/self/maps");
	std::string   line;

	while(std::getline(maps, line))
	{
		if(line.find(libname) != std::string::npos &&
		   line.find("r-xp") != std::string::npos)
		{
			uintptr_t         base;
			std::stringstream ss(line);
			ss >> std::hex >> base;
			return base;
		}
	}
	return 0;
}  //end find_library_base()

//==============================================================================
void resolve_stack_entry(const std::string& so_path,
                         const std::string& real_name,
                         const std::string& offset_begin,  // e.g. "+0x249d"
                         const std::string& offset_end     // e.g. "[0x7f5518fa28fd]"
)
{
	// Extract runtime address from a string like "[0x....]".
	// Be defensive: validate delimiters before parsing to avoid exceptions.
	const std::size_t pos0x = offset_end.find("0x");
	if(pos0x == std::string::npos)
	{
		__COUTS__(52) << "resolve_stack_entry: could not find \"0x\" in offset_end: '"
		              << offset_end << "'" << __E__;
		return;
	}

	const std::size_t posBracket = offset_end.find(']', pos0x);
	if(posBracket == std::string::npos || posBracket < pos0x + 3)
	{
		__COUTS__(52) << "resolve_stack_entry: could not find closing ']' with at least "
		                 "one hex digit after \"0x\" "
		                 "in offset_end: '"
		              << offset_end << "'" << __E__;
		return;
	}

	const std::string addr_str = offset_end.substr(pos0x, posBracket - pos0x);

	uintptr_t runtime_addr = 0;
	try
	{
		runtime_addr = std::stoull(addr_str, nullptr, 16);
	}
	catch(const std::exception& e)
	{
		__COUTS__(52) << "resolve_stack_entry: failed to parse runtime address from '"
		              << addr_str << "': " << e.what() << __E__;
		return;
	}

	std::string so_name = so_path.substr(so_path.find_last_of('/') + 1);

	uintptr_t base = find_library_base(so_name);
	if(!base)
	{
		std::cerr << "Could not find base for " << so_name << "\n";
		return;
	}

	uintptr_t file_addr = runtime_addr - base;

	std::ostringstream cmd;
	cmd << "addr2line -f -C -e " << so_path << " 0x" << std::hex << file_addr;

	__COUT__ << "\nResolving:\n"
	         << so_path << " : " << real_name << offset_begin << " [" << std::hex
	         << runtime_addr << "]\n\n";

	std::string result = StringMacros::exec(cmd.str().c_str());
	__COUTV__(result);
}  //end resolve_stack_entry()

//==============================================================================
/// stackTrace
///	static function
///	https://gist.github.com/fmela/591333/c64f4eb86037bb237862a8283df70cdfc25f01d3
#include <cxxabi.h>    //for abi::__cxa_demangle
#include <execinfo.h>  //for back trace of stack
// #include "TUnixSystem.h"
std::string StringMacros::stackTrace()
{
	__SS__ << "ots::stackTrace:\n";

	void*  array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);
	// backtrace_symbols_fd(array, size, STDERR_FILENO);

	// https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
	char** messages = backtrace_symbols(array, size);

	// skip first stack frame (points here)
	// char syscom[256];
	for(unsigned int i = 1; i < size && messages != NULL; ++i)
	{
		// mangled name needs to be converted to get nice name and line number
		// line number not working... FIXME

		//		sprintf(syscom,"addr2line %p -e %s",
		//				array[i],
		//				messages[i]); //last parameter is the name of this app
		//		ss << StringMacros::exec(syscom) << __E__;
		//		system(syscom);

		// continue;

		char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

		// find parentheses and +address offset surrounding mangled name
		for(char* p = messages[i]; *p; ++p)
		{
			if(*p == '(')
			{
				mangled_name = p;
			}
			else if(*p == '+')
			{
				offset_begin = p;
			}
			else if(*p == ')')
			{
				offset_end = p;
				break;
			}
		}

		// if the line could be processed, attempt to demangle the symbol
		if(mangled_name && offset_begin && offset_end && mangled_name < offset_begin)
		{
			*mangled_name++ = '\0';
			*offset_begin++ = '\0';
			*offset_end++   = '\0';

			int   status;
			char* real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

			// if demangling is successful, output the demangled function name
			if(status == 0)
			{
				ss << "[" << i << "] " << messages[i] << " : " << real_name << "+"
				   << offset_begin << offset_end << std::endl;
				// Too slow to resolve lines (stackTrace getting called too much)!
				// resolve_stack_entry(messages[i],real_name,offset_begin,offset_end);
			}
			// otherwise, output the mangled function name
			else
			{
				ss << "[" << i << "] " << messages[i] << " : " << mangled_name << "+"
				   << offset_begin << offset_end << std::endl;
			}
			free(real_name);
		}
		// otherwise, print the whole line
		else
		{
			ss << "[" << i << "] " << messages[i] << std::endl;
		}
	}
	ss << std::endl;
	ss << std::endl;

	free(messages);

	// call ROOT's stack trace to get line numbers of ALL threads
	// gSystem->StackTrace();

	return ss.str();
}  // end stackTrace

//==============================================================================
/// otsGetEnvironmentVarable
/// 		declare special ots environment variable get,
///		that throws exception instead of causing crashes with null pointer.
///		Note: usually called with __ENV__(X) in CoutMacros.h
char* StringMacros::otsGetEnvironmentVarable(const char*         name,
                                             const std::string&  location,
                                             const unsigned int& line)
{
	char* environmentVariablePtr = getenv(name);
	if(!environmentVariablePtr)
	{
		__SS__ << "Environment variable '$" << name << "' not defined at " << location
		       << ":" << line << __E__;
		ss << "\n\n" << StringMacros::stackTrace() << __E__;
		__SS_ONLY_THROW__;
	}
	return environmentVariablePtr;
}  // end otsGetEnvironmentVarable()

//=========================================================================
///extract valueField for field from xml looking forwards from after
/// occurence = 0 is first occurence
std::string StringMacros::extractXmlField(const std::string& xml,
                                          const std::string& field,
                                          uint32_t           occurrence,
                                          size_t             after,
                                          size_t* returnFindPos /* = nullptr */,
                                          const std::string& valueField /* = "value=" */,
                                          const std::string& quoteType /* = "'" */)
{
	if(returnFindPos)
		*returnFindPos = std::string::npos;

	__COUTVS__(41, xml);

	size_t lo, findpos = after, hi;
	for(uint32_t i = 0; i <= occurrence; ++i)
	{
		bool anyFound = false;
		while((findpos =
		           xml.find("<" + field,  //allow for immediate closing of xml tag with >
		                    findpos)) != std::string::npos &&
		      findpos + 1 + field.size() < xml.size())
		{
			__COUTS__(40) << "find: ---- '<" << field << " findpos=" << findpos
			              << "findpos " << findpos << " " << xml[findpos] << " "
			              << xml[findpos + 1 + field.size()] << " "
			              << (int)xml[findpos + 1 + field.size()] << __E__;

			findpos +=
			    1 +
			    field
			        .size();  //to point to closing white space and advance for next forward search

			//verify white space after the field
			if((quoteType == ">" && xml[findpos] == '>') || xml[findpos] == ' ' ||
			   xml[findpos] == '\n' || xml[findpos] == '\t')
			{
				anyFound = true;  //flag
				break;
			}
		}

		if(!anyFound)
		{
			__COUTS__(40) << "Field '" << field << "' not found" << __E__;
			return "";
		}
	}

	lo = xml.find(valueField + quoteType, findpos) + valueField.size() + quoteType.size();

	if(TTEST(40) && quoteType.size())
	{
		__COUTS__(40) << "Neighbors of field '" << field << "' and value '" << valueField
		              << "' w/quote = " << quoteType << __E__;
		for(size_t i = lo - valueField.size(); i < lo + 10 && i < xml.size(); ++i)
			__COUTS__(40) << "xml[" << i << "] " << xml[i] << " vs " << quoteType << " ? "
			              << (int)xml[i] << " vs " << (int)quoteType[0] << __E__;
	}

	if((hi = xml.find(
	        quoteType == ">" ? "<" : quoteType,  //if xml tag, change closing direction
	        lo)) == std::string::npos)
	{
		__COUTS__(40) << "Value closing not found" << __E__;
		return "";
	}

	if(returnFindPos)
		*returnFindPos = findpos - (1 + field.size());  //remove offset that was added

	__COUTS__(40) << "after: " << after << ", findpos: " << findpos << ", hi/lo: " << hi
	              << "/" << lo << ", size: " << xml.size() << __E__;
	__COUTVS__(40, xml.substr(lo, hi - lo));
	return xml.substr(lo, hi - lo);
}  //end extractXmlField()

//=========================================================================
///extract valueField for field from xml looking backwards from before
/// occurence = 0 is first occurence
std::string StringMacros::rextractXmlField(const std::string& xml,
                                           const std::string& field,
                                           uint32_t           occurrence,
                                           size_t             before,
                                           size_t* returnFindPos /* = nullptr */,
                                           const std::string& valueField /* = "value=" */,
                                           const std::string& quoteType /* = "'" */)
{
	if(returnFindPos)
		*returnFindPos = std::string::npos;

	__COUTVS__(41, xml);

	size_t lo = 0, hi, findpos = before;
	for(uint32_t i = 0; i <= occurrence; ++i)
	{
		bool anyFound = false;
		while((findpos =
		           xml.rfind("<" + field,  //allow for immediate closing of xml tag with >
		                     findpos)) != std::string::npos &&
		      findpos + 1 + field.size() < xml.size())
		{
			__COUTS__(40) << "rfind: ---- '<" << field << " findpos=" << findpos << " "
			              << xml[findpos] << " " << xml[findpos + 1 + field.size()] << " "
			              << (int)xml[findpos + 1 + field.size()] << __E__;

			findpos += 1 + field.size();

			//verify white space after the field
			if((quoteType == ">" && xml[findpos] == '>') || xml[findpos] == ' ' ||
			   xml[findpos] == '\n' || xml[findpos] == '\t')
			{
				anyFound = true;  //flag
				break;
			}
			else
				findpos -= 1 + field.size() + 1;  //for next reverse search
		}
		if(!anyFound)
		{
			__COUTS__(40) << "Field '" << field << "' not found" << __E__;
			return "";
		}
	}

	lo = xml.find(valueField + quoteType, findpos) + valueField.size() + quoteType.size();

	if(TTEST(40) && quoteType.size())
	{
		__COUTS__(40) << "Neighbors?" << __E__;
		for(size_t i = findpos; i < lo + 10 && i < xml.size(); ++i)
			__COUTS__(40) << "xml[" << i << "] " << xml[i] << " vs " << quoteType << " ? "
			              << (int)xml[i] << " vs " << (int)quoteType[0] << __E__;
	}

	if((hi = xml.find(
	        quoteType == ">" ? "<" : quoteType,  //if xml tag, change closing direction
	        lo)) == std::string::npos)
	{
		__COUTS__(40) << "Value closing not found" << __E__;
		return "";
	}

	if(returnFindPos)
		*returnFindPos =
		    findpos - (1 + field.size());  //return found position of "< + field"

	__COUTS__(40) << "before: " << before << ", findpos: " << findpos << ", hi/lo: " << hi
	              << "/" << lo << ", size: " << xml.size() << __E__;
	__COUTVS__(40, xml.substr(lo, hi - lo));
	return xml.substr(lo, hi - lo);
}  //end rextractXmlField()

//=========================================================================
/// Breaks up long string into multiple TRACE TLOG calls split on the delimiter
///	to avoid truncation by a single TLOG call. The lvl parameter is the offset from TLVL_DEBUG.
void StringMacros::coutSplit(const std::string&    str,
                             uint8_t               lvl /* = 0 */,
                             const std::set<char>& delimiter /* = {',', '\n', ';'} */)
{
	auto splitArr =
	    StringMacros::getVectorFromString(str, delimiter, {} /* whitespace */);
	__COUTV__(splitArr.size());
	__COUTVS__(lvl, splitArr.size());
	for(const auto& split : splitArr)
		__COUTS__(lvl) << split;
}  //end coutSplit()

#ifdef __GNUG__
#include <cxxabi.h>
#include <cstdlib>
#include <memory>

//==============================================================================
/// demangleTypeName
std::string StringMacros::demangleTypeName(const char* name)
{
	int status = -4;  // some arbitrary value to eliminate the compiler warning

	// enable c++11 by passing the flag -std=c++11 to g++
	std::unique_ptr<char, void (*)(void*)> res{
	    abi::__cxa_demangle(name, NULL, NULL, &status), std::free};

	return (status == 0) ? res.get() : name;
}  // end demangleTypeName()

#else  // does nothing if not g++
//==============================================================================
/// demangleTypeName
///
std::string StringMacros::demangleTypeName(const char* name) { return name; }
#endif
