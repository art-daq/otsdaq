#include "otsdaq/CodeEditor/CodeEditor.h"
#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include <dirent.h>    //DIR and dirent
#include <sys/stat.h>  //for mkdir
#include <cctype>      //for std::toupper
#include <thread>      //for std::thread

using namespace ots;

#define CODE_EDITOR_DATA_PATH std::string(__ENV__("SERVICE_DATA_PATH")) + "/" + "CodeEditorData"

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "CodeEditor"

const std::string CodeEditor::SPECIAL_TYPE_FEInterface   = "FEInterface";
const std::string CodeEditor::SPECIAL_TYPE_DataProcessor = "DataProcessor";
const std::string CodeEditor::SPECIAL_TYPE_Table         = "Table";
const std::string CodeEditor::SPECIAL_TYPE_SlowControls  = "SlowControls";
const std::string CodeEditor::SPECIAL_TYPE_Tools         = "Tools";
const std::string CodeEditor::SPECIAL_TYPE_UserData      = "UserData";
const std::string CodeEditor::SPECIAL_TYPE_OutputData    = "OutputData";

const std::string CodeEditor::SOURCE_BASE_PATH = std::string(__ENV__("OTS_SOURCE")) + "/";
const std::string CodeEditor::USER_DATA_PATH   = std::string(__ENV__("USER_DATA")) + "/";
const std::string CodeEditor::OTSDAQ_DATA_PATH = std::string(__ENV__("OTSDAQ_DATA")) + "/";

//==============================================================================
// CodeEditor
CodeEditor::CodeEditor()
    : ALLOWED_FILE_EXTENSIONS_(
          {"h", "hh", "hpp", "hxx", "c", "cc", "cpp", "cxx", "icc", "dat", "txt", "sh", "css", "html", "htm", "js", "py", "fcl", "xml", "xsd", "cfg"})
{
	std::string path = CODE_EDITOR_DATA_PATH;
	DIR*        dir  = opendir(path.c_str());
	if(dir)
		closedir(dir);
	else if(-1 == mkdir(path.c_str(), 0755))
	{
		// lets create the service folder (for first time)
		__SS__ << "Service directory creation failed: " << path << std::endl;
		__SS_THROW__;
	}

}  // end CodeEditor()

//==============================================================================
// xmlRequest
//	all requests are handled here
void CodeEditor::xmlRequest(const std::string& option, bool readOnlyMode, cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut, const std::string& username)
try
{
	__COUTV__(option);

	// request options:
	//
	//	getDirectoryContent
	//	getFileContent
	//	saveFileContent
	//	cleanBuild
	//	incrementalBuild
	//	getAllowedExtensions
	//	getFileGitURL
	//

	if(option == "getDirectoryContent")
	{
		getDirectoryContent(cgiIn, xmlOut);
	}
	else if(option == "getFileContent")
	{
		getFileContent(cgiIn, xmlOut);
	}
	else if(!readOnlyMode && option == "saveFileContent")
	{
		saveFileContent(cgiIn, xmlOut, username);
	}
	else if(!readOnlyMode && option == "build")
	{
		build(cgiIn, xmlOut, username);
	}
	else if(option == "getAllowedExtensions")
	{
		xmlOut->addTextElementToData("AllowedExtensions", StringMacros::setToString(ALLOWED_FILE_EXTENSIONS_, ","));
	}
	else if(option == "getFileGitURL")
	{
		getFileGitURL(cgiIn, xmlOut);
	}
	else
	{
		__SS__ << "Unrecognized request option '" << option << ".'" << __E__;
		__SS_THROW__;
	}
}
catch(const std::runtime_error& e)
{
	__SS__ << "Error encountered while handling the Code Editor request option '" << option << "': " << e.what() << __E__;
	xmlOut->addTextElementToData("Error", ss.str());
}
catch(...)
{
	__SS__ << "Unknown error encountered while handling the Code Editor request option '" << option << "!'" << __E__;
	xmlOut->addTextElementToData("Error", ss.str());
}  // end xmlRequest()

