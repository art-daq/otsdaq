#include "otsdaq/XmlUtilities/XmlDocument.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/MessageTools.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/XmlUtilities/ConvertFromXML.h"
#include "otsdaq/XmlUtilities/ConvertToXML.h"

#include <stdexcept>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentType.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
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

#include <boost/regex.hpp>

#include <iostream>
#include <list>
#include <sstream>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace ots;

//==============================================================================
XmlDocument::XmlDocument(const std::string& rootName) : rootTagName_(rootName)
{
	__COUTS__(50) << "in" << std::endl;
	initDocument();

	rootElement_ = theDocument_->getDocumentElement();
	__COUTS__(50) << "out" << std::endl;
}  //end constructor()

//==============================================================================
XmlDocument::XmlDocument(const XmlDocument& doc) : rootTagName_(doc.rootTagName_)
{
	__COUTS__(50) << "in" << std::endl;
	*this = doc;
	__COUTS__(50) << "out" << std::endl;
}  //end constructor()

//==============================================================================
XmlDocument& XmlDocument::operator=(const XmlDocument& doc)
{
	__COUTS__(50) << "in" << std::endl;
	initDocument();
	rootElement_ = theDocument_->getDocumentElement();
	recursiveElementCopy(doc.rootElement_, rootElement_);
	__COUTS__(50) << "out" << std::endl;
	return *this;
}  //end assignment operator

//==============================================================================
XmlDocument::~XmlDocument(void)
{
	__COUTS__(50) << "Xml Destructor" << std::endl;
	terminatePlatform();
}  //end destructor()

//==============================================================================
void XmlDocument::initDocument(void)
{
	initPlatform();

	theImplementation_ =
	    xercesc::DOMImplementationRegistry::getDOMImplementation(CONVERT_TO_XML("Core"));

	if(theImplementation_)
	{
		try
		{
			theDocument_ = theImplementation_->createDocument(
			    CONVERT_TO_XML("http://www.w3.org/2001/XMLSchema-instance"),  // root
			                                                                  // element
			                                                                  // namespace
			                                                                  // URI.
			    CONVERT_TO_XML(rootTagName_),  // root element name
			    0);                            // theDocument_ type object (DTD).
		}
		catch(const xercesc::OutOfMemoryException&)
		{
			__SS__ << "OutOfMemoryException" << __E__;
			__SS_THROW__;
		}
		catch(const xercesc::DOMException& e)
		{
			__SS__ << "DOMException code is:  " << e.code << __E__;
			__SS_THROW__;
		}
		catch(const xercesc::XMLException& e)
		{
			__SS__ << "Error Message: " << XML_TO_CHAR(e.getMessage()) << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "An error occurred creating the theDocument_" << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SS_THROW__;
		}
	}
	else
	{
		__SS__ << "Requested theImplementation_ is not supported" << __E__;
		__SS_THROW__;
	}
	darioXMLStyle_  = false;
	isALeaf_[true]  = "true";
	isALeaf_[false] = "false";
}  //end initDocument()

//==============================================================================
void XmlDocument::initPlatform(void)
{
	try
	{
		xercesc::XMLPlatformUtils::Initialize();  // Initialize Xerces infrastructure
		                                          //__COUT__ << "Initialized new
		                                          // theDocument_" << std::endl;
	}
	catch(xercesc::XMLException& e)
	{
		__COUT__ << "XML toolkit initialization error: " << XML_TO_CHAR(e.getMessage())
		         << std::endl;
	}
}  //end initPlatform()

//==============================================================================
void XmlDocument::terminatePlatform(void)
{
	try
	{
		__COUTS__(50) << "Releasing the document" << std::endl;
		theDocument_->release();
		__COUTS__(50) << "document released" << std::endl;
	}
	catch(...)
	{
		XERCES_STD_QUALIFIER cerr << "An error occurred destroying the theDocument_"
		                          << XERCES_STD_QUALIFIER endl;
	}

	try
	{
		xercesc::XMLPlatformUtils::Terminate();  // Terminate after release of memory
	}
	catch(xercesc::XMLException& e)
	{
		__COUT__ << "XML toolkit teardown error: " << XML_TO_CHAR(e.getMessage())
		         << std::endl;
		// XMLString::release(&message);
	}
}  //end terminatePlatform()

