#include "otsdaq-core/ConfigurationInterface/ConfigurationTree.h"
#include "otsdaq-core/ConfigurationDataFormats/ConfigurationBase.h"

#include "otsdaq-core/ConfigurationInterface/ConfigurationManager.h"

#include <typeinfo>


using namespace ots;


#undef 	__MF_SUBJECT__
#define __MF_SUBJECT__ "ConfigurationTree"

const std::string ConfigurationTree::DISCONNECTED_VALUE 		= "X";
const std::string ConfigurationTree::VALUE_TYPE_DISCONNECTED 	= "Disconnected";
const std::string ConfigurationTree::VALUE_TYPE_NODE			= "Node";

//==============================================================================
ConfigurationTree::ConfigurationTree()
: configMgr_				(0),
  configuration_			(0),
  groupId_					(""),
  linkColName_				(""),
  linkColValue_				(""),
  disconnectedTargetName_	(""),
  disconnectedLinkID_		(""),
  childLinkIndex_			(""),
  row_						(0),
  col_						(0),
  configView_				(0)
{
	//std::cout << __PRETTY_FUNCTION__ << std::endl;
	//std::cout << __PRETTY_FUNCTION__ << "EMPTY CONSTRUCTOR ConfigManager: " << configMgr_ << " configuration: " << configuration_  << std::endl;
}
//==============================================================================
ConfigurationTree::ConfigurationTree(const ConfigurationManager* const &configMgr, const ConfigurationBase* const &config)
: ConfigurationTree(configMgr, config, "", "", "", "", "" /*disconnectedLinkID_*/, "", ConfigurationView::INVALID, ConfigurationView::INVALID)
{
	//std::cout << __PRETTY_FUNCTION__ << std::endl;
	//std::cout << __PRETTY_FUNCTION__ << "SHORT CONTRUCTOR ConfigManager: " << configMgr_ << " configuration: " << configuration_  << std::endl;
}

//==============================================================================
ConfigurationTree::ConfigurationTree(
		const ConfigurationManager* const& configMgr,
		const ConfigurationBase* const&    config,
		const std::string&                 groupId,
		const std::string&                 linkColName,
		const std::string&                 linkColValue,
		const std::string&                 disconnectedTargetName,
		const std::string&                 disconnectedLinkID,
		const std::string&                 childLinkIndex,
		const unsigned int                 row,
		const unsigned int                 col)
: configMgr_				(configMgr),
  configuration_			(config),
  groupId_					(groupId),
  linkColName_				(linkColName),
  linkColValue_				(linkColValue),
  disconnectedTargetName_ 	(disconnectedTargetName),
  disconnectedLinkID_		(disconnectedLinkID),
  childLinkIndex_			(childLinkIndex),
  row_						(row),
  col_						(col),
  configView_				(0)
{
	//std::cout << __PRETTY_FUNCTION__ << std::endl;
	//std::cout << __PRETTY_FUNCTION__ << "FULL CONTRUCTOR ConfigManager: " << configMgr_ << " configuration: " << configuration_ << std::endl;
	if(!configMgr_)// || !configuration_ || !configView_)
	{
		std::stringstream ss;
		ss << __COUT_HDR_FL__ << "Invalid empty pointer given to tree!\n" <<
				"\n\tconfigMgr_=" << configMgr_ <<
				"\n\tconfiguration_=" << configuration_ <<
				"\n\tconfigView_=" << configView_ <<
				std::endl;
		__MOUT__ << "\n" << ss.str();
		throw std::runtime_error(ss.str());
	}

	if(configuration_)
		configView_	 = &(configuration_->getView());

	//verify UID column exists
	if(configView_ &&
			configView_->getColumnInfo(configView_->getColUID()).getType() != ViewColumnInfo::TYPE_UID)
	{
		__SS__ << "Missing UID column (must column of type  " << ViewColumnInfo::TYPE_UID <<
				") in config view : " << configView_->getTableName() << std::endl;
		__MOUT__ << "\n" << ss.str();
		throw std::runtime_error(ss.str());
	}
}

//==============================================================================
//destructor
ConfigurationTree::~ConfigurationTree(void)
{
	//std::cout << __PRETTY_FUNCTION__ << std::endl;
}

//==============================================================================
//print
//	print out tree from this node for desired depth
//	depth of 0 means print out only this node's value
//	depth of 1 means include this node's children's values, etc..
//	depth of -1 means print full tree
void ConfigurationTree::print(const unsigned int &depth, std::ostream &out) const
{
	recursivePrint(*this,depth,out,"\t");
}

//==============================================================================
void ConfigurationTree::recursivePrint(const ConfigurationTree &t, unsigned int depth, std::ostream &out, std::string space)
{
	if(t.isValueNode())
		out << space << t.getValueName() << " :\t" << t.getValueAsString() << std::endl;
	else
	{
		if(t.isLinkNode())
		{
			out << space << t.getValueName();
			if(t.isDisconnected())
			{
				out << " :\t" << t.getValueAsString() << std::endl;
				return;
			}
			out << " (" <<
					(t.isGroupLinkNode()?"Group":"U") <<
					"ID=" << t.getValueAsString() <<
					") : " << std::endl;
		}
		else
			out << space << t.getValueAsString() << " : " << std::endl;

		//if depth>=1 print all children
		//	child.print(depth-1)
		if(depth >= 1)
		{
			auto C = t.getChildren();
			if(!C.empty())
				out << space << "{" << std::endl;
			for(auto &c:C)
				recursivePrint(c.second,depth-1,out,space + "   ");
			if(!C.empty())
				out << space << "}" << std::endl;
		}
	}
}


