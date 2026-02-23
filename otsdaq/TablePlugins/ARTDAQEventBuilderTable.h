#ifndef _ots_ARTDAQEventBuilderTable_h_
#define _ots_ARTDAQEventBuilderTable_h_

#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"
#include "otsdaq/TablePlugins/SlowControlsTableBase/SlowControlsTableBase.h"

namespace ots
{
class XDAQContextTable;

class ARTDAQEventBuilderTable : public ARTDAQTableBase, public SlowControlsTableBase
{
  public:
	ARTDAQEventBuilderTable(void);
	virtual ~ARTDAQEventBuilderTable(void);

	/// Methods
	void init(ConfigurationManager* configManager) override;

	virtual unsigned int slowControlsHandlerConfig(
	    std::stringstream&    out,
	    ConfigurationManager* configManager,
	    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>*
	        channelList /*= 0*/
	) const override;

	virtual std::string setFilePath(void) const override;
	virtual void        genFlatFHiCL(void) override;
};

}  // namespace ots
#endif
