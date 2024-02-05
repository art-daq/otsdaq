#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/DesktopIconTable.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include "otsdaq/WebUsersUtilities/WebUsers.h"

#include <stdio.h>
#include <fstream>  // std::fstream
#include <iostream>
using namespace ots;

#define DESKTOP_ICONS_FILE std::string(__ENV__("SERVICE_DATA_PATH")) + "/OtsWizardData/iconList.dat"

// DesktopIconTable Column names

const std::string DesktopIconTable::COL_NAME                    = "IconName";
const std::string DesktopIconTable::COL_STATUS                  = TableViewColumnInfo::COL_NAME_STATUS;
const std::string DesktopIconTable::COL_CAPTION                 = "Caption";
const std::string DesktopIconTable::COL_ALTERNATE_TEXT          = "AlternateText";
const std::string DesktopIconTable::COL_FORCE_ONLY_ONE_INSTANCE = "ForceOnlyOneInstance";
const std::string DesktopIconTable::COL_PERMISSIONS             = "RequiredPermissionLevel";
const std::string DesktopIconTable::COL_IMAGE_URL               = "ImageURL";
const std::string DesktopIconTable::COL_WINDOW_CONTENT_URL      = "WindowContentURL";
const std::string DesktopIconTable::COL_APP_LINK                = "LinkToApplicationTable";
const std::string DesktopIconTable::COL_APP_LINK_UID            = "LinkToApplicationUID";

const std::string DesktopIconTable::COL_PARAMETER_LINK     = "LinkToParameterTable";
const std::string DesktopIconTable::COL_PARAMETER_LINK_GID = "LinkToParameterGroupID";
const std::string DesktopIconTable::COL_FOLDER_PATH        = "FolderPath";

const std::string DesktopIconTable::COL_PARAMETER_GID   = "windowParameterGroupID";
const std::string DesktopIconTable::COL_PARAMETER_KEY   = "windowParameterKey";
const std::string DesktopIconTable::COL_PARAMETER_VALUE = "windowParameterValue";

const std::string DesktopIconTable::ICON_TABLE      = ConfigurationManager::DESKTOP_ICON_TABLE_NAME;
const std::string DesktopIconTable::PARAMETER_TABLE = "DesktopWindowParameterTable";

// #define COL_NAME "IconName"
// #define COL_STATUS TableViewColumnInfo::COL_NAME_STATUS
// #define COL_CAPTION "Caption"
// #define COL_ALTERNATE_TEXT "AlternateText"
// #define COL_FORCE_ONLY_ONE_INSTANCE "ForceOnlyOneInstance"
// #define COL_REQUIRED_PERMISSION_LEVEL "RequiredPermissionLevel"
// #define COL_IMAGE_URL "ImageURL"
// #define COL_WINDOW_CONTENT_URL "WindowContentURL"
// #define COL_APP_LINK "LinkToApplicationTable"
// #define COL_PARAMETER_LINK "LinkToParameterTable"
// #define COL_PARAMETER_KEY "windowParameterKey"
// #define COL_PARAMETER_VALUE "windowParameterValue"
// #define COL_FOLDER_PATH "FolderPath"

// XDAQ App Column names
const std::string DesktopIconTable::COL_APP_ID = "Id";
// #define COL_APP_ID "Id"

//==============================================================================
DesktopIconTable::DesktopIconTable(void) : TableBase(DesktopIconTable::ICON_TABLE)
{
	// Icon list no longer passes through file! so delete it from user's $USER_DATA
	std::system(("rm -rf " + (std::string)DESKTOP_ICONS_FILE).c_str());

	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  //
	//////////////////////////////////////////////////////////////////////
}

//==============================================================================
DesktopIconTable::~DesktopIconTable(void) {}

