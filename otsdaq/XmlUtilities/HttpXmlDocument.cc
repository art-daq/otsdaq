#include "otsdaq/XmlUtilities/HttpXmlDocument.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/XmlUtilities/ConvertFromXML.h"
#include "otsdaq/XmlUtilities/ConvertToXML.h"
// #include "otsdaq_cmsoutertracker/otsdaq-cmsoutertracker/Ph2_ACF/Utils/MessageTools.h"

#include <stdexcept>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentType.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
// #include <xercesc/dom/DOMLSSerializer.hpp>
// #include <xercesc/dom/DOMLSOutput.hpp>
#include <xercesc/dom/DOMNodeIterator.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/validators/common/Grammar.hpp>

#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLUni.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>

#include <iostream>
#include <list>
#include <sstream>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace ots;

//==============================================================================
// HttpXmlDocument::HttpXmlDocument
//	Constructor to initialize XML theDocument_ for ots.
//	The XML theDocument_ can be written to xdaq output stream to client
//
//	theDocument_ result:
//	<ROOT>
//		<HEADER>
//			--optional with value <CookieCode>, <DisplayName> added in constructor
//		<DATA>
//			--optional data elements with value and any field name
//
HttpXmlDocument::HttpXmlDocument(std::string cookieCode, std::string displayName)
    : XmlDocument("ROOT")
    , headerElement_(0)
    , dataElement_(0)
    , headerTagName_("HEADER")
    , dataTagName_("DATA")
    , cookieCodeTagName_("CookieCode")
    , displayNameTagName_("DisplayName")
{
	// __COUT__ << "in" << std::endl;
	//<HEADER>
	if(cookieCode != "" || displayName != "")  // add header
	{
		headerElement_ = theDocument_->createElement(CONVERT_TO_XML(headerTagName_));
		rootElement_->appendChild(headerElement_);
		if(cookieCode != "")  // add cookie code to header
			addTextElementToParent(cookieCodeTagName_, cookieCode, headerElement_);
		if(displayName != "")  // add display name to header
			addTextElementToParent(displayNameTagName_, displayName, headerElement_);
	}

	//<DATA>
	dataElement_ = theDocument_->createElement(CONVERT_TO_XML(dataTagName_));
	rootElement_->appendChild(dataElement_);
	// __COUT__ << "out" << std::endl;
}

//==============================================================================
HttpXmlDocument::HttpXmlDocument(const HttpXmlDocument& doc)
    : XmlDocument(doc)
    , headerElement_(0)
    , dataElement_(0)
    , headerTagName_(doc.headerTagName_)
    , dataTagName_(doc.dataTagName_)
    , cookieCodeTagName_(doc.cookieCodeTagName_)
    , displayNameTagName_(doc.displayNameTagName_)
{
	// __COUT__ << "in" << std::endl;
	*this = doc;
	// __COUT__ << "out" << std::endl;
}

//==============================================================================
HttpXmlDocument& HttpXmlDocument::operator=(const HttpXmlDocument& doc)
{
	// __COUT__ << "in" << std::endl;
	recursiveElementCopy(doc.rootElement_, rootElement_);
	// __COUT__ << "in" << std::endl;
	if(doc.headerElement_ != 0)
		headerElement_ = (xercesc::DOMElement*)rootElement_->getElementsByTagName(CONVERT_TO_XML(headerTagName_))->item(0);
	// __COUT__ << "in" << std::endl;
	dataElement_ = (xercesc::DOMElement*)rootElement_->getElementsByTagName(CONVERT_TO_XML(dataTagName_))->item(0);
	// __COUT__ << "out" << std::endl;
	return *this;
}

//==============================================================================
HttpXmlDocument::~HttpXmlDocument(void) {}

