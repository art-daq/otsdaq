#ifndef OTSDAQ_OTSDAQ_TABLEPLUGINS_ARTDAQDISPATCHERTABLE_H_
#define OTSDAQ_OTSDAQ_TABLEPLUGINS_ARTDAQDISPATCHERTABLE_H_

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

	/// Methods
	void 					init						(ConfigurationManager* configManager) override;

	virtual unsigned int	slowControlsHandlerConfig	(
															  std::stringstream& out
															, ConfigurationManager* configManager
															, std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
														) const override;

	virtual std::string		setFilePath					(void) const override;
	virtual void			genFlatFHiCL				(void) override;
};
// clang-format on
}  // namespace ots
#endif  // OTSDAQ_OTSDAQ_TABLEPLUGINS_ARTDAQDISPATCHERTABLE_H_