//==============================================================================
// safePathString
std::string CodeEditor::safePathString(const std::string& path)
{
	__COUTVS__(20,path);
	// remove all non ascii and non /, -, _,, space
	std::string fullpath = "";
	for(unsigned int i = 0; i < path.length(); ++i)
		if((path[i] >= 'a' && path[i] <= 'z') || (path[i] >= 'A' && path[i] <= 'Z') || path[i] >= '_' || path[i] >= '-' || path[i] >= ' ' || path[i] >= '/')
			fullpath += path[i];
	__COUTVS__(20,fullpath);
	if(!fullpath.length())
	{
		__SS__ << "Invalid path '" << fullpath << "' found!" << __E__;
		__SS_THROW__;
	}
	return fullpath;
}  // end safePathString()

//==============================================================================
// safeExtensionString
//	remove all non ascii and make lower case
std::string CodeEditor::safeExtensionString(const std::string& extension)
{
	//__COUTV__(extension);

	std::string retExt = "";
	// remove all non ascii
	//	skip first potential '.' (depends on parent calling function if extension includes
	//'.')
	for(unsigned int i = 0; i < extension.length(); ++i)
		if((extension[i] >= 'a' && extension[i] <= 'z'))
			retExt += extension[i];
		else if((extension[i] >= 'A' && extension[i] <= 'Z'))
			retExt += extension[i] + 32;  // make lowercase
		else if(i > 0 || extension[i] != '.')
		{
			__SS__ << "Invalid extension non-alpha " << int(extension[i]) << " found!" << __E__;
			__SS_ONLY_THROW__;
		}

	//__COUTV__(retExt);

	if(ALLOWED_FILE_EXTENSIONS_.find(retExt) ==  // should match get directory content restrictions
	   ALLOWED_FILE_EXTENSIONS_.end())
	{
		__SS__ << "Invalid extension '" << retExt << "' found!" << __E__;
		__SS_ONLY_THROW__;
	}
	return retExt;
}  // end safeExtensionString()