xercesc::DOMElement* XmlDocument::createChildElement(const std::string&   childClass,
                                                     xercesc::DOMElement* parent)
{
	if(parent == nullptr)
	{
		__SS__ << "Illegal Null Parent Pointer!" << __E__;
		__SS_THROW__;
	}
	xercesc::DOMElement* child = nullptr;
	try
	{
		child = theDocument_->createElement(CONVERT_TO_XML(childClass));
	}
	catch(xercesc::DOMException& e)
	{
		__COUT__ << "Can't use the class: " << childClass
		         << " to create the child element because the exception says: "
		         << XML_TO_CHAR(e.getMessage())
		         << ". Very likely you have a name that starts with a number and that's "
		            "not allowed!"
		         << std::endl;
	}
	parent->appendChild(child);

	return child;
}

xercesc::DOMElement* XmlDocument::addAttributeToNode(const std::string&   attributeName,
                                                     const std::string&   attributeValue,
                                                     xercesc::DOMElement* node)
{
	if(node == nullptr)
	{
		__SS__ << "Illegal Null Parent Pointer!" << __E__;
		__SS_THROW__;
	}

	try
	{
		node->setAttribute(CONVERT_TO_XML(attributeName), CONVERT_TO_XML(attributeValue));
	}
	catch(xercesc::DOMException& e)
	{
		__COUT__ << "Can't use the name: " << attributeName
		         << " to create the attribute because the exception says: "
		         << XML_TO_CHAR(e.getMessage())
		         << ". Very likely you have a name that starts with a number and that's "
		            "not allowed!"
		         << std::endl;
	}

	return node;
}

//==============================================================================
/// addTextElementToParent
///	add to parent by pointer to parent
///	returns pointer to element that is added
xercesc::DOMElement* XmlDocument::addTextElementToParent(const std::string&   childName,
                                                         const std::string&   childText,
                                                         xercesc::DOMElement* parent)
{
	if(parent == nullptr)
	{
		__SS__ << "Illegal Null Parent Pointer!" << __E__;
		__SS_THROW__;
	}
	xercesc::DOMElement* child = nullptr;
	try
	{
		child = theDocument_->createElement(CONVERT_TO_XML(childName));
	}
	catch(xercesc::DOMException& e)
	{
		__COUT__ << "Can't use the name: " << childName
		         << " to create the child element because the exception says: "
		         << XML_TO_CHAR(e.getMessage())
		         << ". Very likely you have a name that starts with a number and that's "
		            "not allowed!"
		         << std::endl;
	}
	parent->appendChild(child);

	try
	{
		child->appendChild(theDocument_->createTextNode(CONVERT_TO_XML(childText)));
	}
	catch(...)  // sometimes see TranscodingException
	{
		__COUT__ << StringMacros::stackTrace() << std::endl;
		__COUT_ERR__ << "Error caught attempting to create a text node for this text: "
		             << childText << ". Converting instead to 'Illegal text..'"
		             << std::endl;
		child->appendChild(theDocument_->createTextNode(
		    CONVERT_TO_XML("Illegal text content blocked.")));
	}

	return child;
}  //end addTextElementToParent()

//==============================================================================
/// addTextElementToParent
///	add to parent by instance number of parent name
///	returns pointer to element that is added
xercesc::DOMElement* XmlDocument::addTextElementToParent(const std::string& childName,
                                                         const std::string& childText,
                                                         const std::string& parentName,
                                                         unsigned int       parentIndex)
{
	xercesc::DOMNodeList* nodeList =
	    theDocument_->getElementsByTagName(CONVERT_TO_XML(parentName));

	if(parentIndex >= nodeList->getLength())
	{
		__COUT__ << "WARNING: Illegal parent index attempted in tags with name: "
		         << parentName << ", index: " << parentIndex << std::endl;
		return 0;  // illegal index attempted
	}

	return addTextElementToParent(
	    childName, childText, (xercesc::DOMElement*)(nodeList->item(parentIndex)));
}  //end addTextElementToParent()

