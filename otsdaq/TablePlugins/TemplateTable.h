#ifndef _ots_TemplateTable_h_
#define _ots_TemplateTable_h_

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class TemplateTable : public TableBase
{
  public:
	TemplateTable(void);
	virtual ~TemplateTable(void);

	// Methods
	void init(ConfigurationManager* configManager);

	// Getters

  private:
	// Column names
	struct ColTemplate
	{
		std::string const colColumnName_ = "ColumnName";
	} colNames_;
};
}  // namespace ots
#endif
