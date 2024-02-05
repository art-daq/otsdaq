#ifndef _ots_FESlowControlsTable_h_
#define _ots_FESlowControlsTable_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"

#include <string>
#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class FESlowControlsTable : public TableBase
{
  public:
	FESlowControlsTable(void);
	virtual ~FESlowControlsTable(void);

	// Methods
	void init(ConfigurationManager* configManager);

	// Getters

  private:
	// Column names
	struct ColSlowControls
	{
		std::string const colDataType_ = "ChannelDataType";
	} colNames_;
};
}  // namespace ots
#endif