//==============================================================================
void XmlDocument::copyDocument(const xercesc::DOMDocument* toCopy,
                               xercesc::DOMDocument*       copy)
{
	recursiveElementCopy(toCopy->getDocumentElement(), copy->getDocumentElement());
}  //end copyDocument()

//==============================================================================
void XmlDocument::recursiveElementCopy(const xercesc::DOMElement* toCopy,
                                       xercesc::DOMElement*       copy)
{
	xercesc::DOMNodeList* nodeListToCopy =
	    toCopy->getChildNodes();  // get all children of the list to copy
	xercesc::DOMNode*     iNode;
	xercesc::DOMDocument* copyDocument = copy->getOwnerDocument();
	for(unsigned int i = 0; i < nodeListToCopy->getLength(); i++)
	{
		iNode                      = nodeListToCopy->item(i);
		xercesc::DOMElement* child = copyDocument->createElement(iNode->getNodeName());
		copy->appendChild(child);
		if(child->getFirstChild() != NULL)
		{
			if(iNode->getFirstChild() != 0 &&
			   iNode->getFirstChild()->getNodeType() ==
			       xercesc::DOMNode::TEXT_NODE)  // if has a text node first, insert as
			                                     // value attribute
			{
				child->appendChild(
				    copyDocument->createTextNode(child->getFirstChild()->getNodeValue()));
			}
			recursiveElementCopy((xercesc::DOMElement*)(iNode), child);
		}
	}
}  //end recursiveElementCopy()

//==============================================================================
/// XmlDocument::outputXmlDocument
///	recurse through XML theDocument_ and std out and output to stream parameter if not
/// null
void XmlDocument::outputXmlDocument(std::ostringstream* out, bool dispStdOut)
{
	recursiveOutputXmlDocument(theDocument_->getDocumentElement(), out, dispStdOut);
}  //end outputXmlDocument()