//==============================================================================
// getDirectoryContent
void CodeEditor::getDirectoryContent(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut)
{
	std::string path = CgiDataUtilities::getData(cgiIn, "path");
	path             = safePathString(StringMacros::decodeURIComponent(path));
 	__COUTV__(path);
	__COUTV__(CodeEditor::SOURCE_BASE_PATH);

	xmlOut->addTextElementToData("path", path);

	const unsigned int numOfTypes         = 7;
	std::string        specialTypeNames[] = {"Front-End Plugins",
	                                         "Data Processor Plugins",
	                                         "Configuration Table Plugins",
	                                         "Slow Controls Interface Plugins",
	                                         "Tools and Scripts",
	                                         "$USER_DATA",
	                                         "$OTSDAQ_DATA"};
	std::string        specialTypes[]     = {SPECIAL_TYPE_FEInterface,
	                                         SPECIAL_TYPE_DataProcessor,
	                                         SPECIAL_TYPE_Table,
	                                         SPECIAL_TYPE_SlowControls,
	                                         SPECIAL_TYPE_Tools,
	                                         SPECIAL_TYPE_UserData,
	                                         SPECIAL_TYPE_OutputData};

	std::string pathMatchPrepend = "/";  // some requests come in with leading "/" and
	                                     // "//"
	if(path.length() > 1 && path[1] == '/')
		pathMatchPrepend += '/';

	for(unsigned int i = 0; i < numOfTypes; ++i)
		if(path == pathMatchPrepend + specialTypeNames[i])
		{
			__COUT__ << "Getting all " << specialTypeNames[i] << "..." << __E__;

			// handle UserData and OutputData differently
			//	since there is only one path to check
			if(specialTypes[i] == SPECIAL_TYPE_UserData)
			{
				getPathContent("/", CodeEditor::USER_DATA_PATH, xmlOut);
				return;
			}
			else if(specialTypes[i] == SPECIAL_TYPE_OutputData)
			{
				getPathContent("/", CodeEditor::OTSDAQ_DATA_PATH, xmlOut);
				return;
			}

			std::map<std::string /*special type*/, std::set<std::string> /*special file paths*/> retMap = CodeEditor::getSpecialsMap();
			if(retMap.find(specialTypes[i]) != retMap.end())
			{
				for(const auto& specialTypeFile : retMap[specialTypes[i]])
				{
					xmlOut->addTextElementToData("specialFile", specialTypeFile);
				}
			}
			else
			{
				__SS__ << "No files for type '" << specialTypeNames[i] << "' were found." << __E__;
				__SS_THROW__;
			}
			return;
		}

	// if root directory, add special directory for types
	if(path == "/")
		for(unsigned int i = 0; i < numOfTypes; ++i)
			xmlOut->addTextElementToData("special", specialTypeNames[i]);

	std::string contents;
	size_t      i;
	if((i = path.find("$USER_DATA/")) == 0 || (i == 1 && path[0] == '/'))  // if leading / or without
		getPathContent(CodeEditor::USER_DATA_PATH, path.substr(std::string("/$USER_DATA/").size()), xmlOut);
	else if((i = path.find("$OTSDAQ_DATA/")) == 0 || (i == 1 && path[0] == '/'))  // if leading / or without
		getPathContent(CodeEditor::OTSDAQ_DATA_PATH, path.substr(std::string("/$OTSDAQ_DATA/").size()), xmlOut);
	else
		getPathContent(CodeEditor::SOURCE_BASE_PATH, path, xmlOut);

}  // end getDirectoryContent()

//==============================================================================
// getPathContent
void CodeEditor::getPathContent(const std::string& basepath, const std::string& path, HttpXmlDocument* xmlOut)
{
	DIR*           pDIR;
	struct dirent* entry;
	bool           isDir;
	std::string    name;
	int            type;

	if(!(pDIR = opendir((basepath + path).c_str())))
	{
		__SS__ << "Path '" << path << "' could not be opened!" << __E__;
		__SS_THROW__;
	}

	// add to set for alpha ordering
	//	with insensitive compare
	struct InsensitiveCompare
	{
		bool operator()(const std::string& as, const std::string& bs) const
		{
			// return true, if as < bs
			const char* a = as.c_str();
			const char* b = bs.c_str();
			int         d;

			// compare each character, until difference, or end of string
			while((d = (std::toupper(*a) - std::toupper(*b))) == 0 && *a)
				++a, ++b;

			//__COUT__ << as << " vs " << bs << " = " << d << " " << (d<0) << __E__;

			return d < 0;
		}
	};
	std::set<std::string, InsensitiveCompare> orderedDirectories;
	std::set<std::string, InsensitiveCompare> orderedFiles;

	std::string extension;

	// else directory good, get all folders, .h, .cc, .txt files
	while((entry = readdir(pDIR)))
	{
		name = std::string(entry->d_name);
		type = int(entry->d_type);

		//__COUT__ << type << " " << name << "\n" << std::endl;

		if(name[0] != '.' && (type == 0 ||  // 0 == UNKNOWN (which can happen - seen in SL7 VM)
		                      type == 4 ||  // directory type
		                      type == 8 ||  // file type
		                      type == 10    // 10 == link (could be directory or file, treat as unknown)
		                      ))
		{
			isDir = false;

			if(type == 0 || type == 10)
			{
				// unknown type .. determine if directory
				DIR* pTmpDIR = opendir((basepath + path + "/" + name).c_str());
				if(pTmpDIR)
				{
					isDir = true;
					closedir(pTmpDIR);
				}
				// else //assume file
				//	__COUT__ << "Unable to open path as directory: " <<
				//		(basepath + path + "/" + name) << __E__;
			}

			if(type == 4)
				isDir = true;  // flag directory types

			// handle directories

			if(isDir)
			{
				//__COUT__ << "Directory: " << type << " " << name << __E__;

				orderedDirectories.emplace(name);
				// xmlOut->addTextElementToData("directory",name);
			}
			else  // type 8 or 0 is file
			{
				//__COUT__ << "File: " << type << " " << name << "\n" << std::endl;

				try
				{
					if(name != "ots")
						safeExtensionString(name.substr(name.rfind('.')));
					//__COUT__ << "EditFile: " << type << " " << name << __E__;

					orderedFiles.emplace(name);
				}
				catch(...)
				{
					__COUTT__ << "Invalid file extension, skipping '" << name << "' ..." << __E__;
				}
			}
		}
	}  // end directory traversal

	closedir(pDIR);

	__COUT__ << "Found " << orderedDirectories.size() << " directories." << __E__;
	__COUT__ << "Found " << orderedFiles.size() << " files." << __E__;

	for(const auto& name : orderedDirectories)
		xmlOut->addTextElementToData("directory", name);
	for(const auto& name : orderedFiles)
		xmlOut->addTextElementToData("file", name);
}  // end getPathContent()