//==============================================================================
void DesktopIconTable::init(ConfigurationManager* configManager)
{
	//	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << std::endl;
	//	__COUT__ << configManager->__SELF_NODE__ << std::endl;

	unsigned int intVal;

	auto childrenMap = configManager->__SELF_NODE__.getChildren();

	ConfigurationTree       contextTableNode = configManager->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME);
	const XDAQContextTable* contextTable     = configManager->getTable<XDAQContextTable>(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME);

	// find gateway host origin string, to avoid modifying icons with same host
	std::string gatewayContextUID = contextTable->getContextOfGateway(configManager);

	activeDesktopIcons_.clear();

	DesktopIconTable::DesktopIcon* icon;
	bool                           addedAppId;
	bool                           numeric;
	unsigned int                   i;
	for(auto& child : childrenMap)
	{
		if(!child.second.getNode(COL_STATUS).getValue<bool>())
			continue;
		// __COUTV__(child.first);

		activeDesktopIcons_.push_back(DesktopIconTable::DesktopIcon());
		icon = &(activeDesktopIcons_.back());

		icon->caption_                   = child.second.getNode(COL_CAPTION).getValue<std::string>();
		icon->alternateText_             = child.second.getNode(COL_ALTERNATE_TEXT).getValue<std::string>();
		icon->enforceOneWindowInstance_  = child.second.getNode(COL_FORCE_ONLY_ONE_INSTANCE).getValue<bool>();
		icon->permissionThresholdString_ = child.second.getNode(COL_PERMISSIONS).getValue<std::string>();
		icon->imageURL_                  = child.second.getNode(COL_IMAGE_URL).getValue<std::string>();
		icon->windowContentURL_          = child.second.getNode(COL_WINDOW_CONTENT_URL).getValue<std::string>();
		icon->folderPath_                = child.second.getNode(COL_FOLDER_PATH).getValue<std::string>();

		if(icon->folderPath_ == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
			icon->folderPath_ = "";  // convert DEFAULT to empty string

		if(icon->permissionThresholdString_ == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
			icon->permissionThresholdString_ = "1";  // convert DEFAULT to standard user allow

		numeric = true;
		for(i = 0; i < icon->permissionThresholdString_.size(); ++i)
			if(!(icon->permissionThresholdString_[i] >= '0' && icon->permissionThresholdString_[i] <= '9'))
			{
				numeric = false;
				break;
			}
		// for backwards compatibility, if permissions threshold is a single number
		//	assume it is the threshold intended for the WebUsers::DEFAULT_USER_GROUP group
		if(numeric)
			icon->permissionThresholdString_ = WebUsers::DEFAULT_USER_GROUP + ":" + icon->permissionThresholdString_;

		// remove all commas from member strings because desktop icons are served to
		// client in comma-separated string
		icon->caption_          = removeCommas(icon->caption_, false /*andHexReplace*/, true /*andHTMLReplace*/);
		icon->alternateText_    = removeCommas(icon->alternateText_, false /*andHexReplace*/, true /*andHTMLReplace*/);
		icon->imageURL_         = removeCommas(icon->imageURL_, true /*andHexReplace*/);
		icon->windowContentURL_ = removeCommas(icon->windowContentURL_, true /*andHexReplace*/);
		icon->folderPath_       = removeCommas(icon->folderPath_, false /*andHexReplace*/, true /*andHTMLReplace*/);

		// add application origin and URN/LID to windowContentURL_, if link is given
		addedAppId                = false;
		ConfigurationTree appLink = child.second.getNode(COL_APP_LINK);
		if(!appLink.isDisconnected())
		{
			// first check app origin
			if(icon->windowContentURL_.size() && icon->windowContentURL_[0] == '/')
			{
				// if starting with opening slash, then assume app should come from
				//	appLink context's origin (to avoid cross-origin issues communicating
				//	with app/supervisor)
				try
				{
					std::string contextUID = contextTable->getContextOfApplication(configManager, appLink.getValueAsString());

					// only prepend address if not same as gateway
					if(contextUID != gatewayContextUID)
					{
						// __COUTV__(contextUID);
						ConfigurationTree contextNode = contextTableNode.getNode(contextUID);

						std::string  contextAddress = contextNode.getNode(XDAQContextTable::colContext_.colAddress_).getValue<std::string>();
						unsigned int contextPort    = contextNode.getNode(XDAQContextTable::colContext_.colPort_).getValue<unsigned int>();

						//__COUTV__(contextAddress);
						icon->windowContentURL_ = contextAddress + ":" + std::to_string(contextPort) + icon->windowContentURL_;
						//__COUTV__(icon->windowContentURL_);
					
					}
				}
				catch(const std::runtime_error& e)
				{
					__SS__ << "Error finding XDAQ Application origin which was linked to Desktop Icon '" << child.first << "': " << e.what() << __E__;
					ss << "\n\nPlease fix by disabling the Icon, enabling the App or fixing the link in the Configurate Tree." << __E__;
					__SS_THROW__;
				}
			}  // end app origin check

			// if last character is not '='
			//	then assume need to add "?urn="
			if(icon->windowContentURL_[icon->windowContentURL_.size() - 1] != '=')
				icon->windowContentURL_ += "?urn=";

			// __COUT__ << "Following Application link." << std::endl;
			appLink.getNode(COL_APP_ID).getValue(intVal);
			icon->windowContentURL_ += std::to_string(intVal);

			// __COUT__ << "URN/LID=" << intVal << std::endl;
			addedAppId = true;
		}
		// __COUTV__(icon->windowContentURL_);

		// add parameters if link is given
		if(!child.second.getNode(COL_PARAMETER_LINK).isDisconnected())
		{
			// if there is no '?' found
			//	then assume need to add "?"
			if(icon->windowContentURL_.find('?') == std::string::npos)
				icon->windowContentURL_ += '?';
			else if(addedAppId || icon->windowContentURL_[icon->windowContentURL_.size() - 1] != '?')  // if not first parameter, add &
				icon->windowContentURL_ += '&';

			// now add each paramter separated by &
			auto paramGroupMap = child.second.getNode(COL_PARAMETER_LINK).getChildren();
			bool notFirst      = false;
			for(const auto& param : paramGroupMap)
			{
				if(!param.second.isEnabled())
					continue;

				if(notFirst)
					icon->windowContentURL_ += '&';
				else
					notFirst = true;
				icon->windowContentURL_ += StringMacros::encodeURIComponent(param.second.getNode(COL_PARAMETER_KEY).getValue<std::string>()) + "=" +
				                           StringMacros::encodeURIComponent(param.second.getNode(COL_PARAMETER_VALUE).getValue<std::string>());
			}
		}
	}  // end main icon extraction loop

}  // end init()

//==============================================================================
std::string DesktopIconTable::removeCommas(const std::string& str, bool andHexReplace, bool andHTMLReplace)
{
	std::string retStr = "";
	retStr.reserve(str.length());

	for(unsigned int i = 0; i < str.length(); ++i)
		if(str[i] != ',')
			retStr += str[i];
		else if(andHexReplace)
			retStr += "%2C";
		else if(andHTMLReplace)
			retStr += "&#44;";

	return retStr;
}  // end removeCommas()

//==============================================================================
void DesktopIconTable::setAllDesktopIcons(const std::vector<DesktopIconTable::DesktopIcon>& newIcons)
{
	activeDesktopIcons_.clear();
	for(const auto& newIcon : newIcons)
		activeDesktopIcons_.push_back(newIcon);

}  // end setAllDesktopIcons

DEFINE_OTS_TABLE(DesktopIconTable)
