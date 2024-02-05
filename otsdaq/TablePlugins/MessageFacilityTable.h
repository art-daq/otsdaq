#ifndef _ots_DesktopIconTable_h_
#define _ots_DesktopIconTable_h_

#include <string>

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class MessageFacilityTable : public TableBase
{
  public:
	MessageFacilityTable(void);
	virtual ~MessageFacilityTable(void);

	// Methods
	void init(ConfigurationManager* configManager);

  private:
};
}  // namespace ots
#endif