//==============================================================================
// getFileContent
void CodeEditor::getFileContent(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut)
{
	std::string path = CgiDataUtilities::getData(cgiIn, "path");
	path             = safePathString(StringMacros::decodeURIComponent(path));
	xmlOut->addTextElementToData("path", path);

	std::string extension = CgiDataUtilities::getData(cgiIn, "ext");
	if(extension == "ots")
		extension = "";  // special handling of ots extension (to get bash script
		                 // properly)
	if(!( extension == "bin" || //allow bin for read-only
		(path.length() > 4 && path.substr(path.length() - 4) == "/ots")))
		extension = safeExtensionString(extension);
	xmlOut->addTextElementToData("ext", extension);

	std::string contents;
	size_t      i;
	if((i = path.find("$USER_DATA/")) == 0 || (i == 1 && path[0] == '/'))  // if leading / or without
		CodeEditor::readFile(
		    CodeEditor::USER_DATA_PATH, path.substr(i + std::string("$USER_DATA/").size()) + (extension.size() ? "." : "") + extension, contents, extension == "bin");
	else if((i = path.find("$OTSDAQ_DATA/")) == 0 || (i == 1 && path[0] == '/'))  // if leading / or without
		CodeEditor::readFile(
		    CodeEditor::OTSDAQ_DATA_PATH, path.substr(std::string("/$OTSDAQ_DATA/").size()) + (extension.size() ? "." : "") + extension, contents, extension == "bin");
	else
		CodeEditor::readFile(CodeEditor::SOURCE_BASE_PATH, path + (extension.size() ? "." : "") + extension, contents, extension == "bin");

	xmlOut->addTextElementToData("content", contents);

}  // end getFileContent()

//==============================================================================
// getFileGitURL
void CodeEditor::getFileGitURL(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut)
{
	std::string path = CgiDataUtilities::getData(cgiIn, "path");
	path             = safePathString(StringMacros::decodeURIComponent(path));
	xmlOut->addTextElementToData("path", path);

	std::string extension = CgiDataUtilities::getData(cgiIn, "ext");
	if(extension == "ots")
		extension = "";  // special handling of ots extension (to get bash script
		                 // properly)
	if(!( extension == "bin" || //allow bin for read-only
		(path.length() > 4 && path.substr(path.length() - 4) == "/ots")))
		extension = safeExtensionString(extension);
	xmlOut->addTextElementToData("ext", extension);

	std::string gitPath;
	size_t      i;
	if((i = path.find("$USER_DATA/")) == 0 || (i == 1 && path[0] == '/'))  // if leading / or without
		gitPath = CodeEditor::getFileGitURL(
		    CodeEditor::USER_DATA_PATH, path.substr(i + std::string("$USER_DATA/").size()) + (extension.size() ? "." : "") + extension);
	else if((i = path.find("$OTSDAQ_DATA/")) == 0 || (i == 1 && path[0] == '/'))  // if leading / or without
		gitPath = CodeEditor::getFileGitURL(
		    CodeEditor::OTSDAQ_DATA_PATH, path.substr(std::string("/$OTSDAQ_DATA/").size()) + (extension.size() ? "." : "") + extension);
	else
		gitPath = CodeEditor::getFileGitURL(CodeEditor::SOURCE_BASE_PATH, path + (extension.size() ? "." : "") + extension);

	xmlOut->addTextElementToData("gitPath", gitPath);

}  // end getFileGitURL()