//==============================================================================
//getValue (only std::string value)
//special version of getValue for string type
//	Note: necessary because types of std::basic_string<char> cause compiler problems if no string specific function
void ConfigurationTree::getValue(std::string& value) const
{
	//__MOUT__ << row_ << " " << col_ << " p: " << configView_<< std::endl;

	if(row_ != ConfigurationView::INVALID && col_ != ConfigurationView::INVALID)	//this node is a value node
	{
		//attempt to interpret the value as a tree node path itself
		try
		{
			ConfigurationTree valueAsTreeNode = getValueAsTreeNode();
			//valueAsTreeNode.getValue<T>(value);
			__MOUT__ << "Success following path to tree node!" << std::endl;
			//value has been interpreted as a tree node value
			//now verify result under the rules of this column
			//if(typeid(std::string) == typeid(value))

			//Note: want to interpret table value as though it is in column of different table
			//	this allows a number to be read as a string, for example, without exceptions
			value =	configView_->validateValueForColumn(
					valueAsTreeNode.getValueAsString(),col_);


			__MOUT__ << "Successful value!" << std::endl;

			//else
			//	value = configView_->validateValueForColumn<T>(
			//		valueAsTreeNode.getValueAsString(),col_);

			return;
		}
		catch(...) //tree node path interpretation failed
		{
			//__MOUT__ << "Invalid path, just returning normal value." << std::endl;
		}

		//else normal return
		configView_->getValue(value,row_,col_);
	}
	else if(row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID)	//this node is config node maybe with groupId
	{
		if(isLinkNode() && isDisconnected())
			value = (groupId_ == "") ? getValueName():groupId_; //a disconnected link still knows its table name or groupId
		else
			value = (groupId_ == "") ? configuration_->getConfigurationName():groupId_;
	}
	else if(row_ == ConfigurationView::INVALID)
	{
		__MOUT__ << std::endl;
		throw std::runtime_error("Malformed ConfigurationTree");
	}
	else if(col_ == ConfigurationView::INVALID)						//this node is uid node
		configView_->getValue(value,row_,configView_->getColUID());
	else
	{
		__MOUT__ << std::endl;
		throw std::runtime_error("Impossible.");
	}

}

//==============================================================================
//getValue
//	Only std::string value will work.
//		If this is a value node, and not type string, configView->getValue should
//		throw exception.
//
//	NOTE: getValueAsString() method should be preferred if getting the Link UID
//		because when disconnected will return "X". getValue() would return the
//		column name of the link when disconnected.
//
////special version of getValue for string type
//	Note: necessary because types of std::basic_string<char> cause compiler problems if no string specific function
std::string ConfigurationTree::getValue() const
{
	std::string value;
	ConfigurationTree::getValue(value);
	return value;

	//__MOUT__ << row_ << " " << col_ << " p: " << configView_<< std::endl;

//	std::string value = "";
//	if(row_ != ConfigurationView::INVALID && col_ != ConfigurationView::INVALID)	//this node is a value node
//		configView_->getValue(value,row_,col_);
//	else if(row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID)	//this node is config node maybe with groupId
//	{
//		if(isLinkNode() && isDisconnected())
//			value = (groupId_ == "") ? getValueName():groupId_; //a disconnected link still knows its table name or groupId
//		else
//			value = (groupId_ == "") ? configuration_->getConfigurationName():groupId_;
//	}
//	else if(row_ == ConfigurationView::INVALID)
//	{
//		__MOUT__ << std::endl;
//		throw std::runtime_error("Malformed ConfigurationTree");
//	}
//	else if(col_ == ConfigurationView::INVALID)						//this node is uid node
//		configView_->getValue(value,row_,configView_->getColUID());
//	else
//	{
//		__MOUT__ << std::endl;
//		throw std::runtime_error("Impossible.");
//	}
//	return value;

}

//==============================================================================
//getEscapedValue
//	Only works if a value node, other exception thrown
std::string ConfigurationTree::getEscapedValue() const
{
	if(row_ != ConfigurationView::INVALID && col_ != ConfigurationView::INVALID)	//this node is a value node
		return configView_->getEscapedValueAsString(row_,col_);

	__SS__ << "Can't get escaped value except from a value node!" <<
			" This node is type '" << getNodeType() << "." << std::endl;
	__MOUT_ERR__ << "\n" << ss.str();
	throw std::runtime_error(ss.str());
}

//==============================================================================
//getConfigurationName
const std::string& ConfigurationTree::getConfigurationName(void) const
{
	if(!configuration_)
	{
		__SS__ << "Can't get configuration name of node with no configuration pointer!" << std::endl;
		throw std::runtime_error(ss.str());
	}
	return configuration_->getConfigurationName();
}

//==============================================================================
//getDisconnectedTableName
const std::string& ConfigurationTree::getDisconnectedTableName(void) const
{
	if(isLinkNode() && isDisconnected()) return disconnectedTargetName_;

	__SS__ << "Can't get disconnected target name of node unless it is a disconnected link node!" << std::endl;
	throw std::runtime_error(ss.str());
}

//==============================================================================
//getDisconnectedLinkID
const std::string& ConfigurationTree::getDisconnectedLinkID(void) const
{
	if(isLinkNode() && isDisconnected()) return disconnectedLinkID_;

	__SS__ << "Can't get disconnected target name of node unless it is a disconnected link node!" << std::endl;
	throw std::runtime_error(ss.str());
}

//==============================================================================
//getConfigurationVersion
const ConfigurationVersion& ConfigurationTree::getConfigurationVersion(void) const
{
	if(!configView_)
	{
		__SS__ << "Can't get configuration version of node with no config view pointer!" << std::endl;
		throw std::runtime_error(ss.str());
	}
	return configView_->getVersion();
}

//==============================================================================
//getConfigurationCreationTime
const time_t& ConfigurationTree::getConfigurationCreationTime(void) const
{
	if(!configView_)
	{
		__SS__ << "Can't get configuration creation time of node with no config view pointer!" << std::endl;
		throw std::runtime_error(ss.str());
	}
	return configView_->getCreationTime();
}


//==============================================================================
//getFixedChoices
//	returns vector of default + data choices
//	Used as choices for tree-view, for example.
std::vector<std::string> ConfigurationTree::getFixedChoices(void) const
{
	if(!configView_)
	{
		__SS__ << "Can't get fixed choices of node with no config view pointer!" << std::endl;
		throw std::runtime_error(ss.str());
	}

	if(getValueType() != ViewColumnInfo::TYPE_FIXED_CHOICE_DATA &&
			getValueType() != ViewColumnInfo::TYPE_BITMAP_DATA)
	{
		__SS__ << "Can't get fixed choices of node with value type of '" <<
				getValueType() << ".' Value type must be '" <<
				ViewColumnInfo::TYPE_BITMAP_DATA << "' or '" <<
				ViewColumnInfo::TYPE_FIXED_CHOICE_DATA << ".'" << std::endl;
		throw std::runtime_error(ss.str());
	}

	//return vector of default + data choices
	std::vector<std::string> retVec;
	retVec.push_back(configView_->getColumnInfo(col_).getDefaultValue());
	std::vector<std::string> choices = configView_->getColumnInfo(col_).getDataChoices();
	for(const auto &choice:choices)
		retVec.push_back(choice);

	return retVec;
}

