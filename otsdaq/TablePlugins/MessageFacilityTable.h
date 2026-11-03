#ifndef OTSDAQ_OTSDAQ_TABLEPLUGINS_MESSAGEFACILITYTABLE_H_
#define OTSDAQ_OTSDAQ_TABLEPLUGINS_MESSAGEFACILITYTABLE_H_

#include <string>

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class MessageFacilityTable : public TableBase
{
  public:
	MessageFacilityTable(void);
	virtual ~MessageFacilityTable(void);

	/// Methods
	void init(ConfigurationManager* configManager);

  private:
};
}  // namespace ots
#endif  // OTSDAQ_OTSDAQ_TABLEPLUGINS_MESSAGEFACILITYTABLE_H_