//==============================================================================
// getFileGitURL
std::string CodeEditor::getFileGitURL(const std::string& basepath, const std::string& path)
{
	__COUTV__(basepath);
	__COUTV__(path);
	std::string fullpath;
	if(path.find(basepath) == 0) //check if path is already complete
		fullpath = path;
	else
		fullpath = basepath + "/" + path;
	__COUTV__(fullpath);

	//look for environments

	return "unknown";
}  // end getFileGitURL()

//==============================================================================
// readFile
void CodeEditor::readFile(const std::string& basepath, const std::string& path, std::string& contents, 
	bool binaryRead /* = false*/)
{
	__COUTV__(basepath);
	__COUTV__(path);
	std::string fullpath;
	if(path.find(basepath) == 0) //check if path is already complete
		fullpath = path;
	else
		fullpath = basepath + "/" + path;
	__COUTV__(fullpath);

	std::FILE* fp = std::fopen(fullpath.c_str(), "rb");
	if(!fp)
	{
		__SS__ << "Could not open file at " << fullpath << 
			". Error: " << errno << " - " << strerror(errno) << __E__;
		__SS_THROW__;
	}

	if(binaryRead)
	{
		std::string binaryContents;
		std::fseek(fp, 0, SEEK_END);
		binaryContents.resize(std::ftell(fp));
		std::rewind(fp);
		std::fread(&binaryContents[0], 1, binaryContents.size(), fp);
		std::fclose(fp);

		for(size_t i = 0; i < binaryContents.size(); i += 8)
		{
			contents += "0x"; //no need to send line number (it is in web display already)

			for(size_t j = 0; j < 8 && 
							j < binaryContents.size() - i; ++j)
			{
				size_t jj = i + 7 - j; //go in reverse order so least significant is at right
				if(i + 8 > binaryContents.size()) //if not a multiple of 8 at end
					jj = i + (binaryContents.size() - i) - j;

				if(j == 4) contents += ' '; //one space at 32b groups
				contents += "  "; //declare the 2 bytes of space, then fill with sprintf
				sprintf(&contents[contents.length() - 2],"%2.2x",
					binaryContents[jj]);				
			} //end binary hex char loop			
			contents += '\n';
		} //end binary line loop
		contents += '\n';
		
		return;
	}
	//else standard text read
	std::fseek(fp, 0, SEEK_END);
	contents.resize(std::ftell(fp));
	std::rewind(fp);
	std::fread(&contents[0], 1, contents.size(), fp);
	std::fclose(fp);
}  // end readFile

