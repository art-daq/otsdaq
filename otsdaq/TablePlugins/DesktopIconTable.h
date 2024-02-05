#ifndef _ots_DesktopIconTable_h_
#define _ots_DesktopIconTable_h_

#include <string>

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class DesktopIconTable : public TableBase
{
  public:
	DesktopIconTable(void);
	virtual ~DesktopIconTable(void);

	// Methods
	void init(ConfigurationManager* configManager);

	struct DesktopIcon
	{
		bool        enforceOneWindowInstance_;
		std::string caption_, alternateText_, imageURL_, windowContentURL_, folderPath_;
		std::string permissionThresholdString_;  // <groupName>:<permissionsThreshold>
		                                         // pairs separated by ',' '&' or '|'
	};

	const std::vector<DesktopIconTable::DesktopIcon>& getAllDesktopIcons() const { return activeDesktopIcons_; }  // activeDesktopIcons_ is setup in init
	void setAllDesktopIcons(const std::vector<DesktopIconTable::DesktopIcon>& newIcons);                          // overwrite dynamically the init result

	static const std::string COL_NAME;
	static const std::string COL_STATUS;
	static const std::string COL_CAPTION;
	static const std::string COL_ALTERNATE_TEXT;
	static const std::string COL_FORCE_ONLY_ONE_INSTANCE;
	static const std::string COL_PERMISSIONS;
	static const std::string COL_IMAGE_URL;
	static const std::string COL_WINDOW_CONTENT_URL;
	static const std::string COL_APP_LINK;
	static const std::string COL_APP_LINK_UID;
	static const std::string COL_PARAMETER_LINK;
	static const std::string COL_PARAMETER_LINK_GID;
	static const std::string COL_FOLDER_PATH;

	static const std::string COL_PARAMETER_GID;
	static const std::string COL_PARAMETER_KEY;
	static const std::string COL_PARAMETER_VALUE;

	static const std::string ICON_TABLE;
	static const std::string PARAMETER_TABLE;

	static const std::string COL_APP_ID;

  private:
	std::string removeCommas(const std::string& str, bool andHexReplace = false, bool andHTMLReplace = false);

	std::vector<DesktopIconTable::DesktopIcon> activeDesktopIcons_;  // only icons with status=true
};
}  // namespace ots
#endif
