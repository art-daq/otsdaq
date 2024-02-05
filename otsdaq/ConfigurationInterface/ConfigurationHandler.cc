#include "otsdaq/ConfigurationInterface/ConfigurationHandler.h"
#include "otsdaq/ConfigurationInterface/TimeFormatter.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/XmlUtilities/ConvertFromXML.h"
#include "otsdaq/XmlUtilities/ConvertToXML.h"
#include "otsdaq/XmlUtilities/DOMTreeErrorReporter.h"

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMLSOutput.hpp>
#include <xercesc/dom/DOMLSSerializer.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLUni.hpp>
// #include <xercesc/dom/DOMWriter.hpp>

#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>

#include <sys/types.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <errno.h>
#include <sys/stat.h>

#include "otsdaq/TableCore/TableBase.h"

using namespace ots;

#undef __COUT_HDR__
#define __COUT_HDR__ "ConfigHandler"

// The tag values must be given after the XML platform is initialized so they are defined
// in initPlatform
XMLCh* ConfigurationHandler::rootTag_               = 0;
XMLCh* ConfigurationHandler::headerTag_             = 0;
XMLCh* ConfigurationHandler::typeTag_               = 0;
XMLCh* ConfigurationHandler::extensionTableNameTag_ = 0;
XMLCh* ConfigurationHandler::nameTag_               = 0;
XMLCh* ConfigurationHandler::runTag_                = 0;
XMLCh* ConfigurationHandler::runTypeTag_            = 0;
XMLCh* ConfigurationHandler::runNumberTag_          = 0;
XMLCh* ConfigurationHandler::runBeginTimestampTag_  = 0;
XMLCh* ConfigurationHandler::locationTag_           = 0;
XMLCh* ConfigurationHandler::datasetTag_            = 0;
XMLCh* ConfigurationHandler::versionTag_            = 0;
XMLCh* ConfigurationHandler::commentDescriptionTag_ = 0;
XMLCh* ConfigurationHandler::createdByUserTag_      = 0;
XMLCh* ConfigurationHandler::partTag_               = 0;
XMLCh* ConfigurationHandler::nameLabelTag_          = 0;
XMLCh* ConfigurationHandler::kindOfPartTag_         = 0;
XMLCh* ConfigurationHandler::dataTag_               = 0;

//==============================================================================
ConfigurationHandler::ConfigurationHandler(void) {}

//==============================================================================
ConfigurationHandler::~ConfigurationHandler(void) {}

//==============================================================================
void ConfigurationHandler::initPlatform(void)
{
	try
	{
		xercesc::XMLPlatformUtils::Initialize();  // Initialize Xerces infrastructure
	}
	catch(xercesc::XMLException& e)
	{
		__MOUT_ERR__ << "XML toolkit initialization error: " << XML_TO_CHAR(e.getMessage()) << std::endl;
		// throw exception here to return ERROR_XERCES_INIT
	}

	rootTag_               = xercesc::XMLString::transcode("ROOT");
	headerTag_             = xercesc::XMLString::transcode("HEADER");
	typeTag_               = xercesc::XMLString::transcode("TYPE");
	extensionTableNameTag_ = xercesc::XMLString::transcode("EXTENSION_TABLE_NAME");
	nameTag_               = xercesc::XMLString::transcode("NAME");
	runTag_                = xercesc::XMLString::transcode("RUN");
	runTypeTag_            = xercesc::XMLString::transcode("RUN_TYPE");
	runNumberTag_          = xercesc::XMLString::transcode("RUN_NUMBER");
	runBeginTimestampTag_  = xercesc::XMLString::transcode("RUN_BEGIN_TIMESTAMP");
	locationTag_           = xercesc::XMLString::transcode("LOCATION");
	datasetTag_            = xercesc::XMLString::transcode("DATA_SET");
	versionTag_            = xercesc::XMLString::transcode("VERSION");
	commentDescriptionTag_ = xercesc::XMLString::transcode("COMMENT_DESCRIPTION");
	createdByUserTag_      = xercesc::XMLString::transcode("CREATED_BY_USER");
	partTag_               = xercesc::XMLString::transcode("PART");
	nameLabelTag_          = xercesc::XMLString::transcode("NAME_LABEL");
	kindOfPartTag_         = xercesc::XMLString::transcode("KIND_OF_PART");
	dataTag_               = xercesc::XMLString::transcode("DATA");
}