//==============================================================================
// writeFile
void CodeEditor::writeFile(const std::string&        basepath,
                           const std::string&        path,
                           const std::string&        contents,
                           const std::string&        username,
                           const unsigned long long& insertPos,
                           const std::string&        insertString)
{
	std::string fullpath = basepath + path;
	__COUTV__(fullpath);

	FILE* fp;

	long long int oldSize = 0;
	try
	{
		// get old file size
		fp = fopen(fullpath.c_str(), "rb");
		if(!fp)
		{
			__SS__ << "Could not open file for saving at " << fullpath << __E__;
			__SS_THROW__;
		}
		std::fseek(fp, 0, SEEK_END);
		oldSize = std::ftell(fp);
		fclose(fp);
	}
	catch(...)
	{
		__COUT_WARN__ << "Ignoring file not existing..." << __E__;
	}

	fp = fopen(fullpath.c_str(), "wb");
	if(!fp)
	{
		__SS__ << "Could not open file for saving at " << fullpath << __E__;
		__SS_THROW__;
	}

	if(insertPos == (unsigned long long)-1)
		std::fwrite(&contents[0], 1, contents.size(), fp);
	else  // do insert
	{
		std::fwrite(&contents[0], 1, insertPos, fp);
		std::fwrite(&insertString[0], 1, insertString.size(), fp);
		std::fwrite(&contents[insertPos], 1, contents.size() - insertPos, fp);
	}
	std::fclose(fp);

	// log changes
	{
		std::string logpath = CODE_EDITOR_DATA_PATH + "/codeEditorChangeLog.txt";
		fp                  = fopen(logpath.c_str(), "a");
		if(!fp)
		{
			__SS__ << "Could not open change log for change tracking at " << logpath << __E__;
			__SS_THROW__;
		}
		fprintf(fp,
		        "time=%lld author%s old-size=%lld new-size=%lld path=%s\n",
		        (long long)time(0),
		        username.c_str(),
		        oldSize,
		        (long long)contents.size(),
		        fullpath.c_str());

		fclose(fp);
		__COUT__ << "Changes logged to: " << logpath << __E__;
	}  // end log changes

}  // end writeFile

//==============================================================================
// saveFileContent
void CodeEditor::saveFileContent(cgicc::Cgicc& cgiIn, HttpXmlDocument* xmlOut, const std::string& username)
{
	std::string path = CgiDataUtilities::getData(cgiIn, "path");
	path             = safePathString(StringMacros::decodeURIComponent(path));
	xmlOut->addTextElementToData("path", path);

	std::string basepath = CodeEditor::SOURCE_BASE_PATH;

	std::string pathMatchPrepend = "/";  // some requests come in with leading "/" and
	                                     // "//"
	if(path.length() > 1 && path[1] == '/')
		pathMatchPrepend += '/';

	//__COUTV__(path);
	//__COUTV__(pathMatchPrepend);

	// fix path for special environment variables
	if(path.substr(0, (pathMatchPrepend + "$USER_DATA/").size()) == pathMatchPrepend + "$USER_DATA/")
	{
		basepath = "/";
		path     = CodeEditor::USER_DATA_PATH + "/" + path.substr((pathMatchPrepend + "$USER_DATA/").size());
	}
	else if(path.substr(0, (pathMatchPrepend + "$OTSDAQ_DATA/").size()) == pathMatchPrepend + "$OTSDAQ_DATA/")
	{
		basepath = "/";
		path     = CodeEditor::OTSDAQ_DATA_PATH + "/" + path.substr((pathMatchPrepend + "$OTSDAQ_DATA/").size());
	}
	//__COUTV__(path);
	//__COUTV__(basepath);

	std::string extension = CgiDataUtilities::getData(cgiIn, "ext");
	if(!(path.length() > 4 && path.substr(path.length() - 4) == "/ots"))
		extension = safeExtensionString(extension);
	xmlOut->addTextElementToData("ext", extension);

	std::string contents = CgiDataUtilities::postData(cgiIn, "content");
	//__COUTV__(contents);
	contents = StringMacros::decodeURIComponent(contents);

	CodeEditor::writeFile(basepath, path + (extension.size() ? "." : "") + extension, contents, username);

}  // end saveFileContent

