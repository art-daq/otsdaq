#include "otsdaq/RootUtilities/RootFileExplorer.h"
#include "otsdaq/Macros/MessageTools.h"
// clang-format off
//================================================================================================
RootFileExplorer::RootFileExplorer(
		                    std::string   fSystemPath  ,
		                    std::string   fRootPath    ,
                                    std::string   fFoldersPath ,
                                    std::string   fHistName    ,
                                    std::string   fRFoldersPath,
                                    std::string   fFileName    ,
                                    TFile       * rootFile
                                  ) : rootTagName_("ROOT")
{
 fSystemPath_    = fSystemPath   ;
 fRootPath_      = fRootPath     ;
 fFoldersPath_   = fFoldersPath  ;
 fHistName_      = fHistName     ;
 fRFoldersPath_  = fRFoldersPath ;
 fFileName_      = fFileName     ;
 STDLINE(std::string("fSystemPath  : ")+fSystemPath_  ,ACCyan);
 STDLINE(std::string("fRootPath    : ")+fRootPath_    ,ACCyan);
 STDLINE(std::string("fFoldersPath : ")+fFoldersPath_ ,ACCyan);
 STDLINE(std::string("fHistName    : ")+fHistName_    ,ACCyan);
 STDLINE(std::string("fRFoldersPath: ")+fRFoldersPath_,ACCyan);
 STDLINE(std::string("fFileName    : ")+fFileName_    ,ACCyan);
 level_          = 0             ;
 counter_        = 0             ;
 debug_          = true          ;
 isALeaf_[true]  = "true"        ;
 isALeaf_[false] = "false"       ;
 rootFile_       = rootFile      ;
 anchorNodeLast_ = NULL          ;
 hierarchyPaths_.clear() ;

}

