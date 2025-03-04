#ifndef ots_XmlDocument_h
#define ots_XmlDocument_h

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#if defined(XERCES_NEW_IOSTREAMS)
#include <iostream>
#else
#include <iostream.h>
#endif

// clang-format off
//===============================================================================================================
namespace ots
{
/// Note that XmlDocument functionality is extended by HttpXmlDocument class
class XmlDocument
{
	///---------------------------------------------------------------------------------------------------------------
  public:
										XmlDocument(const std::string& rootName = "ROOT");
										XmlDocument(const XmlDocument& doc);
										XmlDocument& operator=(const XmlDocument& doc);
										~XmlDocument(void);

	xercesc::DOMElement* 				addTextElementToParent(const std::string& childName, const std::string& childText, xercesc::DOMElement* parent);
	xercesc::DOMElement* 				addTextElementToParent(const std::string& childName, const std::string& childText, const std::string& parentName, unsigned int parentIndex = 0);
	void                 				saveXmlDocument(const std::string& filePath);
	void                 				recursiveRemoveChild(xercesc::DOMElement* childEl, xercesc::DOMElement* parentEl);
	bool                 				loadXmlDocument(const std::string& filePath);
	void                 				outputXmlDocument(std::ostringstream* out, bool dispStdOut = false);
	void                 				makeDirectoryBinaryTree(const std::string& name, const std::string& rootPath, int indent, xercesc::DOMElement* anchorNode);
	xercesc::DOMElement* 				populateBinaryTreeNode(xercesc::DOMElement* anchorNode, const std::string& name, int indent, bool isLeaf);
	void                 				setAnchors(const std::string& fSystemPath, const std::string& fRootPath);
	void                 				setDocument(xercesc::DOMDocument* doc);
	void                 				setDarioStyle(bool darioStyle);
	void                 				setRootPath(const std::string& rootPath) { fRootPath_ = rootPath; }
	///---------------------------------------------------------------------------------------------------------------
  protected:
	void        						copyDocument(const xercesc::DOMDocument* toCopy, xercesc::DOMDocument* copy);
	void        						recursiveElementCopy(const xercesc::DOMElement* toCopy, xercesc::DOMElement* copy);
	void        						initDocument(void);
	void        						initPlatform(void);
	void        						terminatePlatform(void);
	void        						recursiveOutputXmlDocument(xercesc::DOMElement* currEl, std::ostringstream* out, bool dispStdOut = false, const std::string& tabStr = "");

	xercesc::DOMImplementation* 		theImplementation_;
	xercesc::DOMDocument*       		theDocument_;
	xercesc::DOMElement*        		rootElement_;
	const std::string           		rootTagName_;

	xercesc::DOMDocument*               doc;
	xercesc::DOMElement*                rootElem;
	DIR*                                dir;
	struct dirent*                      entry;
	int                                 lastIndent;
	int                                 errorCode;
	int                                 level;
	std::string                         fullPath;
	std::string                         fullFPath;
	std::stringstream                   ss_;
	std::map<int, xercesc::DOMElement*> theNodes_;
	std::map<int, std::string>          theNames_;
	std::vector<std::string>            hierarchyPaths_;
	xercesc::DOMImplementation*         impl;
	xercesc::DOMLSSerializer*           pSerializer;
	xercesc::DOMConfiguration*          pDomConfiguration;
	bool                                darioXMLStyle_;

	std::string                 		fSystemPath_;
	std::string                 		fRootPath_;
	std::string                 		fFoldersPath_;
	std::string                 		fFileName_;
	std::string                 		fThisFolderPath_;
	int                         		indent_;
	std::map<bool, std::string> 		isALeaf_;
};
}  // namespace ots

// clang-format on
#endif  // ots_XmlDocument_h