//==============================================================================
//getValueAsString
//	NOTE: getValueAsString() method should be preferred if getting the Link UID
//		because when disconnected will return "X". getValue() would return the
//		column name of the link when disconnected.
//
//	returnLinkTableValue returns the value in the source table as though link was
//		not followed to destination table.
const std::string& ConfigurationTree::getValueAsString(bool returnLinkTableValue) const
{
	if(isLinkNode())
	{
		if(returnLinkTableValue)
			return linkColValue_;
		else if(isDisconnected())
			return ConfigurationTree::DISCONNECTED_VALUE;
		else if(row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID)	//this link is groupId node
			return (groupId_ == "")?configuration_->getConfigurationName():groupId_;
		else if(col_ == ConfigurationView::INVALID)						//this link is uid node
			return configView_->getDataView()[row_][configView_->getColUID()];
		else
		{
			__MOUT__ << std::endl;
			throw std::runtime_error("Impossible Link.");
		}
	}
	else if(row_ != ConfigurationView::INVALID && col_ != ConfigurationView::INVALID)	//this node is a value node
		return configView_->getDataView()[row_][col_];
	else if(row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID)	//this node is config node maybe with groupId
		return (groupId_ == "")?configuration_->getConfigurationName():groupId_;
	else if(row_ == ConfigurationView::INVALID)
	{
		__MOUT__ << std::endl;
		throw std::runtime_error("Malformed ConfigurationTree");
	}
	else if(col_ == ConfigurationView::INVALID)						//this node is uid node
		return configView_->getDataView()[row_][configView_->getColUID()];
	else
	{
		__MOUT__ << std::endl;
		throw std::runtime_error("Impossible.");
	}
}

//==============================================================================
//getUIDAsString
//	returns UID associated with current value node or UID-Link node
//
const std::string& ConfigurationTree::getUIDAsString(void) const
{
	if(isValueNode() || isUIDLinkNode())
		return configView_->getDataView()[row_][configView_->getColUID()];

	{
		__SS__ << "Can't get UID of node with type '" <<
				getNodeType() << ".' Node type must be '" <<
				ConfigurationTree::NODE_TYPE_VALUE << "' or '" <<
				ConfigurationTree::NODE_TYPE_UID_LINK << ".'" << std::endl;
		throw std::runtime_error(ss.str());
	}
}

//==============================================================================
//getValueDataType
//	e.g. used to determine if node is type NUMBER
const std::string& ConfigurationTree::getValueDataType(void) const
{
	if(isValueNode())
		return configView_->getColumnInfo(col_).getDataType();
	else	//must be std::string
		return ViewColumnInfo::DATATYPE_STRING;
}

//==============================================================================
//isDefaultValue
//	returns true if is a value node and value is the default for the type
bool ConfigurationTree::isDefaultValue(void) const
{
	if(!isValueNode()) return false;

	if(getValueDataType() == ViewColumnInfo::DATATYPE_STRING)
	{
		if(getValueType() == ViewColumnInfo::TYPE_ON_OFF ||
				getValueType() == ViewColumnInfo::TYPE_TRUE_FALSE ||
				getValueType() == ViewColumnInfo::TYPE_YES_NO)
			return getValueAsString() == ViewColumnInfo::DATATYPE_BOOL_DEFAULT; //default to OFF, NO, FALSE
		else if(getValueType() == ViewColumnInfo::TYPE_COMMENT)
			return getValueAsString() == ViewColumnInfo::DATATYPE_COMMENT_DEFAULT ||
					getValueAsString() == ""; //in case people delete default comment, allow blank also
		else
			return getValueAsString() == ViewColumnInfo::DATATYPE_STRING_DEFAULT;
	}
	else if(getValueDataType() == ViewColumnInfo::DATATYPE_NUMBER)
		return getValueAsString() == ViewColumnInfo::DATATYPE_NUMBER_DEFAULT;
	else if(getValueDataType() == ViewColumnInfo::DATATYPE_TIME)
		return getValueAsString() == ViewColumnInfo::DATATYPE_TIME_DEFAULT;
	else
		return false;
}

//==============================================================================
//getValueType
//	e.g. used to determine if node is type TYPE_DATA, TYPE_ON_OFF, etc.
const std::string& ConfigurationTree::getValueType(void) const
{
	if(isValueNode())
		return configView_->getColumnInfo(col_).getType();
	else if(isLinkNode() && isDisconnected())
		return ConfigurationTree::VALUE_TYPE_DISCONNECTED;
	else	//just call all non-value nodes data
		return ConfigurationTree::VALUE_TYPE_NODE;
}

//==============================================================================
//getColumnInfo
//	only sensible for value node
const ViewColumnInfo& ConfigurationTree::getColumnInfo(void) const
{
	if(isValueNode())
		return configView_->getColumnInfo(col_);
	else
	{
		__SS__ << "Can only get column info from a value node! " <<
				"The node type is " << getNodeType() << std::endl;
		__MOUT__ << "\n" << ss.str() << std::endl;
		throw std::runtime_error(ss.str());
	}

}

//==============================================================================
//getRow
const unsigned int& ConfigurationTree::getRow(void) const
{	return row_;	}

//==============================================================================
//getColumn
const unsigned int& ConfigurationTree::getColumn(void) const
{	return col_;	}

//==============================================================================
//getChildLinkIndex
const std::string& ConfigurationTree::getChildLinkIndex(void) const
{
	if(!isLinkNode())
	{
		__SS__ << "Can only get link ID from a link! " <<
				"The node type is " << getNodeType() << std::endl;
		__MOUT__ << "\n" << ss.str() << std::endl;
		throw std::runtime_error(ss.str());
	}
	return childLinkIndex_;
}

//==============================================================================
//getValueName
//	e.g. used to determine column name of value node
const std::string& ConfigurationTree::getValueName(void) const
{
	if(isValueNode())
		return configView_->getColumnInfo(col_).getName();
	else if(isLinkNode())
		return linkColName_;
	else
	{
		__SS__ << "Can only get value name of a value node!" << std::endl;
		__MOUT__ << "\n" << ss.str() << std::endl;
		throw std::runtime_error(ss.str());
	}

}

//==============================================================================
//recurse
//	Used by ConfigurationTree to handle / syntax of getNode
ConfigurationTree ConfigurationTree::recurse(const ConfigurationTree& tree, const std::string& childPath)
{
	//__MOUT__ << tree.row_ << " " << tree.col_ << std::endl;
	//__MOUT__ << "childPath=" << childPath << " " << childPath.length() << std::endl;
	if(childPath.length() <= 1) //only "/" or ""
		return tree;
	return tree.getNode(childPath);
}