//==============================================================================
void ConfigurationHandler::terminatePlatform(void)
{
	try
	{
		xercesc::XMLString::release(&rootTag_);
		xercesc::XMLString::release(&headerTag_);
		xercesc::XMLString::release(&typeTag_);
		xercesc::XMLString::release(&extensionTableNameTag_);
		xercesc::XMLString::release(&nameTag_);
		xercesc::XMLString::release(&runTag_);
		xercesc::XMLString::release(&runTypeTag_);
		xercesc::XMLString::release(&runNumberTag_);
		xercesc::XMLString::release(&runBeginTimestampTag_);
		xercesc::XMLString::release(&locationTag_);
		xercesc::XMLString::release(&datasetTag_);
		xercesc::XMLString::release(&versionTag_);
		xercesc::XMLString::release(&commentDescriptionTag_);
		xercesc::XMLString::release(&createdByUserTag_);
		xercesc::XMLString::release(&partTag_);
		xercesc::XMLString::release(&nameLabelTag_);
		xercesc::XMLString::release(&kindOfPartTag_);
		xercesc::XMLString::release(&dataTag_);
	}
	catch(...)
	{
		__MOUT_ERR__ << "Unknown exception encountered in TagNames destructor" << std::endl;
	}

	try
	{
		xercesc::XMLPlatformUtils::Terminate();  // Terminate after release of memory
	}
	catch(xercesc::XMLException& e)
	{
		__MOUT_ERR__ << "XML ttolkit teardown error: " << XML_TO_CHAR(e.getMessage()) << std::endl;
	}
}

//==============================================================================
bool ConfigurationHandler::validateNode(XMLCh* tagName, xercesc::DOMNode* node, const std::string& expectedValue)
{
	if(node->getFirstChild() == 0)
	{
		__COUT__ << "Tag " << XML_TO_CHAR(tagName) << " doesn't have a value!" << std::endl;
		return false;
	}

	if(XML_TO_STRING(node->getFirstChild()->getNodeValue()) != expectedValue)
	{
		__COUT__ << "The tag " << XML_TO_CHAR(tagName) << " with value " << XML_TO_CHAR(node->getFirstChild()->getNodeValue())
		         << " doesn't match the expected value " << expectedValue << std::endl;
		return false;
	}

	return true;
}

//==============================================================================
xercesc::DOMNode* ConfigurationHandler::getNode(XMLCh* tagName, xercesc::DOMNode* parent, unsigned int itemNumber)
{
	return getNode(tagName, dynamic_cast<xercesc::DOMElement*>(parent), itemNumber);
}

//==============================================================================
xercesc::DOMNode* ConfigurationHandler::getNode(XMLCh* tagName, xercesc::DOMElement* parent, unsigned int itemNumber)
{
	xercesc::DOMNodeList* nodeList = parent->getElementsByTagName(tagName);

	if(!nodeList)
	{
		throw(std::runtime_error(std::string("Can't find ") + XML_TO_STRING(tagName) + " tag!"));
		__COUT__ << (std::string("Can't find ") + XML_TO_STRING(tagName) + " tag!") << std::endl;
	}

	//    __COUT__<< "Name: "  << XML_TO_CHAR(nodeList->item(itemNumber)->getNodeName())
	//    << std::endl; if( nodeList->item(itemNumber)->getFirstChild() != 0 )
	//        __COUT__<< "Value: " <<
	//        XML_TO_CHAR(nodeList->item(itemNumber)->getFirstChild()->getNodeValue()) <<
	//        std::endl;
	return nodeList->item(itemNumber);
}

//==============================================================================
xercesc::DOMElement* ConfigurationHandler::getElement(XMLCh* tagName, xercesc::DOMNode* parent, unsigned int itemNumber)
{
	return dynamic_cast<xercesc::DOMElement*>(getNode(tagName, parent, itemNumber));
}

//==============================================================================
xercesc::DOMElement* ConfigurationHandler::getElement(XMLCh* tagName, xercesc::DOMElement* parent, unsigned int itemNumber)
{
	return dynamic_cast<xercesc::DOMElement*>(getNode(tagName, parent, itemNumber));
}

//==============================================================================
void ConfigurationHandler::readXML(TableBase* table, TableVersion version) { readXML(*table, version); }