//==============================================================================
void XmlDocument::setDocument(xercesc::DOMDocument* doc) { theDocument_ = doc; }
//==============================================================================
/// XmlDocument::recursiveOutputXmlDocument
///	recursively printout XML theDocument_ to std out and output stream if not null
void XmlDocument::recursiveOutputXmlDocument(xercesc::DOMElement* currEl,
                                             std::ostringstream*  out,
                                             bool                 dispStdOut,
                                             const std::string&   tabStr)
{
	// open field tag
	if(dispStdOut)
	{
		__COUT__ << tabStr << "<" << XML_TO_CHAR(currEl->getNodeName());
	}
	if(out)
	{
		*out << tabStr << "<" << XML_TO_CHAR(currEl->getNodeName());
	}

	// insert value if text node child
	if(currEl->getFirstChild() != NULL &&
	   currEl->getFirstChild()->getNodeType() ==
	       xercesc::DOMNode::TEXT_NODE)  // if has a text node first, insert as value
	                                     // attribute
	{
		if(dispStdOut)
			std::cout << " value='"
			          << StringMacros::escapeString(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()), true) << "'";
		if(out)
			*out << " value='" << StringMacros::escapeString(XML_TO_CHAR(currEl->getFirstChild()->getNodeValue()), true)
			     << "'";
	}

	xercesc::DOMNamedNodeMap* attrList = currEl->getAttributes();
	if(attrList)
	{
		for(XMLSize_t ii = 0; ii < attrList->getLength(); ++ii)
		{
			if(dispStdOut)
				std::cout << " " << XML_TO_CHAR(attrList->item(ii)->getNodeName()) << "='"
				          << StringMacros::escapeString(XML_TO_CHAR(attrList->item(ii)->getNodeValue()),true) << "'";
			if(out)
				*out << " " << XML_TO_CHAR(attrList->item(ii)->getNodeName()) << "='"
				     << StringMacros::escapeString(XML_TO_CHAR(attrList->item(ii)->getNodeValue()), true) << "'";
		}
	}

	xercesc::DOMNodeList* nodeList = currEl->getChildNodes();  // get all children

	// close opening field tag
	if(dispStdOut)
		std::cout << ((nodeList->getLength() == 0 ||
		               (nodeList->getLength() == 1 &&
		                currEl->getFirstChild()->getNodeType() ==
		                    xercesc::DOMNode::TEXT_NODE))
		                  ? "/"
		                  : "")
		          << ">" << std::endl;
	if(out)
		*out << ((nodeList->getLength() == 0 ||
		          (nodeList->getLength() == 1 &&
		           currEl->getFirstChild()->getNodeType() == xercesc::DOMNode::TEXT_NODE))
		             ? "/"
		             : "")
		     << ">" << std::endl;

	// insert children
	std::string newTabStr = tabStr + "\t";
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		if(nodeList->item(i)->getNodeType() !=
		   xercesc::DOMNode::TEXT_NODE)  // ignore text node children
			recursiveOutputXmlDocument(
			    (xercesc::DOMElement*)(nodeList->item(i)), out, dispStdOut, newTabStr);

	// close tag if children
	if(nodeList->getLength() > 1 ||
	   (nodeList->getLength() == 1 &&
	    currEl->getFirstChild()->getNodeType() != xercesc::DOMNode::TEXT_NODE))
	{
		if(dispStdOut)
			__COUT__ << tabStr << "</" << XML_TO_CHAR(currEl->getNodeName()) << ">"
			         << std::endl;
		if(out)
			*out << tabStr << "</" << XML_TO_CHAR(currEl->getNodeName()) << ">"
			     << std::endl;
	}
}  //end recursiveOutputXmlDocument()

//==============================================================================
/// XmlDocument::recursiveRemoveChild
///	remove child and all of child's sub-tree from parent
void XmlDocument::recursiveRemoveChild(xercesc::DOMElement* childEl,
                                       xercesc::DOMElement* parentEl)
{
	// release child's children first
	xercesc::DOMNodeList* nodeList =
	    childEl->getChildNodes();  // get all children	within data
	for(unsigned int i = 0; i < nodeList->getLength(); ++i)
		recursiveRemoveChild(
		    (xercesc::DOMElement*)(nodeList->item(nodeList->getLength() - 1 - i)),
		    childEl);

	// then release child
	parentEl->removeChild(childEl);
	childEl->release();
}  //end recursiveRemoveChild()

