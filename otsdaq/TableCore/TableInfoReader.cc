#include "otsdaq/TableCore/TableInfoReader.h"

#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/XmlUtilities/ConvertFromXML.h"
#include "otsdaq/XmlUtilities/DOMTreeErrorReporter.h"

// #include "TimeFormatter.h"

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
// #include <xercesc/dom/DOMWriter.hpp>

#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <errno.h>
#include <sys/stat.h>

#include "otsdaq/TableCore/TableBase.h"

using namespace ots;

#undef __COUT_HDR__
#define __COUT_HDR__ "TableInfoReader <>"

// const std::string TableInfoReader::CONFIGURATION_BACKEND_TYPE_ =
//    __ENV__("CONFIGURATION_TYPE");
#define CONFIGURATION_BACKEND_TYPE_ __ENV__("CONFIGURATION_TYPE")

//==============================================================================
TableInfoReader::TableInfoReader(bool allowIllegalColumns) : allowIllegalColumns_(allowIllegalColumns)
{
	initPlatform();
	rootTag_                        = xercesc::XMLString::transcode("ROOT");
	tableTag_                       = xercesc::XMLString::transcode("TABLE");
	tableNameAttributeTag_          = xercesc::XMLString::transcode("Name");
	viewTag_                        = xercesc::XMLString::transcode("VIEW");
	viewNameAttributeTag_           = xercesc::XMLString::transcode("Name");
	viewTypeAttributeTag_           = xercesc::XMLString::transcode("Type");
	viewDescriptionAttributeTag_    = xercesc::XMLString::transcode("Description");
	columnTag_                      = xercesc::XMLString::transcode("COLUMN");
	columnTypeAttributeTag_         = xercesc::XMLString::transcode("Type");
	columnNameAttributeTag_         = xercesc::XMLString::transcode("Name");
	columnStorageNameAttributeTag_  = xercesc::XMLString::transcode("StorageName");
	columnDataTypeAttributeTag_     = xercesc::XMLString::transcode("DataType");
	columnDataChoicesAttributeTag_  = xercesc::XMLString::transcode("DataChoices");
	columnDefaultValueAttributeTag_ = xercesc::XMLString::transcode("DefaultValue");
	columnMinValueAttributeTag_     = xercesc::XMLString::transcode("MinValue");
	columnMaxValueAttributeTag_     = xercesc::XMLString::transcode("MaxValue");
}

//==============================================================================
TableInfoReader::~TableInfoReader(void)
{
	try
	{
		xercesc::XMLString::release(&rootTag_);
		xercesc::XMLString::release(&tableTag_);
		xercesc::XMLString::release(&tableNameAttributeTag_);
		xercesc::XMLString::release(&viewTag_);
		xercesc::XMLString::release(&viewNameAttributeTag_);
		xercesc::XMLString::release(&viewTypeAttributeTag_);
		xercesc::XMLString::release(&viewDescriptionAttributeTag_);
		xercesc::XMLString::release(&columnTag_);
		xercesc::XMLString::release(&columnTypeAttributeTag_);
		xercesc::XMLString::release(&columnNameAttributeTag_);
		xercesc::XMLString::release(&columnStorageNameAttributeTag_);
		xercesc::XMLString::release(&columnDataTypeAttributeTag_);
		xercesc::XMLString::release(&columnDataChoicesAttributeTag_);
		xercesc::XMLString::release(&columnDefaultValueAttributeTag_);
		xercesc::XMLString::release(&columnMinValueAttributeTag_);
		xercesc::XMLString::release(&columnMaxValueAttributeTag_);
	}
	catch(...)
	{
		__COUT_ERR__ << "Unknown exception encountered in TagNames destructor" << __E__;
	}
	terminatePlatform();
}

//==============================================================================
void TableInfoReader::initPlatform(void)
{
	try
	{
		xercesc::XMLPlatformUtils::Initialize();  // Initialize Xerces infrastructure
	}
	catch(xercesc::XMLException& e)
	{
		__COUT_ERR__ << "XML toolkit initialization error: " << XML_TO_CHAR(e.getMessage()) << __E__;
		// throw exception here to return ERROR_XERCES_INIT
	}
}

//==============================================================================
void TableInfoReader::terminatePlatform(void)
{
	try
	{
		xercesc::XMLPlatformUtils::Terminate();  // Terminate after release of memory
	}
	catch(xercesc::XMLException& e)
	{
		__COUT_ERR__ << "XML tolkit teardown error: " << XML_TO_CHAR(e.getMessage()) << __E__;
	}
}

//==============================================================================
void TableInfoReader::setAllowColumnErrors(bool setValue) { allowIllegalColumns_ = setValue; }
//==============================================================================
const bool& TableInfoReader::getAllowColumnErrors(void) { return allowIllegalColumns_; }

