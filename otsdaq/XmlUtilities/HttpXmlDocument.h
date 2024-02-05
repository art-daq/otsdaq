#ifndef ots_HttpXmlDocument_h
#define ots_HttpXmlDocument_h

#include "otsdaq/XmlUtilities/XmlDocument.h"

#include <stdexcept>
#include <vector>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>

namespace ots
{
// clang-format off
class HttpXmlDocument : public XmlDocument
{
  public:
								HttpXmlDocument		   (std::string cookieCode = "", std::string displayName = "");
								HttpXmlDocument		   (const HttpXmlDocument& doc);
								HttpXmlDocument& operator= (const HttpXmlDocument& doc);
								~HttpXmlDocument	   (void);

	void 		      			setHeader                       (std::string  cookieCode = "", std::string displayName = "");
	xercesc::DOMElement* 		getRootDataElement              (void ) { return dataElement_; }
	xercesc::DOMElement* 		addTextElementToData            (const std::string          & field					,  	const std::string                       & value = ""   		);
	xercesc::DOMElement* 		addBinaryStringToData           (const std::string          & field					,  	const std::string                       & binary        	);
	void 		      			copyDataChildren                (      HttpXmlDocument      & document             );	
	std::string 	      		getMatchingValue                (const std::string          & field					,  	const unsigned int                        occurance = 0 	);
	void 		      			getAllMatchingValues            (const std::string          & field					,    	  std::vector<std::string>          & retVec);	
	xercesc::DOMElement* 		getMatchingElement              (const std::string          & field					,	const unsigned int                        occurance = 0 	);
	xercesc::DOMElement* 		getMatchingElementInSubtree     (      xercesc::DOMElement  * currEl				, 	const std::string                       & field,  				const unsigned int           			occurance = 0     		);
	void                  		getAllMatchingElements          (const std::string          & field					,   	  std::vector<xercesc::DOMElement*> & retVec        	);
	void                  		outputXmlDocument               (      std::ostringstream   * out					,   	  bool                                dispStdOut = false,  		  bool                   			allowWhiteSpace = false	);
	bool                  		loadXmlDocument                 (const std::string          & filePath             );
	unsigned int 	      		getChildrenCount                (      xercesc::DOMElement  * parent         = 0   );
	void 		      			removeDataElement               (  	   unsigned int           dataChildIndex = 0   );  // default to first child

	std::stringstream			dataSs_		   ; /* use for large xml response construction */
  private:
	void 		      			recursiveAddElementToParent     (      xercesc::DOMElement  * child           		, 	xercesc::DOMElement               		* parent         	,     	  bool                                html = false          );
	void 		      			recursiveFindAllElements        (      xercesc::DOMElement  * currEl          		, 	const std::string                       & field          	, 		  std::vector<std::string>          * retVec                );
	void 		      			recursiveFindAllElements        (      xercesc::DOMElement  * currEl          		, 	const std::string                       & field          	,   	  std::vector<xercesc::DOMElement*> * retVec                );
	void                  		recursiveOutputXmlDocument      (      xercesc::DOMElement  * currEl          		, 	std::ostringstream                		* out            	,		  bool                                dispStdOut = false 	,	std::string   tabStr = ""    , bool allowWhiteSpace = false);
	void                  		recursiveFixTextFields          (      xercesc::DOMElement  * currEl     			);
	std::string           		recursiveFindElementValue       (      xercesc::DOMElement  * currEl               	, 	const std::string                       & field          	,	const unsigned int                        occurance             ,   unsigned int  & count        );
	xercesc::DOMElement* 		recursiveFindElement            (      xercesc::DOMElement  * currEl               	, 	const std::string                       & field          	,	const unsigned int                        occurance             ,  	unsigned int  & count        );

	xercesc::DOMElement* 		headerElement_     ;
	xercesc::DOMElement* 		dataElement_       ;

	const std::string    		headerTagName_     ;
	const std::string    		dataTagName_       ;
	const std::string    		cookieCodeTagName_ ;
	const std::string    		displayNameTagName_;
};
// clang-format on
}  // namespace ots

#endif  // ots_HttpXmlDocument_h