//==============================================================================
/// XmlDocument::saveXmlDocument
///	wrapper for private outputXML
///	Warning: filePath must be accessible or program will crash!
void XmlDocument::saveXmlDocument(const std::string& filePath)
{
	__COUT__ << "Saving theDocument_ to file: " << filePath << std::endl;
	// Return the first registered theImplementation_ that has the desired features. In
	// this case, we are after a DOM theImplementation_ that has the LS feature... or
	// Load/Save.  DOMImplementation *theImplementation_ =
	// DOMImplementationRegistry::getDOMImplementation(L"LS");
	xercesc::DOMImplementation* saveImplementation =
	    xercesc::DOMImplementationRegistry::getDOMImplementation(CONVERT_TO_XML("LS"));

	__COUTT__ << "XERCES Version: " << _XERCES_VERSION << std::endl;

#if _XERCES_VERSION >= 30000

	//__COUT__ << "making file" << filePath << std::endl;
	// Create a DOMLSSerializer which is used to serialize a DOM tree into an XML
	// theDocument_.
	xercesc::DOMLSSerializer* serializer =
	    ((xercesc::DOMImplementationLS*)saveImplementation)->createLSSerializer();

	// Make the output more human readable by inserting line feeds.
	if(serializer->getDomConfig()->canSetParameter(
	       xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true))
		serializer->getDomConfig()->setParameter(
		    xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);

	// The end-of-line sequence of characters to be used in the XML being written out.
	serializer->setNewLine(CONVERT_TO_XML("\r\n"));

	// Convert the path into Xerces compatible XMLCh*.
	// XMLCh *tempFilePath = const_cast<XMLCh*>(CONVERT_TO_XML(filePath));

	// Specify the target for the XML output.
	xercesc::XMLFormatTarget* formatTarget;
	try
	{
		// formatTarget = new xercesc::LocalFileFormatTarget(tempFilePath);
		formatTarget = new xercesc::LocalFileFormatTarget(filePath.c_str());
	}
	catch(...)
	{
		__COUT__ << "Inaccessible file path: " << filePath << std::endl;
		serializer->release();
		// xercesc::XMLString::release(&tempFilePath);

		return;
	}

	// Create a new empty output destination object.
	xercesc::DOMLSOutput* output =
	    ((xercesc::DOMImplementationLS*)saveImplementation)->createLSOutput();

	// Set the stream to our target.
	output->setByteStream(formatTarget);
	// Write the serialized output to the destination.
	serializer->write(theDocument_, output);
	serializer->release();
	// xercesc::XMLString::release(&tempFilePath);
	delete formatTarget;
#else

	xercesc::DOMWriter* serializer =
	    ((xercesc::DOMImplementationLS*)saveImplementation)->createDOMWriter();
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
	XMLCh* tempFilePath = xercesc::XMLString::transcode(filePath.c_str());
	xercesc::XMLFormatTarget* formatTarget;
	try
	{
		formatTarget = new xercesc::LocalFileFormatTarget(tempFilePath);
	}
	catch(...)
	{
		__COUT__ << "Inaccessible file path: " << filePath << std::endl;
		serializer->release();
		xercesc::XMLString::release(&tempFilePath);
		return;
	}

	// Write the serialized output to the target.

	serializer->writeNode(formatTarget, *theDocument_);
	serializer->release();
	xercesc::XMLString::release(&tempFilePath);
	delete formatTarget;
#endif

	// Cleanup.
	__COUTS__(50) << "delete format target" << std::endl;

#if _XERCES_VERSION >= 30000

	__COUTS__(50) << "delete output0" << std::endl;
	output->release();
	__COUTS__(50) << "delete output1" << std::endl;

#endif
}  //end saveXmlDocument()

//==============================================================================
bool XmlDocument::loadXmlDocument(const std::string& filePath)
{
	__COUT__ << "Loading theDocument_ from file: " << filePath << std::endl;

	struct stat fileStatus;

	if(stat(filePath.c_str(), &fileStatus) != 0)
	{
		__COUT__ << "File not accessible." << std::endl;
		return false;
	}

	terminatePlatform();
	initPlatform();

	xercesc::XercesDOMParser* parser = new xercesc::XercesDOMParser;
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
			throw(std::runtime_error("empty XML theDocument_"));
	}
	catch(xercesc::XMLException& e)
	{
		__COUT__ << "Error parsing file." << std::endl;
		return false;
	}
	delete parser;

	return true;
}  //end loadXmlDocument()

//============================================================================
void XmlDocument::setAnchors(const std::string& fSystemPath, const std::string& fRootPath)
{
	fSystemPath_ = fSystemPath;
	fRootPath_   = fRootPath;
}  //end setAnchors()

