#ifndef __RootFileExplorer_H__
#define __RootFileExplorer_H__

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <TBuffer.h>
#include <TBufferFile.h>
#include <TBufferJSON.h>
#include <TList.h>
#include <TRegexp.h>

// #include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
// #include "otsdaq/XmlUtilities/ConvertFromXML.h"
// #include "otsdaq/XmlUtilities/ConvertToXML.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentType.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMNodeIterator.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/XMLUni.hpp>
#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/validators/common/Grammar.hpp>
// #include <xercesc/dom/DOMLSSerializer.hpp>
// #include <xercesc/dom/DOMLSOutput.hpp>

#include "otsdaq/Macros/BinaryStringMacros.h"
// #include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#if defined(XERCES_NEW_IOSTREAMS)
#include <iostream>
#else
#include <iostream.h>
#endif

#include <TDirectory.h>
#include <TFile.h>
#include <TKey.h>

using namespace ots;
using namespace std;

XERCES_CPP_NAMESPACE_USE

// clang-format off
class RootFileExplorer
{
 public:

                         RootFileExplorer        (string                fSystemPath     ,
                                                  string                fRootPath       ,
                                                  string                fFoldersPath    ,
                                                  string                fHistName       ,
                                                  string                fRFoldersPath   ,
                                                  string                fFileName       ,
                                                  TFile               * rootFile = NULL  )  ;
                        ~RootFileExplorer        (void                                   ) {;}
  xercesc::DOMDocument * initialize              (bool                  liveDQMFlag      )  ; 
  void                   makeDirectoryBinaryTree (TDirectory          * currentDirectory,
                                                  int                   indent          ,
                                                  xercesc::DOMElement * anchorNode       )  ;
  void                   makeLiveDQMBinaryTree   (TDirectory          * currentDirectory,
                                                  int                   indent          ,
                                                  std::string           subDirName      ,
                                                  xercesc::DOMElement * anchorNode       )  ;
  xercesc::DOMElement  * populateBinaryTreeNode  (xercesc::DOMElement * anchorNode      ,
                                                  std::string           name            ,
                                                  //int                   level           ,
                                                  bool                  isLeaf           )  ;
  void                   initializeXMLWriter     (void                                   )  ;
 
 private:

  std::string            blanks                  (int                    level           )  ;
  void                   computeRFoldersPath     (void                                   )  ;
  std::string            computeHierarchyPaths   (void                                   )  ;
  void                   dumpHierarchyPaths      (std::string            what            )  ;
  void                   shrinkHierarchyPaths    (int                    number          )  ;

  bool                                           debug_            ;
  bool                                           liveDQMFlag_      ;
  string                                         fSystemPath_      ;
  string                                         fRootPath_        ;
  string                                         fFoldersPath_     ;
  string                                         fFileName_        ;
  string                                         fRFoldersPath_    ;
  string                                         fHistName_        ;
  string                                         fHistTitle_       ;
  string                                         rootDirectoryName_;
  //std::string::size_type                         LDQM_pos_         ;
  xercesc::DOMImplementation                   * theImplementation_;
  xercesc::DOMDocument                         * theDocument_      ;
  xercesc::DOMElement                          * rootElement_      ;
  xercesc::DOMElement                          * anchorNodeLast_   ;
  std::map<std::string, xercesc::DOMElement *>   theNodes_         ;
  std::map<std::string, xercesc::DOMElement *>   theNodesB_        ;
  std::map<std::string, std::string>             theNodeName_      ;
  std::map<std::string, std::string>             theNodeNameB_     ;
  std::vector<std::string>                       hierarchyPaths_   ;
  std::string                                    previousAncestor_ ;
  std::map<bool,std::string>                     isALeaf_          ;
  const std::string                              rootTagName_      ;
  TFile                                        * rootFile_         ;
  int                                            level_            ;
  int                                            counter_          ;
  stringstream                                   ss_               ;
  xercesc::DOMLSSerializer                     * theSerializer_    ;
  xercesc::XMLFormatTarget                     * myFormTarget_     ;
  xercesc::DOMLSOutput                         * theOutput_        ;
//  HttpXmlDocument                                xmlOut_           ;
} ;
// clang-format on
#endif