////==============================================================================
////getRecordFieldValueAsString
////
////	This function throws error if not called on a record (uid node)
////
////Note: that ConfigurationTree maps both fields associated with a link
////	to the same node instance.
////The behavior is likely not expected as response for this function..
////	so for links return actual value for field name specified
////	i.e. if Table of link is requested give that; if linkID is requested give that.
//std::string ConfigurationTree::getRecordFieldValueAsString(std::string fieldName) const
//{
//	//enforce that starting point is a table node
//	if(!isUIDNode())
//	{
//		__SS__ << "Can only get getRecordFieldValueAsString from a uid node! " <<
//				"The node type is " << getNodeType() << std::endl;
//		__MOUT__ << "\n" << ss.str() << std::endl;
//		throw std::runtime_error(ss.str());
//	}
//
//	unsigned int c = configView_->findCol(fieldName);
//	return configView_->getDataView()[row_][c];
//}

//==============================================================================
//getNode
//	nodeString can be a multi-part path using / delimiter
//	use:
//			getNode(/uid/col) or getNode(uid)->getNode(col)
//
// if doNotThrowOnBrokenUIDLinks
//		then catch exceptions on UID links and call disconnected
ConfigurationTree ConfigurationTree::getNode(const std::string &nodeString,
		bool doNotThrowOnBrokenUIDLinks) const
{
	//__MOUT__ << "nodeString=" << nodeString << " " << nodeString.length() << std::endl;

	//get nodeName (in case of / syntax)
	if(nodeString.length() < 1) throw std::runtime_error("Invalid node name!");

	bool startingSlash = nodeString[0] == '/';

	std::string nodeName = nodeString.substr(startingSlash?1:0, nodeString.find('/',1)-(startingSlash?1:0));
	//__MOUT__ << "nodeName=" << nodeName << " " << nodeName.length() << std::endl;

	std::string childPath = nodeString.substr(nodeName.length() + (startingSlash?1:0));
	//__MOUT__ << "childPath=" << childPath << " " << childPath.length() << std::endl;

	//if this tree is beginning at a configuration.. then go to uid, and vice versa

	try
	{

		//__MOUT__ << row_ << " " << col_ <<  " " << groupId_ << " " << configView_ << std::endl;
		if(row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID)
		{
			if(!configView_)
			{
				__SS__ << "Missing configView pointer! Likely attempting to access a child node through a disconnected link node." << std::endl;
				__MOUT_ERR__ << "\n" << ss.str();
				throw std::runtime_error(ss.str());
			}

			//this node is config node, so return uid node considering groupid
			return recurse(ConfigurationTree(
					configMgr_,
					configuration_,
					"", //no new groupId string, not a link
					"", //link node name, not a link
					"", //link node value, not a link
					"",	//ignored disconnected target name, not a link
					"", //ignored disconnected link id, not a link
					"",
					//if this node is group config node, consider that when getting rows
					(groupId_ == "")?
							configView_->findRow(configView_->getColUID(),nodeName)
							: configView_->findRowInGroup(configView_->getColUID(),
									nodeName,groupId_,childLinkIndex_) ),
					childPath);
		}
		else if(row_ == ConfigurationView::INVALID)
		{
			__SS__ << "Malformed ConfigurationTree" << std::endl;
			__MOUT_ERR__ << "\n" << ss.str();
			throw std::runtime_error(ss.str());
		}
		else if(col_ == ConfigurationView::INVALID)
		{
			//this node is uid node, so return link, group link, disconnected, or value node

			//__MOUT__ << "nodeName=" << nodeName << " " << nodeName.length() << std::endl;

			//if the value is a unique link ..
			//return a uid node!
			//if the value is a group link
			//return a config node with group string
			//else.. return value node

			if(!configView_)
			{
				__SS__ << "Missing configView pointer! Likely attempting to access a child node through a disconnected link node." << std::endl;
				__MOUT_ERR__ << "\n" << ss.str();
				throw std::runtime_error(ss.str());
			}

			unsigned int c = configView_->findCol(nodeName);
			std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
			bool isGroupLink, isLink;
			if((isLink = configView_->getChildLink(c, &isGroupLink, &linkPair)) &&
					!isGroupLink)
			{
				//__MOUT__ << "nodeName=" << nodeName << " " << nodeName.length() << std::endl;
				//is a unique link, return uid node in new configuration
				//	need new configuration pointer
				//	and row of linkUID in new configuration

				const ConfigurationBase* childConfig;
				try
				{
					childConfig = configMgr_->getConfigurationByName(configView_->getDataView()[row_][linkPair.first]);
					childConfig->getView(); //get view as a test for an active view

					if(doNotThrowOnBrokenUIDLinks) //try a test of getting row
					{
						childConfig->getView().findRow(childConfig->getView().getColUID(),
								configView_->getDataView()[row_][linkPair.second]);
					}
				}
				catch(...)
				{
					//					__MOUT_WARN__ << "Found disconnected node! (" << nodeName <<
					//							":" << configView_->getDataView()[row_][linkPair.first] << ")" <<
					//							" at entry with UID " <<
					//							configView_->getDataView()[row_][configView_->getColUID()] <<
					//							std::endl;
					//do not recurse further
					return ConfigurationTree(
							configMgr_,
							0,
							"",
							nodeName,
							configView_->getDataView()[row_][c], //this the link node field associated value (matches targeted column)
							configView_->getDataView()[row_][linkPair.first], //give disconnected target name
							configView_->getDataView()[row_][linkPair.second], //give disconnected link ID
							configView_->getColumnInfo(c).getChildLinkIndex());
				}

				return recurse(
						ConfigurationTree(   //this is a link node
								configMgr_,
								childConfig,
								"", //no new groupId string
								nodeName, //this is a link node
								configView_->getDataView()[row_][c], //this the link node field associated value (matches targeted column)
								"", //ignore since is connected
								"", //ignore since is connected
								configView_->getColumnInfo(c).getChildLinkIndex(),
								childConfig->getView().findRow(childConfig->getView().getColUID(),
										configView_->getDataView()[row_][linkPair.second])
						),
						childPath);
			}
			else if(isLink)
			{
				//__MOUT__ << "nodeName=" << nodeName << " " << nodeName.length() << std::endl;
				//is a group link, return new configuration with group string
				//	need new configuration pointer
				//	and group string

				const ConfigurationBase* childConfig;
				try
				{
					childConfig = configMgr_->getConfigurationByName(
							configView_->getDataView()[row_][linkPair.first]);
					childConfig->getView(); //get view as a test for an active view
				}
				catch(...)
				{
					if(configView_->getDataView()[row_][linkPair.first] !=
							ViewColumnInfo::DATATYPE_LINK_DEFAULT)
						__MOUT_WARN__ << "Found disconnected node! Failed link target from nodeName=" <<
							nodeName << " to table:id=" <<
							configView_->getDataView()[row_][linkPair.first] << ":" <<
							configView_->getDataView()[row_][linkPair.second] <<
							std::endl;

					//do not recurse further
					return ConfigurationTree(configMgr_,0,
							configView_->getDataView()[row_][linkPair.second],
							nodeName,
							configView_->getDataView()[row_][c], //this the link node field associated value (matches targeted column)
							configView_->getDataView()[row_][linkPair.first], //give disconnected target name
							configView_->getDataView()[row_][linkPair.second], //give disconnected target name
							configView_->getColumnInfo(c).getChildLinkIndex()
					);
				}

				return recurse(
						ConfigurationTree( 	//this is a link node
								configMgr_,
								childConfig,
								configView_->getDataView()[row_][linkPair.second],	//groupId string
								nodeName,  //this is a link node
								configView_->getDataView()[row_][c], //this the link node field associated value (matches targeted column)
								"", //ignore since is connected
								"", //ignore since is connected
								configView_->getColumnInfo(c).getChildLinkIndex()
						),
						childPath);
			}
			else
			{
				//__MOUT__ << "nodeName=" << nodeName << " " << nodeName.length() << std::endl;
				//return value node
				return ConfigurationTree(
						configMgr_,
						configuration_,"","","","",""/*disconnectedLinkID*/,"",
						row_,c);
			}
		}

	}
	catch(std::runtime_error &e)
	{
		__SS__ << "\n\nError occurred descending from node '" << getValue() <<
				"' in table '" << getConfigurationName() <<
				"' looking for child '" << nodeName << "'\n\n" << std::endl;
		ss << "--- Additional error detail: \n\n" << e.what() << std::endl;
		throw std::runtime_error(ss.str());
	}
	catch(...)
	{
		__SS__ << "\n\nError occurred descending from node '" << getValue() <<
				"' in table '" << getConfigurationName() <<
				"' looking for child '" << nodeName << "'\n\n" << std::endl;
		throw std::runtime_error(ss.str());
	}

	//this node is value node, so has no node to choose from
	__SS__ << "\n\nError occurred descending from node '" << getValue() <<
			"' in table '" << getConfigurationName() <<
			"' looking for child '" << nodeName << "'\n\n" <<
			"Invalid depth! getNode() called from a value point in the Configuration Tree." << std::endl;
	throw std::runtime_error(ss.str());	// this node is value node, cant go any deeper!
}


