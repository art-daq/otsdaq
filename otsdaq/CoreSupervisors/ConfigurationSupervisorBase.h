#ifndef _ots_ConfigurationSupervisorBase_h_
#define _ots_ConfigurationSupervisorBase_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include "otsdaq/TablePlugins/DesktopIconTable.h"

namespace ots
{
// clang-format off

// ConfigurationSupervisorBase
//	This class provides supervisor level features for manipulating the configuration
class ConfigurationSupervisorBase
{
public:

	static void 				getConfigurationStatusXML(HttpXmlDocument& xmlOut, ConfigurationManagerRW* cfgMgr);

	static TableVersion 		saveModifiedVersionXML(HttpXmlDocument&        xmlOut,
		                                    ConfigurationManagerRW* cfgMgr,
		                                    const std::string&      tableName,
		                                    TableVersion            originalVersion,
		                                    bool                    makeTemporary,
		                                    TableBase*              config,
		                                    TableVersion            temporaryModifiedVersion,
		                                    bool                    ignoreDuplicates = false,
		                                    bool lookForEquivalent                   = false);

	static void 				handleCreateTableXML(HttpXmlDocument&        xmlOut,
										  ConfigurationManagerRW* cfgMgr,
										  const std::string&      tableName,
										  TableVersion            version,
										  bool                    makeTemporary,
										  const std::string&      data,
										  const int&              dataOffset,
										  const std::string&      author,
										  const std::string&      comment,
										  bool                    sourceTableAsIs,
										  bool                    lookForEquivalent);

	static void 				handleCreateTableGroupXML(HttpXmlDocument&        xmlOut,
									   ConfigurationManagerRW* cfgMgr,
									   const std::string&      groupName,
									   const std::string&      configList,
									   bool                    allowDuplicates   = false,
									   bool                    ignoreWarnings    = false,
									   const std::string&      groupComment      = "",
									   bool                    lookForEquivalent = false);

	static void 				handleGetTableGroupXML(HttpXmlDocument&        xmlOut,
										ConfigurationManagerRW* cfgMgr,
										const std::string&      groupName,
										TableGroupKey           groupKey,
										bool                    ignoreWarnings = false);

	static bool					handleAddDesktopIconXML(
										HttpXmlDocument&        xmlOut,
										ConfigurationManagerRW* cfgMgr,
										const std::string&      iconCaption,
										const std::string&      iconAltText,
										const std::string&      iconFolderPath,
										const std::string&      iconImageURL,
										const std::string&      iconWindowURL,
										const std::string&      iconPermissions,
										std::string      		windowLinkedApp = "",
										unsigned int	      	windowLinkedAppLID = 0,
										bool				    enforceOneWindowInstance = false,
										const std::string&      windowParameters = "");
	static void					recursiveCopyTreeUIDNode	(
										HttpXmlDocument&        xmlOut,
										ConfigurationManagerRW* cfgMgr,
										std::map<std::string /*modified table*/, TableVersion /* modified version */>& modifiedTablesMap,										
										const unsigned int 		startingDepth,
										const unsigned int      depth,
										const unsigned int 		numberOfInstances,
										TableView*				cfgView,
										const std::string&		uidToCopy
										);
	
	
};

// clang-format on

}  // namespace ots

#endif
