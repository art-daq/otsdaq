#ifndef _ots_TableInfoReader_h_
#define _ots_TableInfoReader_h_

#include <string>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/util/XMLChar.hpp>

namespace ots
{
class TableBase;

class TableInfoReader
{
  public:
	TableInfoReader(bool allowIllegalColumns = false);
	~TableInfoReader(void);
	std::string read(TableBase& table);
	std::string read(TableBase* table);

	void        setAllowColumnErrors(bool setValue);
	const bool& getAllowColumnErrors(void);

  private:
	void initPlatform(void);
	void terminatePlatform(void);
	bool checkViewType(std::string type);

	xercesc::DOMNode*    getNode(XMLCh* tagName, xercesc::DOMNode* parent, unsigned int itemNumber);
	xercesc::DOMNode*    getNode(XMLCh* tagName, xercesc::DOMElement* parent, unsigned int itemNumber);
	xercesc::DOMElement* getElement(XMLCh* tagName, xercesc::DOMNode* parent, unsigned int itemNumber);
	xercesc::DOMElement* getElement(XMLCh* tagName, xercesc::DOMElement* parent, unsigned int itemNumber);

	XMLCh* rootTag_;
	XMLCh* tableTag_;
	XMLCh* tableNameAttributeTag_;
	XMLCh* viewTag_;
	XMLCh* viewNameAttributeTag_;
	XMLCh* viewTypeAttributeTag_;
	XMLCh* viewDescriptionAttributeTag_;
	XMLCh* columnTag_;
	XMLCh* columnTypeAttributeTag_;
	XMLCh* columnNameAttributeTag_;
	XMLCh* columnStorageNameAttributeTag_;
	XMLCh* columnDataTypeAttributeTag_;
	XMLCh* columnDataChoicesAttributeTag_;
	XMLCh* columnDefaultValueAttributeTag_;
	XMLCh* columnMinValueAttributeTag_;
	XMLCh* columnMaxValueAttributeTag_;

	// adding two new tags for min and max

	bool allowIllegalColumns_;

	// static const std::string CONFIGURATION_BACKEND_TYPE_;
};

}  // namespace ots

#endif