//==============================================================================
ConfigurationTree ConfigurationTree::getBackNode(std::string nodeName, unsigned int backSteps) const
{
	for(unsigned int i=0; i<backSteps; i++)
		nodeName = nodeName.substr(0, nodeName.find_last_of('/'));

	return getNode(nodeName);
}

//==============================================================================
//isValueNode
//	if true, then this is a leaf node, i.e. there can be no children, only a value
bool ConfigurationTree::isValueNode(void) const
{
	return (row_ != ConfigurationView::INVALID && col_ != ConfigurationView::INVALID);
}

//==============================================================================
//isDisconnected
//	if true, then this is a disconnected node, i.e. there is a configuration link missing
//	(this is possible when the configuration is loaded in stages and the complete tree
//		may not be available, yet)
bool ConfigurationTree::isDisconnected(void) const
{
	if(!isLinkNode())
	{
		__SS__ << "This is not a Link node! Only a Link node can be disconnected." << std::endl;
		__MOUT__ << "\n" << ss.str();
		throw std::runtime_error(ss.str());
	}

	return !configuration_ || !configView_;
}

//==============================================================================
//isLinkNode
//	if true, then this is a link node
bool ConfigurationTree::isLinkNode(void) const
{
	return linkColName_ != "";
}
//
//==============================================================================
//getNodeType
//	return node type as string
const std::string ConfigurationTree::NODE_TYPE_GROUP_TABLE 	= "GroupConfigurationNode";
const std::string ConfigurationTree::NODE_TYPE_TABLE      	= "ConfigurationNode";
const std::string ConfigurationTree::NODE_TYPE_GROUP_LINK	= "GroupLinkNode";
const std::string ConfigurationTree::NODE_TYPE_UID_LINK		= "UIDLinkNode";
const std::string ConfigurationTree::NODE_TYPE_VALUE		= "ValueNode";
const std::string ConfigurationTree::NODE_TYPE_UID			= "UIDNode";
std::string ConfigurationTree::getNodeType(void) const
{
	if(isConfigurationNode() && groupId_ != "") return ConfigurationTree::NODE_TYPE_GROUP_TABLE;
	if(isConfigurationNode())                   return ConfigurationTree::NODE_TYPE_TABLE;
	if(isGroupLinkNode())                       return ConfigurationTree::NODE_TYPE_GROUP_LINK;
	if(isLinkNode())                            return ConfigurationTree::NODE_TYPE_UID_LINK;
	if(isValueNode())                           return ConfigurationTree::NODE_TYPE_VALUE;
	return ConfigurationTree::NODE_TYPE_UID;
}

//==============================================================================
//isGroupLinkNode
//	if true, then this is a group link node
bool ConfigurationTree::isGroupLinkNode(void) const
{
	return (isLinkNode() && groupId_ != "");
}

//==============================================================================
//isUIDLinkNode
//	if true, then this is a uid link node
bool ConfigurationTree::isUIDLinkNode(void) const
{
	return (isLinkNode() && groupId_ == "");
}

//==============================================================================
//isUIDNode
//	if true, then this is a uid node
bool ConfigurationTree::isUIDNode(void) const
{
	return (row_ != ConfigurationView::INVALID && col_ == ConfigurationView::INVALID);
}


