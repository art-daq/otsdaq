#ifndef _ots_CodeEditor_h_
#define _ots_CodeEditor_h_

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include <xgi/Method.h>  //for cgicc::Cgicc
#pragma GCC diagnostic pop

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ots
{
class HttpXmlDocument;

// CodeEditor
//	This class provides the functionality for editing, saving, and building code
class CodeEditor
{
  public:
	CodeEditor();

	// request are handled here
	void xmlRequest(const std::string& option, bool readOnlyMode, cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut, const std::string& username);

  private:
	void getDirectoryContent(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut);
	void getPathContent(const std::string& basepath, const std::string& path, HttpXmlDocument* xmlOut);
	void getFileContent(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut);
	void saveFileContent(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut, const std::string& username);
	void build(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut, const std::string& username);

	std::string safePathString(const std::string& path);
	std::string safeExtensionString(const std::string& extension);

  public:
	static const std::string SPECIAL_TYPE_FEInterface, SPECIAL_TYPE_DataProcessor, SPECIAL_TYPE_Table, SPECIAL_TYPE_SlowControls, SPECIAL_TYPE_Tools,
	    SPECIAL_TYPE_UserData, SPECIAL_TYPE_OutputData;

	static const std::string SOURCE_BASE_PATH, USER_DATA_PATH, OTSDAQ_DATA_PATH;

	static std::map<std::string /*special type*/, std::set<std::string> /*special file paths*/> getSpecialsMap(void);

	static void readFile(const std::string& basepath, const std::string& path, std::string& contents, bool binaryRead = false);
	static void writeFile(const std::string&        basepath,
	                      const std::string&        path,
	                      const std::string&        contents,
	                      const std::string&        username,
	                      const unsigned long long& insertPos    = -1,
	                      const std::string&        insertString = "");

	const std::set<std::string> ALLOWED_FILE_EXTENSIONS_;
};

}  // namespace ots

#endif