//============================================================================
void XmlDocument::makeDirectoryBinaryTree(const std::string&   fSystemPath,
                                          const std::string&   fRootPath,
                                          int                  indent,
                                          xercesc::DOMElement* anchorNode)
{
	DIR*           dir;
	struct dirent* entry;

	std::string newFullPath = "";
	char        fchar       = '.';
	char        schar       = '.';

	fSystemPath_ = fSystemPath;
	fRootPath_   = fRootPath;

	std::string fullPathName =
	    fSystemPath_ + std::string("/") + fRootPath_ + std::string("/") + fFoldersPath_;

	if(!anchorNode)
		anchorNode = rootElement_;

	if(!(dir = opendir(fullPathName.c_str())))
		return;

	while((entry = readdir(dir)) != NULL)
	{
		std::string sName = std::string(entry->d_name);
		fchar             = sName.at(0);
		if(sName.size() == 2)
			schar = sName.at(1);
		if(((sName.size() == 1) && fchar == '.') || ((sName.size() == 2) && schar == '.'))
		{
			continue;  // do not consider . and .. pseudo-folders
		}

		if(entry->d_type == DT_DIR)
		{
			fThisFolderPath_ = std::string(entry->d_name);
			newFullPath = fSystemPath_ + fRootPath + std::string("/") + fThisFolderPath_;
			hierarchyPaths_.push_back(std::string(entry->d_name) + std::string(""));
			fFoldersPath_ += hierarchyPaths_.back() + "/";
			xercesc::DOMElement* node = this->populateBinaryTreeNode(
			    anchorNode, std::string(entry->d_name), indent, false);
			this->makeDirectoryBinaryTree(fSystemPath, fRootPath, indent + 1, node);
			if(hierarchyPaths_.size() > 0)
				hierarchyPaths_.pop_back();
			if(hierarchyPaths_.size() > 0)
			{
				fFoldersPath_ = hierarchyPaths_.back() + "/";
			}
			else
			{
				fFoldersPath_ = "/";
			}
		}
		else
		{
			newFullPath = fSystemPath_ + std::string("/") + std::string(entry->d_name);
			boost::smatch what;
			boost::regex  re{".*\\.root$"};
			if(boost::regex_search(newFullPath, what, re))
			{
				fFileName_    = std::string(entry->d_name);
				fFoldersPath_ = "";
				for(unsigned int i = 0; i < hierarchyPaths_.size(); ++i)
				{
					fFoldersPath_ += hierarchyPaths_[i] + std::string("/");
				}
				this->populateBinaryTreeNode(anchorNode, fFileName_, indent, true);
			}
		}
	}
	closedir(dir);
}  //end makeDirectoryBinaryTree()