//==============================================================================
bool TableInfoReader::checkViewType(std::string type)
{
	std::vector<std::string> types;
	int                      currentIndex = 0;
	while(type.find(',', currentIndex) != std::string::npos)
	{
		types.push_back(type.substr(currentIndex, type.find(',', currentIndex) - currentIndex));
		currentIndex = type.find(',', currentIndex) + 1;
	}
	types.push_back(type.substr(currentIndex, type.size()));

	const std::string systemType = CONFIGURATION_BACKEND_TYPE_;

	for(unsigned int i = 0; i < types.size(); i++)
	{
		if(types[i] == systemType)
			return true;
	}
	// In case I don't succeed let's check if maybe there is something wrong with the
	// names
	const unsigned int allowedNamesSize               = 3;
	const std::string  allowedNames[allowedNamesSize] = {"File", "Database", "DatabaseTest"};
	if(systemType != allowedNames[0] && systemType != allowedNames[1] && systemType != allowedNames[2])
	{
		__COUT__ << "The type defined in CONFIGURATION_BACKEND_TYPE (" << systemType
		         << ") doesn't match with any of the allowed types: File,Database or "
		            "DatabaseTest"
		         << __E__;

		throw(std::runtime_error("Illegal table type"));
	}
	for(unsigned int i = 0; i < types.size(); i++)
	{
		if(types[i] != allowedNames[0] && types[i] != allowedNames[1] && types[i] != allowedNames[2])
		{
			__COUT__ << "The type defined in the info file (" << types[i] << ") doesn't match with any of the allowed types: " << allowedNames[0] << ", "
			         << allowedNames[1] << " or " << allowedNames[2] << __E__;
			throw(std::runtime_error("Illegal Type!"));
		}
	}

	return false;
}

//==============================================================================
xercesc::DOMNode* TableInfoReader::getNode(XMLCh* tagName, xercesc::DOMNode* parent, unsigned int itemNumber)
{
	return getNode(tagName, dynamic_cast<xercesc::DOMElement*>(parent), itemNumber);
}

//==============================================================================
xercesc::DOMNode* TableInfoReader::getNode(XMLCh* tagName, xercesc::DOMElement* parent, unsigned int itemNumber)
{
	xercesc::DOMNodeList* nodeList = parent->getElementsByTagName(tagName);
	if(!nodeList)
	{
		throw(std::runtime_error(std::string("Can't find ") + XML_TO_CHAR(tagName) + " tag!"));
		__COUT__ << (std::string("Can't find ") + XML_TO_CHAR(tagName) + " tag!") << __E__;
	}
	//    __COUT__<< "Name: "  << XML_TO_CHAR(nodeList->item(itemNumber)->getNodeName())
	//    << __E__; if( nodeList->item(itemNumber)->getFirstChild() != 0 )
	//        __COUT__<< "Value: " <<
	//        XML_TO_CHAR(nodeList->item(itemNumber)->getFirstChild()->getNodeValue()) <<
	//        __E__;
	return nodeList->item(itemNumber);
}

//==============================================================================
xercesc::DOMElement* TableInfoReader::getElement(XMLCh* tagName, xercesc::DOMNode* parent, unsigned int itemNumber)
{
	return dynamic_cast<xercesc::DOMElement*>(getNode(tagName, parent, itemNumber));
}

//==============================================================================
xercesc::DOMElement* TableInfoReader::getElement(XMLCh* tagName, xercesc::DOMElement* parent, unsigned int itemNumber)
{
	return dynamic_cast<xercesc::DOMElement*>(getNode(tagName, parent, itemNumber));
}