//==============================================================================
void ConfigurationHandler::readXML(TableBase& table, TableVersion version)
{
	initPlatform();
	std::string configFile = getXMLFileName(table, version);

	__COUT__ << "Reading: " << configFile << std::endl;
	__COUT__ << "Into View with Table Name: " << table.getViewP()->getTableName() << std::endl;
	__COUT__ << "Into View with version: " << table.getViewP()->getVersion() << " and version-to-read: " << version << std::endl;

	struct stat fileStatus;
	// stat returns -1 on error, status in errno
	if(stat(configFile.c_str(), &fileStatus) < 0)
	{
		__COUT__ << "Error reading path: " << configFile << std::endl;
		std::stringstream ss;
		ss << __COUT_HDR__;
		if(errno == ENOENT)
			ss << ("Path file_name does not exist.");
		else if(errno == ENOTDIR)
			ss << ("A component of the path is not a directory.");
		else if(errno == ELOOP)
			ss << ("Too many symbolic links encountered while traversing the path.");
		else if(errno == EACCES)
			ss << ("Permission denied.");
		else if(errno == ENAMETOOLONG)
			ss << ("File name too long.");
		else
			ss << ("File can not be read.");
		ss << std::endl;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}

	xercesc::XercesDOMParser* parser = new xercesc::XercesDOMParser;

	// Configure DOM parser.
	parser->setValidationScheme(xercesc::XercesDOMParser::Val_Auto);  // Val_Never
	parser->setDoNamespaces(true);
	parser->setDoSchema(false);  // RAR set to false to get rid of "error reading primary document *.xsd"
	                             // uses if true:
	                             // rootElement->setAttribute(CONVERT_TO_XML("xsi:noNamespaceSchemaLocation"),CONVERT_TO_XML("TableBase.xsd"));
	parser->useCachedGrammarInParse(false);
	DOMTreeErrorReporter* errorHandler = new DOMTreeErrorReporter();
	parser->setErrorHandler(errorHandler);

	//__COUT__ << configFile << std::endl;
	try
	{
		//__COUT__ << "Parsing" << std::endl;
		parser->parse(configFile.c_str());
		//__COUT__ << "Parsed" << std::endl;

		// no need to free this pointer - owned by the parent parser object
		xercesc::DOMDocument* document = parser->getDocument();

		// Get the top-level element: Name is "root". No attributes for "root"
		xercesc::DOMElement* elementRoot = document->getDocumentElement();

		if(!elementRoot)
			throw(std::runtime_error("empty XML document"));
		//<HEADER>
		//__COUT__ << "Reading header" << std::endl;
		xercesc::DOMNode* headerNode = getNode(headerTag_, elementRoot, 0);
		if(!headerNode)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(headerTag_)));

		//<TYPE>
		//__COUT__ << "Reading type" << std::endl;
		xercesc::DOMElement* typeElement = getElement(typeTag_, headerNode, 0);
		if(!typeElement)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(typeTag_)));
		xercesc::DOMNode* extensionTableNameNode = getNode(extensionTableNameTag_, typeElement, 0);
		if(!validateNode(extensionTableNameTag_, extensionTableNameNode, table.getView().getTableName()))
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(extensionTableNameTag_)));
		xercesc::DOMNode* nameNode = getNode(nameTag_, typeElement, 0);
		if(!validateNode(nameTag_, nameNode, table.getTableName()))
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(nameTag_)));

		//__COUT__ << configFile << std::endl;
		//</TYPE>
		//<RUN>
		//__COUT__ << "Reading run" << std::endl;
		xercesc::DOMElement* runElement = getElement(runTag_, headerNode, 0);
		if(!runElement)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(runTag_)));
		[[maybe_unused]] xercesc::DOMNode* runTypeNode = getNode(runTypeTag_, runElement, 0);
		assert(validateNode(runTypeTag_, runTypeNode, table.getTableName()));
		xercesc::DOMNode* runNumberNode = getNode(runNumberTag_, runElement, 0);
		if(!runNumberNode)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(runNumberTag_)));
		xercesc::DOMNode* runBeginTimestampNode = getNode(runBeginTimestampTag_, runElement, 0);
		if(!runBeginTimestampNode)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(runBeginTimestampTag_)));
		xercesc::DOMNode* locationNode = getNode(locationTag_, runElement, 0);
		if(!locationNode)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(locationTag_)));

		//__COUT__ << configFile << std::endl;
		//</RUN>
		//</HEADER>

		//<DATA_SET>
		//__COUT__ << "Reading Data Set" << std::endl;
		xercesc::DOMElement* datasetElement = getElement(datasetTag_, elementRoot, 0);
		if(!datasetElement)
			throw(std::runtime_error(std::string("The document is missing the mandatory tag: ") + XML_TO_STRING(datasetTag_)));

		//   <PART>
		//__COUT__ << "Reading Part" << std::endl;
		// xercesc::DOMNode* partNode       = getNode(partTag_, datasetElement, 0);
		// xercesc::DOMNode* nameLabelNode  = getNode(nameLabelTag_, partNode, 0);
		// xercesc::DOMNode* kindOfPartNode = getNode(kindOfPartTag_, partNode, 0);

		//  </PART>
		xercesc::DOMNode* versionNode = getNode(versionTag_, datasetElement, 0);

		if(versionNode->getFirstChild() == 0)  // if version tag is missing
		{
			throw(std::runtime_error(std::string("Missing version tag: ") + XML_TO_CHAR(versionNode->getFirstChild()->getNodeValue())));
		}
		else  // else verify version tag matches parameter version
		{
			char tmpVersionStr[100];
			sprintf(tmpVersionStr, "%d", version.version());
			__COUT__ << version << "-" << XML_TO_CHAR(versionNode->getFirstChild()->getNodeValue()) << std::endl;
			if(strcmp(tmpVersionStr, XML_TO_CHAR(versionNode->getFirstChild()->getNodeValue())) != 0)
				throw(std::runtime_error(std::string("Mis-matched version tag: ") + XML_TO_CHAR(versionNode->getFirstChild()->getNodeValue()) + " vs " +
				                         tmpVersionStr));
		}
		// version is valid
		table.getViewP()->setVersion(XML_TO_CHAR(versionNode->getFirstChild()->getNodeValue()));

		xercesc::DOMNode* commentDescriptionNode = getNode(commentDescriptionTag_, datasetElement, 0);
		if(commentDescriptionNode->getFirstChild() != 0)
			table.getViewP()->setComment(XML_TO_CHAR(commentDescriptionNode->getFirstChild()->getNodeValue()));

		xercesc::DOMNode* createdByUserNode = getNode(createdByUserTag_, datasetElement, 0);
		if(createdByUserNode->getFirstChild() != 0)
			table.getViewP()->setAuthor(XML_TO_CHAR(createdByUserNode->getFirstChild()->getNodeValue()));

		//<DATA>
		//__COUT__ << "Reading Data" << std::endl;
		xercesc::DOMNodeList* dataNodeList = datasetElement->getElementsByTagName(dataTag_);

		if(!dataNodeList)
			throw(std::runtime_error(std::string("Can't find ") + XML_TO_STRING(dataTag_) + " tag!"));

		//__COUT__ << "Number of data nodes: " << dataNodeList->getLength() << std::endl;
		// First I need to setup the data container which is a [row][col] matrix where
		// each <dataTag_> is a row  and row 0 has the names of the column which will go
		// in the columnInfo container
		if(!dataNodeList->getLength())  // I must have at least 1 data!
		{
			__SS__ << "Must be non-empty data set!";
			__SS_THROW__;
		}

		//__COUT__ << table.getView().getColumnsInfo().size() << std::endl;
		// First I can build the matrix and then fill it since I know the number of rows
		// and columns
		table.getViewP()->resizeDataView(dataNodeList->getLength(), table.getView().getNumberOfColumns());

		for(XMLSize_t row = 0; row < dataNodeList->getLength(); row++)
		{
			// DOMElement* dataElement = dynamic_cast< xercesc::DOMElement* >(
			// dataNodeList->item(row) );
			xercesc::DOMNodeList* columnNodeList = dataNodeList->item(row)->getChildNodes();
			unsigned int          colNumber      = 0;

			//__COUT__ << "Row: " << row << " w " <<  columnNodeList->getLength() <<
			// std::endl;
			for(XMLSize_t col = 0; col < columnNodeList->getLength(); col++)
			{
				//__COUT__ << "Col: " << col << std::endl;

				if(!columnNodeList->item(col)->getNodeType() ||
				   columnNodeList->item(col)->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)  // true is not 0 && is element
					continue;

				xercesc::DOMElement* columnElement = dynamic_cast<xercesc::DOMElement*>(columnNodeList->item(col));

				if(table.getView().getColumnInfo(colNumber).getStorageName() != XML_TO_STRING(columnElement->getTagName()))
				{
					std::stringstream error;
					error << __COUT_HDR__ << std::endl
					      << "The column number " << colNumber << " named " << table.getView().getColumnInfo(colNumber).getStorageName()
					      << " defined in the view " << table.getView().getTableName() << " doesn't match the file column order, since the " << colNumber + 1
					      << (colNumber == 0 ? "st" : (colNumber == 1 ? "nd" : (colNumber == 2 ? "rd" : "th"))) << " element found in the file at "
					      << XML_TO_CHAR(dataTag_) << " tag number " << row << " is " << XML_TO_CHAR(columnElement->getTagName());
					__MOUT_ERR__ << error.str();
					throw(std::runtime_error(error.str()));
				}

				//                if( row==0 )
				//                {
				//                    table.getViewP()->getDataViewP()->push_back(vector<string>(dataNodeList->getLength()+1));//#
				//                    of data + names
				//                    (*table.getViewP()->getDataViewP())[colNumber][0]
				//                    = XML_TO_STRING(columnElement->getTagName());
				//                }
				table.getViewP()->setValueAsString(XML_TO_STRING(columnNodeList->item(col)->getFirstChild()->getNodeValue()), row, colNumber);
				//(*table.getViewP()->getDataViewP())[row][colNumber] =
				// XML_TO_STRING(columnNodeList->item(col)->getFirstChild()->getNodeValue());
				++colNumber;
			}
		}
	}
	catch(xercesc::XMLException& e)
	{
		__COUT__ << "Error parsing file: " << configFile << std::endl;
		std::ostringstream errBuf;
		errBuf << "Error parsing file: " << XML_TO_CHAR(e.getMessage()) << std::flush;
	}

	__COUT__ << "Done with table file: " << configFile << std::endl;

	delete parser;
	delete errorHandler;
	terminatePlatform();
}