void HttpXmlDocument::setHeader(std::string cookieCode, std::string displayName)
{
	if(headerElement_)
	{
		std::stringstream ss;
		ss << __COUT_HDR_FL__
		   << "Can NOT set header to doc with a header! Only allowed for docs without "
		      "header element.";
		__SS_THROW__;
	}

	if(cookieCode != "" || displayName != "")  // add header
	{
		headerElement_ = theDocument_->createElement(CONVERT_TO_XML(headerTagName_));
		rootElement_->appendChild(headerElement_);
		if(cookieCode != "")  // add cookie code to header
			addTextElementToParent(cookieCodeTagName_, cookieCode, headerElement_);
		if(displayName != "")  // add display name to header
			addTextElementToParent(displayNameTagName_, displayName, headerElement_);
	}
}

//==============================================================================
xercesc::DOMElement* HttpXmlDocument::addTextElementToData(const std::string& childName, const std::string& childValue)
{
	// __COUT__ << "in - " << childName << " value: " << childValue <<std::endl << std::endl;
	return addTextElementToParent(childName, childValue, dataElement_);
}

//==============================================================================
xercesc::DOMElement* HttpXmlDocument::addBinaryStringToData(const std::string& childName, const std::string& binary)
{
	std::string convertStr = "";
	char        hexStr[3];
	for(unsigned int i = 0; i < binary.length(); ++i)
	{
		// for every byte make hex
		sprintf(hexStr, "%2.2X", ((unsigned char)binary[i]));
		hexStr[2] = '\0';
		convertStr += hexStr;
	}

	return addTextElementToParent(childName, convertStr, dataElement_);
}

