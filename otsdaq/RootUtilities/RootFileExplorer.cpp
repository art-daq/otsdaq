#include "otsdaq/RootUtilities/RootFileExplorer.h"
#include "otsdaq/otsdaq/Macros/MessageTools.h"

//================================================================================================
RootFileExplorer::RootFileExplorer(
		                            std::string fSystemPath ,
		                            std::string fRootPath   ,
                                    std::string fFoldersPath,
                                    std::string fHistName   ,
                                    std::string fFileName   ,
                                    HttpXmlDocument & xmlOut
                                  ) : rootTagName_("ROOT")
{
 fSystemPath_    = fSystemPath ;
 fRootPath_      = fRootPath   ;
 fFoldersPath_   = fFoldersPath;
 fHistName_      = fHistName   ;
 fFileName_      = fFileName   ;
 xmlOut_         = xmlOut      ;
 level_          = 0           ;
 debug_          = true        ;
 isALeaf_[true]  = "true"      ;
 isALeaf_[false] = "false"     ;
  
 if( debug_ ) 
 {
  STDLINE(string("fSystemPath_ : ")+fSystemPath_ ,ACWhite) ;
  STDLINE(string("fRootPath_   : ")+fRootPath_   ,ACWhite) ; 
  STDLINE(string("fFoldersPath_: ")+fFoldersPath_,ACWhite) ;
  STDLINE(string("fHistName_   : ")+fHistName_   ,ACWhite) ;
  STDLINE(string("fFileName_   : ")+fFileName_   ,ACWhite) ;
 }
 
 rootFile_ = new TFile((fSystemPath_+string("/") +
                        fRootPath_  +string("/") +
                        fFileName).c_str()        ) ;
 
 if( debug_) rootFile_->ls() ;

}