//==============================================================================
std::string ConfigurationHandler::writeXML(const TableBase& table)
{
	initPlatform();

	std::string configFile = getXMLFileName(table,
	                                        table.getViewVersion());  // std::string(__ENV__("CONFIGURATION_DATA_PATH")) + "/"
	                                                                  // + table.getTableName() + "_write.xml";

	xercesc::DOMImplementation* implementation = xercesc::DOMImplementationRegistry::getDOMImplementation(CONVERT_TO_XML("Core"));
	if(implementation != 0)
	{
		try
		{
			//<ROOT>
			xercesc::DOMDocument* document = implementation->createDocument(0,         // root element namespace URI.
			                                                                rootTag_,  // root element name
			                                                                0);        // document type object (DTD).

			xercesc::DOMElement* rootElement = document->getDocumentElement();
			rootElement->setAttribute(CONVERT_TO_XML("xmlns:xsi"), CONVERT_TO_XML("http://www.w3.org/2001/XMLSchema-instance"));
			rootElement->setAttribute(CONVERT_TO_XML("xsi:noNamespaceSchemaLocation"),
			                          CONVERT_TO_XML("TableBase.xsd"));  // configFile.substr(configFile.rfind("/")+1).c_str()));
			                                                             // //put the file name here..? TableBase.xsd"));
			//${OTSDAQ_DIR}/otsdaq/ConfigurationDataFormats/TableInfo.xsd
			// now moved to $USER_DATA/TableInfo/TableInfo.xsd

			//<HEADER>
			xercesc::DOMElement* headerElement = document->createElement(headerTag_);
			rootElement->appendChild(headerElement);

			//<TYPE>
			xercesc::DOMElement* typeElement = document->createElement(typeTag_);
			headerElement->appendChild(typeElement);

			xercesc::DOMElement* extensionTableNameElement = document->createElement(extensionTableNameTag_);
			typeElement->appendChild(extensionTableNameElement);
			xercesc::DOMText* extensionTableNameValue = document->createTextNode(CONVERT_TO_XML(table.getView().getTableName().c_str()));
			extensionTableNameElement->appendChild(extensionTableNameValue);

			xercesc::DOMElement* nameElement = document->createElement(nameTag_);
			typeElement->appendChild(nameElement);
			xercesc::DOMText* nameValue = document->createTextNode(CONVERT_TO_XML(table.getTableName().c_str()));
			nameElement->appendChild(nameValue);
			//</TYPE>

			//<RUN>
			xercesc::DOMElement* runElement = document->createElement(runTag_);
			headerElement->appendChild(runElement);

			xercesc::DOMElement* runTypeElement = document->createElement(runTypeTag_);
			runElement->appendChild(runTypeElement);
			xercesc::DOMText* runTypeValue = document->createTextNode(CONVERT_TO_XML(table.getTableName().c_str()));
			runTypeElement->appendChild(runTypeValue);

			xercesc::DOMElement* runNumberElement = document->createElement(runNumberTag_);
			runElement->appendChild(runNumberElement);
			xercesc::DOMText* runNumberValue = document->createTextNode(CONVERT_TO_XML("1"));  // This is dynamic and need to be created when I write the file
			runNumberElement->appendChild(runNumberValue);

			xercesc::DOMElement* runBeginTimestampElement = document->createElement(runBeginTimestampTag_);
			runElement->appendChild(runBeginTimestampElement);
			xercesc::DOMText* runBeginTimestampValue = document->createTextNode(CONVERT_TO_XML(TimeFormatter::getTime().c_str()));  // This is dynamic and
			                                                                                                                        // need to be created
			                                                                                                                        // when I write the
			                                                                                                                        // files
			runBeginTimestampElement->appendChild(runBeginTimestampValue);

			xercesc::DOMElement* locationElement = document->createElement(locationTag_);
			runElement->appendChild(locationElement);
			xercesc::DOMText* locationValue = document->createTextNode(CONVERT_TO_XML("CERN P5"));  // This is dynamic and need to be created
			                                                                                        // when I write the file
			locationElement->appendChild(locationValue);
			//</RUN>

			xercesc::DOMElement* datasetElement = document->createElement(datasetTag_);
			rootElement->appendChild(datasetElement);
			//<PART>
			xercesc::DOMElement* partElement = document->createElement(partTag_);
			datasetElement->appendChild(partElement);

			xercesc::DOMElement* nameLabelElement = document->createElement(nameLabelTag_);
			partElement->appendChild(nameLabelElement);
			xercesc::DOMText* nameLabelValue = document->createTextNode(CONVERT_TO_XML("CMS--ROOT"));
			nameLabelElement->appendChild(nameLabelValue);

			xercesc::DOMElement* kindOfPartElement = document->createElement(kindOfPartTag_);
			partElement->appendChild(kindOfPartElement);
			xercesc::DOMText* kindOfPartValue = document->createTextNode(CONVERT_TO_XML("Detector ROOT"));
			kindOfPartElement->appendChild(kindOfPartValue);

			xercesc::DOMElement* versionElement = document->createElement(versionTag_);
			datasetElement->appendChild(versionElement);
			xercesc::DOMText* versionValue = document->createTextNode(CONVERT_TO_XML(table.getView().getVersion().version()));
			versionElement->appendChild(versionValue);

			xercesc::DOMElement* commentDescriptionElement = document->createElement(commentDescriptionTag_);
			datasetElement->appendChild(commentDescriptionElement);
			xercesc::DOMText* commentDescriptionValue = document->createTextNode(CONVERT_TO_XML(table.getView().getComment().c_str()));
			commentDescriptionElement->appendChild(commentDescriptionValue);

			xercesc::DOMElement* createdByUserElement = document->createElement(createdByUserTag_);
			datasetElement->appendChild(createdByUserElement);
			xercesc::DOMText* createdByUserValue = document->createTextNode(CONVERT_TO_XML(table.getView().getAuthor().c_str()));
			createdByUserElement->appendChild(createdByUserValue);
			// for(TableView::iterator it=table.getView().begin();
			// it!=table.getView().end(); it++)

			for(unsigned int row = 0; row < table.getView().getNumberOfRows(); row++)
			{
				xercesc::DOMElement* dataElement = document->createElement(dataTag_);
				datasetElement->appendChild(dataElement);

				for(unsigned int col = 0; col < table.getView().getNumberOfColumns(); col++)
				{
					xercesc::DOMElement* element = document->createElement(CONVERT_TO_XML(table.getView().getColumnInfo(col).getStorageName().c_str()));
					dataElement->appendChild(element);
					xercesc::DOMText* value = document->createTextNode(CONVERT_TO_XML(table.getView().getDataView()[row][col].c_str()));
					element->appendChild(value);
				}
			}

			outputXML(document, configFile);

			document->release();
		}
		catch(const xercesc::OutOfMemoryException&)
		{
			XERCES_STD_QUALIFIER cerr << "OutOfMemoryException" << XERCES_STD_QUALIFIER endl;
			// errorCode = 5;
		}
		catch(const xercesc::DOMException& e)
		{
			XERCES_STD_QUALIFIER cerr << "DOMException code is:  " << e.code << XERCES_STD_QUALIFIER endl;
			// errorCode = 2;
		}
		catch(const xercesc::XMLException& e)
		{
			std::string message = XML_TO_STRING(e.getMessage());
			__COUT__ << "Error Message: " << message << std::endl;
			// return 1;
			return message;
		}
		catch(...)
		{
			XERCES_STD_QUALIFIER cerr << "An error occurred creating the document" << XERCES_STD_QUALIFIER endl;
			// errorCode = 3;
		}
	}  // (inpl != 0)
	else
	{
		XERCES_STD_QUALIFIER cerr << "Requested implementation is not supported" << XERCES_STD_QUALIFIER endl;
		// errorCode = 4;
	}

	terminatePlatform();
	return configFile;
}