//==============================================================================
// HttpXmlDocument::getChildrenCount
//	get count of children ignoring text nodes.
unsigned int HttpXmlDocument::getChildrenCount(xercesc::DOMElement* parent)
{
	if(!parent)
		parent = dataElement_;  // default to data element

	xercesc::DOMNodeList* nodeList = parent->getChildNodes();  // get all children	within parent
	unsigned int          count    = 0;

	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
	{
		if(nodeList->item(i)->getNodeType() != xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			++count;
	}

	return count;
}

//==============================================================================
// HttpXmlDocument::removeDataElement
//	Remove child and child's sub-tree from dataElement. The child is
//	identified with dataChildIndex.
void HttpXmlDocument::removeDataElement(unsigned int dataChildIndex)
{
	xercesc::DOMNodeList* nodeList = dataElement_->getChildNodes();  // get all children	within data

	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
	{
		if(nodeList->item(i)->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			continue;

		if(!dataChildIndex)  // remove
		{
			recursiveRemoveChild((xercesc::DOMElement*)(nodeList->item(i)), dataElement_);
			return;
		}

		--dataChildIndex;  // find proper child
	}

	// here, then child doesnt exist
} // end removeDataElement()

//==============================================================================
// HttpXmlDocument::addXmlData
//	Append <DATA> from xmldoc to this XML doc
void HttpXmlDocument::copyDataChildren(HttpXmlDocument& document)
{
	// add all first level child elements of data and recurse on them
	xercesc::DOMNodeList* nodeList = document.dataElement_->getChildNodes();  // get all children	within data
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
	{
		if(nodeList->item(i)->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			continue;

		recursiveAddElementToParent((xercesc::DOMElement*)(nodeList->item(i)), dataElement_, true);
	}
} // end copyDataChildren()

//==============================================================================
// HttpXmlDocument::outputXmlDocument
//	recurse through XML theDocument_ and std out and output to stream parameter if not
// null
void HttpXmlDocument::outputXmlDocument(std::ostringstream* out, bool dispStdOut, bool allowWhiteSpace)
{
	recursiveOutputXmlDocument(theDocument_->getDocumentElement(), out, dispStdOut, "", allowWhiteSpace);
} // end outputXmlDocument()

//==============================================================================
// HttpXmlDocument::recursiveOutputXmlDocument
//	recursively printout XML theDocument_ to std out and output stream if not null
void HttpXmlDocument::recursiveOutputXmlDocument(
    xercesc::DOMElement* currEl, std::ostringstream* out, bool dispStdOut, std::string tabStr, bool allowWhiteSpace)
{
	// open field tag
	if(dispStdOut)
		std::cout << tabStr << "<" << XML_TO_CHAR(currEl->getNodeName());
	if(out)
		*out << tabStr << "<" << XML_TO_CHAR(currEl->getNodeName());

	// insert value if text node child
	if(currEl->getFirstChild() != NULL && currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // if has a text node first, insert as value
	                                                                                                              // attribute
	{
		if(dispStdOut)
			std::cout << " value='" << StringMacros::escapeString(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()), allowWhiteSpace) << "'";
		if(out)
			*out << " value='" << StringMacros::escapeString(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()), allowWhiteSpace) << "'";
	}

	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children

	// close opening field tag
	if(dispStdOut)
		std::cout << ((nodeList->getLength() == 0 || (nodeList->getLength() == 1 && currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE))
		                  ? "/"
		                  : "")
		          << ">"
		          << " len:" << nodeList->getLength() << std::endl;
	if(out)
	{
		//  		*out << ((nodeList->getLength() == 0 ||
		// 		          (nodeList->getLength() == 1 &&
		// 		           currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE))
		// 		             ? "/"
		// 		             : "")
		// 		     << ">" << std::endl;
		// Dario-style...
		std::string outText = "";
		if(currEl->getFirstChild() != NULL && currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)
			outText = StringMacros::escapeString(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()), allowWhiteSpace);
		{
			if(darioXMLStyle_ && !(std::string(XML_TO_CHAR(currEl->getNodeName())) == "ROOT" || std::string(XML_TO_CHAR(currEl->getNodeName())) == "HEADER" ||
			                       std::string(XML_TO_CHAR(currEl->getNodeName())) == "DATA" || std::string(XML_TO_CHAR(currEl->getNodeName())) == "node" ||
			                       std::string(XML_TO_CHAR(currEl->getNodeName())) == "nodes"))
			{
				*out << ">" << outText << "</" << XML_TO_CHAR(currEl->getNodeName()) << ">" << std::endl;
			}
			else
			{
				*out << ((nodeList->getLength() == 0 || (nodeList->getLength() == 1 && currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE))
				             ? "/"
				             : "")
				     << ">" << std::endl;
			}
		}
		// Dario-style...
	}
	// insert children
	std::string newTabStr = tabStr + "\t";
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() != xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			recursiveOutputXmlDocument((xercesc::DOMElement*)(nodeList->item(i)), out, dispStdOut, newTabStr, allowWhiteSpace);

	if(currEl == dataElement_ && 	//append the data string stream 
			dataSs_.str().length()) //(for large data blocks that do not need to be escaped)
	{
		if(dispStdOut)
			std::cout << dataSs_.str() << std::endl;
		if(out)
			*out << dataSs_.str() << std::endl;
	}

	// close tag if children
	if(nodeList->getLength() > 1 || (nodeList->getLength() == 1 && currEl->getFirstChild()->getNodeType() != xercesc::DOMNode::TEXT_NODE))
	{
		if(dispStdOut)
			std::cout << tabStr << "</" << XML_TO_CHAR(currEl->getNodeName()) << ">" << std::endl;
		if(out)
			*out << tabStr << "</" << XML_TO_CHAR(currEl->getNodeName()) << ">" << std::endl;
	}
} // end recursiveOutputXmlDocument()

//==============================================================================
// HttpXmlDocument::getMatchingValue
//  returns the value for field found occurance number of times
//  returns empty std::string "" if field was not found
std::string HttpXmlDocument::getMatchingValue(const std::string& field, const unsigned int occurance)
{
	unsigned int count = 0;
	return recursiveFindElementValue(theDocument_->getDocumentElement(), field, occurance, count);
}

//==============================================================================
// HttpXmlDocument::recursiveFindElement
//  recursively searches and returns the value for field found occurance number of times
std::string HttpXmlDocument::recursiveFindElementValue(xercesc::DOMElement* currEl, const std::string& field, const unsigned int occurance, unsigned int& count)
{
	if(XML_TO_CHAR(currEl->getNodeName()) == field && occurance == count++)  // found, done!!
	{
		if(currEl->getFirstChild() != NULL && currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // if has a text node first, return as
		                                                                                                              // value attribute
			return StringMacros::escapeString(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()));
		else
			return "";  // empty value attribute
	}

	std::string retStr;
	// look through children recursively
	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() != xercesc::DOMNode::TEXT_NODE)  // ignore text node children
		{
			retStr = recursiveFindElementValue((xercesc::DOMElement*)(nodeList->item(i)), field, occurance, count);
			if(retStr != "")
				return retStr;  // found among children already, done
				                // else continue search within children recursively
		}
	return "";  // nothing found
}