//================================================================================================
xercesc::DOMDocument * RootFileExplorer::initialize(void)
{
 try
 {
  xercesc::XMLPlatformUtils::Initialize();  // Initialize Xerces infrastructure
 }
 catch(xercesc::XMLException& e)
 {
  string msg = xercesc::XMLString::transcode(e.getMessage()) ;
  //STDLINE(string("XML toolkit initialization error: ")+msg,ACRed) ;
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
   //STDLINE(string("Error Message: ")+xercesc::XMLString::transcode(e.getMessage()),ACRed) ;
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

 rootElement_ = theDocument_->getDocumentElement();

 this->makeDirectoryBinaryTree(rootFile_,0,NULL) ; 
 return theDocument_ ;   
}
//==========================================================================================
xercesc::DOMElement * RootFileExplorer::populateBinaryTreeNode(xercesc::DOMElement * anchorNode, 
                                                               std::string           name      ,
                                                               int                   level     ,
                                                               bool                  isLeaf    )
{ 
    xercesc::DOMElement * nodes ;
    if( theNodes_.find(level) == theNodes_.end() ) // a new node
    {
        nodes                             = theDocument_->createElement( xercesc::XMLString::transcode("nodes"          ));
        anchorNode->appendChild(nodes);
        theNodes_[level] = nodes  ;
    }
    else // Is already there
    {
        nodes = theNodes_.find(level)->second ;
    }
    
    ss_.str(""); ss_ << "level: " << level << "]\t" << fFoldersPath_ ;
    STDLINE(ss_.str(),ACCyan) ;
    STDLINE(nodes,string(ACCyan)+string(ACReverse)) ;
    xercesc::DOMElement * node            = theDocument_->createElement( xercesc::XMLString::transcode("node"               ));
    nodes->appendChild(node);      
     
    xercesc::DOMElement * nChilds         = theDocument_->createElement( xercesc::XMLString::transcode("nChilds"            ));
    node->appendChild(nChilds);      
     
    xercesc::DOMText    * nChildsVal      = theDocument_->createTextNode(xercesc::XMLString::transcode("x"                  ));
    nChilds->appendChild(nChildsVal); 
 
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

    xercesc::DOMElement * fThisFolderPath   = NULL ; 
    xercesc::DOMElement * fFileOrHistName   = NULL ; 
    xercesc::DOMText    * fileOrDirNameVal  = NULL ;   
    xercesc::DOMText    * thisFolderNameVal = NULL ; 
    STDLINE(nodes,string(ACCyan)+string(ACReverse)) ;
    
    fThisFolderPath      = theDocument_->createElement( xercesc::XMLString::transcode("fDisplayName"         ));    
   
    if( isLeaf )    
    {    
    	fFileOrHistName   = theDocument_->createElement( xercesc::XMLString::transcode("fHistName"            ));    
        thisFolderNameVal = theDocument_->createTextNode(xercesc::XMLString::transcode(name.c_str()           ));
    }       
    else    
    {  
   	std::string blank ;  
    	fFileOrHistName   = theDocument_->createElement( xercesc::XMLString::transcode("fFileName"             ));    
        thisFolderNameVal = theDocument_->createTextNode(xercesc::XMLString::transcode(fThisFolderPath_.c_str()));
    }    
    STDLINE(nodes,string(ACCyan)+string(ACReverse)) ;
    
    node->appendChild(fFileOrHistName);     
    fFileOrHistName->appendChild(thisFolderNameVal);     
    STDLINE(nodes,string(ACCyan)+string(ACReverse)) ;
        
    node->appendChild(fThisFolderPath) ;
    fThisFolderPath->appendChild(thisFolderNameVal) ;
    STDLINE(nodes,string(ACCyan)+string(ACReverse)) ;

    // xercesc::DOMElement * fHistName       = theDocument_->createElement( xercesc::XMLString::transcode("fHistName"           ));
    // node->appendChild(fHistName); 
 
    // xercesc::DOMText    * fHistNameVal    = theDocument_->createTextNode(xercesc::XMLString::transcode(name.c_str()         ));
    // fHistName->appendChild(fHistNameVal); 
 
    xercesc::DOMElement * leaf            = theDocument_->createElement( xercesc::XMLString::transcode("leaf"                ));
    node->appendChild(leaf);
    STDLINE(nodes,string(ACCyan)+string(ACReverse)) ;

    xercesc::DOMText    * leafVal        = theDocument_->createTextNode(xercesc::XMLString::transcode(isALeaf_[isLeaf].c_str()));
    leaf->appendChild(leafVal);
    
    STDLINE(string("fSystemPath_ : ")+fSystemPath_ ,ACRed);
    STDLINE(string("fHistName_   : ")+name         ,ACRed);
    STDLINE(string("fFoldersPath_: ")+fFoldersPath_,ACRed);
    STDLINE(string("fRootPath_   : ")+fRootPath_   ,ACRed);
    
    fFoldersPath_ = "" ;

    return node;
}
//================================================================================================
void RootFileExplorer::makeDirectoryBinaryTree(TDirectory          * currentDirectory, 
                                               int                   level           ,
                                               xercesc::DOMElement * anchorNode       )
{
    if( !anchorNode) anchorNode = rootElement_ ;

    TKey * keyH = NULL ;
    TIter hList(currentDirectory->GetListOfKeys());
    while((keyH = (TKey*)hList()))
    {
        std::string hName = keyH->GetName() ;  
        string what = keyH->GetClassName() ;
        if( what == "TTree"       ) continue ;
        if( what == "TNtuple"     ) continue ;
        if( what == "TGeoManager" ) continue ;
        if( what == "TGeoVolume" ) continue ;
        if( debug_ ) STDLINE(string("currentDirectory: ")+string(currentDirectory->GetName()),ACCyan) ;
        if( debug_ ) STDLINE(string("Object type     : ")+what,ACRed) ;
        if( keyH->IsFolder() ) 
        {
            fThisFolderPath_ = hName ;
            currentDirectory->cd(hName.c_str());
            TDirectory * subDir = gDirectory ;
            if( theHierarchy_.find(level) == theHierarchy_.end() ) 
            {
                theHierarchy_[level] = subDir->GetName() ;
                ss_.str("") ; ss_ << "theHierarchy_[" << level << "] = " << theHierarchy_[level] ;
                STDLINE(ss_.str(),ACWhite) ;
            }
            if( debug_ ) STDLINE(fFoldersPath_,ACBlue) ;
            if( debug_ ) STDLINE(subDir->GetName(),ACCyan) ;
            xercesc::DOMElement * node = this->populateBinaryTreeNode(anchorNode, hName, level, false) ;
            this->makeDirectoryBinaryTree(subDir,level+1,node) ;
            level-- ;
            theHierarchy_.erase(level) ;
        }
        else
        {
            fFoldersPath_ = "" ;
            if( debug_ ) STDLINE(hName,"") ;
            for(int i=0; i<(int)theHierarchy_.size()-1; i++)
            {
                fFoldersPath_ += theHierarchy_[i] + string("/") ;
                ss_.str("") ; ss_ << "fFoldersPath_: " << fFoldersPath_  ;
                STDLINE(ss_.str(),ACWhite) ;
            }
            //fFoldersPath_ += currentDirectory->GetName() ;
            STDLINE(string("fFoldersPath_: ")+fFoldersPath_,"") ;
            xercesc::DOMElement * node = this->populateBinaryTreeNode(anchorNode, hName, level, true  ) ;
        }
    }
}