//==============================================================================
std::string TableInfoReader::read(TableBase& table)
{
	std::string accumulatedExceptions = "";

	// KEEP For debugging... added by Gennadiy...
	// if table name starts with "TestTable00" then turn off the reading of information
	//	auto tmp_test_table_prefix = std::string{"TestTable00"};
	//        auto tmp_table_name = table.getTableName();
	//	if (std::equal(tmp_test_table_prefix.begin(), tmp_test_table_prefix.end(),
	// tmp_table_name.begin())) return accumulatedExceptions;  KEEP End debugging... for
	// Gennadiy...

	// These environment variables are required
	if(__ENV__("CONFIGURATION_TYPE") == NULL)
		__COUT__ << "Missing env variable: CONFIGURATION_TYPE. It must be set!" << __E__;
	// if(__ENV__("CONFIGURATION_DATA_PATH") == NULL) __COUT__ << "Missing env variable:
	// CONFIGURATION_DATA_PATH. It must be set!" << __E__;
	if(__ENV__("TABLE_INFO_PATH") == NULL)
		__COUT__ << "Missing env variable: TABLE_INFO_PATH. It must be set!" << __E__;

	// example c++ setting of necessary environment variables
	// setenv("CONFIGURATION_TYPE","File",1);
	// setenv("CONFIGURATION_DATA_PATH",(std::string(__ENV__("USER_DATA")) +
	// "/TableDataExamples").c_str(),1);
	// setenv("TABLE_INFO_PATH",(std::string(__ENV__("USER_DATA")) +
	// "/TableInfo").c_str(),1);

	std::string tableDataDir = std::string(__ENV__("TABLE_INFO_PATH")) + "/";
	std::string tableFile    = tableDataDir + table.getTableName() + "Info.xml";
	//__COUT__ << tableFile << __E__;
	struct stat fileStatus;

	int iretStat = stat(tableFile.c_str(), &fileStatus);
	if(iretStat == ENOENT)
	{
		__SS__ << ("Path file_name does not exist, or path is an empty std::string.") << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
	else if(iretStat == ENOTDIR)
	{
		__SS__ << ("A component of the path is not a directory.") << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
	else if(iretStat == ELOOP)
	{
		__SS__ << ("Too many symbolic links encountered while traversing the path.") << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
	else if(iretStat == EACCES)
	{
		__SS__ << ("Permission denied.") << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
	else if(iretStat == ENAMETOOLONG)
	{
		__SS__ << ("File can not be read. Name too long.") << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}

	xercesc::XercesDOMParser* parser = new xercesc::XercesDOMParser;
	// Configure DOM parser.
	parser->setValidationScheme(xercesc::XercesDOMParser::Val_Auto);  // Val_Never
	parser->setDoNamespaces(true);
	parser->setDoSchema(true);
	parser->useCachedGrammarInParse(false);

	DOMTreeErrorReporter* errorHandler = new DOMTreeErrorReporter();
	parser->setErrorHandler(errorHandler);
	try
	{
		parser->parse(tableFile.c_str());

		// no need to free this pointer - owned by the parent parser object
		xercesc::DOMDocument* xmlDocument = parser->getDocument();

		// Get the top-level element: Name is "root". No attributes for "root"
		xercesc::DOMElement* elementRoot = xmlDocument->getDocumentElement();
		if(!elementRoot)
		{
			delete parser;
			delete errorHandler;
			throw(std::runtime_error("empty XML document"));
		}

		//<TABLE>
		xercesc::DOMElement* tableElement = getElement(tableTag_, elementRoot, 0);
		if(table.getTableName() != XML_TO_CHAR(tableElement->getAttribute(tableNameAttributeTag_)))
		{
			__SS__ << "In " << tableFile << " the table name " << XML_TO_CHAR(tableElement->getAttribute(tableNameAttributeTag_))
			       << " doesn't match the the class table name " << table.getTableName() << __E__;

			delete parser;
			delete errorHandler;
			__COUT_ERR__ << "\n" << ss.str();
			throw(std::runtime_error(ss.str()));
		}
		//<VIEW>
		xercesc::DOMNodeList* viewNodeList     = tableElement->getElementsByTagName(viewTag_);
		bool                  storageTypeFound = false;

		if(viewNodeList->getLength() != 1)
		{
			__SS__ << "In " << tableFile << " the table name " << XML_TO_CHAR(tableElement->getAttribute(tableNameAttributeTag_))
			       << " there must only be one view. There were " << viewNodeList->getLength() << " found." << __E__;

			delete parser;
			delete errorHandler;
			__COUT_ERR__ << "\n" << ss.str();
			throw(std::runtime_error(ss.str()));
		}

		for(XMLSize_t view = 0; view < viewNodeList->getLength(); view++)
		{
			if(!viewNodeList->item(view)->getNodeType() ||
			   viewNodeList->item(view)->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)  // true is not 0 && is element
				continue;
			xercesc::DOMElement* viewElement = dynamic_cast<xercesc::DOMElement*>(viewNodeList->item(view));
			std::string          viewType    = XML_TO_CHAR(viewElement->getAttribute(viewTypeAttributeTag_));
			if(!checkViewType(viewType))
				continue;
			storageTypeFound = true;

			// table name is now constant, set by parent TableBase
			// table.getMockupViewP()->setTableName(XML_TO_CHAR(viewElement->getAttribute(viewNameAttributeTag_)));
			// check for consistency, and show warning
			if(std::string(XML_TO_CHAR(viewElement->getAttribute(viewNameAttributeTag_))) != table.getMockupViewP()->getTableName())
				__COUT_WARN__ << "Table Info name mismatch: " << std::string(XML_TO_CHAR(viewElement->getAttribute(viewNameAttributeTag_))) << " vs "
				              << table.getMockupViewP()->getTableName() << __E__;

			xercesc::DOMNodeList* columnNodeList = viewElement->getElementsByTagName(columnTag_);

			for(XMLSize_t column = 0; column < columnNodeList->getLength(); column++)
			{
				//<COLUMN>
				xercesc::DOMElement* columnElement = dynamic_cast<xercesc::DOMElement*>(columnNodeList->item(column));
				//__COUT__ <<
				// XML_TO_CHAR(columnElement->getAttribute(columnNameAttributeTag_)) <<
				// __E__;

				// Check for default value tag being there (for backwards compatibility)
				//  Documentation :https://xerces.apache.org/xerces-c/apiDocs-3/classDOMElement.html#a9d6a102d853eafe6619be4324c1555c3
				std::string defaultValue;
				bool        isDefaultValue = columnElement->getAttributeNode(columnDefaultValueAttributeTag_) ? true : false;
				if(isDefaultValue)
				{
					defaultValue = StringMacros::decodeURIComponent(XML_TO_CHAR(columnElement->getAttribute(columnDefaultValueAttributeTag_)));

					//__COUT__ << "FOUND default value! " << defaultValue << __E__;
				}
				std::string minValue;
				bool        isMinValue = columnElement->getAttributeNode(columnMinValueAttributeTag_) ? true : false;
				if(isMinValue)
				{
					minValue = StringMacros::decodeURIComponent(XML_TO_CHAR(columnElement->getAttribute(columnMinValueAttributeTag_)));

					// __COUT__ << "FOUND min value! " << minValue << __E__;
				}
				std::string maxValue;
				bool        isMaxValue = columnElement->getAttributeNode(columnMaxValueAttributeTag_) ? true : false;
				if(isMaxValue)
				{
					maxValue = StringMacros::decodeURIComponent(XML_TO_CHAR(columnElement->getAttribute(columnMaxValueAttributeTag_)));

					// __COUT__ << "FOUND max value! " << maxValue << __E__;
				}
				// TODO read min/max, almost done??
				else
				{
					//__COUT__ << "DID NOT find default value! or min and max values!" << __E__;
				}

				// automatically delete the persistent version of the column info
				std::string capturedException;
				// TODO add reading of min / max
				table.getMockupViewP()->getColumnsInfoP()->push_back(
				    TableViewColumnInfo(XML_TO_CHAR(columnElement->getAttribute(columnTypeAttributeTag_)),
				                        XML_TO_CHAR(columnElement->getAttribute(columnNameAttributeTag_)),
				                        XML_TO_CHAR(columnElement->getAttribute(columnStorageNameAttributeTag_)),
				                        XML_TO_CHAR(columnElement->getAttribute(columnDataTypeAttributeTag_)),
				                        isDefaultValue ? &defaultValue : 0,
				                        XML_TO_CHAR(columnElement->getAttribute(columnDataChoicesAttributeTag_)),
				                        isMinValue ? &minValue : 0,
				                        isMaxValue ? &maxValue : 0,
				                        allowIllegalColumns_ ? &capturedException : 0));  // capture exception string if allowing illegal columns

				// if error detected (this implies allowing illegal columns)
				//	accumulate and return accumulated errors at end
				if(capturedException != "")
					accumulatedExceptions += std::string("\n\nColumn Error:") + capturedException;

				//</COLUMN>
			}

			// handle view description (which is actually the table
			//	description since only one view allowed)
			std::string tableDescription = XML_TO_CHAR(viewElement->getAttribute(viewDescriptionAttributeTag_));

			table.setTableDescription(StringMacros::decodeURIComponent(tableDescription));
			//__COUT__ << "tableDescription = " << tableDescription << __E__;

			//</VIEW>
		}
		if(!storageTypeFound)
		{
			__COUT__ << "The type defined in CONFIGURATION_BACKEND_TYPE (" << CONFIGURATION_BACKEND_TYPE_ << ") doesn't match with any of the types defined in "
			         << tableFile << __E__;

			delete parser;
			delete errorHandler;
			throw(std::runtime_error("Table Type mismatch!"));
		}

		//</TABLE>
	}
	catch(xercesc::XMLException& e)
	{
		std::ostringstream errBuf;
		errBuf << "Error parsing file: " << XML_TO_CHAR(e.getMessage()) << std::flush;
	}
	delete parser;
	delete errorHandler;

	//__COUT__ << __E__;

	// if exceptions have been accumulated
	//	then in allowIllegalColumns mode
	// return accumulated exception strings to next level
	return accumulatedExceptions;
}

//==============================================================================
// returns accumulated exception string (while allowIllegalColumns == true)
// otherwise "" if no exceptions
std::string TableInfoReader::read(TableBase* table) { return read(*table); }