//==============================================================================
// HttpXmlDocument::getAllMatchingValues
//  returns all of the values found for the field in a vector
//  if none found vector will have size 0
void HttpXmlDocument::getAllMatchingValues(const std::string& field, std::vector<std::string>& retVec)
{
	recursiveFindAllElements(theDocument_->getDocumentElement(), field, &retVec);
}

//==============================================================================
// HttpXmlDocument::recursiveFindElement
//  recursively searches and returns the value for field found occurance number of times
void HttpXmlDocument::recursiveFindAllElements(xercesc::DOMElement* currEl, const std::string& field, std::vector<std::string>* retVec)
{
	if(XML_TO_CHAR(currEl->getNodeName()) == field && currEl->getFirstChild() != NULL &&
	   currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // if has a text node first, return as value
	                                                                           // attribute
		retVec->push_back(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()));

	// look through children recursively
	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() != xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			recursiveFindAllElements((xercesc::DOMElement*)(nodeList->item(i)), field, retVec);
}

//==============================================================================
// HttpXmlDocument::getMatchingElement
//  returns the element for field found occurance number of times
//  returns null if field was not found
xercesc::DOMElement* HttpXmlDocument::getMatchingElement(const std::string& field, const unsigned int occurance)
{
	return getMatchingElementInSubtree(theDocument_->getDocumentElement(), field, occurance);
}

//==============================================================================
// HttpXmlDocument::getMatchingElementInSubtree
//  returns the element for field found occurance number of times within the subtree
//		specified by parentEl
//  returns null if field was not found
xercesc::DOMElement* HttpXmlDocument::getMatchingElementInSubtree(xercesc::DOMElement* parentEl, const std::string& field, const unsigned int occurance)
{
	unsigned int count = 0;
	return recursiveFindElement(parentEl, field, occurance, count);
}

//==============================================================================
// HttpXmlDocument::recursiveFindElement
//  recursively searches and returns the element for field found occurance number of times
xercesc::DOMElement* HttpXmlDocument::recursiveFindElement(xercesc::DOMElement* currEl,
                                                           const std::string&   field,
                                                           const unsigned int   occurance,
                                                           unsigned int&        count)
{
	if(XML_TO_CHAR(currEl->getNodeName()) == field && occurance == count++)  // found, done!!
	{
		if(currEl->getFirstChild() != NULL && currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // if has a text node first, return as
		                                                                                                              // value attribute
			return currEl;
		else
			return 0;  // empty value attribute
	}

	xercesc::DOMElement* retEl;
	// look through children recursively
	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() != xercesc::DOMNode::TEXT_NODE)  // ignore text node children
		{
			retEl = recursiveFindElement((xercesc::DOMElement*)(nodeList->item(i)), field, occurance, count);
			if(retEl)
				return retEl;  // found among children already, done
				               // else continue search within children recursively
		}
	return 0;  // nothing found
}

