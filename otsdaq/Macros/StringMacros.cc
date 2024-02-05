#include "otsdaq/Macros/StringMacros.h"

#include <array>

using namespace ots;

//==============================================================================
// wildCardMatch
//	find needle in haystack
//		allow needle to have leading and/or trailing wildcard '*'
//		consider priority in matching, no matter the order in the haystack:
//			- 0: no match!
//			- 1: highest priority is exact match
//			- 2: next highest is partial TRAILING-wildcard match
//			- 3: next highest is partial LEADING-wildcard match
//			- 4: lowest priority is partial full-wildcard match
//		return priority found by reference
bool StringMacros::wildCardMatch(const std::string& needle, const std::string& haystack, unsigned int* priorityIndex)
try
{
	//	__COUT__ << "\t\t wildCardMatch: " << needle <<
	//			" =in= " << haystack << " ??? " <<
	//			std::endl;

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

	// trailing wildcard
	if(needle[needle.size() - 1] == '*' && needle.substr(0, needle.size() - 1) == haystack.substr(0, needle.size() - 1))
	{
		if(priorityIndex)
			*priorityIndex = 2;  // trailing wildcard match
		return true;
	}

	// leading wildcard
	if(needle[0] == '*' && needle.substr(1) == haystack.substr(haystack.size() - (needle.size() - 1)))
	{
		if(priorityIndex)
			*priorityIndex = 3;  // leading wildcard match
		return true;
	}

	// leading wildcard and trailing wildcard
	if(needle[0] == '*' && needle[needle.size() - 1] == '*' && std::string::npos != haystack.find(needle.substr(1, needle.size() - 2)))
	{
		if(priorityIndex)
			*priorityIndex = 4;  // leading and trailing wildcard match
		return true;
	}

	// else no match
	if(priorityIndex)
		*priorityIndex = 0;  // no match
	return false;
}
catch(...)
{
	if(priorityIndex)
		*priorityIndex = 0;  // no match
	return false;            // if out of range
}

//==============================================================================
// inWildCardSet ~
//	returns true if needle is in haystack (considering wildcards)
bool StringMacros::inWildCardSet(const std::string& needle, const std::set<std::string>& haystack)
{
	for(const auto& haystackString : haystack)
		// use wildcard match, flip needle parameter.. because we want haystack to have
		// the wildcards
		if(StringMacros::wildCardMatch(haystackString, needle))
			return true;
	return false;
}

//==============================================================================
// decodeURIComponent
//	converts all %## to the ascii character
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
			sprintf(encodeStr, "%%%2.2X", c);
			retStr += encodeStr;
		}
	return retStr;
}  // end encodeURIComponent()