//================================================================================================
xercesc::DOMDocument * RootFileExplorer::initialize(bool liveDQMFlag)
{

 liveDQMFlag_ = liveDQMFlag;

 try
 {
  xercesc::XMLPlatformUtils::Initialize();  // Initialize Xerces infrastructure
 }
 catch(xercesc::XMLException& e)
 {
  string msg = xercesc::XMLString::transcode(e.getMessage()) ;
 }

 theImplementation_ = xercesc::DOMImplementationRegistry::getDOMImplementation(xercesc::XMLString::transcode("Core"));

 if(theImplementation_)
 {
  try
  {
   theDocument_ = theImplementation_->createDocument(
                                                     xercesc::XMLString::transcode("http://www.w3.org/2001/XMLSchema-instance"),  
                                                     xercesc::XMLString::transcode(rootTagName_.c_str()),  
                                                     0
                                                    );                           
  }
  catch(const xercesc::OutOfMemoryException&)
  {
   XERCES_STD_QUALIFIER cerr << "OutOfMemoryException"
                             << XERCES_STD_QUALIFIER endl;
  }
  catch(const xercesc::DOMException& e)
  {
   XERCES_STD_QUALIFIER cerr << "DOMException code is:  " 
                             << e.code 
                             << " " 
                             << xercesc::XMLString::transcode(e.getMessage())
                             << XERCES_STD_QUALIFIER endl;
  }
  catch(const xercesc::XMLException& e)
  {
   STDLINE(string("Error Message: ")+xercesc::XMLString::transcode(e.getMessage()),ACRed) ;
  }
  catch(...)
  {
   XERCES_STD_QUALIFIER cerr << "An error occurred creating the theDocument_"
                             << XERCES_STD_QUALIFIER endl;
  }
 }
 else
 {
  XERCES_STD_QUALIFIER cerr << "Requested theImplementation_ is not supported"
                            << XERCES_STD_QUALIFIER endl;
 }

 this->initializeXMLWriter() ;
 
 if( liveDQMFlag_ ) 
 {
  rootElement_ = theDocument_->getDocumentElement();

  ss_.str(""); ss_ << "Main anchor (document): " << rootElement_ ;
  STDLINE(ss_.str(),string(ACCyan)+string(ACReverse)) ;
  STDLINE(rootFile_->GetName(),string(ACCyan)+string(ACReverse)) ;

  this->makeLiveDQMBinaryTree(rootFile_,0,"",NULL) ; 
 } 
 else 
 {
  string fName = fSystemPath_ +string("/") +
                 fRootPath_   ; 
  if( fFoldersPath_.length() > 0 ) fName += string("/") + fFoldersPath_; 
  if( fFileName_   .length() > 0 ) fName += string("/") + fFileName_   ;
  STDLINE(string("Opening fFileName_: ")+fFileName_, ACYellow) ;
  STDLINE(string("Opening fName     : ")+fName     , ACYellow) ;
  rootFile_ = new TFile(fName.c_str()) ;
 
  if( debug_) rootFile_->ls() ;

  rootElement_ = theDocument_->getDocumentElement();

  this->makeDirectoryBinaryTree(rootFile_,0,NULL) ; 
 }
 
 return theDocument_ ;   
}
//==========================================================================================
xercesc::DOMElement * RootFileExplorer::populateBinaryTreeNode(xercesc::DOMElement * anchorNode, 
                                                               std::string           name      ,
//                                                               int                   level     ,
                                                               bool                  isLeaf    )
{
    ss_.str(""); ss_ << "fRFoldersPath_: " << fRFoldersPath_   ; STDLINE(ss_.str(),ACGreen) ;
    ss_.str(""); ss_ << "previous      : " << previousAncestor_; STDLINE(ss_.str(),ACGreen) ;
    xercesc::DOMElement * nodes ;
    if( theNodes_.find(previousAncestor_) == theNodes_.end() ) // a new node
    {
        nodes                             = theDocument_->createElement( xercesc::XMLString::transcode("nodes"          ));
        ss_.str(""); ss_ << "NEW nodes (" << nodes << ") for name " << name ; STDLINE(ss_.str(),"") ;
        anchorNode->appendChild(nodes);
        theNodes_   [previousAncestor_] = nodes  ;
        theNodeName_[previousAncestor_] = name   ;
    }
    else                                           // Is already there
    {
        nodes = theNodes_.find(previousAncestor_)->second ;
        ss_.str(""); ss_ << name << " points to an OLD nodes (" << nodes << ") parallel to " << theNodeName_[previousAncestor_]; STDLINE(ss_.str(),"") ;
    }
    xercesc::DOMElement * node            = theDocument_->createElement( xercesc::XMLString::transcode("node"               ));
    node->setAttribute(xercesc::XMLString::transcode("class"                         ),
                       xercesc::XMLString::transcode("x-tree-icon x-tree-icon-parent")) ;
    nodes->appendChild(node);          

    ss_.str(""); ss_ << "Attaching node " << node << " previousAncestor_ " << previousAncestor_ << " to nodes " << nodes; STDLINE(ss_.str(),"") ;

    xercesc::DOMElement * ancestor     = theDocument_->createElement (xercesc::XMLString::transcode("ancestor"));
    xercesc::DOMText    * ancestorVal  = theDocument_->createTextNode(xercesc::XMLString::transcode(previousAncestor_.c_str()));
    ancestor->appendChild(ancestorVal); 
    node->appendChild(ancestor); 
 
    xercesc::DOMElement * fSystemPath     = theDocument_->createElement( xercesc::XMLString::transcode("fSystemPath"        ));
    node->appendChild(fSystemPath); 
 
    xercesc::DOMText    * fSystemPathVal  = theDocument_->createTextNode(xercesc::XMLString::transcode(fSystemPath_.c_str() ));
    fSystemPath->appendChild(fSystemPathVal); 
 
    xercesc::DOMElement * fRootPath       = theDocument_->createElement( xercesc::XMLString::transcode("fRootPath"          ));
    node->appendChild(fRootPath);

    xercesc::DOMText    * fRootPathVal    = theDocument_->createTextNode(xercesc::XMLString::transcode(fRootPath_.c_str()   ));
    fRootPath->appendChild(fRootPathVal);

    xercesc::DOMElement * fFoldersPath    = theDocument_->createElement( xercesc::XMLString::transcode("fFoldersPath"       ));
    node->appendChild(fFoldersPath);

    xercesc::DOMText    * fFoldersPathVal = theDocument_->createTextNode(xercesc::XMLString::transcode(fFoldersPath_.c_str()));
    fFoldersPath->appendChild(fFoldersPathVal);

    xercesc::DOMElement * fDisplayName     = NULL ; 
    xercesc::DOMElement * fFileName        = NULL ; 
    xercesc::DOMElement * fRFoldersPath    = NULL ; 
    xercesc::DOMElement * fHistName        = NULL ; 
 
    xercesc::DOMText    * fDisplayNameVal  = NULL ;   
    xercesc::DOMText    * fFileNameVal     = NULL ;   
    xercesc::DOMText    * fRFoldersPathVal = NULL ; 
    xercesc::DOMText    * fHistNameVal     = NULL ; 
      
    fDisplayName     = theDocument_->createElement( xercesc::XMLString::transcode("fDisplayName"        )); 
    fRFoldersPath    = theDocument_->createElement( xercesc::XMLString::transcode("fRFoldersPath"       )); 
    fHistName        = theDocument_->createElement( xercesc::XMLString::transcode("fHistName"           ));    
    fFileName        = theDocument_->createElement( xercesc::XMLString::transcode("fFileName"           ));   

    fFileNameVal     = theDocument_->createTextNode(xercesc::XMLString::transcode(fFileName_.c_str()    ));
    fHistNameVal     = theDocument_->createTextNode(xercesc::XMLString::transcode(fHistName_.c_str()    ));
    fRFoldersPathVal = theDocument_->createTextNode(xercesc::XMLString::transcode(fRFoldersPath_.c_str()));

    if(isLeaf)
    {
        fDisplayNameVal = theDocument_->createTextNode(xercesc::XMLString::transcode(fHistName_.c_str() )); 
    }
    else
    {
        fDisplayNameVal = theDocument_->createTextNode(xercesc::XMLString::transcode(name.c_str()       ));
    }  

    node         ->appendChild(fDisplayName    );
    node         ->appendChild(fRFoldersPath   );
    node         ->appendChild(fFileName       );     
    node         ->appendChild(fHistName       ); 

    fDisplayName ->appendChild(fDisplayNameVal );     
    fFileName    ->appendChild(fFileNameVal    );     
    fHistName    ->appendChild(fHistNameVal    );
    fRFoldersPath->appendChild(fRFoldersPathVal);

    xercesc::DOMElement * leaf    = theDocument_->createElement( xercesc::XMLString::transcode("leaf"                  ));
    node->appendChild(leaf);

    xercesc::DOMText    * leafVal = theDocument_->createTextNode(xercesc::XMLString::transcode(isALeaf_[isLeaf].c_str()));
    leaf->appendChild(leafVal);
    
//    theSerializer_->write(theDocument_, theOutput_);
    
//     xercesc::DOMElement * iconCls = theDocument_->createElement( xercesc::XMLString::transcode("iconCls"              ));
//     node->appendChild(iconCls); 
//  
//     if(isLeaf)
//     {
//      xercesc::DOMText    * iconVal = theDocument_->createTextNode(xercesc::XMLString::transcode("histogram-leaf-icon"  ));
//      iconCls->appendChild(iconVal);
//     } else {
//      xercesc::DOMText    * iconVal = theDocument_->createTextNode(xercesc::XMLString::transcode("x-tree-icon x-tree-icon-parent"  ));
//      iconCls->appendChild(iconVal);
//     }
    
//     if( debug_ ) 
//     {
//         STDLINE(string("fSystemPath_  : ")+fSystemPath_  ,ACRed);
//         STDLINE(string("fRootPath_    : ")+fRootPath_    ,ACRed);
//         STDLINE(string("fFoldersPath_ : ")+fFoldersPath_ ,ACRed);
//         STDLINE(string("fFileName_    : ")+fFileName_    ,ACRed);
//         STDLINE(string("fRFoldersPath_: ")+fRFoldersPath_,string(ACRed)+string(ACReverse));
//         STDLINE(string("fHistName_    : ")+name          ,ACRed);
//     }
    return node;
}
//================================================================================================
void RootFileExplorer::makeLiveDQMBinaryTree(TDirectory          * currentDir, 
                                             int                   level     ,
                                             std::string           subDirName,
                                             xercesc::DOMElement * anchorNode )
{
// xercesc::DOMElement * node  = NULL ;
 if( !anchorNode) anchorNode = rootElement_ ;
 currentDir = currentDir->GetDirectory(subDirName.c_str()) ;
 if(currentDir != 0) 
 {
//  STDLINE(currentDir->GetName(),string(ACCyan)+string(ACReverse)) ;
  TObject* obj;
  TIter    nextobj(currentDir->GetList());
//  STDLINE("currentDir->ls()",ACReverse) ;
//  currentDir->ls() ;
  while((obj = (TObject*)nextobj()))
  {
   string objName = obj->GetName() ;
   ss_.str("") ; ss_ << "Exploring " << objName << " class: " << string(obj->ClassName()); STDLINE(ss_.str(),"") ;
   if( string(obj->ClassName()) == "TTree"          ) continue ;
   if( string(obj->ClassName()) == "TNtuple"        ) continue ;
   if( string(obj->ClassName()) == "TGeoManager"    ) continue ;
   if( string(obj->ClassName()) == "TGeoVolume"     ) continue ;
   if( obj->IsFolder() )
   {
    ss_.str("") ; ss_ << "Enter     " << objName ;
    STDLINE(ss_.str(),"") ;
    hierarchyPaths_.push_back(objName) ;
    fRFoldersPath_ = "" ; 
    fHistName_     = ""  ;
    for(int i=0; i<(int)hierarchyPaths_.size(); ++i) {fRFoldersPath_ += hierarchyPaths_[i];}
    computeRFoldersPath() ;
    previousAncestor_ = currentDir->GetName() ;
    anchorNodeLast_   = this->populateBinaryTreeNode(anchorNode, string(obj->GetName()), false) ;
    makeLiveDQMBinaryTree(currentDir,level+1,objName,anchorNodeLast_) ;
    fHistName_  = obj->GetName () ;
    fHistTitle_ = obj->GetTitle() ;
    if(theNodes_.find(previousAncestor_) != theNodes_.end()) 
    {
     this->shrinkHierarchyPaths(1) ;
     //theNodes_.erase(theNodes_.find(previousAncestor_)) ; 
    }
    computeRFoldersPath() ;

   }
   else // is TH1 or TCanvas
   {
    fHistName_        = obj->GetName() ;
    previousAncestor_ = currentDir->GetName() ;
    anchorNode        = this->populateBinaryTreeNode(anchorNodeLast_, fHistName_, true) ;
   }
  }
 }
 else
 {
  STDLINE("No currentDir!!!",string(ACRed)+string(ACReverse)) ;
 } 
}
//================================================================================================
void RootFileExplorer::makeDirectoryBinaryTree(TDirectory          * currentDirectory, 
                                               int                   level           ,
                                               xercesc::DOMElement * anchorNode       )
{
    if( !anchorNode      ) anchorNode      = rootElement_ ;
    if( !anchorNodeLast_ ) anchorNodeLast_ = anchorNode   ;

    TKey * keyH = NULL ;
    TIter hList(currentDirectory->GetListOfKeys());
//    STDLINE(currentDirectory->GetName(),string(ACCyan)+string(ACReverse)) ;
    while((keyH = (TKey*)hList()))
    {
        std::string objName = keyH->GetName() ;
        ss_.str("") ; ss_ << "Exploring " << objName << " level: " << level << " COUNTER: " << ++counter_;
//        STDLINE(ss_.str(),"") ;
        string what = keyH->GetClassName () ;
        if( what == "TTree"       ) continue ;
        if( what == "TNtuple"     ) continue ;
        if( what == "TGeoManager" ) continue ;
        if( what == "TGeoVolume"  ) continue ;
        if( what == "TDirectoryFile" )
        {
            ss_.str("") ; ss_ << "Enter     " << objName << " level: " << level ;
//            STDLINE(ss_.str(),"") ;
            previousAncestor_ = currentDirectory->GetName() ;
            currentDirectory->cd(objName.c_str());
            TDirectory * subDir = gDirectory ;
            hierarchyPaths_.push_back(std::string(subDir->GetName())) ;

            computeRFoldersPath() ;
            fHistName_      = ""  ;
//            anchorNodeLast_ = this->populateBinaryTreeNode(anchorNode, objName, level, false) ;
            anchorNodeLast_ = this->populateBinaryTreeNode(anchorNode, objName, false) ;
            this->makeDirectoryBinaryTree(subDir,level+1,anchorNodeLast_) ;
            this->shrinkHierarchyPaths(1) ; 
            computeRFoldersPath() ;
        }
        else
        {
            fHistName_        = objName ;
            STDLINE(fHistName_,ACCyan) ;
            previousAncestor_ = currentDirectory->GetName() ;
//            anchorNode        = this->populateBinaryTreeNode(anchorNodeLast_, objName, level, true  ) ;            
            anchorNode        = this->populateBinaryTreeNode(anchorNodeLast_, objName, true  ) ;            
        }
    }
}
//================================================================================================
void RootFileExplorer::computeRFoldersPath(void)
{
 fRFoldersPath_ = "" ;
 for(int i=0; i<(int)hierarchyPaths_.size(); ++i)
 {
  fRFoldersPath_ += hierarchyPaths_[i] + "/";
 }
}
//================================================================================================
void RootFileExplorer::dumpHierarchyPaths(string what)
{
 ss_.str("") ; ss_ << what << " - hierarchyPaths_.size(): " << hierarchyPaths_.size() ;
 STDLINE(ss_.str(),"") ;
 for(int i=0; i<(int)hierarchyPaths_.size(); i++)
 {
  ss_.str("") ; ss_ << i << "] " << hierarchyPaths_[i] ;
  STDLINE(ss_.str(),"") ;
 }
}
//================================================================================================
std::string RootFileExplorer::computeHierarchyPaths(void)
{
 this->dumpHierarchyPaths("Computing...") ;
 std::string name = "" ;
 for(int i=0; i<(int)hierarchyPaths_.size(); i++)
 {
  name += hierarchyPaths_[i] ;
  if( i != (int)hierarchyPaths_.size() - 1 ) name += string("/") ;
 }
 return name ;
}
//================================================================================================
void RootFileExplorer::shrinkHierarchyPaths(int number)
{
 for(int i=0; i<number; ++i)
 {
  if( hierarchyPaths_.size() > 0 ) hierarchyPaths_.pop_back() ;
 }
}
//================================================================================================
void RootFileExplorer::initializeXMLWriter(void)
{
 XMLCh tempStr[100];
 XMLString::transcode("LS", tempStr, 99);
 DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(tempStr);
 theSerializer_ = ((DOMImplementationLS*)impl)->createLSSerializer();
 if (theSerializer_->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTDiscardDefaultContent, true))
     theSerializer_->getDomConfig()->setParameter(XMLUni::fgDOMWRTDiscardDefaultContent, true);

 if (theSerializer_->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
     theSerializer_->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);

 myFormTarget_ = new StdOutFormatTarget();
 theOutput_    = ((DOMImplementationLS*)impl)->createLSOutput();
 theOutput_->setByteStream(myFormTarget_);
}
//================================================================================================
string RootFileExplorer::blanks(int level)
{
 string s = "" ;
 for(int i=0; i<level; ++i)
 {
  s += "\t" ;
 }
 return s ;
}

// clang-format on