//==============================================================================
// HttpXmlDocument::recursiveAddElementToParent
//	add currEl and its children tree to parentEl
//	note: attributes are not considered here
void HttpXmlDocument::recursiveAddElementToParent(xercesc::DOMElement* child, xercesc::DOMElement* parent, bool html)
{
	std::string childText = "";

	std::string childName = XML_TO_CHAR(child->getNodeName());  // XML_TO_CHAR(currEl->getNodeName());

	if(child->getFirstChild() != NULL && child->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // if has a text node first, insert as value
	                                                                                                            // attribute
	{
		childText = XML_TO_CHAR(child->getFirstChild()->getNodeValue());
		if(html)
			childText = StringMacros::escapeString(childText);
	}
	// __COUT__<< "childName " << childName <<  " childText " <<
	// childText << std::endl;

	// insert child
	xercesc::DOMElement* newParent = addTextElementToParent(childName, childText, parent);

	// insert rest of child tree
	xercesc::DOMNodeList* nodeList = child->getChildNodes();  // get all children	of child
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
	{
		if(nodeList->item(i)->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			continue;

		recursiveAddElementToParent((xercesc::DOMElement*)(nodeList->item(i)), newParent, html);
	}
}

//==============================================================================
// HttpXmlDocument::getAllMatchingElements
//  returns all of the values found for the field in a vector
//  if none found vector will have size 0
void HttpXmlDocument::getAllMatchingElements(const std::string& field, std::vector<xercesc::DOMElement*>& retVec)
{
	recursiveFindAllElements(theDocument_->getDocumentElement(), field, &retVec);
}

//==============================================================================
// HttpXmlDocument::recursiveFindElement
//  recursively searches and returns the value for field found occurance number of times
void HttpXmlDocument::recursiveFindAllElements(xercesc::DOMElement* currEl, const std::string& field, std::vector<xercesc::DOMElement*>* retVec)
{
	if(XML_TO_CHAR(currEl->getNodeName()) == field && currEl->getFirstChild() != NULL &&
	   currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // if has a text node first, return as value
	                                                                           // attribute
		retVec->push_back(currEl);

	// look through children recursively
	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() != xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			recursiveFindAllElements((xercesc::DOMElement*)(nodeList->item(i)), field, retVec);
}
//==============================================================================
// HttpXmlDocument::StringMacros::escapeString
//	convert quotes to html quote characters &apos = ' and &quot = "
//	remove new line characters
//	and remove white space (so that read from file white space artifact removed)
/*
std::string	HttpXmlDocument::StringMacros::escapeString(string inString)
{
    unsigned int ws = -1;
    for(unsigned int i=0;i<inString.length();++i)
        if(inString[i] != ' ')
        {
            if(inString[i] == '\r' || inString[i] == '\n') //remove new line chars
            {
                inString.erase(i,1);        // replace special character with ;
                --i; //step back so next char to check is correct
                continue;
            }

            ws = i; //last non white space char
            if(inString[i] == '\"' || inString[i] == '\'')
            {
                inString.insert(i,(inString[i] == '\'')?"&apos":"&quot"); //insert \
before quotes inString.replace(i+5,1,1,';');        // replace special character with ;
                i+=5; //skip to next char to check
            }
        }

    if(ws == (unsigned int)-1) return ""; //empty std::string since all white space
    return inString.substr(0,ws+1); //trim right white space
}
*/
//==============================================================================
// loadXmlDocument
//	returns false if file does not exist
bool HttpXmlDocument::loadXmlDocument(const std::string& filePath)
{
	// __COUT__<< "Loading theDocument_ from file: " << filePath <<
	// std::endl;

	struct stat fileStatus;

	if(stat(filePath.c_str(), &fileStatus) != 0)
	{
		//__SS__ << "File not accessible: " << filePath << std::endl;
		//__SS_THROW__;
		return false;  // not an error for file to not exist
	}

	// reset xml platform and theDocument_
	terminatePlatform();
	initPlatform();

	xercesc::XercesDOMParser* parser = new xercesc::XercesDOMParser;
	// Configure xercesc::DOM parser.
	parser->setValidationScheme(xercesc::XercesDOMParser::Val_Auto);
	parser->setDoNamespaces(true);
	parser->setDoSchema(true);
	parser->useCachedGrammarInParse(false);

	try
	{
		parser->parse(filePath.c_str());

		// theDocument_ memory object owned by the parent parser object
		theDocument_ = parser->adoptDocument();  // instead of getDocument() so parser
		                                         // will not free theDocument_ when
		                                         // released

		// Get the top-level element: Name is "root". No attributes for "root"
		rootElement_ = theDocument_->getDocumentElement();
		if(!rootElement_)
		{
			__SS__ << "empty XML theDocument_: " << filePath << std::endl;
			__SS_THROW__;
			throw(std::runtime_error("empty XML theDocument_"));
		}

		recursiveFixTextFields(rootElement_);  // remove space and new lines from value attribute

		xercesc::DOMNodeList* nodeList = theDocument_->getElementsByTagName(CONVERT_TO_XML(headerTagName_));
		if(nodeList->getLength())  // may not always be header element
			headerElement_ = (xercesc::DOMElement*)(theDocument_->getElementsByTagName(CONVERT_TO_XML(headerTagName_))->item(0));
		else
			headerElement_ = 0;

		dataElement_ = (xercesc::DOMElement*)(theDocument_->getElementsByTagName(CONVERT_TO_XML(dataTagName_))->item(0));  // always is data
	}
	catch(xercesc::XMLException& e)
	{
		__SS__ << "Error parsing file: " << filePath << std::endl;
		__SS_THROW__;
		__COUT__ << "Error parsing file." << std::endl;
		return false;
	}
	delete parser;

	return true;
}

//==============================================================================
// HttpXmlDocument::recursiveFixTextFields
//	recursively printout XML theDocument_ to std out and output stream if not null
void HttpXmlDocument::recursiveFixTextFields(xercesc::DOMElement* currEl)
{
	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children

	// recurse through children
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() == xercesc::DOMNode::TEXT_NODE)  // fix text nodes
			((xercesc::DOMElement*)(nodeList->item(i)))
			    ->setTextContent(CONVERT_TO_XML(  // change text value to escaped version
			        StringMacros::escapeString(XML_TO_CHAR(((xercesc::DOMElement*)(nodeList->item(i)))->getNodeValue()))));
		else
			recursiveFixTextFields((xercesc::DOMElement*)(nodeList->item(i)));
}

//==============================================================================
// HttpXmlDocument::addDataElement
//	Add field/value element to XML doc at parentIndexArray (with depth of parent indicated
// by parentIndexArraySize) 	If parentIndexArray = NULL, element is added with <DATA>
// parent 		otherwise, parentIndexArray indicates the parent within the node list for
//<DATA> where 		the element will be added
// 	On Success, The child index of the added element with respect to the parent is
// returned and can be used to add
//		children to the new element
//	On Failure, return -1
/*
xercesc::DOMElement* HttpXmlDocument::addDataElement ( std::string field, std::string
value, unsigned int *parentIndexArray, unsigned int parentIndexArraySize)
{

    //__COUT__ << "field: " << field << ", value: " << value << ",
parent: " << parentIndexArraySize << std::endl;

    xercesc::DOMElement* parentEl = dataElement_; // initialize parent to <DATA>

    if(parentIndexArray) //if there passed an array find parent relative to data element
    {
        //__COUT__<< "Using Parent Index Array" << std::endl;

        xercesc::DOMNodeList *nodeList;

        //iterate through nested parents based on parentIndexArray
        unsigned int tmpi,cntNotTxt;
        for(unsigned int i=0;i<parentIndexArraySize;++i)
        {
            nodeList = parentEl->getChildNodes();	 //get all children
            cntNotTxt = 0;

            //get cntNotTxt to proper non text node
            for(tmpi=0;tmpi<nodeList->getLength();++tmpi)
            {
                if(((xercesc::DOMElement*)(nodeList->item(tmpi)))->getNodeType() ==
xercesc::DOMNode::TEXT_NODE) continue; //skip text nodes

                if(cntNotTxt == parentIndexArray[i]) break; //at proper parent node!
                ++cntNotTxt; //else look for next
            }

            //in theory, only first child can be text - ignore text node children
            //if(parentEl->getFirstChild() != NULL &&
parentEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE) ++tmpi;

            if(tmpi >= nodeList->getLength())
            {
                __COUT__ << "illegal child index attempted in nested
parents: " << parentIndexArray[i] << ", depth: " << i << ", tmpi: " << tmpi << std::endl;
                return 0; //illegal child index attempted in nested parents
            }

            parentEl = (xercesc::DOMElement*)(nodeList->item(tmpi));
        }
    }

    return addTextElementToParent(field,value,parentEl);
}
*/