//==============================================================================
// StringMacros::escapeString
//	convert quotes to html quote characters &apos; = ' and &quot; = "
//	remove new line characters
//	and (if !allowWhiteSpace) remove white space (so that read from file white space
// artifact removed)
//
//	convert &amp; = &
//	if(allowWhiteSpace) convert \t to 8 &#160; spaces and \n to <br>
std::string StringMacros::escapeString(std::string inString, bool allowWhiteSpace /* = false */)
{
	bool doit = false;

	unsigned int ws = -1;
	char         htmlTmp[10];

	for(unsigned int i = 0; i < inString.length(); i++)
		if(inString[i] != ' ')
		{
			if(doit)
				__COUT__ << inString[i] << ":" << (int)inString[i] << ":" << inString << std::endl;

			// remove new lines and unprintable characters
			if(inString[i] == '\r' || inString[i] == '\n' ||          // remove new line chars
			   inString[i] == '\t' ||                                 // remove tabs
			   inString[i] < 32 ||                                    // remove un-printable characters (they mess up xml
			                                                          // interpretation)
			   (inString[i] > char(126) && inString[i] < char(161)))  // this is aggravated by the bug in
			                                                          // MFextensions (though Eric says he fixed on
			                                                          // 8/24/2016)  Note: greater than 255 should be
			                                                          // impossible if by byte (but there are html
			                                                          // chracters in 300s and 8000s)
			{
				if(  // maintain new lines and tabs
				    inString[i] == '\n')
				{
					if(allowWhiteSpace)
					{
						sprintf(htmlTmp, "&#%3.3d", inString[i]);
						inString.insert(i, std::string(htmlTmp));  // insert html str sequence
						inString.replace(i + 5, 1, 1, ';');        // replace special character with ;
						i += 6;                                    // skip to next char to check
						--i;
					}
					else  // translate to ' '
						inString[i] = ' ';
				}
				else if(  // maintain new lines and tabs
				    inString[i] == '\t')
				{
					if(allowWhiteSpace)
					{
						if(0)
						{
							// tab = 8 spaces
							sprintf(htmlTmp, "&#160;&#160;&#160;&#160;&#160;&#160;&#160;&#160");
							inString.insert(i, std::string(htmlTmp));  // insert html str sequence
							inString.replace(i + 47, 1, 1, ';');       // replace special character with ;
							i += 48;                                   // skip to next char to check
							--i;
						}
						else  // tab =  0x09
						{
							sprintf(htmlTmp, "&#009");
							inString.insert(i, std::string(htmlTmp));  // insert html str sequence
							inString.replace(i + 5, 1, 1, ';');        // replace special character with ;
							i += 6;                                    // skip to next char to check
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
				if(doit)
					__COUT__ << inString << std::endl;
				continue;
			}

			if(doit)
				__COUT__ << inString << std::endl;

			// replace special characters
			if(inString[i] == '\"' || inString[i] == '\'')
			{
				inString.insert(i,
				                (inString[i] == '\'') ? "&apos" : "&quot");  // insert HTML name before quotes
				inString.replace(i + 5, 1, 1, ';');                          // replace special character with ;
				i += 5;                                                      // skip to next char to check
				                                                             //__COUT__ <<  inString << std::endl;
			}
			else if(inString[i] == '&')
			{
				inString.insert(i, "&amp");          // insert HTML name before special character
				inString.replace(i + 4, 1, 1, ';');  // replace special character with ;
				i += 4;                              // skip to next char to check
			}
			else if(inString[i] == '<' || inString[i] == '>')
			{
				inString.insert(i,
				                (inString[i] == '<') ? "&lt" : "&gt");  // insert HTML name before special character
				inString.replace(i + 3, 1, 1, ';');                     // replace special character with ;
				i += 3;                                                 // skip to next char to check
			}
			else if(inString[i] >= char(161) && inString[i] <= char(255))  // printable special characters
			{
				sprintf(htmlTmp, "&#%3.3d", inString[i]);
				inString.insert(i, std::string(htmlTmp));  // insert html number sequence
				inString.replace(i + 5, 1, 1, ';');        // replace special character with ;
				i += 5;                                    // skip to next char to check
			}

			if(doit)
				__COUT__ << inString << std::endl;

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

	if(doit)
		__COUT__ << inString.size() << " " << ws << std::endl;

	// inString.substr(0,ws+1);

	if(doit)
		__COUT__ << inString.size() << " " << inString << std::endl;

	if(allowWhiteSpace)  // keep all white space
		return inString;
	// else trim trailing white space

	if(ws == (unsigned int)-1)
		return "";                      // empty std::string since all white space
	return inString.substr(0, ws + 1);  // trim right white space
}  // end escapeString()

//==============================================================================
// convertEnvironmentVariables ~
//	static recursive function
//
//	allows environment variables entered as $NAME or ${NAME}
std::string StringMacros::convertEnvironmentVariables(const std::string& data)
{
	size_t begin = data.find("$");
	if(begin != std::string::npos)
	{
		size_t      end;
		std::string envVariable;
		std::string converted = data;  // make copy to modify

		if(data[begin + 1] == '{')  // check if using ${NAME} syntax
		{
			end         = data.find("}", begin + 2);
			envVariable = data.substr(begin + 2, end - begin - 2);
			++end;  // replace the closing } too!
		}
		else  // else using $NAME syntax
		{
			// end is first non environment variable character
			for(end = begin + 1; end < data.size(); ++end)
				if(!((data[end] >= '0' && data[end] <= '9') || (data[end] >= 'A' && data[end] <= 'Z') || (data[end] >= 'a' && data[end] <= 'z') ||
				     data[end] == '-' || data[end] == '_' || data[end] == '.' || data[end] == ':'))
					break;  // found end
			envVariable = data.substr(begin + 1, end - begin - 1);
		}
		//__COUTV__(data);
		//__COUTV__(envVariable);
		char* envResult = __ENV__(envVariable.c_str());

		if(envResult)
		{
			// proceed recursively
			return convertEnvironmentVariables(converted.replace(begin, end - begin, envResult));
		}
		else
		{
			__SS__ << ("The environmental variable '" + envVariable + "' is not set! Please make sure you set it before continuing!") << std::endl;
			__SS_THROW__;
		}
	}
	// else no environment variables found in string
	//__COUT__ << "Result: " << data << __E__;
	return data;
}

//==============================================================================
// isNumber ~~
//	returns true if one or many numbers separated by operations (+,-,/,*) is
//		present in the string.
//	Numbers can be hex ("0x.."), binary("b..."), or base10.
bool StringMacros::isNumber(const std::string& s)
{
	// extract set of potential numbers and operators
	std::vector<std::string> numbers;
	std::vector<char>        ops;

	if(!s.size())
		return false;

	StringMacros::getVectorFromString(s,
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
				if(!((number[i] >= '0' && number[i] <= '9') || (number[i] >= 'A' && number[i] <= 'F') || (number[i] >= 'a' && number[i] <= 'f')))
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
				if(!((number[i] >= '0' && number[i] <= '9') || number[i] == '.' || number[i] == '+' || number[i] == '-'))
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
// getNumberType ~~
//	returns string of number type: "unsigned long long", "double"
//	or else "nan" for not-a-number
//
//	Numbers can be hex ("0x.."), binary("b..."), or base10.
std::string StringMacros::getNumberType(const std::string& s)
{
	// extract set of potential numbers and operators
	std::vector<std::string> numbers;
	std::vector<char>        ops;

	bool hasDecimal = false;

	StringMacros::getVectorFromString(s,
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
				if(!((number[i] >= '0' && number[i] <= '9') || (number[i] >= 'A' && number[i] <= 'F') || (number[i] >= 'a' && number[i] <= 'f')))
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
				if(!((number[i] >= '0' && number[i] <= '9') || number[i] == '.' || number[i] == '+' || number[i] == '-'))
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
//	for bool, but not all other number types
//	return false if string is not a bool
// template<>
// inline bool StringMacros::getNumber<bool>(const std::string& s, bool& retValue)
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
// getTimestampString ~~
//	returns ots style timestamp string
//	of known fixed size: Thu Aug 23 14:55:02 2001 CST
std::string StringMacros::getTimestampString(const std::string& linuxTimeInSeconds)
{
	time_t timestamp(strtol(linuxTimeInSeconds.c_str(), 0, 10));
	return getTimestampString(timestamp);
}  // end getTimestampString()

//==============================================================================
// getTimestampString ~~
//	returns ots style timestamp string
//	of known fixed size: Thu Aug 23 14:55:02 2001 CST
std::string StringMacros::getTimestampString(const time_t& linuxTimeInSeconds)
{
	std::string retValue(30, '\0');  // known fixed size: Thu Aug 23 14:55:02 2001 CST

	struct tm tmstruct;
	::localtime_r(&linuxTimeInSeconds, &tmstruct);
	::strftime(&retValue[0], 30, "%c %Z", &tmstruct);
	retValue.resize(strlen(retValue.c_str()));

	return retValue;
}  // end getTimestampString()

//==============================================================================
// validateValueForDefaultStringDataType
//
std::string StringMacros::validateValueForDefaultStringDataType(const std::string& value, bool doConvertEnvironmentVariables)
try
{
	return doConvertEnvironmentVariables ? StringMacros::convertEnvironmentVariables(value) : value;
}
catch(const std::runtime_error& e)
{
	__SS__ << "Failed to validate value for default string data type. " << __E__ << e.what() << __E__;
	__SS_THROW__;
}

//==============================================================================
// getSetFromString
//	extracts the set of elements from string that uses a delimiter
//		ignoring whitespace
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
		if((whitespace.find(inputString[j]) != whitespace.end() ||  // ignore leading white space or delimiter
		    delimiter.find(inputString[j]) != delimiter.end()) &&
		   i == j)
			++i;
		else if((whitespace.find(inputString[j]) != whitespace.end() ||  // trailing white space or delimiter indicates end
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
// getVectorFromString
//	extracts the list of elements from string that uses a delimiter
//		ignoring whitespace
//	optionally returns the list of delimiters encountered, which may be useful
//		for extracting which operator was used.
//
//
//	Note: lists are returned as vectors
//	Note: the size() of delimiters will be one less than the size() of the returned values
//		unless there is a leading delimiter, in which case vectors will have the same
// size.
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

		if(whitespace.find(inputString[c]) != whitespace.end()  // ignore leading white space
		   && i == j)
		{
			++i;
			++j;
			// if(isDelimiter)
			//	foundLeadingDelimiter = true;
		}
		else if(whitespace.find(inputString[c]) != whitespace.end() && i != j)  // trailing white space, assume possible end of element
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
			listToReturn.push_back(decodeURIComponents ? StringMacros::decodeURIComponent(inputString.substr(i, j - i)) : inputString.substr(i, j - i));

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
		listToReturn.push_back(decodeURIComponents ? StringMacros::decodeURIComponent(inputString.substr(i, j - i)) : inputString.substr(i, j - i));
	}

	// assert that there is one less delimiter than values
	if(listOfDelimiters && listToReturn.size() - 1 != listOfDelimiters->size() && listToReturn.size() != listOfDelimiters->size())
	{
		__SS__ << "There is a mismatch in delimiters to entries (should be equal or one "
		          "less delimiter): "
		       << listOfDelimiters->size() << " vs " << listToReturn.size() << __E__ << "Entries: " << StringMacros::vectorToString(listToReturn) << __E__
		       << "Delimiters: " << StringMacros::vectorToString(*listOfDelimiters) << __E__;
		__SS_THROW__;
	}

}  // end getVectorFromString()

//==============================================================================
// getVectorFromString
//	extracts the list of elements from string that uses a delimiter
//		ignoring whitespace
//	optionally returns the list of delimiters encountered, which may be useful
//		for extracting which operator was used.
//
//
//	Note: lists are returned as vectors
//	Note: the size() of delimiters will be one less than the size() of the returned values
//		unless there is a leading delimiter, in which case vectors will have the same
// size.
std::vector<std::string> StringMacros::getVectorFromString(const std::string&    inputString,
                                                           const std::set<char>& delimiter,
                                                           const std::set<char>& whitespace,
                                                           std::vector<char>*    listOfDelimiters,
                                                           bool                  decodeURIComponents)
{
	std::vector<std::string> listToReturn;

	StringMacros::getVectorFromString(inputString, listToReturn, delimiter, whitespace, listOfDelimiters, decodeURIComponents);
	return listToReturn;
}  // end getVectorFromString()

//==============================================================================
// getMapFromString
//	extracts the map of name-value pairs from string that uses two delimiters
//		ignoring whitespace
void StringMacros::getMapFromString(const std::string&                  inputString,
                                    std::map<std::string, std::string>& mapToReturn,
                                    const std::set<char>&               pairPairDelimiter,
                                    const std::set<char>&               nameValueDelimiter,
                                    const std::set<char>&               whitespace)
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
			if((whitespace.find(inputString[j]) != whitespace.end() ||  // ignore leading white space or delimiter
			    pairPairDelimiter.find(inputString[j]) != pairPairDelimiter.end()) &&
			   i == j)
				++i;
			else if((whitespace.find(inputString[j]) != whitespace.end() ||  // trailing white space or delimiter indicates end
			         nameValueDelimiter.find(inputString[j]) != nameValueDelimiter.end()) &&
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
			if((whitespace.find(inputString[j]) != whitespace.end() ||  // ignore leading white space or delimiter
			    nameValueDelimiter.find(inputString[j]) != nameValueDelimiter.end()) &&
			   i == j)
				++i;
			else if(whitespace.find(inputString[j]) != whitespace.end() ||              // trailing white space or delimiter indicates end
			        pairPairDelimiter.find(inputString[j]) != pairPairDelimiter.end())  // &&
			                                                                            //  i != j)  // assume end of value name
			{
				//__COUT__ << "Map value found: " <<
				//		inputString.substr(i,j-i) << std::endl;

				auto /*pair<it,success>*/ emplaceReturn =
				    mapToReturn.emplace(std::pair<std::string, std::string>(name, validateValueForDefaultStringDataType(inputString.substr(i, j - i))  // value
				                                                            ));

				if(!emplaceReturn.second)
				{
					__COUT__ << "Ignoring repetitive value ('" << inputString.substr(i, j - i) << "') and keeping current value ('"
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
		    mapToReturn.emplace(std::pair<std::string, std::string>(name, validateValueForDefaultStringDataType(inputString.substr(i, j - i))  // value
		                                                            ));

		if(!emplaceReturn.second)
		{
			__COUT__ << "Ignoring repetitive value ('" << inputString.substr(i, j - i) << "') and keeping current value ('" << emplaceReturn.first->second
			         << "'). " << __E__;
		}
	}
}  // end getMapFromString()
catch(const std::runtime_error& e)
{
	__SS__ << "Error while extracting a map from the string '" << inputString << "'... is it a valid map?" << __E__ << e.what() << __E__;
	__SS_THROW__;
}

//==============================================================================
// mapToString
std::string StringMacros::mapToString(const std::map<std::string, uint8_t>& mapToReturn,
                                      const std::string&                    primaryDelimeter,
                                      const std::string&                    secondaryDelimeter)
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
// setToString
std::string StringMacros::setToString(const std::set<uint8_t>& setToReturn, const std::string& delimeter)
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
// vectorToString
std::string StringMacros::vectorToString(const std::vector<uint8_t>& setToReturn, const std::string& delimeter)
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
}  // end vectorToString()

//==============================================================================
// extractCommonChunks
//	return the common chunks from the vector of strings
//		e.g. if the strings were created from a template
//	string like reader*_east*, this function will return
//	a vector of size 3 := {"reader","_east",""} and
//	a vector of wildcards that would replace the *
//
//	Returns true if common chunks and wildcards found,
//	returns false if all inputs were the same (i.e. no wildcards needed)
bool StringMacros::extractCommonChunks(const std::vector<std::string>& haystack,
                                       std::vector<std::string>&       commonChunksToReturn,
                                       std::vector<std::string>&       wildcardStringsToReturn,
                                       unsigned int&                   fixedWildcardLength)
{
	fixedWildcardLength = 0;  // default

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
	std::pair<unsigned int /*lo*/, unsigned int /*hi*/> wildcardBounds(std::make_pair(-1, 0));  // initialize to illegal wildcard

	// look for starting matching segment
	for(unsigned int n = 1; n < haystack.size(); ++n)
		for(unsigned int i = 0, j = 0; i < haystack[0].length() && j < haystack[n].length(); ++i, ++j)
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
	// __COUT__ << "Low side = " << wildcardBounds.first << " " << haystack[0].substr(0, wildcardBounds.first) << __E__;

	// look for end matching segment
	for(unsigned int n = 1; n < haystack.size(); ++n)
		for(int i = haystack[0].length() - 1, j = haystack[n].length() - 1; i >= (int)wildcardBounds.first && j >= (int)wildcardBounds.first; --i, --j)
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

	// __COUT__ << "High side = " << wildcardBounds.second << " " << haystack[0].substr(wildcardBounds.second) << __E__;

	// add first common chunk
	commonChunksToReturn.push_back(haystack[0].substr(0, wildcardBounds.first));

	if(wildcardBounds.first != (unsigned int)-1)  // potentially more chunks if not end
	{
		//  - use start and end to determine if there is more than one *
		for(int i = (wildcardBounds.first + wildcardBounds.second) / 2 + 1; i < (int)wildcardBounds.second; ++i)
			if(haystack[0][wildcardBounds.first] == haystack[0][i] &&
			   haystack[0].substr(wildcardBounds.first, wildcardBounds.second - i) == haystack[0].substr(i, wildcardBounds.second - i))
			{
				std::string multiWildcardString = haystack[0].substr(i, wildcardBounds.second - i);
				__COUT__ << "Multi-wildcard found: " << multiWildcardString << __E__;

				std::vector<unsigned int /*lo index*/> wildCardInstances;
				// add front one now, and back one later
				wildCardInstances.push_back(wildcardBounds.first);

				unsigned int offset       = wildCardInstances[0] + multiWildcardString.size() + 1;
				std::string  middleString = haystack[0].substr(offset, (i - 1) - offset);
				__COUTV__(middleString);

				// search for more wildcard instances in new common area
				size_t k;
				while((k = middleString.find(multiWildcardString)) != std::string::npos)
				{
					__COUT__ << "Multi-wildcard found at " << k << __E__;

					wildCardInstances.push_back(offset + k);

					middleString = middleString.substr(k + multiWildcardString.size() + 1);
					offset += k + multiWildcardString.size() + 1;
					__COUTV__(middleString);
				}

				// add back one last
				wildCardInstances.push_back(i);

				for(unsigned int w = 0; w < wildCardInstances.size() - 1; ++w)
				{
					commonChunksToReturn.push_back(haystack[0].substr(wildCardInstances[w] + wildCardInstances.size(),
					                                                  wildCardInstances[w + 1] - (wildCardInstances[w] + wildCardInstances.size())));
				}
			}

		// check if all common chunks end in 0 to add fixed length

		for(unsigned int i = 0; i < commonChunksToReturn[0].size(); ++i)
			if(commonChunksToReturn[0][commonChunksToReturn[0].size() - 1 - i] == '0')
				++fixedWildcardLength;
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
				__SS__ << "Invalid fixed length found, please simplify indexing between these common chunks: "
				       << StringMacros::vectorToString(commonChunksToReturn) << __E__;
				__SS_THROW__;
			}
		}
		// __COUTV__(fixedWildcardLength);

		if(fixedWildcardLength)  // take trailing 0s out of common chunks
			for(unsigned int c = 0; c < commonChunksToReturn.size(); ++c)
				commonChunksToReturn[c] = commonChunksToReturn[c].substr(0, commonChunksToReturn[c].size() - fixedWildcardLength);

		// add last common chunk
		commonChunksToReturn.push_back(haystack[0].substr(wildcardBounds.second));
	}  // end handling more chunks

	// now determine wildcard strings
	size_t       k;
	unsigned int i;
	unsigned int ioff            = fixedWildcardLength;
	bool         wildcardsNeeded = false;

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
					// __COUTV__(i);
					// __COUTV__(k);
					// __COUTV__(k - i);

					wildcard = haystack[n].substr(i, k - i);
					if(fixedWildcardLength && n == 0)
						fixedWildcardLength += wildcard.size();

					// __COUT__ << "name[" << n << "] = " << wildcard << " fixed @ " << fixedWildcardLength << __E__;

					break;
				}
				else if(0 /*skip validation in favor of speed*/ && wildcard != haystack[n].substr(i, k - i))
				{
					__SS__ << "Invalid wildcard! for name[" << n << "] = " << haystack[n]
					       << " - the extraction algorithm is confused, please simplify your naming convention." << __E__;
					__SS_THROW__;
				}

				i = k;
			}  // end commonChunksToReturn loop

		if(wildcard.size())
			wildcardsNeeded = true;
		wildcardStringsToReturn.push_back(wildcard);

	}  // end name loop

	// __COUTV__(StringMacros::vectorToString(commonChunksToReturn));
	// __COUTV__(StringMacros::vectorToString(wildcardStringsToReturn));

	if(wildcardStringsToReturn.size() != haystack.size())
	{
		__SS__ << "There was a problem during common chunk extraction!" << __E__;
		__SS_THROW__;
	}

	return wildcardsNeeded;

}  // end extractCommonChunks()

//==============================================================================
// IgnoreCaseCompareStruct operator used to order
//	std::set, etc ignoring letter case
// e.g. used here: void ConfigurationGUISupervisor::handleTablesXML
bool StringMacros::IgnoreCaseCompareStruct::operator()(const std::string& lhs, const std::string& rhs) const
{
	//__COUTV__(lhs);
	//__COUTV__(rhs);
	// return true if lhs < rhs (lhs will be ordered first)

	for(unsigned int i = 0; i < lhs.size() && i < rhs.size(); ++i)
	{
		//__COUT__ << i << "\t" << lhs[i] << "\t" << rhs[i] << __E__;
		if((lhs[i] >= 'A' && lhs[i] <= 'Z' && rhs[i] >= 'A' && rhs[i] <= 'Z') || (lhs[i] >= 'a' && lhs[i] <= 'z' && rhs[i] >= 'a' && rhs[i] <= 'z'))
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
// exec
//	run linux command and get result back in string
std::string StringMacros::exec(const char* cmd)
{
	__COUTV__(cmd);

	std::array<char, 128> buffer;
	std::string           result;
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if(!pipe)
		__THROW__("popen() failed!");
	while(!feof(pipe.get()))
	{
		if(fgets(buffer.data(), 128, pipe.get()) != nullptr)
			result += buffer.data();
	}
	//__COUTV__(result);
	return result;
}  // end exec()

//==============================================================================
// stackTrace
//	static function
//	https://gist.github.com/fmela/591333/c64f4eb86037bb237862a8283df70cdfc25f01d3
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
				ss << "[" << i << "] " << messages[i] << " : " << real_name << "+" << offset_begin << offset_end << std::endl;
			}
			// otherwise, output the mangled function name
			else
			{
				ss << "[" << i << "] " << messages[i] << " : " << mangled_name << "+" << offset_begin << offset_end << std::endl;
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

	free(messages);

	// call ROOT's stack trace to get line numbers of ALL threads
	// gSystem->StackTrace();

	return ss.str();
}  // end stackTrace

//==============================================================================
// otsGetEnvironmentVarable
// 		declare special ots environment variable get,
//		that throws exception instead of causing crashes with null pointer.
//		Note: usually called with __ENV__(X) in CoutMacros.h
char* StringMacros::otsGetEnvironmentVarable(const char* name, const std::string& location, const unsigned int& line)
{
	char* environmentVariablePtr = getenv(name);
	if(!environmentVariablePtr)
	{
		__SS__ << "Environment variable '" << name << "' not defined at " << location << "[" << line << "]" << __E__;
		ss << "\n\n" << StringMacros::stackTrace() << __E__;
		__SS_ONLY_THROW__;
	}
	return environmentVariablePtr;
}  // end otsGetEnvironmentVarable()

#ifdef __GNUG__
#include <cxxabi.h>
#include <cstdlib>
#include <memory>

//==============================================================================
// demangleTypeName
std::string StringMacros::demangleTypeName(const char* name)
{
	int status = -4;  // some arbitrary value to eliminate the compiler warning

	// enable c++11 by passing the flag -std=c++11 to g++
	std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, NULL, NULL, &status), std::free};

	return (status == 0) ? res.get() : name;
}  // end demangleTypeName()

#else  // does nothing if not g++
//==============================================================================
// demangleTypeName
//
std::string StringMacros::demangleTypeName(const char* name) { return name; }
#endif