//==============================================================================
//getCommonFields
//	wrapper for ...recursiveGetCommonFields
//
//	returns common fields in order encountered
//		including through UID links depending on depth specified
//
//	Field := {Table, UID, Column Name, Relative Path, ViewColumnInfo}
//
//	if fieldAcceptList or fieldRejectList are not empty,
//		then reject any that are not in field accept filter list
//		and reject any that are in field reject filter list
//
//	will only go to specified depth looking for fields
//		(careful to prevent infinite loops in tree navigation)
//
std::vector<ConfigurationTree::RecordField> ConfigurationTree::getCommonFields(
		const std::vector<std::string /*uid*/> &recordList,
		const std::vector<std::string /*relative-path*/> &fieldAcceptList,
		const std::vector<std::string /*relative-path*/> &fieldRejectList,
		unsigned int depth) const
{
	//enforce that starting point is a table node
	if(!isConfigurationNode())
	{
		__SS__ << "Can only get getCommonFields from a table node! " <<
				"The node type is " << getNodeType() << std::endl;
		__MOUT__ << "\n" << ss.str() << std::endl;
		throw std::runtime_error(ss.str());
	}

	std::vector<ConfigurationTree::RecordField> fieldCandidateList;
	std::vector<int> fieldCount; //-1 := guaranteed, else count must match num of records

	--depth; //decrement for recursion

	//for each record in <record list>
	//	loop through all record's children
	//		if isValueNode (value nodes are possible field candidates!)
	//			if first uid record
	//				add field to <field candidates list> if in <field filter list>
	//				mark <field count> as guaranteed -1 (all these fields must be common for UIDs in same table)
	//			else not first uid record, do not need to check, must be same as first record!
	//		else if depth > 0 and UID-Link Node
	//			recursively (call recursiveGetCommonFields())
	//				=====================
	//				Start recursiveGetCommonFields()
	//					--depth;
	//					loop through all children
	//						if isValueNode (value nodes are possible field candidates!)
	//							if first uid record
	//								add field to <field candidates list> if in <field filter list>
	//								initial mark <field count> as 1
	//							else
	//								if field is in <field candidates list>,
	//									increment <field count> for field candidate
	//								else if field is not in list, ignore field
	//						else if depth > 0 and is UID-Link
	//							if Link Table/UID pair is not found in <field candidates list> (avoid endless loops through tree)
	//								recursiveGetCommonFields()
	//				=====================
	//
	//
	//loop through all field candidates
	//	remove those with <field count> != num of records
	//
	//
	//return result

	bool found; //used in loops
	auto tableName = getConfigurationName(); //all records will share this table name

	for(unsigned int i=0;i<recordList.size();++i)
	{
		//__MOUT__ << "Checking " << recordList[i] << std::endl;

		auto recordChildren = getNode(recordList[i]).getChildren();
		for(const auto &fieldNode : recordChildren)
		{
			//__MOUT__ << "All... " << fieldNode.second.getNodeType() <<
			//		" -- " << fieldNode.first << std::endl;
			if(fieldNode.second.isValueNode())
			{
				//skip author and record insertion time
				if(fieldNode.second.getColumnInfo().getType() ==
						ViewColumnInfo::TYPE_AUTHOR ||
						fieldNode.second.getColumnInfo().getType() ==
								ViewColumnInfo::TYPE_TIMESTAMP)
					continue;

				//__MOUT__ << "isValueNode " << fieldNode.first << std::endl;
				if(!i) //first uid record
				{
					//check field accept filter list
					found = fieldAcceptList.size()?false:true; //accept if no filter list
					for(const auto &fieldFilter : fieldAcceptList)
						if(fieldFilter[0] == '*') //leading wildcard
						{
							if(fieldNode.first ==
									fieldFilter.substr(1))
							{
								found = true;
								break;
							}
						}
						else if(fieldFilter.size() &&
								fieldFilter[fieldFilter.size()-1] == '*') //trailing wildcard
						{
							if(fieldNode.first.substr(0,fieldFilter.size()-1) ==
									fieldFilter.substr(0,fieldFilter.size()-1))
							{
								found = true;
								break;
							}
						}
						else //no leading wildcard
						{
							if(fieldNode.first == fieldFilter)
							{
								found = true;
								break;
							}
						}


					if(found)
					{
						//check field reject filter list

						found = true; //accept if no filter list
						for(const auto &fieldFilter : fieldRejectList)
							if(fieldFilter[0] == '*') //leading wildcard
							{
								if(fieldNode.first ==
										fieldFilter.substr(1))
								{
									found = false; //reject if match
									break;
								}
							}
							else if(fieldFilter.size() &&
									fieldFilter[fieldFilter.size()-1] == '*') //trailing wildcard
							{
								if(fieldNode.first.substr(0,fieldFilter.size()-1) ==
										fieldFilter.substr(0,fieldFilter.size()-1))
								{
									found = false; //reject if match
									break;
								}
							}
							else //no leading wildcard
							{
								if(fieldNode.first == fieldFilter)
								{
									found = false; //reject if match
									break;
								}
							}

					}

					//if found, guaranteed field (all these fields must be common for UIDs in same table)
					if(found)
					{
						fieldCandidateList.push_back(
								ConfigurationTree::RecordField(
										tableName,
										recordList[i],
										fieldNode.first,
										"", //relative path, not including columnName_
										&fieldNode.second.getColumnInfo()
								));
						fieldCount.push_back(-1); //mark guaranteed field
					}
				}
				//else // not first uid record, do not need to check, must be same as first record!

			}
			else if(depth > 0 &&
					fieldNode.second.isUIDLinkNode() &&
					!fieldNode.second.isDisconnected())
			{
				//__MOUT__ << "isUIDLinkNode " << fieldNode.first << std::endl;
				fieldNode.second.recursiveGetCommonFields(
						fieldCandidateList,
						fieldCount,
						fieldAcceptList,
						fieldRejectList,
						depth,
						fieldNode.first + "/", //relativePathBase
						!i	//launch inFirstRecord (or not) depth search
						);
			}
		}

	}

	//__MOUT__ << "======================= check for count = " <<
	//		(int)recordList.size() << std::endl;

	//loop through all field candidates
	//	remove those with <field count> != num of records
	for(unsigned int i=0;i<fieldCandidateList.size();++i)
	{
		//__MOUT__ << "Checking " << fieldCandidateList[i].relativePath_ <<
		//		fieldCandidateList[i].columnName_ << " = " <<
		//		fieldCount[i] << std::endl;
		if(fieldCount[i] != -1 &&
				fieldCount[i] != (int)recordList.size())
		{
			//__MOUT__ << "Erasing " << fieldCandidateList[i].relativePath_ <<
			//		fieldCandidateList[i].columnName_ << std::endl;

			fieldCount.erase(fieldCount.begin() + i);
			fieldCandidateList.erase(fieldCandidateList.begin() + i);
			--i; //rewind to look at next after deleted
		}
	}

	for(unsigned int i=0;i<fieldCandidateList.size();++i)
		__MOUT__ << "Final " << fieldCandidateList[i].relativePath_ <<
				fieldCandidateList[i].columnName_ << std::endl;

	return fieldCandidateList;
}