//    return errorCode;

//==============================================================================
void ConfigurationHandler::outputXML(xercesc::DOMDocument* pmyDOMDocument, std::string fileName)
{
	std::string directory = fileName.substr(0, fileName.rfind("/") + 1);
	__COUT__ << "Saving XML to " << fileName << " in directory: " << directory << std::endl;

	mkdir(directory.c_str(), 0755);

	// Return the first registered implementation that has the desired features. In this
	// case, we are after a DOM implementation that has the LS feature... or Load/Save.
	// DOMImplementation *implementation =
	// DOMImplementationRegistry::getDOMImplementation(L"LS");
	xercesc::DOMImplementation* implementation = xercesc::DOMImplementationRegistry::getDOMImplementation(CONVERT_TO_XML("LS"));

#if _XERCES_VERSION >= 30000
	// Create a DOMLSSerializer which is used to serialize a DOM tree into an XML
	// document.
	xercesc::DOMLSSerializer* serializer = ((xercesc::DOMImplementationLS*)implementation)->createLSSerializer();

	// Make the output more human readable by inserting line feeds.

	if(serializer->getDomConfig()->canSetParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true))
		serializer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);

	// The end-of-line sequence of characters to be used in the XML being written out.
	// serializer->setNewLine(CONVERT_TO_XML("\r\n"));

	// Convert the path into Xerces compatible XMLCh*.

	// Specify the target for the XML output.
	xercesc::XMLFormatTarget* formatTarget = new xercesc::LocalFileFormatTarget(CONVERT_TO_XML(fileName));

	// Create a new empty output destination object.
	xercesc::DOMLSOutput* output = ((xercesc::DOMImplementationLS*)implementation)->createLSOutput();

	// Set the stream to our target.
	output->setByteStream(formatTarget);

	// Write the serialized output to the destination.
	serializer->write(pmyDOMDocument, output);