//==============================================================================
// build
//	cleanBuild and incrementalBuild
void CodeEditor::build(cgicc::Cgicc& cgiIn, HttpXmlDocument* /*xmlOut*/, const std::string& username)
{
	bool clean = CgiDataUtilities::getDataAsInt(cgiIn, "clean") ? true : false;

	__MCOUT_INFO__("Build (clean=" << clean << ") launched by '" << username << "'..." << __E__);

	// launch as thread so it does not lock up rest of code
	std::thread(
	    [](bool clean) {
		    std::string cmd;
		    if(clean)
		    {
			    // clean
			    {
				    cmd = "mrb z 2>&1";

				    std::array<char, 128> buffer;
				    std::string           result;
				    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
				    if(!pipe)
					    __THROW__("popen() failed!");

				    size_t i = 0;
				    // size_t j;

				    while(!feof(pipe.get()))
				    {
					    if(fgets(buffer.data(), 128, pipe.get()) != nullptr)
					    {
						    result += buffer.data();

						    // each time there is a new line print out
						    i = result.find('\n');
						    __COUTV__(result.substr(0, i));
						    __MOUT__ << result.substr(0, i);
						    result = result.substr(i + 1);  // discard before new line
					    }
				    }

				    __COUTV__(result);
				    __MOUT__ << result.substr(0, i);
			    }

			    sleep(1);
			    // mrbsetenv
			    {
				    cmd = "source mrbSetEnv 2>&1";

				    std::array<char, 128> buffer;
				    std::string           result;
				    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
				    if(!pipe)
					    __THROW__("popen() failed!");

				    size_t i = 0;
				    // size_t j;

				    while(!feof(pipe.get()))
				    {
					    if(fgets(buffer.data(), 128, pipe.get()) != nullptr)
					    {
						    result += buffer.data();

						    // each time there is a new line print out
						    i = result.find('\n');
						    __COUTV__(result.substr(0, i));
						    __MOUT__ << result.substr(0, i);
						    result = result.substr(i + 1);  // discard before new line
					    }
				    }

				    __COUTV__(result);
				    __MOUT__ << result.substr(0, i);
			    }
			    sleep(1);
		    }

		    // build
		    {
			    cmd = "mrb b 2>&1";

			    std::array<char, 128> buffer;
			    std::string           result;
			    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
			    if(!pipe)
				    __THROW__("popen() failed!");

			    size_t i = 0;
			    // size_t j;

			    while(!feof(pipe.get()))
			    {
				    if(fgets(buffer.data(), 128, pipe.get()) != nullptr)
				    {
					    result += buffer.data();

					    // each time there is a new line print out
					    i = result.find('\n');
					    //__COUTV__(result.substr(0,i));
					    __MOUT__ << result.substr(0, i);
					    result = result.substr(i + 1);  // discard before new line
				    }
			    }

			    //__COUTV__(result);
			    __MOUT__ << result.substr(0, i);
		    }
	    },
	    clean)
	    .detach();

}  // end build()