//==============================================================================
//getUniqueValuesForField
//
//	returns sorted unique values for the specified records and field
//
std::set<std::string /*unique-value*/> ConfigurationTree::getUniqueValuesForField(
		const std::vector<std::string /*relative-path*/> &recordList,
		const std::string &fieldName) const
{
	//enforce that starting point is a table node
	if(!isConfigurationNode())
	{
		__SS__ << "Can only get getCommonFields from a table node! " <<
				"The node type is " << getNodeType() << std::endl;
		__MOUT__ << "\n" << ss.str() << std::endl;
		throw std::runtime_error(ss.str());
	}

	std::set<std::string /*unique-value*/> uniqueValues;

	//for each record in <record list>
	//	emplace value at field into set
	//
	//return result

	for(unsigned int i=0;i<recordList.size();++i)
	{
		__MOUT__ << "Checking " << recordList[i] << std::endl;

		//Note: that ConfigurationTree maps both fields associated with a link
		//	to the same node instance.
		//The behavior is likely not expected as response for this function..
		//	so for links return actual value for field name specified
		//	i.e. if Table of link is requested give that; if linkID is requested give that.
		//use TRUE in getValueAsString for proper behavior
		uniqueValues.emplace(getNode(recordList[i]).getNode(fieldName).getValueAsString(true));
	}

	return uniqueValues;
}

//==============================================================================
//recursiveGetCommonFields
//	wrapper is ...getCommonFields
void ConfigurationTree::recursiveGetCommonFields(
		std::vector<ConfigurationTree::RecordField> &fieldCandidateList,
		std::vector<int> &fieldCount,
		const std::vector<std::string /*relative-path*/> &fieldAcceptList,
		const std::vector<std::string /*relative-path*/> &fieldRejectList,
		unsigned int depth,
		const std::string &relativePathBase,
		bool inFirstRecord
	) const
{
	--depth;
	//__MOUT__ << "relativePathBase " << relativePathBase << std::endl;

	//	=====================
	//	Start recursiveGetCommonFields()
	//		--depth;
	//		loop through all children
	//			if isValueNode (value nodes are possible field candidates!)
	//				if first uid record
	//					add field to <field candidates list> if in <field filter list>
	//					initial mark <field count> as 1
	//				else
	//					if field is in list,
	//						increment count for field candidate
	//						//?increment fields in list count for record
	//					else if field is not in list, discard field
	//			else if depth > 0 and is UID-Link
	//				if Link Table/UID pair is not found in <field candidates list> (avoid endless loops through tree)
	//					recursiveGetCommonFields()
	//	=====================


	bool found; //used in loops
	auto tableName = getConfigurationName(); //all fields will share this table name
	auto uid = getUIDAsString();	//all fields will share this uid
	unsigned int j;

	auto recordChildren = getChildren();
	for(const auto &fieldNode : recordChildren)
	{
		if(fieldNode.second.isValueNode())
		{
			//skip author and record insertion time
			if(fieldNode.second.getColumnInfo().getType() ==
					ViewColumnInfo::TYPE_AUTHOR ||
					fieldNode.second.getColumnInfo().getType() ==
							ViewColumnInfo::TYPE_TIMESTAMP)
				continue;

			//__MOUT__ << "isValueNode " << fieldNode.first << std::endl;
			if(inFirstRecord) //first uid record
			{
				//check field accept filter list
				found = fieldAcceptList.size()?false:true; //accept if no filter list
				for(const auto &fieldFilter : fieldAcceptList)
					if(fieldFilter[0] == '*') //leading wildcard
					{
						if(fieldNode.first ==
								fieldFilter.substr(1))
						{
							found = true;
							break;
						}
					}
					else if(fieldFilter.size() &&
							fieldFilter[fieldFilter.size()-1] == '*') //trailing wildcard
					{
						if((relativePathBase + fieldNode.first).substr(
								0,fieldFilter.size()-1) ==
								fieldFilter.substr(0,fieldFilter.size()-1))
						{
							found = true;
							break;
						}
					}
					else //no leading wildcard
					{
						if((relativePathBase + fieldNode.first) ==
							fieldFilter)
						{
							found = true;
							break;
						}
					}

				if(found)
				{
					//check field reject filter list

					found = true; //accept if no filter list
					for(const auto &fieldFilter : fieldRejectList)
						if(fieldFilter[0] == '*') //leading wildcard
						{
							if(fieldNode.first ==
									fieldFilter.substr(1))
							{
								found = false; //reject if match
								break;
							}
						}
						else if(fieldFilter.size() &&
								fieldFilter[fieldFilter.size()-1] == '*') //trailing wildcard
						{
							if((relativePathBase + fieldNode.first).substr(
									0,fieldFilter.size()-1) ==
									fieldFilter.substr(0,fieldFilter.size()-1))
							{
								found = false; //reject if match
								break;
							}
						}
						else //no leading wildcard
						{
							if((relativePathBase + fieldNode.first) ==
									fieldFilter)
							{
								found = false; //reject if match
								break;
							}
						}
				}

				//if found, new field (since this is first record)
				if(found)
				{
					//__MOUT__ << "FOUND field " <<
					//		(relativePathBase + fieldNode.first) << std::endl;
					fieldCandidateList.push_back(
							ConfigurationTree::RecordField(
									tableName,
									uid,
									fieldNode.first,
									relativePathBase, //relative path, not including columnName_
									&fieldNode.second.getColumnInfo()
							));
					fieldCount.push_back(1); //init count to 1
				}
			}
			else //not first record
			{
				//if field is in <field candidates list>, increment <field count>
				//	else ignore
				for(j=0;j<fieldCandidateList.size();++j)
				{
					if((relativePathBase + fieldNode.first) ==
							(fieldCandidateList[j].relativePath_ +
									fieldCandidateList[j].columnName_))
					{
						//__MOUT__ << "incrementing " << j <<
						//		" " << fieldCandidateList[j].relativePath_ << std::endl;
						//found, so increment <field count>
						++fieldCount[j];
						break;
					}
				}
			}
		}
		else if(depth > 0 &&
				fieldNode.second.isUIDLinkNode() &&
				!fieldNode.second.isDisconnected())
		{
			//__MOUT__ << "isUIDLinkNode " << (relativePathBase + fieldNode.first) << std::endl;
			fieldNode.second.recursiveGetCommonFields(
					fieldCandidateList,
					fieldCount,
					fieldAcceptList,
					fieldRejectList,
					depth,
					(relativePathBase + fieldNode.first) + "/", //relativePathBase
					inFirstRecord	//continue inFirstRecord (or not) depth search
			);
		}
	}
}

