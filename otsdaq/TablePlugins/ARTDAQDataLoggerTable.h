#ifndef _ots_ARTDAQDataLoggerTable_h_
#define _ots_ARTDAQDataLoggerTable_h_

#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"
#include "otsdaq/TablePlugins/SlowControlsTableBase/SlowControlsTableBase.h"

namespace ots
{
class XDAQContextTable;
// clang-format off
class ARTDAQDataLoggerTable : public ARTDAQTableBase, public SlowControlsTableBase
{
  public:
	ARTDAQDataLoggerTable(void);
	virtual ~ARTDAQDataLoggerTable(void);

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
