#ifndef OTSDAQ_OTSDAQ_TABLEPLUGINS_ARTDAQEVENTBUILDERTABLE_H_
#define OTSDAQ_OTSDAQ_TABLEPLUGINS_ARTDAQEVENTBUILDERTABLE_H_

#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"
#include "otsdaq/TablePlugins/SlowControlsTableBase/SlowControlsTableBase.h"

namespace ots
{
class XDAQContextTable;
// clang-format off
class ARTDAQEventBuilderTable : public ARTDAQTableBase, public SlowControlsTableBase
{
  public:
	ARTDAQEventBuilderTable(void);
	virtual ~ARTDAQEventBuilderTable(void);

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
#endif  // OTSDAQ_OTSDAQ_TABLEPLUGINS_ARTDAQEVENTBUILDERTABLE_H_