#else
	xercesc::DOMWriter* serializer = ((xercesc::DOMImplementationLS*)implementation)->createDOMWriter();

	serializer->setFeature(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);

	/*
	Choose a location for the serialized output. The 3 options are:
	    1) StdOutFormatTarget     (std output stream -  good for debugging)
	    2) MemBufFormatTarget     (to Memory)
	    3) LocalFileFormatTarget  (save to file)
	    (Note: You'll need a different header file for each one)
	 */
	// XMLFormatTarget* pTarget = new StdOutFormatTarget();
	// Convert the path into Xerces compatible XMLCh*.

	xercesc::XMLFormatTarget* formatTarget = new xercesc::LocalFileFormatTarget(CONVERT_TO_XML(fileName.c_str()));

	// Write the serialized output to the target.
	serializer->writeNode(formatTarget, *pmyDOMDocument);

#endif

	// Cleanup.
	serializer->release();

	delete formatTarget;

#if _XERCES_VERSION >= 30000

	output->release();

#endif
	__COUT__ << "Done writing " << std::endl;
}

//==============================================================================
std::string ConfigurationHandler::writeXML(const TableBase* table) { return writeXML(*table); }

//==============================================================================
std::string ConfigurationHandler::getXMLFileName(const TableBase& table, TableVersion version)
{
	std::stringstream fileName;
	fileName << getXMLDir(&table) << version << '/' << table.getTableName() << "_v" << version << ".xml";
	return fileName.str();
}

//==============================================================================
std::string ConfigurationHandler::getXMLDir(const TableBase* table)
{
	return std::string(__ENV__("CONFIGURATION_DATA_PATH")) + '/' + table->getTableName() + '/';
}
