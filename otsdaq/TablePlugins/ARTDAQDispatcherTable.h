#ifndef _ots_ARTDAQDispatcherTable_h_
#define _ots_ARTDAQDispatcherTable_h_

#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"
#include "otsdaq/TablePlugins/SlowControlsTableBase/SlowControlsTableBase.h"

namespace ots
{
class XDAQContextTable;
// clang-format off
class ARTDAQDispatcherTable : public ARTDAQTableBase, public SlowControlsTableBase
{
  public:
	ARTDAQDispatcherTable(void);
	virtual ~ARTDAQDispatcherTable(void);

	// Methods
	void 					init						(ConfigurationManager* configManager) override;

	virtual unsigned int	slowControlsHandlerConfig	(
															  std::stringstream& out
															, ConfigurationManager* configManager
															, std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
														) const override;

	virtual std::string		setFilePath					() const override;
};
// clang-format on
}  // namespace ots
#endif