//==========================================================================================
xercesc::DOMElement* XmlDocument::populateBinaryTreeNode(xercesc::DOMElement* anchorNode,
                                                         const std::string&   name,
                                                         int                  indent,
                                                         bool                 isLeaf)
{
	std::string          nm    = "unassigned";
	xercesc::DOMElement* nodes = NULL;

	//  if( isLeaf )
	//  {
	//   STDLINE("","") ;
	//   if( theNodes_.find(indent) != theNodes_.end() ) nodes = theNodes_.find(indent)->second ;
	//   if( theNames_.find(indent) != theNames_.end() ) nm    = theNames_.find(indent)->second ;
	//   ss_.str("") ; ss_ << "Attaching " << name << " to "  << nm << " size: " << theNames_.size();
	//   STDLINE(ss_.str(),ACGreen) ;
	//  }
	//  else
	//  {
	if(theNodes_.find(indent) != theNodes_.end())  // a new node
	{
		if(theNodes_.find(indent) != theNodes_.end())
			nodes = theNodes_.find(indent)->second;
		if(theNames_.find(indent) != theNames_.end())
			nm = theNames_.find(indent)->second;
	}
	else
	{
		nodes = theDocument_->createElement(xercesc::XMLString::transcode("nodes"));
		theNodes_[indent] = nodes;
		theNames_[indent] = name;
		anchorNode->appendChild(nodes);
	}
	//  }
	xercesc::DOMElement* node =
	    theDocument_->createElement(xercesc::XMLString::transcode("node"));
	nodes->appendChild(node);

	xercesc::DOMElement* nChilds =
	    theDocument_->createElement(xercesc::XMLString::transcode("nChilds"));
	node->appendChild(nChilds);

	xercesc::DOMText* nChildsVal =
	    theDocument_->createTextNode(xercesc::XMLString::transcode("x"));
	nChilds->appendChild(nChildsVal);

	xercesc::DOMElement* fSystemPathNode =
	    theDocument_->createElement(xercesc::XMLString::transcode("fSystemPath"));
	node->appendChild(fSystemPathNode);

	xercesc::DOMText* fSystemPathVal =
	    theDocument_->createTextNode(xercesc::XMLString::transcode(fSystemPath_.c_str()));
	fSystemPathNode->appendChild(fSystemPathVal);

	xercesc::DOMElement* fRootPathNode =
	    theDocument_->createElement(xercesc::XMLString::transcode("fRootPath"));
	node->appendChild(fRootPathNode);

	xercesc::DOMText* fRootPathVal =
	    theDocument_->createTextNode(xercesc::XMLString::transcode(fRootPath_.c_str()));
	fRootPathNode->appendChild(fRootPathVal);

	xercesc::DOMElement* fFoldersPathNode =
	    theDocument_->createElement(xercesc::XMLString::transcode("fFoldersPath"));
	node->appendChild(fFoldersPathNode);

	xercesc::DOMText* foldersPathVal = theDocument_->createTextNode(
	    xercesc::XMLString::transcode(fFoldersPath_.c_str()));
	fFoldersPathNode->appendChild(foldersPathVal);

	xercesc::DOMElement* fThisFolderPath   = NULL;
	xercesc::DOMElement* fFileOrHistName   = NULL;
	xercesc::DOMText*    fileOrDirNameVal  = NULL;
	xercesc::DOMText*    thisFolderNameVal = NULL;

	fThisFolderPath =
	    theDocument_->createElement(xercesc::XMLString::transcode("fDisplayName"));

	if(isLeaf)
	{
		fFileOrHistName =
		    theDocument_->createElement(xercesc::XMLString::transcode("fFileName"));
		fileOrDirNameVal =
		    theDocument_->createTextNode(xercesc::XMLString::transcode(name.c_str()));
		thisFolderNameVal =
		    theDocument_->createTextNode(xercesc::XMLString::transcode(name.c_str()));
		ss_.str("");
		ss_ << "name: " << ACRed << name << ACPlain << "/" << ACGreen << name;
		STDLINE(ss_.str(), "");
	}
	else
	{
		std::string blank;
		fFileOrHistName =
		    theDocument_->createElement(xercesc::XMLString::transcode("fFileName"));
		fileOrDirNameVal =
		    theDocument_->createTextNode(xercesc::XMLString::transcode(blank.c_str()));
		thisFolderNameVal = theDocument_->createTextNode(
		    xercesc::XMLString::transcode(fThisFolderPath_.c_str()));
		ss_.str("");
		ss_ << "name: " << ACRed << fThisFolderPath_ << ACPlain << "/" << ACGreen << name;
		STDLINE(ss_.str(), "");
	}

	node->appendChild(fFileOrHistName);
	fFileOrHistName->appendChild(fileOrDirNameVal);

	node->appendChild(fThisFolderPath);
	fThisFolderPath->appendChild(thisFolderNameVal);

	xercesc::DOMElement* leaf =
	    theDocument_->createElement(xercesc::XMLString::transcode("leaf"));
	node->appendChild(leaf);

	xercesc::DOMText* leafVal = theDocument_->createTextNode(
	    xercesc::XMLString::transcode(isALeaf_[isLeaf].c_str()));
	leaf->appendChild(leafVal);

	return node;
}  //end populateBinaryTreeNode()

//==========================================================================================
/// Used by developer Dario Menasce for alternative XML requests at web user interface.
void XmlDocument::setDarioStyle(bool darioStyle)
{
	darioXMLStyle_ = darioStyle;
}  //end setDarioStyle()