//==============================================================================
//getChildren
//	returns them in order encountered in the table
//	if filterMap criteria, then rejects any that do not meet all criteria
std::vector<std::pair<std::string,ConfigurationTree> > ConfigurationTree::getChildren(
		std::map<std::string /*relative-path*/, std::string /*value*/> filterMap) const
{
	std::vector<std::pair<std::string,ConfigurationTree> > retMap;

	//__MOUT__ << "Children of node: " << getValueAsString() << std::endl;

	bool filtering = filterMap.size();
	bool skip;
	std::string fieldValue;

	std::vector<std::string> childrenNames = getChildrenNames();
	for(auto &childName : childrenNames)
	{
		//__MOUT__ << "\tChild: " << childName << std::endl;

		if(filtering)
		{
			//if all criteria are not met, then skip
			skip = false;

			//for each filter, check value
			for(const auto &filterPair:filterMap)
			{
				std::string filterPath = childName + "/" + filterPair.first;
				try
				{


					//extract field value list
					std::istringstream f(filterPair.second);

					skip = true;
					//for each field check if any match
					while (getline(f, fieldValue, ','))
					{
						//Note: that ConfigurationTree maps both fields associated with a link
						//	to the same node instance.
						//The behavior is likely not expected as response for this function..
						//	so for links return actual value for field name specified
						//	i.e. if Table of link is requested give that; if linkID is requested give that.
						//use TRUE in getValueAsString for proper behavior
						__MOUT__ << "\t\tCheck: " << filterPair.first <<
								" == " << fieldValue << " ??? " <<
								this->getNode(filterPath).getValueAsString(true) <<
								std::endl;
						if(this->getNode(filterPath).getValueAsString(true) ==
								ConfigurationView::decodeURIComponent(fieldValue))
						{
							//found a match for the field/value pair
							skip = false;
							break;
						}
					}
				}
				catch(...)
				{
					__SS__ << "Failed to access filter path '" <<
							filterPath << "' - aborting." << std::endl;
					__MOUT_ERR__ << "\n" << ss.str();
					throw std::runtime_error(ss.str());
				}

				if(skip) break; //no match for this field, so stop checking and skip this record
			}

			if(skip) continue; //skip this record

			__MOUT__ << "\tChild accepted: " << childName << std::endl;
		}

		retMap.push_back(std::pair<std::string,ConfigurationTree>(childName,
				this->getNode(childName, true)));
	}

	//__MOUT__ << "Done w/Children of node: " << getValueAsString() << std::endl;
	return retMap;
}

//==============================================================================
//getChildren
//	returns them in order encountered in the table
std::map<std::string,ConfigurationTree> ConfigurationTree::getChildrenMap(void) const
{
	std::map<std::string,ConfigurationTree> retMap;

	//__MOUT__ << "Children of node: " << getValueAsString() << std::endl;

	std::vector<std::string> childrenNames = getChildrenNames();
	for(auto& childName : childrenNames)
	{
		//__MOUT__ << "\tChild: " << childName << std::endl;
		retMap.insert(std::pair<std::string,ConfigurationTree>(childName, this->getNode(childName)));
	}

	//__MOUT__ << "Done w/Children of node: " << getValueAsString() << std::endl;
	return retMap;
}

//==============================================================================
bool ConfigurationTree::isConfigurationNode(void) const
{
	return (row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID);
}

//==============================================================================
//getChildrenNames
//	returns them in order encountered in the table
std::vector<std::string> ConfigurationTree::getChildrenNames(void) const
{
	std::vector<std::string> retSet;

	if(!configView_)
	{
		__SS__ << "Can not get children names of '" <<
				getValueAsString() <<
				"' with null configuration view pointer!" << std::endl;
		if(isLinkNode() && isDisconnected())
			ss << " This node is a disconnected link to " <<
				getDisconnectedTableName() << std::endl;
		__MOUT_ERR__ << "\n" << ss.str();
		throw std::runtime_error(ss.str());
	}

	if(row_ == ConfigurationView::INVALID && col_ == ConfigurationView::INVALID)
	{
		//this node is config node
		//so return all uid node strings that match groupId

		for(unsigned int r = 0; r<configView_->getNumberOfRows(); ++r)
			if(groupId_ == "" ||
					configView_->isEntryInGroup(r,childLinkIndex_,groupId_))
//					groupId_ == configView_->getDataView()[r][configView_->getColLinkGroupID(
//							childLinkIndex_)])
				retSet.push_back(configView_->getDataView()[r][configView_->getColUID()]);
	}
	else if(row_ == ConfigurationView::INVALID)
		throw std::runtime_error("Malformed ConfigurationTree");
	else if(col_ == ConfigurationView::INVALID)
	{
		//this node is uid node
		//so return all link and value nodes

		for(unsigned int c = 0; c<configView_->getNumberOfColumns(); ++c)
			if(c == configView_->getColUID() ||  //skip UID and linkID columns (only show link column, to avoid duplicates)
					configView_->getColumnInfo(c).isChildLinkGroupID() ||
					configView_->getColumnInfo(c).isChildLinkUID())
				continue;
			else
				retSet.push_back(configView_->getColumnInfo(c).getName());
	}
	else //this node is value node, so has no node to choose from
	{
		__MOUT__ << "\n\nError occurred looking for children of nodeName=" << getValueName() << "\n\n" << std::endl;
		throw std::runtime_error("Invalid depth! getChildrenValues() called from a value point in the Configuration Tree.");	// this node is value node, cant go any deeper!
	}

	return retSet;
}


//==============================================================================
//getValueAsTreeNode
//	returns tree node for value of this node, treating the value
//		as a string for the absolute path string from root of tree
ConfigurationTree ConfigurationTree::getValueAsTreeNode(void) const
{
	//check if first character is a /, .. if so try to get value in tree
	//	if exception, just take value
	//note: this call will throw an error, in effect, if not a "value" node
	if(!configView_)
	{
		__SS__ << "Invalid node for get value." << std::endl;
		__MOUT__ << ss.str();
		throw std::runtime_error(ss.str());
	}

	std::string valueString = configView_->getValueAsString(row_,col_,true /* convertEnvironmentVariables */);
	//__MOUT__ << valueString << std::endl;
	if(valueString.size() && valueString[0] == '/')
	{
		__MOUT__ << "Starts with '/' - check if valid tree path: " << valueString << std::endl;
		try
		{
			ConfigurationTree retNode = configMgr_->getNode(valueString);
			__MOUT__ << "Success!" << std::endl;
			return retNode;
		}
		catch(...)
		{
			__SS__ << "Invalid tree path." << std::endl;
			__MOUT__ << ss.str();
			throw std::runtime_error(ss.str());
		}
	}


	{
		__SS__ << "Invalid value string '" << valueString <<
				"' - must start with a '/' character." << std::endl;
		throw std::runtime_error(ss.str());
	}
}











