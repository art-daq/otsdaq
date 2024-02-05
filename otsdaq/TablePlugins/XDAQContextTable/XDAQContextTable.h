#ifndef _ots_XDAQContextTable_h_
#define _ots_XDAQContextTable_h_

#include <string>

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
// clang-format off
class XDAQContextTable : public TableBase
{
  public:

	struct XDAQApplicationProperty
	{
		bool        status_;
		std::string name_, type_, value_;
	};

	struct XDAQApplication
	{
		static const uint8_t 		DEFAULT_PRIORITY;
		static const unsigned int 	GATEWAY_APP_ID;

		std::string  applicationGroupID_;
		std::string  applicationUID_;
		bool         status_;
		std::string  class_;
		unsigned int id_;
		unsigned int instance_;
		std::string  network_;
		std::string  group_;
		std::string  module_;
		std::string  sourceConfig_;
		std::map<std::string /*FSM command*/, uint8_t /*priority*/>
		    stateMachineCommandPriority_;

		std::vector<XDAQApplicationProperty> properties_;
	};

	struct XDAQContext
	{
		std::string                  contextUID_;
		std::string                  sourceConfig_;
		bool                         status_;
		unsigned int                 id_;
		std::string                  address_;
		unsigned int                 port_;
		std::vector<XDAQApplication> applications_;
	};

	XDAQContextTable											(void);
	virtual ~XDAQContextTable									(void);

	// Methods
	void 							init						(ConfigurationManager* configManager);
	void 							extractContexts				(ConfigurationManager* configManager);
	void 							outputXDAQXML				(std::ostream& out);
	// void 								outputAppPriority		(std::ostream &out,
	// const std::string& stateMachineCommand);  void outputXDAQScript (std::ostream
	// &out); void outputARTDAQScript		(std::ostream &out);

	std::string 					getContextUID				(const std::string& url) const;
	std::string 					getApplicationUID			(const std::string& url, unsigned int id) const;
	std::string 					getContextOfApplication		(ConfigurationManager* configManager, const std::string& appUID) const;
	std::string 					getContextOfGateway			(ConfigurationManager* configManager) const;

	const std::vector<XDAQContext>& getContexts					(void) const { return contexts_; }

	static ConfigurationTree 		getContextNode				(const ConfigurationManager* configManager, const std::string& contextUID);
	static ConfigurationTree 		getApplicationNode			(const ConfigurationManager* configManager, const std::string& contextUID, const std::string& appUID);
	static ConfigurationTree 		getSupervisorConfigNode		(const ConfigurationManager* configManager, const std::string& contextUID, const std::string& appUID);
	std::string  					getContextAddress			(const std::string& contextUID = "X", bool wantHttp = false) const;

	// artdaq specific get methods
	const XDAQContext* 				getTheARTDAQSupervisorContext	(void) const;

  private:
	std::vector<XDAQContext> 		contexts_;
	unsigned int /*contextIndex*/ 	artdaqSupervisorContext_;

  public:
	// XDAQ Context Column names
	static struct ColContext
	{
		std::string const colContextUID_               	= "ContextUID";
		std::string const colLinkToApplicationTable_   	= "LinkToApplicationTable";
		std::string const colLinkToApplicationGroupID_ 	= "LinkToApplicationGroupID";
		std::string const colStatus_  					= TableViewColumnInfo::COL_NAME_STATUS;
		std::string const colId_      					= "Id";
		std::string const colAddress_ 					= "Address";
		std::string const colPort_    					= "Port";
	} colContext_;

	// XDAQ App Column names
	static struct ColApplication
	{
		std::string const colApplicationGroupID_    	= "ApplicationGroupID";
		std::string const colApplicationUID_        	= "ApplicationUID";
		std::string const colLinkToSupervisorTable_ 	= "LinkToSupervisorTable";
		std::string const colLinkToSupervisorUID_   	= "LinkToSupervisorUID";
		std::string const colStatus_              		= TableViewColumnInfo::COL_NAME_STATUS;
		std::string const colClass_               		= "Class";
		std::string const colId_                  		= "Id";
		std::string const colInstance_            		= "Instance";
		std::string const colNetwork_             		= "Network";
		std::string const colGroup_               		= "Group";
		std::string const colModule_              		= "Module";
		std::string const colConfigurePriority_   		= "ConfigurePriority";
		std::string const colStartPriority_       		= "StartPriority";
		std::string const colStopPriority_        		= "StopPriority";
		std::string const colLinkToPropertyTable_ 		= "LinkToPropertyTable";
		std::string const colLinkToPropertyGroupID_ 	= "LinkToPropertyGroupID";
	} colApplication_;

	// XDAQ App Property Column names
	static struct ColApplicationProperty
	{
		std::string const colPropertyGroupID_ 			= "PropertyGroupID";
		std::string const colPropertyUID_     			= "UID";
		std::string const colStatus_          			= TableViewColumnInfo::COL_NAME_STATUS;
		std::string const colPropertyName_    			= "PropertyName";
		std::string const colPropertyType_    			= "PropertyType";
		std::string const colPropertyValue_   			= "PropertyValue";
	} colAppProperty_;


  public:
	static const std::set<std::string> 					FETypeClassNames_, DMTypeClassNames_,
	    												LogbookTypeClassNames_, MacroMakerTypeClassNames_, ChatTypeClassNames_,
	    												ConsoleTypeClassNames_, ConfigurationGUITypeClassNames_, CodeEditorTypeClassNames_,
														VisualizerTypeClassNames_, SlowControlsTypeClassNames_;
	static const std::string 							GATEWAY_SUPERVISOR_CLASS, WIZARD_SUPERVISOR_CLASS,
	    												DEPRECATED_SUPERVISOR_CLASS, ARTDAQ_SUPERVISOR_CLASS;
	static const std::map<std::string /*class*/, 
		std::string /*module*/> 						AppClassModuleLookup_;
}; //end XDAQContextTable class
}  // namespace ots
// clang-format on
#endif