//==============================================================================
std::map<std::string /*special type*/, std::set<std::string> /*special file paths*/> CodeEditor::getSpecialsMap(void)
{
	std::map<std::string /*special type*/, std::set<std::string> /*special file paths*/> retMap;
	std::string                                                                          path = std::string(__ENV__("OTS_SOURCE"));

	__COUTV__(path);

	std::vector<std::string> specialFolders({"FEInterfaces",
	                                         "DataProcessorPlugins",
	                                         "UserTableDataFormats",
	                                         "TablePlugins",
	                                         "TablePluginDataFormats",
	                                         "UserTablePlugins",
	                                         "UserTablePluginDataFormats",
	                                         "SlowControlsInterfacePlugins",
	                                         "ControlsInterfacePlugins",
	                                         "FEInterfacePlugins",
	                                         "tools"});
	std::vector<std::string> specialMapTypes({CodeEditor::SPECIAL_TYPE_FEInterface,
	                                          CodeEditor::SPECIAL_TYPE_DataProcessor,
	                                          CodeEditor::SPECIAL_TYPE_Table,
	                                          CodeEditor::SPECIAL_TYPE_Table,
	                                          CodeEditor::SPECIAL_TYPE_Table,
	                                          CodeEditor::SPECIAL_TYPE_Table,
	                                          CodeEditor::SPECIAL_TYPE_Table,
	                                          CodeEditor::SPECIAL_TYPE_SlowControls,
	                                          CodeEditor::SPECIAL_TYPE_SlowControls,
	                                          CodeEditor::SPECIAL_TYPE_FEInterface,
	                                          CodeEditor::SPECIAL_TYPE_Tools});

	// Note: can not do lambda recursive function if using auto to declare the function,
	//	and must capture reference to the function. Also, must capture specialFolders
	//	reference for use internally (const values already are captured).
	std::function<void(const std::string&, const std::string&, const unsigned int, const int)> localRecurse =
	    [&specialFolders, &specialMapTypes, &retMap, &localRecurse](
	        const std::string& path, const std::string& offsetPath, const unsigned int depth, const int specialIndex) {
		    //__COUTV__(path);
		    //__COUTV__(depth);

		    DIR*           pDIR;
		    struct dirent* entry;
		    bool           isDir;
		    if(!(pDIR = opendir(path.c_str())))
		    {
			    __SS__ << "Plugin base path '" << path << "' could not be opened!" << __E__;
			    __SS_THROW__;
		    }

		    // else directory good, get all folders and look for special folders
		    std::string name;
		    int         type;
		    int         childSpecialIndex;
		    while((entry = readdir(pDIR)))
		    {
			    name = std::string(entry->d_name);
			    type = int(entry->d_type);

			    //__COUT__ << type << " " << name << "\n" << std::endl;

			    if(name[0] != '.' && (type == 0 ||  // 0 == UNKNOWN (which can happen - seen in SL7 VM)
			                          type == 4 || type == 8))
			    {
				    isDir = false;

				    if(type == 0)
				    {
					    // unknown type .. determine if directory
					    DIR* pTmpDIR = opendir((path + "/" + name).c_str());
					    if(pTmpDIR)
					    {
						    isDir = true;
						    closedir(pTmpDIR);
					    }
					    // else //assume file
				    }

				    if(type == 4)
					    isDir = true;  // flag directory types

				    // handle directories

				    if(isDir)
				    {
					    //__COUT__ << "Directory: " << type << " " << name << __E__;

					    childSpecialIndex = -1;
					    for(unsigned int i = 0; i < specialFolders.size(); ++i)
						    if(name == specialFolders[i])
						    {
							    //__COUT__ << "Found special folder '" <<
							    //		specialFolders[i] <<
							    //		"' at path " <<	path << __E__;

							    childSpecialIndex = i;
							    break;
						    }

					    // recurse deeper!
					    if(depth < 4)  // limit search depth
						    localRecurse(path + "/" + name, offsetPath + "/" + name, depth + 1, childSpecialIndex);
				    }
				    else if(specialIndex >= 0)
				    {
					    // get special files!!

					    if(name.find(".h") == name.length() - 2 || name.find(".cc") == name.length() - 3 || name.find(".txt") == name.length() - 4 ||
					       name.find(".sh") == name.length() - 3 || name.find(".py") == name.length() - 3)
					    {
						    //__COUT__ << "Found special '" <<
						    // specialFolders[specialIndex] <<
						    //		"' file '" << name << "' at path " <<
						    //		path << " " << specialIndex << __E__;

						    retMap[specialMapTypes[specialIndex]].emplace(offsetPath + "/" + name);
					    }
				    }
			    }
		    }  // end directory traversal

		    closedir(pDIR);
	    };  // end localRecurse() definition

	// start recursive traversal to find special folders
	localRecurse(path, "" /*offsetPath*/, 0 /*depth*/, -1 /*specialIndex*/);

	//__COUTV__(StringMacros::mapToString(retMap));
	return retMap;
}  // end getSpecialsMap()
