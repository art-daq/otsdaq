#include "otsdaq/ConfigurationInterface/ConfigurationTree.h"

#include <typeinfo>

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/TableCore/TableBase.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "ConfigurationTree"

const std::string ConfigurationTree::DISCONNECTED_VALUE      = "X";
const std::string ConfigurationTree::VALUE_TYPE_DISCONNECTED = "Disconnected";
const std::string ConfigurationTree::VALUE_TYPE_NODE         = "Node";
const std::string ConfigurationTree::ROOT_NAME               = "/";

//==============================================================================
ConfigurationTree::ConfigurationTree()
    : configMgr_(0)
    , table_(0)
    , groupId_("")
    , linkParentTable_(0)
    , linkColName_("")
    , linkColValue_("")
    , linkBackRow_(0)
    , linkBackCol_(0)
    , disconnectedTargetName_("")
    , disconnectedLinkID_("")
    , childLinkIndex_("")
    , row_(0)
    , col_(0)
    , tableView_(0)
{
	//__COUT__ << __E__;
	//__COUT__ << "EMPTY CONSTRUCTOR ConfigManager: " << configMgr_ << " configuration: "
	//<< table_  << __E__;
}  // end empty constructor

//==============================================================================
ConfigurationTree::ConfigurationTree(const ConfigurationManager* const& configMgr, const TableBase* const& table)
    : ConfigurationTree(configMgr,
                        table,
                        "" /*groupId_*/,
                        0 /*linkParentTable_*/,
                        "" /*linkColName_*/,
                        "" /*linkColValue_*/,
                        TableView::INVALID /*linkBackRow_*/,
                        TableView::INVALID /*linkBackCol_*/,
                        "" /*disconnectedTargetName_*/,
                        "" /*disconnectedLinkID_*/,
                        "" /*childLinkIndex_*/,
                        TableView::INVALID /*row_*/,
                        TableView::INVALID /*col_*/)
{
	//__COUT__ << __E__;
	//__COUT__ << "SHORT CONTRUCTOR ConfigManager: " << configMgr_ << " configuration: "
	//<< table_  << __E__;
}  // end short constructor

//==============================================================================
ConfigurationTree::ConfigurationTree(const ConfigurationManager* const& configMgr,
                                     const TableBase* const&            table,
                                     const std::string&                 groupId,
                                     const TableBase* const&            linkParentConfig,
                                     const std::string&                 linkColName,
                                     const std::string&                 linkColValue,
                                     const unsigned int                 linkBackRow,
                                     const unsigned int                 linkBackCol,
                                     const std::string&                 disconnectedTargetName,
                                     const std::string&                 disconnectedLinkID,
                                     const std::string&                 childLinkIndex,
                                     const unsigned int                 row,
                                     const unsigned int                 col)
    : configMgr_(configMgr)
    , table_(table)
    , groupId_(groupId)
    , linkParentTable_(linkParentConfig)
    , linkColName_(linkColName)
    , linkColValue_(linkColValue)
    , linkBackRow_(linkBackRow)
    , linkBackCol_(linkBackCol)
    , disconnectedTargetName_(disconnectedTargetName)
    , disconnectedLinkID_(disconnectedLinkID)
    , childLinkIndex_(childLinkIndex)
    , row_(row)
    , col_(col)
    , tableView_(0)
{
	if(!configMgr_) 
	{
		__SS__ << "Invalid empty pointer given to tree!\n"
		       << "\n\tconfigMgr_=" << configMgr_ << "\n\tconfiguration_=" << table_ << "\n\tconfigView_=" << tableView_ << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}

	if(table_)
		tableView_ = &(table_->getView());

	// verify UID column exists
	if(tableView_ && tableView_->getColumnInfo(tableView_->getColUID()).getType() != TableViewColumnInfo::TYPE_UID)
	{
		__SS__ << "Missing UID column (must column of type  " << TableViewColumnInfo::TYPE_UID << ") in config view : " << tableView_->getTableName() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
}  // end full constructor

//==============================================================================
// destructor
ConfigurationTree::~ConfigurationTree(void)
{
	//__COUT__ << __E__;
}  // end destructor

//==============================================================================
// print
//	print out tree from this node for desired depth
//	depth of 0 means print out only this node's value
//	depth of 1 means include this node's children's values, etc..
//	depth of -1 means print full tree
void ConfigurationTree::print(const unsigned int& depth, std::ostream& out) const { recursivePrint(*this, depth, out, "\t"); }  // end print()

//==============================================================================
void ConfigurationTree::recursivePrint(const ConfigurationTree& t, unsigned int depth, std::ostream& out, std::string space)
{
	if(t.isValueNode())
		out << space << t.getValueName() << " :\t" << t.getValueAsString() << __E__;
	else
	{
		if(t.isLinkNode())
		{
			out << space << t.getValueName();
			if(t.isDisconnected())
			{
				out << " :\t" << t.getValueAsString() << __E__;
				return;
			}
			out << " (" << (t.isGroupLinkNode() ? "Group" : "U") << "ID=" << t.getValueAsString() << ") : " << __E__;
		}
		else
			out << space << t.getValueAsString() << " : " << __E__;

		// if depth>=1 print all children
		//	child.print(depth-1)
		if(depth >= 1)
		{
			auto C = t.getChildren();
			if(!C.empty())
				out << space << "{" << __E__;
			for(auto& c : C)
				recursivePrint(c.second, depth - 1, out, space + "   ");
			if(!C.empty())
				out << space << "}" << __E__;
		}
	}
}  // end recursivePrint()

//==============================================================================
std::string ConfigurationTree::handleValidateValueForColumn(const TableView* configView, std::string value, unsigned int col, ots::identity<std::string>) const
{
	if(!configView)
	{
		__SS__ << "Null configView" << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
	__COUT__ << "handleValidateValueForColumn<string>" << __E__;
	return configView->validateValueForColumn(value, col);
}  // end std::string handleValidateValueForColumn()

//==============================================================================
// getValue (only std::string value)
// special version of getValue for string type
//	Note: necessary because types of std::basic_string<char> cause compiler problems if no
// string specific function
void ConfigurationTree::getValue(std::string& value) const
{
	//__COUT__ << row_ << " " << col_ << " p: " << tableView_<< __E__;

	if(row_ != TableView::INVALID && col_ != TableView::INVALID)  // this node is a value node
	{
		// attempt to interpret the value as a tree node path itself
		try
		{
			ConfigurationTree valueAsTreeNode = getValueAsTreeNode();
			// valueAsTreeNode.getValue<T>(value);
			__COUT__ << "Success following path to tree node!" << __E__;
			// value has been interpreted as a tree node value
			// now verify result under the rules of this column
			// if(typeid(std::string) == typeid(value))

			// Note: want to interpret table value as though it is in column of different
			// table 	this allows a number to be read as a string, for example, without
			// exceptions
			value = tableView_->validateValueForColumn(valueAsTreeNode.getValueAsString(), col_);

			__COUT__ << "Successful value!" << __E__;

			// else
			//	value = tableView_->validateValueForColumn<T>(
			//		valueAsTreeNode.getValueAsString(),col_);

			return;
		}
		catch(...)  // tree node path interpretation failed
		{
			//__COUT__ << "Invalid path, just returning normal value." << __E__;
		}

		// else normal return
		tableView_->getValue(value, row_, col_);
	}
	else if(row_ == TableView::INVALID && col_ == TableView::INVALID)  // this node is table node maybe with groupId
	{
		if(isLinkNode() && isDisconnected())
			value = (groupId_ == "") ? getValueName() : groupId_;  // a disconnected link
			                                                       // still knows its table
			                                                       // name or groupId
		else
			value = (groupId_ == "") ? table_->getTableName() : groupId_;
	}
	else if(row_ == TableView::INVALID)
	{
		__SS__ << "Malformed ConfigurationTree" << __E__;
		__SS_THROW__;
	}
	else if(col_ == TableView::INVALID)  // this node is uid node
		tableView_->getValue(value, row_, tableView_->getColUID());
	else
	{
		__SS__ << "Impossible." << __E__;
		__SS_THROW__;
	}
}  // end getValue()

//==============================================================================
// getValue
//	Only std::string value will work.
//		If this is a value node, and not type string, configView->getValue should
//		throw exception.
//
//	NOTE: getValueAsString() method should be preferred if getting the Link UID
//		because when disconnected will return "X". getValue() would return the
//		column name of the link when disconnected.
//
////special version of getValue for string type
//	Note: if called without template, necessary because types of std::basic_string<char>
// cause compiler problems if no string specific function
std::string ConfigurationTree::getValue() const
{
	std::string value;
	ConfigurationTree::getValue(value);
	return value;
}  // end getValue()
//==============================================================================
// getValueWithDefault
//	Only std::string value will work.
//		If this is a value node, and not type string, configView->getValue should
//		throw exception.
//
//	NOTE: getValueAsString() method should be preferred if getting the Link UID
//		because when disconnected will return "X". getValue() would return the
//		column name of the link when disconnected.
//
////special version of getValue for string type
//	Note: if called without template, necessary because types of std::basic_string<char>
// cause compiler problems if no string specific function
std::string ConfigurationTree::getValueWithDefault(const std::string& defaultValue) const
{
	if(isDefaultValue())
		return defaultValue;
	else
		return ConfigurationTree::getValue();
}  // end getValueWithDefault()

//==============================================================================
// getValue (only ConfigurationTree::BitMap value)
// special version of getValue for string type
//	Note: necessary because types of std::basic_string<char> cause compiler problems if no
// string specific function
void ConfigurationTree::getValueAsBitMap(ConfigurationTree::BitMap& bitmap) const
{
	//__COUT__ << row_ << " " << col_ << " p: " << tableView_<< __E__;

	if(row_ != TableView::INVALID && col_ != TableView::INVALID)  // this node is a value node
	{
		std::string bitmapString;
		tableView_->getValue(bitmapString, row_, col_);

		__COUTV__(bitmapString);
		if(bitmapString == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
		{
			bitmap.isDefault_ = true;
			return;
		}
		else
			bitmap.isDefault_ = false;

		// extract bit map
		{
			bitmap.bitmap_.clear();
			int          row      = -1;
			bool         openRow  = false;
			unsigned int startInt = -1;
			for(unsigned int i = 0; i < bitmapString.length(); i++)
			{
				__COUTV__(bitmapString[i]);
				__COUTV__(row);
				__COUTV__(openRow);
				__COUTV__(startInt);
				__COUTV__(i);

				if(!openRow)  // need start of row
				{
					if(bitmapString[i] == '[')
					{  // open a new row
						openRow = true;
						++row;
						bitmap.bitmap_.push_back(std::vector<uint64_t>());
					}
					else if(bitmapString[i] == ']')
					{
						break;  // ending bracket, done with string
					}
					else if(bitmapString[i] == ',')  // end characters found not within
					                                 // row
					{
						__SS__ << "Too many ']' or ',' characters in bit map configuration" << __E__;

						ss << nodeDump() << __E__;
						__SS_ONLY_THROW__;
					}
				}
				else if(startInt == (unsigned int)-1)  // need to find start of number
				{
					if(bitmapString[i] == ']')  // found end of row, instead of start of
					                            // number, assume row ended
					{
						openRow = false;
					}
					else if(bitmapString[i] >= '0' && bitmapString[i] <= '9')  // found start of number
					{
						startInt = i;
					}
					else if(bitmapString[i] == ',')  // comma found without number
					{
						__SS__ << "Too many ',' characters in bit map configuration" << __E__;

						ss << nodeDump() << __E__;
						__SS_ONLY_THROW__;
					}
				}
				else
				{
					// looking for end of number

					if(bitmapString[i] == ']')  // found end of row, assume row and number ended
					{
						openRow = false;
						bitmap.bitmap_[row].push_back(strtoul(bitmapString.substr(startInt, i - startInt).c_str(), 0, 0));
						startInt = -1;
					}
					else if(bitmapString[i] == ',')  // comma found, assume end of number
					{
						bitmap.bitmap_[row].push_back(strtoul(bitmapString.substr(startInt, i - startInt).c_str(), 0, 0));
						startInt = -1;
					}
				}
			}

			for(unsigned int r = 0; r < bitmap.bitmap_.size(); ++r)
			{
				for(unsigned int c = 0; c < bitmap.bitmap_[r].size(); ++c)
				{
					__COUT__ << r << "," << c << " = " << bitmap.bitmap_[r][c] << __E__;
				}
				__COUT__ << "================" << __E__;
			}
		}
	}
	else
	{
		__SS__ << "Requesting getValue must be on a value node." << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
}  // end getValueAsBitMap()

//==============================================================================
// getValue
//
//	special version of getValue for ConfigurationTree::BitMap type
ConfigurationTree::BitMap ConfigurationTree::getValueAsBitMap() const
{
	ConfigurationTree::BitMap value;
	ConfigurationTree::getValueAsBitMap(value);
	return value;
}  // end getValueAsBitMap()

//==============================================================================
// getEscapedValue
//	Only works if a value node, other exception thrown
std::string ConfigurationTree::getEscapedValue() const
{
	if(row_ != TableView::INVALID && col_ != TableView::INVALID)  // this node is a value node
		return tableView_->getEscapedValueAsString(row_, col_);

	__SS__ << "Can not get escaped value except from a value node!"
	       << " This node is type '" << getNodeType() << "." << __E__;

	ss << nodeDump() << __E__;
	__SS_THROW__;
}  // end getEscapedValue()

//==============================================================================
// getTableName
const std::string& ConfigurationTree::getTableName(void) const
{
	if(!table_)
	{
		__SS__ << "Can not get configuration name of node with no configuration pointer! "
		       << "Is there a broken link? " << __E__;
		if(linkParentTable_)
		{
			ss << "Error occurred traversing from " << linkParentTable_->getTableName() << " UID '"
			   << linkParentTable_->getView().getValueAsString(linkBackRow_, linkParentTable_->getView().getColUID()) << "' at row " << linkBackRow_
			   << " col '" << linkParentTable_->getView().getColumnInfo(linkBackCol_).getName() << ".'" << __E__;

			ss << StringMacros::stackTrace() << __E__;
		}

		__SS_ONLY_THROW__;
	}
	return table_->getTableName();
}  // end getTableName()

//==============================================================================
// getParentTableName
const std::string& ConfigurationTree::getParentTableName(void) const
{
	if(linkParentTable_)
		return linkParentTable_->getTableName();

	__SS__ << "Can not get parent table name of node with no parent table pointer! "
			<< "Was this node initialized correctly? " << __E__;
	__SS_ONLY_THROW__;
}  // end getParentTableName()

//==============================================================================
// getNodeRow
const unsigned int& ConfigurationTree::getNodeRow(void) const
{
	if(isUIDNode() || isValueNode())
		return row_;

	__SS__ << "Can only get row from a UID or value node!" << __E__;
	if(linkParentTable_)
	{
		ss << "Error occurred traversing from " << linkParentTable_->getTableName() << " UID '"
		   << linkParentTable_->getView().getValueAsString(linkBackRow_, linkParentTable_->getView().getColUID()) << "' at row " << linkBackRow_ << " col '"
		   << linkParentTable_->getView().getColumnInfo(linkBackCol_).getName() << ".'" << __E__;

		ss << StringMacros::stackTrace() << __E__;
	}

	__SS_ONLY_THROW__;

}  // end getNodeRow()

//==============================================================================
// getFieldTableName
//	returns the configuration name for the node's field.
//		Note: for link nodes versus value nodes this has different functionality than
// getTableName()
const std::string& ConfigurationTree::getFieldTableName(void) const
{
	// if link node, need config name from parent
	if(isLinkNode())
	{
		if(!linkParentTable_)
		{
			__SS__ << "Can not get configuration name of link node field with no parent "
			          "configuration pointer!"
			       << __E__;
			ss << nodeDump() << __E__;
			__SS_ONLY_THROW__;
		}
		return linkParentTable_->getTableName();
	}
	else
		return getTableName();
}  // end getFieldTableName()

//==============================================================================
// getDisconnectedTableName
const std::string& ConfigurationTree::getDisconnectedTableName(void) const
{
	if(isLinkNode() && isDisconnected())
		return disconnectedTargetName_;

	__SS__ << "Can not get disconnected target name of node unless it is a disconnected "
	          "link node!"
	       << __E__;

	ss << nodeDump() << __E__;
	__SS_ONLY_THROW__;
}  // end getDisconnectedTableName()

//==============================================================================
// getDisconnectedLinkID
const std::string& ConfigurationTree::getDisconnectedLinkID(void) const
{
	if(isLinkNode() && isDisconnected())
		return disconnectedLinkID_;

	__SS__ << "Can not get disconnected target name of node unless it is a disconnected "
	          "link node!"
	       << __E__;

	ss << nodeDump() << __E__;
	__SS_ONLY_THROW__;
}  // end getDisconnectedLinkID()

//==============================================================================
// getTableVersion
const TableVersion& ConfigurationTree::getTableVersion(void) const
{
	if(!tableView_)
	{
		__SS__ << "Can not get configuration version of node with no config view pointer!" << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}
	return tableView_->getVersion();
}  // end getTableVersion()

//==============================================================================
// getTableCreationTime
const time_t& ConfigurationTree::getTableCreationTime(void) const
{
	if(!tableView_)
	{
		__SS__ << "Can not get configuration creation time of node with no config view "
		          "pointer!"
		       << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}
	return tableView_->getCreationTime();
}  // end getTableCreationTime()

//==============================================================================
// getSetOfGroupIDs
//	returns set of group IDs if groupID value node
std::set<std::string> ConfigurationTree::getSetOfGroupIDs(void) const
{
	if(!isGroupIDNode())
	{
		__SS__ << "Can not get set of group IDs of node with value type of '" << getNodeType() << ".' Node must be a GroupID node." << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}

	return tableView_->getSetOfGroupIDs(col_, row_);

}  // end getSetOfGroupIDs()

//==============================================================================
// getFixedChoices
//	returns vector of default + data choices
//	Used as choices for tree-view, for example.
std::vector<std::string> ConfigurationTree::getFixedChoices(void) const
{
	if(getValueType() != TableViewColumnInfo::TYPE_FIXED_CHOICE_DATA && getValueType() != TableViewColumnInfo::TYPE_BITMAP_DATA && !isLinkNode())
	{
		__SS__ << "Can not get fixed choices of node with value type of '" << getValueType() << ".' Node must be a link or a value node with type '"
		       << TableViewColumnInfo::TYPE_BITMAP_DATA << "' or '" << TableViewColumnInfo::TYPE_FIXED_CHOICE_DATA << ".'" << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}

	std::vector<std::string> retVec;

	if(isLinkNode())
	{
		if(!linkParentTable_)
		{
			__SS__ << "Can not get fixed choices of node with no parent config view pointer!" << __E__;

			ss << nodeDump() << __E__;
			__SS_ONLY_THROW__;
		}

		//__COUT__ << getChildLinkIndex() << __E__;
		//__COUT__ << linkColName_ << __E__;

		// for links, col_ = -1, column c needs to change (to ChildLink column of pair)
		// get column from parent config pointer

		const TableView* parentView = &(linkParentTable_->getView());
		int              c          = parentView->findCol(linkColName_);

		std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
		bool                                                               isGroupLink;
		parentView->getChildLink(c, isGroupLink, linkPair);
		c = linkPair.first;

		std::vector<std::string> choices = parentView->getColumnInfo(c).getDataChoices();
		for(const auto& choice : choices)
			retVec.push_back(choice);

		return retVec;
	}

	if(!tableView_)
	{
		__SS__ << "Can not get fixed choices of node with no config view pointer!" << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}

	// return vector of default + data choices
	retVec.push_back(tableView_->getColumnInfo(col_).getDefaultValue());
	std::vector<std::string> choices = tableView_->getColumnInfo(col_).getDataChoices();
	for(const auto& choice : choices)
		retVec.push_back(choice);

	return retVec;
}  // end getFixedChoices()

//==============================================================================
// getValueAsString
//	NOTE: getValueAsString() method should be preferred if getting the Link UID
//		because when disconnected will return "X". getValue() would return the
//		column name of the link when disconnected.
//
//	returnLinkTableValue returns the value in the source table as though link was
//		not followed to destination table.
const std::string& ConfigurationTree::getValueAsString(bool returnLinkTableValue) const
{
	//__COUTV__(col_);__COUTV__(row_);__COUTV__(table_);__COUTV__(tableView_);

	if(isLinkNode())
	{
		if(returnLinkTableValue)
			return linkColValue_;
		else if(isDisconnected())
			return ConfigurationTree::DISCONNECTED_VALUE;
		else if(row_ == TableView::INVALID && col_ == TableView::INVALID)  // this link is groupId node
			return (groupId_ == "") ? table_->getTableName() : groupId_;
		else if(col_ == TableView::INVALID)  // this link is uid node
			return tableView_->getDataView()[row_][tableView_->getColUID()];
		else
		{
			__SS__ << "Impossible Link." << __E__;

			ss << nodeDump() << __E__;
			__SS_THROW__;
		}
	}
	else if(row_ != TableView::INVALID && col_ != TableView::INVALID)  // this node is a value node
		return tableView_->getDataView()[row_][col_];
	else if(row_ == TableView::INVALID && col_ == TableView::INVALID)  // this node is table node maybe with groupId
	{
		// if root node, then no table defined
		if(isRootNode())
			return ConfigurationTree::ROOT_NAME;

		return (groupId_ == "") ? table_->getTableName() : groupId_;
	}
	else if(row_ == TableView::INVALID)
	{
		__SS__ << "Malformed ConfigurationTree" << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
	else if(col_ == TableView::INVALID)  // this node is uid node
		return tableView_->getDataView()[row_][tableView_->getColUID()];
	else
	{
		__SS__ << "Impossible." << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
}  // end getValueAsString()

//==============================================================================
// getUIDAsString
//	returns UID associated with current value node or UID-Link node
//
const std::string& ConfigurationTree::getUIDAsString(void) const
{
	if(isValueNode() || isUIDLinkNode() || isUIDNode())
		return tableView_->getDataView()[row_][tableView_->getColUID()];

	{
		__SS__ << "Can not get UID of node with type '" << getNodeType() << ".' Node type must be '" << ConfigurationTree::NODE_TYPE_VALUE << "' or '"
		       << ConfigurationTree::NODE_TYPE_UID_LINK << ".'" << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}
}  // end getUIDAsString()

//==============================================================================
// getValueDataType
//	e.g. used to determine if node is type NUMBER
const std::string& ConfigurationTree::getValueDataType(void) const
{
	if(isValueNode())
		return tableView_->getColumnInfo(col_).getDataType();
	else  // must be std::string
		return TableViewColumnInfo::DATATYPE_STRING;
}  // end getValueDataType()

//==============================================================================
// isDefaultValue
//	returns true if is a value node and value is the default for the type
bool ConfigurationTree::isDefaultValue(void) const
{
	if(!isValueNode())
		return false;

	if(getValueDataType() == TableViewColumnInfo::DATATYPE_STRING)
	{
		if(getValueType() == TableViewColumnInfo::TYPE_ON_OFF || getValueType() == TableViewColumnInfo::TYPE_TRUE_FALSE ||
		   getValueType() == TableViewColumnInfo::TYPE_YES_NO)
			return getValueAsString() == TableViewColumnInfo::DATATYPE_BOOL_DEFAULT;  // default to OFF, NO,
			                                                                          // FALSE
		else if(getValueType() == TableViewColumnInfo::TYPE_COMMENT)
			return getValueAsString() == TableViewColumnInfo::DATATYPE_COMMENT_DEFAULT ||
			       getValueAsString() == "";  // in case people delete default comment, allow blank also
		else
			return getValueAsString() == TableViewColumnInfo::DATATYPE_STRING_DEFAULT;
	}
	else if(getValueDataType() == TableViewColumnInfo::DATATYPE_NUMBER)
		return getValueAsString() == TableViewColumnInfo::DATATYPE_NUMBER_DEFAULT;
	else if(getValueDataType() == TableViewColumnInfo::DATATYPE_TIME)
		return getValueAsString() == TableViewColumnInfo::DATATYPE_TIME_DEFAULT;
	else
		return false;
}  // end isDefaultValue()

//==============================================================================
// getDefaultValue
//	returns default value if is value node
// TODO: should a function to get if it is min and max value be included here??
const std::string& ConfigurationTree::getDefaultValue(void) const
{
	if(!isValueNode())
	{
		__SS__ << "Can only get default value from a value node! "
		       << "The node type is " << getNodeType() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}

	if(getValueDataType() == TableViewColumnInfo::DATATYPE_STRING)
	{
		if(getValueType() == TableViewColumnInfo::TYPE_ON_OFF || getValueType() == TableViewColumnInfo::TYPE_TRUE_FALSE ||
		   getValueType() == TableViewColumnInfo::TYPE_YES_NO)
			return TableViewColumnInfo::DATATYPE_BOOL_DEFAULT;  // default to OFF, NO,
			                                                    // FALSE
		else if(getValueType() == TableViewColumnInfo::TYPE_COMMENT)
			return TableViewColumnInfo::DATATYPE_COMMENT_DEFAULT;  // in case people delete default comment, allow blank also
		else
			return TableViewColumnInfo::DATATYPE_STRING_DEFAULT;
	}
	else if(getValueDataType() == TableViewColumnInfo::DATATYPE_NUMBER)
		return TableViewColumnInfo::DATATYPE_NUMBER_DEFAULT;
	else if(getValueDataType() == TableViewColumnInfo::DATATYPE_TIME)
		return TableViewColumnInfo::DATATYPE_TIME_DEFAULT;

	{
		__SS__ << "Can only get default value from a value node! "
		       << "The node type is " << getNodeType() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
}  // end isDefaultValue()

//==============================================================================
// getValueType
//	e.g. used to determine if node is type TYPE_DATA, TYPE_ON_OFF, etc.
const std::string& ConfigurationTree::getValueType(void) const
{
	if(isValueNode())
		return tableView_->getColumnInfo(col_).getType();
	else if(isLinkNode() && isDisconnected())
		return ConfigurationTree::VALUE_TYPE_DISCONNECTED;
	else  // just call all non-value nodes data
		return ConfigurationTree::VALUE_TYPE_NODE;
}  // end getValueType()

//==============================================================================
// getColumnInfo
//	only sensible for value node
const TableViewColumnInfo& ConfigurationTree::getColumnInfo(void) const
{
	if(isValueNode())
		return tableView_->getColumnInfo(col_);
	else
	{
		__SS__ << "Can only get column info from a value node! "
		       << "The node type is " << getNodeType() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
}  // end getColumnInfo()

//==============================================================================
// getRow
const unsigned int& ConfigurationTree::getRow(void) const { return row_; }

//==============================================================================
// getColumn
const unsigned int& ConfigurationTree::getColumn(void) const { return col_; }

//==============================================================================
// getFieldRow
//	return field's row (different handling for value vs. link node)
const unsigned int& ConfigurationTree::getFieldRow(void) const
{
	if(isLinkNode())
	{
		// for links, need to use parent info to determine
		return linkBackRow_;
	}
	else
		return row_;
}  // end getFieldRow()

//==============================================================================
// getFieldColumn
//	return field's column (different handling for value vs. link node)
const unsigned int& ConfigurationTree::getFieldColumn(void) const
{
	if(isLinkNode())
	{
		// for links, need to use parent info to determine
		return linkBackCol_;
	}
	else
		return col_;
}  // end getFieldColumn()

//==============================================================================
// getChildLinkIndex
const std::string& ConfigurationTree::getChildLinkIndex(void) const
{
	if(!isLinkNode())
	{
		__SS__ << "Can only get link ID from a link! "
		       << "The node type is " << getNodeType() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
	return childLinkIndex_;
}  // end getChildLinkIndex()

//==============================================================================
// getValueName
//	e.g. used to determine column name of value node
const std::string& ConfigurationTree::getValueName(void) const
{
	if(isValueNode())
		return tableView_->getColumnInfo(col_).getName();
	else if(isLinkNode())
		return linkColName_;
	else
	{
		__SS__ << "Can only get value name of a value node!" << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
}  // end getValueName()

//==============================================================================
// recurse
//	Used by ConfigurationTree to handle / syntax of getNode
ConfigurationTree ConfigurationTree::recurse(const ConfigurationTree& tree,
                                             const std::string&       childPath,
                                             bool                     doNotThrowOnBrokenUIDLinks,
                                             const std::string&       originalNodeString)
{
	//__COUT__ << tree.row_ << " " << tree.col_ << __E__;
	//__COUT__ << "childPath=" << childPath << " " << childPath.length() << __E__;
	if(childPath.length() <= 1)  // only "/" or ""
		return tree;
	return tree.recursiveGetNode(childPath, doNotThrowOnBrokenUIDLinks, originalNodeString);
}  // end recurse()

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
// std::string ConfigurationTree::getRecordFieldValueAsString(std::string fieldName) const
//{
//	//enforce that starting point is a table node
//	if(!isUIDNode())
//	{
//		__SS__ << "Can only get getRecordFieldValueAsString from a uid node! " <<
//				"The node type is " << getNodeType() << __E__;
//		__COUT__ << "\n" << ss.str() << __E__;
//		__SS_THROW__;
//	}
//
//	unsigned int c = tableView_->findCol(fieldName);
//	return tableView_->getDataView()[row_][c];
//}

//==============================================================================
// getNode
//	Connected to recursiveGetNode()
//
//	nodeString can be a multi-part path using / delimiter
//	use:
//			getNode(/uid/col) or getNode(uid)->getNode(col)
//
// if doNotThrowOnBrokenUIDLinks
//		then catch exceptions on UID links and call disconnected
ConfigurationTree ConfigurationTree::getNode(const std::string& nodeString, bool doNotThrowOnBrokenUIDLinks) const
{
	// __COUT__ << "nodeString=" << nodeString << " len=" << nodeString.length() << __E__;
	return recursiveGetNode(nodeString, doNotThrowOnBrokenUIDLinks, "" /*originalNodeString*/);
}  // end getNode() connected to recursiveGetNode()
ConfigurationTree ConfigurationTree::recursiveGetNode(const std::string& nodeString,
                                                      bool               doNotThrowOnBrokenUIDLinks,
                                                      const std::string& originalNodeString) const
{
	// __COUT__ << "nodeString=" << nodeString << " len=" << nodeString.length() << __E__;
	//__COUT__ << "doNotThrowOnBrokenUIDLinks=" << doNotThrowOnBrokenUIDLinks <<
	// __E__;

	// get nodeName (in case of / syntax)
	if(nodeString.length() < 1)
	{
		__SS__ << "Invalid empty node name! Looking for child node '" << nodeString << 
			"' from node '" << getValue() << "'..." << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}


	// ignore multiple starting slashes
	size_t startingIndex = 0;
	while(startingIndex < nodeString.length() && nodeString[startingIndex] == '/')
		++startingIndex;
	size_t endingIndex = nodeString.find('/', startingIndex);
	if(endingIndex == std::string::npos) endingIndex = nodeString.length();

	std::string nodeName = nodeString.substr(startingIndex, endingIndex-startingIndex);
	// __COUT__ << "nodeName=" << nodeName << " len=" << nodeName.length() << __E__;
	
	++endingIndex;
	std::string childPath = (endingIndex >= nodeString.length()?"":nodeString.substr(endingIndex));
	// __COUT__ << "childPath=" << childPath << " len=" << childPath.length() << " endingIndex=" << endingIndex << " nodeString.length()=" << nodeString.length() << __E__;

	// if this tree is beginning at a configuration.. then go to uid, and vice versa

	try
	{
		//__COUT__ << row_ << " " << col_ <<  " " << groupId_ << " " << tableView_ <<
		// __E__;
		if(isRootNode())
		{
			// root node
			// so return table node
			return recurse(configMgr_->getNode(nodeName), childPath, doNotThrowOnBrokenUIDLinks, originalNodeString);
		}
		else if(row_ == TableView::INVALID && col_ == TableView::INVALID)
		{
			// table node

			if(!tableView_)
			{
				__SS__ << "Missing configView pointer! Likely attempting to access a "
				          "child node through a disconnected link node."
				       << __E__;

				ss << nodeDump() << __E__;
				__SS_THROW__;
			}

			// this node is table node, so return uid node considering groupid
			return recurse(ConfigurationTree(configMgr_,
			                                 table_,
			                                 "",  // no new groupId string, not a link
			                                 0 /*linkParentTable_*/,
			                                 "",  // link node name, not a link
			                                 "",  // link node value, not a link
			                                 TableView::INVALID /*linkBackRow_*/,
			                                 TableView::INVALID /*linkBackCol_*/,
			                                 "",  // ignored disconnected target name, not a link
			                                 "",  // ignored disconnected link id, not a link
			                                 "",
			                                 // if this node is group table node, consider that when getting rows
			                                 (groupId_ == "") ? tableView_->findRow(tableView_->getColUID(), nodeName)
			                                                  : tableView_->findRowInGroup(tableView_->getColUID(), nodeName, groupId_, childLinkIndex_)),
			               childPath,
			               doNotThrowOnBrokenUIDLinks,
			               originalNodeString);
		}
		else if(row_ == TableView::INVALID)
		{
			__SS__ << "Malformed ConfigurationTree" << __E__;

			ss << nodeDump() << __E__;
			__SS_THROW__;
		}
		else if(col_ == TableView::INVALID)
		{
			// this node is uid node, so return link, group link, disconnected, or value
			// node

			//__COUT__ << "nodeName=" << nodeName << " " << nodeName.length() <<
			// __E__;

			// if the value is a unique link ..
			// return a uid node!
			// if the value is a group link
			// return a table node with group string
			// else.. return value node

			if(!tableView_)
			{
				__SS__ << "Missing configView pointer! Likely attempting to access a "
				          "child node through a disconnected link node."
				       << __E__;

				ss << nodeDump() << __E__;
				__SS_THROW__;
			}

			unsigned int                                                       c = tableView_->findCol(nodeName);
			std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
			bool                                                               isGroupLink, isLink;
			if((isLink = tableView_->getChildLink(c, isGroupLink, linkPair)) && !isGroupLink)
			{
				//__COUT__ << "nodeName=" << nodeName << " " << nodeName.length() <<
				// __E__;  is a unique link, return uid node in new configuration
				//	need new configuration pointer
				//	and row of linkUID in new configuration

				const TableBase* childConfig;
				try
				{
					childConfig = configMgr_->getTableByName(tableView_->getDataView()[row_][linkPair.first]);
					childConfig->getView();  // get view as a test for an active view

					if(doNotThrowOnBrokenUIDLinks)  // try a test of getting row
					{
						childConfig->getView().findRow(childConfig->getView().getColUID(), tableView_->getDataView()[row_][linkPair.second]);
					}
				}
				catch(...)
				{
					//					__COUT_WARN__ << "Found disconnected node! (" <<
					// nodeName
					//<<
					//							":" <<
					// tableView_->getDataView()[row_][linkPair.first]
					//<< ")" << 							" at entry with UID " <<
					//							tableView_->getDataView()[row_][tableView_->getColUID()]
					//<< 							__E__;  do not recurse further
					return ConfigurationTree(configMgr_,
					                         0,
					                         "",
					                         table_,  // linkParentTable_
					                         nodeName,
					                         tableView_->getDataView()[row_][c],  // this the link node field
					                                                              // associated value (matches
					                                                              // targeted column)
					                         row_ /*linkBackRow_*/,
					                         c /*linkBackCol_*/,
					                         tableView_->getDataView()[row_][linkPair.first],   // give
					                                                                            // disconnected
					                                                                            // target name
					                         tableView_->getDataView()[row_][linkPair.second],  // give
					                                                                            // disconnected
					                                                                            // link ID
					                         tableView_->getColumnInfo(c).getChildLinkIndex());
				}

				return recurse(ConfigurationTree(  // this is a link node
				                   configMgr_,
				                   childConfig,
				                   "",                                  // no new groupId string
				                   table_,                              // linkParentTable_
				                   nodeName,                            // this is a link node
				                   tableView_->getDataView()[row_][c],  // this the link node field
				                                                        // associated value (matches
				                                                        // targeted column)
				                   row_ /*linkBackRow_*/,
				                   c /*linkBackCol_*/,
				                   "",  // ignore since is connected
				                   "",  // ignore since is connected
				                   tableView_->getColumnInfo(c).getChildLinkIndex(),
				                   childConfig->getView().findRow(childConfig->getView().getColUID(), tableView_->getDataView()[row_][linkPair.second])),
				               childPath,
				               doNotThrowOnBrokenUIDLinks,
				               originalNodeString);
			}
			else if(isLink)
			{
				//__COUT__ << "nodeName=" << nodeName << " " << nodeName.length() <<
				// __E__;  is a group link, return new configuration with group string
				//	need new configuration pointer
				//	and group string

				const TableBase* childConfig;
				try
				{
					childConfig = configMgr_->getTableByName(tableView_->getDataView()[row_][linkPair.first]);
					childConfig->getView();  // get view as a test for an active view
				}
				catch(...)
				{
					if(tableView_->getDataView()[row_][linkPair.first] != TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
						__COUT_WARN__ << "Found disconnected node! Failed link target "
						                 "from nodeName="
						              << nodeName << " to table:id=" << tableView_->getDataView()[row_][linkPair.first] << ":"
						              << tableView_->getDataView()[row_][linkPair.second] << __E__;

					// do not recurse further
					return ConfigurationTree(configMgr_,
					                         0,
					                         tableView_->getDataView()[row_][linkPair.second],  // groupID
					                         table_,                                            // linkParentTable_
					                         nodeName,
					                         tableView_->getDataView()[row_][c],  // this the link node field
					                                                              // associated value (matches
					                                                              // targeted column)
					                         row_ /*linkBackRow_*/,
					                         c /*linkBackCol_*/,
					                         tableView_->getDataView()[row_][linkPair.first],   // give
					                                                                            // disconnected
					                                                                            // target name
					                         tableView_->getDataView()[row_][linkPair.second],  // give
					                                                                            // disconnected
					                                                                            // target name
					                         tableView_->getColumnInfo(c).getChildLinkIndex());
				}

				return recurse(ConfigurationTree(  // this is a link node
				                   configMgr_,
				                   childConfig,
				                   tableView_->getDataView()[row_][linkPair.second],  // groupId string
				                   table_,                                            // linkParentTable_
				                   nodeName,                                          // this is a link node
				                   tableView_->getDataView()[row_][c],                // this the link node field
				                                                                      // associated value (matches
				                                                                      // targeted column)
				                   row_ /*linkBackRow_*/,
				                   c /*linkBackCol_*/,
				                   "",  // ignore since is connected
				                   "",  // ignore since is connected
				                   tableView_->getColumnInfo(c).getChildLinkIndex()),
				               childPath,
				               doNotThrowOnBrokenUIDLinks,
				               originalNodeString);
			}
			else
			{
				//__COUT__ << "nodeName=" << nodeName << " " << nodeName.length() <<
				// __E__;  return value node
				return ConfigurationTree(configMgr_,
				                         table_,
				                         "",
				                         0 /*linkParentTable_*/,
				                         "",
				                         "",
				                         TableView::INVALID /*linkBackRow_*/,
				                         TableView::INVALID /*linkBackCol_*/,
				                         "",
				                         "" /*disconnectedLinkID*/,
				                         "",
				                         row_,
				                         c);
			}
		}
	}
	catch(std::runtime_error& e)
	{
		__SS__ << "\n\nError occurred descending from node '" << getValue() << "' in table '" << getTableName() << "' looking for child '" << nodeName
		       << "'\n\n"
		       << __E__;
		ss << "The original node search string was '" << originalNodeString << ".'" << __E__;
		ss << "--- Additional error detail: \n\n" << e.what() << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}
	catch(...)
	{
		__SS__ << "\n\nError occurred descending from node '" << getValue() << "' in table '" << getTableName() << "' looking for child '" << nodeName
		       << "'\n\n"
		       << __E__;
		ss << "The original node search string was '" << originalNodeString << ".'" << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}

	// this node is value node, so has no node to choose from
	__SS__ << "\n\nError occurred descending from node '" << getValue() << "' in table '" << getTableName() << "' looking for child '" << nodeName << "'\n\n"
	       << "Invalid depth! getNode() called from a value point in the Configuration Tree." << __E__;
	ss << "The original node search string was '" << originalNodeString << ".'" << __E__;

	ss << nodeDump() << __E__;
	__SS_ONLY_THROW__;  // this node is value node, cant go any deeper!
}  // end recursiveGetNode()

//==============================================================================
// getNodes
std::map<std::string, ConfigurationTree> ConfigurationTree::getNodes(const std::string& nodeString) const
{
	if (nodeString.length() < 1)
	{
		return getChildrenMap();
	}

	return getNode(nodeString).getChildrenMap();
}
//==============================================================================
// nodeDump
//	Useful for debugging a node failure, like when throwing an exception
std::string ConfigurationTree::nodeDump(void) const
{
	__SS__ << __E__ << __E__;

	ss << "Row=" << (int)row_ << ", Col=" << (int)col_ << ", TablePointer=" << table_ << __E__;

	// stack trace can seg fault on demangle call!... ?
	try
	{
		ss << "\n\n" << StringMacros::stackTrace() << __E__ << __E__;
	}
	catch(...)
	{
	}  // ignore errors

	ss << "ConfigurationTree::nodeDump() "
	      "=====================================\nConfigurationTree::nodeDump():"
	   << __E__;

	// try each level of debug.. and ignore errors
	try
	{
		ss << "\t"
		   << "Node dump initiated from node '" << getValueAsString() << "'..." << __E__;
	}
	catch(...)
	{
	}  // ignore errors
	try
	{
		ss << "\t"
		   << "Node dump initiated from node '" << getValue() << "' in table '" << getTableName() << ".'" << __E__;
	}
	catch(...)
	{
	}  // ignore errors
	
	try
	{		
		//try to avoid recursive throwing of getChildrenNames() until death spiral
		if(isTableNode() || isGroupLinkNode())
		{
			auto children = getChildrenNames();
			ss << "\t"
			<< "Here is the list of possible children (count = " << children.size() << "):" << __E__;
			for(auto& child : children)
				ss << "\t\t" << child << __E__;
			if(tableView_)
			{
				ss << "\n\nHere is the culprit table printout:\n\n";
				tableView_->print(ss);
			}
		}
	}
	catch(...)
	{
	}  // ignore errors trying to show children

	ss << "\n\nend ConfigurationTree::nodeDump() =====================================" << __E__;

	return ss.str();
}  // end nodeDump()

//==============================================================================
ConfigurationTree ConfigurationTree::getBackNode(std::string nodeName, unsigned int backSteps) const
{
	for(unsigned int i = 0; i < backSteps; i++)
		nodeName = nodeName.substr(0, nodeName.find_last_of('/'));

	return getNode(nodeName);
}  // end getBackNode()

//==============================================================================
ConfigurationTree ConfigurationTree::getForwardNode(std::string nodeName, unsigned int forwardSteps) const
{
	unsigned int s = 0;

	// skip all leading /'s
	while(s < nodeName.length() && nodeName[s] == '/')
		++s;

	for(unsigned int i = 0; i < forwardSteps; i++)
		s = nodeName.find('/', s) + 1;

	return getNode(nodeName.substr(0, s));
}  // end getForwardNode()

//==============================================================================
// isValueNode
//	if true, then this is a leaf node, i.e. there can be no children, only a value
bool ConfigurationTree::isValueNode(void) const { return (row_ != TableView::INVALID && col_ != TableView::INVALID); }  // end isValueNode()

//==============================================================================
// isValueBoolType
//	if true, then this is a leaf node with BOOL type
bool ConfigurationTree::isValueBoolType(void) const { return isValueNode() && tableView_->getColumnInfo(col_).isBoolType(); }  // end isValueBoolType()

//==============================================================================
// isValueNumberDataType
//	if true, then this is a leaf node with NUMBER data type
bool ConfigurationTree::isValueNumberDataType(void) const
{
	return isValueNode() && tableView_->getColumnInfo(col_).isNumberDataType();
}  // end isValueBoolType()

//==============================================================================
// isDisconnected
//	if true, then this is a disconnected node, i.e. there is a configuration link missing
//	(this is possible when the configuration is loaded in stages and the complete tree
//		may not be available, yet)
bool ConfigurationTree::isDisconnected(void) const
{
	if(!isLinkNode())
	{
		__SS__ << "\n\nError occurred testing link connection at node with value '" << getValue() << "' in table '" << getTableName() << "'\n\n" << __E__;
		ss << "This is not a Link node! It is node type '" << getNodeType() << ".' Only a Link node can be disconnected." << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}

	return !table_ || !tableView_;
}  // end isDisconnected()

//==============================================================================
// isLinkNode
//	if true, then this is a link node
bool ConfigurationTree::isLinkNode(void) const { return linkColName_ != ""; }

//==============================================================================
// getNodeType
//	return node type as string
const std::string ConfigurationTree::NODE_TYPE_GROUP_TABLE = "GroupTableNode";
const std::string ConfigurationTree::NODE_TYPE_TABLE       = "TableNode";
const std::string ConfigurationTree::NODE_TYPE_GROUP_LINK  = "GroupLinkNode";
const std::string ConfigurationTree::NODE_TYPE_UID_LINK    = "UIDLinkNode";
const std::string ConfigurationTree::NODE_TYPE_VALUE       = "ValueNode";
const std::string ConfigurationTree::NODE_TYPE_UID         = "UIDNode";
const std::string ConfigurationTree::NODE_TYPE_ROOT        = "RootNode";

std::string ConfigurationTree::getNodeType(void) const
{
	if(isRootNode())
		return ConfigurationTree::NODE_TYPE_ROOT;
	if(isTableNode() && groupId_ != "")
		return ConfigurationTree::NODE_TYPE_GROUP_TABLE;
	if(isTableNode())
		return ConfigurationTree::NODE_TYPE_TABLE;
	if(isGroupLinkNode())
		return ConfigurationTree::NODE_TYPE_GROUP_LINK;
	if(isLinkNode())
		return ConfigurationTree::NODE_TYPE_UID_LINK;
	if(isValueNode())
		return ConfigurationTree::NODE_TYPE_VALUE;
	return ConfigurationTree::NODE_TYPE_UID;
}  // end getNodeType()

//==============================================================================
// isGroupLinkNode
//	if true, then this is a group link node
bool ConfigurationTree::isGroupLinkNode(void) const { return (isLinkNode() && groupId_ != ""); }

//==============================================================================
// isUIDLinkNode
//	if true, then this is a uid link node
bool ConfigurationTree::isUIDLinkNode(void) const { return (isLinkNode() && groupId_ == ""); }  // end isUIDLinkNode()

//==============================================================================
// isGroupIDNode
//	if true, then this is a Group ID node
bool ConfigurationTree::isGroupIDNode(void) const { return (isValueNode() && tableView_->getColumnInfo(col_).isGroupID()); }  // end isGroupIDNode()

//==============================================================================
// isUIDNode
//	if true, then this is a uid node
bool ConfigurationTree::isUIDNode(void) const { return (row_ != TableView::INVALID && col_ == TableView::INVALID); }

//==============================================================================
// getCommonFields
//	wrapper for ...recursiveGetCommonFields
//
//	returns common fields in order encountered
//		including through UID links depending on depth specified
//
//	Field := {Table, UID, Column Name, Relative Path, TableViewColumnInfo}
//
//	if fieldAcceptList or fieldRejectList are not empty,
//		then reject any that are not in field accept filter list
//		and reject any that are in field reject filter list
//
//	will only go to specified depth looking for fields
//		(careful to prevent infinite loops in tree navigation)
//
std::vector<ConfigurationTree::RecordField> ConfigurationTree::getCommonFields(const std::vector<std::string /*uid*/>&           recordList,
                                                                               const std::vector<std::string /*relative-path*/>& fieldAcceptList,
                                                                               const std::vector<std::string /*relative-path*/>& fieldRejectList,
                                                                               unsigned int                                      depth,
                                                                               bool                                              autoSelectFilterFields) const
{
	// enforce that starting point is a table node
	if(!isRootNode() && !isTableNode())
	{
		__SS__ << "Can only get getCommonFields from a root or table node! "
		       << "The node type is " << getNodeType() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}

	std::vector<ConfigurationTree::RecordField> fieldCandidateList;
	std::vector<int>                            fieldCount;  //-1 := guaranteed, else count must match num of records

	--depth;  // decrement for recursion

	// for each record in <record list>
	//	loop through all record's children
	//		if isValueNode (value nodes are possible field candidates!)
	//			if first uid record
	//				add field to <field candidates list> if in <field filter list>
	//				mark <field count> as guaranteed -1 (all these fields must be common
	// for  UIDs in same table)
	//			else not first uid record, do not need to check, must be same as first
	// record! 		else if depth > 0 and UID-Link Node 			recursively (call
	// recursiveGetCommonFields())
	//				=====================
	//				Start recursiveGetCommonFields()
	//					--depth;
	//					loop through all children
	//						if isValueNode (value nodes are possible field candidates!)
	//							if first uid record
	//								add field to <field candidates list> if in <field
	// filter  list> 								initial mark <field count> as 1
	//							else
	//								if field is in <field candidates list>,
	//									increment <field count> for field candidate
	//								else if field is not in list, ignore field
	//						else if depth > 0 and is UID-Link
	//							if Link Table/UID pair is not found in <field candidates
	// list> (avoid  endless loops through tree)
	// recursiveGetCommonFields()
	//				=====================
	//
	//
	// loop through all field candidates
	//	remove those with <field count> != num of records
	//
	//
	// return result

	bool found;  // used in loops
	// auto tableName = isRootNode()?"/":getTableName();  //all records will share this
	// table name

	// if no records, just return table fields
	if(!recordList.size() && tableView_)
	{
		const std::vector<TableViewColumnInfo>& colInfo = tableView_->getColumnsInfo();

		for(unsigned int col = 0; col < colInfo.size(); ++col)
		{
			//__COUT__ << "Considering field " <<
			//		colInfo[col].getName() << __E__;

			// check field accept filter list
			found = fieldAcceptList.size() ? false : true;  // accept if no filter
			// list
			for(const auto& fieldFilter : fieldAcceptList)
				if(StringMacros::wildCardMatch(fieldFilter, colInfo[col].getName()))
				{
					found = true;
					break;
				}

			if(found)
			{
				// check field reject filter list

				found = true;  // accept if no filter list
				for(const auto& fieldFilter : fieldRejectList)
					if(StringMacros::wildCardMatch(fieldFilter, colInfo[col].getName()))
					{
						found = false;  // reject if match
						break;
					}
			}

			// if found, new field (since this is first record)
			if(found)
			{
				//__COUT__ << "FOUND field " <<
				//		colInfo[col].getName() << __E__;

				if(colInfo[col].isChildLink())
				{
					//__COUT__ << "isGroupLinkNode " << colInfo[col].getName() << __E__;

					// must get column info differently for group link column

					std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
					bool                                                               isGroupLink;
					tableView_->getChildLink(col, isGroupLink, linkPair);

					// add both link columns

					fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
					                                                            "",  // uid
					                                                            tableView_->getColumnInfo(linkPair.first).getName(),
					                                                            "",  // relative path, not including columnName_
					                                                            &tableView_->getColumnInfo(linkPair.first)));
					fieldCount.push_back(-1);  // mark guaranteed field

					fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
					                                                            "",  // uid
					                                                            tableView_->getColumnInfo(linkPair.second).getName(),
					                                                            "",  // relative path, not including columnName_
					                                                            &tableView_->getColumnInfo(linkPair.second)));
					fieldCount.push_back(-1);  // mark guaranteed field
				}
				else  // value node
				{
					fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
					                                                            "",  // uid
					                                                            colInfo[col].getName(),
					                                                            "",  // relative path, not including columnName_
					                                                            &colInfo[col]));
					fieldCount.push_back(1);  // init count to 1
				}
			}
		}  // end table column loop
	}      // end no record handling

	for(unsigned int i = 0; i < recordList.size(); ++i)
	{
		//__COUT__ << "Checking " << recordList[i] << __E__;
		ConfigurationTree node = getNode(recordList[i]);

		node.recursiveGetCommonFields(fieldCandidateList,
		                              fieldCount,
		                              fieldAcceptList,
		                              fieldRejectList,
		                              depth,
		                              "",  // relativePathBase
		                              !i   // continue inFirstRecord (or not) depth search
		);

	}  // end record loop

	__COUT__ << "======================= check for count = " << (int)recordList.size() << __E__;

	// loop through all field candidates
	//	remove those with <field count> != num of records
	for(unsigned int i = 0; i < fieldCandidateList.size(); ++i)
	{
		__COUT__ << "Checking " << fieldCandidateList[i].relativePath_ << fieldCandidateList[i].columnName_ << " = " << fieldCount[i] << __E__;
		if(recordList.size() != 0 && fieldCount[i] != -1 && fieldCount[i] != (int)recordList.size())
		{
			//__COUT__ << "Erasing " << fieldCandidateList[i].relativePath_ <<
			//		fieldCandidateList[i].columnName_ << __E__;

			fieldCount.erase(fieldCount.begin() + i);
			fieldCandidateList.erase(fieldCandidateList.begin() + i);
			--i;  // rewind to look at next after deleted
		}
	}

	// for(unsigned int i=0;i<fieldCandidateList.size();++i)
	//	__COUT__ << "Pre-Final " << fieldCandidateList[i].relativePath_ <<
	//			fieldCandidateList[i].columnName_ << __E__;

	if(autoSelectFilterFields)
	{
		// filter for just 3 of the best filter fields
		//	i.e. preference	for GroupID, On/Off, and FixedChoice fields.
		std::set<std::pair<unsigned int /*fieldPriority*/, unsigned int /*fieldIndex*/>> prioritySet;

		unsigned int highestPriority = 0;
		unsigned int priorityPenalty;
		for(unsigned int i = 0; i < fieldCandidateList.size(); ++i)
		{
			//				__COUT__ << "Option " << fieldCandidateList[i].relativePath_
			//<< 						fieldCandidateList[i].columnName_ << " : " <<
			//						fieldCandidateList[i].columnInfo_->getType() << ":" <<
			//						fieldCandidateList[i].columnInfo_->getDataType() <<
			//__E__;

			priorityPenalty =
			    std::count(fieldCandidateList[i].relativePath_.begin(), fieldCandidateList[i].relativePath_.end(), '/') * 20;  // penalize if not top level

			if(fieldCandidateList[i].columnInfo_->isBoolType())
			{
				prioritySet.emplace(std::make_pair(0 + priorityPenalty /*fieldPriority*/, i /*fieldIndex*/));
				if(highestPriority < 0 + priorityPenalty)
					highestPriority = 0 + priorityPenalty;
			}
			else if(fieldCandidateList[i].columnInfo_->isGroupID())
			{
				prioritySet.emplace(std::make_pair(1 + priorityPenalty /*fieldPriority*/, i /*fieldIndex*/));
				if(highestPriority < 1 + priorityPenalty)
					highestPriority = 1 + priorityPenalty;
			}
			else if(fieldCandidateList[i].columnInfo_->getType() == TableViewColumnInfo::TYPE_FIXED_CHOICE_DATA)
			{
				prioritySet.emplace(std::make_pair(3 + priorityPenalty /*fieldPriority*/, i /*fieldIndex*/));
				if(highestPriority < 3 + priorityPenalty)
					highestPriority = 3 + priorityPenalty;
			}
			else if(fieldCandidateList[i].columnInfo_->getType() == TableViewColumnInfo::TYPE_DATA)
			{
				prioritySet.emplace(std::make_pair(10 + priorityPenalty /*fieldPriority*/, i /*fieldIndex*/));
				if(highestPriority < 10 + priorityPenalty)
					highestPriority = 10 + priorityPenalty;
			}
			else  // skip other fields and mark for erasing
			{
				fieldCandidateList[i].tableName_ = "";  // clear table name as indicator for erase
				continue;
			}

		}  // done ranking fields

		__COUTV__(StringMacros::setToString(prioritySet));

		// now choose the top 3, and delete the rest
		//	clear table name to indicate field should be erased
		{
			unsigned int cnt = 0;
			for(const auto& priorityFieldIndex : prioritySet)
				if(++cnt > 3)  // then mark for erasing
				{
					//					__COUT__ << cnt << " marking " <<
					// fieldCandidateList[
					//								priorityFieldIndex.second].relativePath_
					//<<
					//							fieldCandidateList[priorityFieldIndex.second].columnName_
					//<<
					//							__E__;
					fieldCandidateList[priorityFieldIndex.second].tableName_ = "";  // clear table name as indicator for erase
				}
		}

		for(unsigned int i = 0; i < fieldCandidateList.size(); ++i)
		{
			if(fieldCandidateList[i].tableName_ == "")  // then erase
			{
				//				__COUT__ << "Erasing " <<
				// fieldCandidateList[i].relativePath_
				//<< 						fieldCandidateList[i].columnName_ << __E__;
				fieldCandidateList.erase(fieldCandidateList.begin() + i);
				--i;  // rewind to look at next after deleted
			}
		}
	}  // end AUTO filter field selection

	for(unsigned int i = 0; i < fieldCandidateList.size(); ++i)
		__COUT__ << "Final " << fieldCandidateList[i].relativePath_ << fieldCandidateList[i].columnName_ << __E__;

	return fieldCandidateList;
}  // end getCommonFields()

//==============================================================================
// getUniqueValuesForField
//
//	returns sorted unique values for the specified records and field
//	Note: treat GroupIDs special, parse the | out of the value to get the distinct values.
//
std::set<std::string /*unique-value*/> ConfigurationTree::getUniqueValuesForField(const std::vector<std::string /*relative-path*/>& recordList,
                                                                                  const std::string&                                fieldName,
                                                                                  std::string* fieldGroupIDChildLinkIndex /* =0 */) const
{
	if(fieldGroupIDChildLinkIndex)
		*fieldGroupIDChildLinkIndex = "";

	// enforce that starting point is a table node
	if(!isTableNode())
	{
		__SS__ << "Can only get getCommonFields from a table node! "
		       << "The node type is " << getNodeType() << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}

	std::set<std::string /*unique-value*/> uniqueValues;

	// for each record in <record list>
	//	emplace value at field into set
	//
	// return result

	// if no records, just return fieldGroupIDChildLinkIndex
	if(!recordList.size() && tableView_ && fieldGroupIDChildLinkIndex)
	{
		const TableViewColumnInfo& colInfo = tableView_->getColumnInfo(tableView_->findCol(fieldName));

		if(colInfo.isGroupID())
			*fieldGroupIDChildLinkIndex = colInfo.getChildLinkIndex();

	}  // end no records

	for(unsigned int i = 0; i < recordList.size(); ++i)
	{
		//__COUT__ << "Checking " << recordList[i] << __E__;

		// Note: that ConfigurationTree maps both fields associated with a link
		//	to the same node instance.
		// The behavior is likely not expected as response for this function..
		//	so for links return actual value for field name specified
		//	i.e. if Table of link is requested give that; if linkID is requested give
		// that.  use TRUE in getValueAsString for proper behavior

		ConfigurationTree node = getNode(recordList[i]).getNode(fieldName);

		if(node.isGroupIDNode())
		{
			// handle groupID node special

			//__COUT__ << "GroupID field " << fieldName << __E__;

			// first time, get field's GroupID Child Link Index, if applicable
			if(i == 0 && fieldGroupIDChildLinkIndex)
				*fieldGroupIDChildLinkIndex = node.getColumnInfo().getChildLinkIndex();

			// return set of groupIDs individually

			std::set<std::string> setOfGroupIDs = node.getSetOfGroupIDs();
			for(auto& groupID : setOfGroupIDs)
				uniqueValues.emplace(groupID);
		}
		else  // normal record, return value as string
			uniqueValues.emplace(node.getValueAsString(true));

	}  // end record loop

	return uniqueValues;
}  // end getUniqueValuesForField()

//==============================================================================
// recursiveGetCommonFields
//	wrapper is ...getCommonFields
void ConfigurationTree::recursiveGetCommonFields(std::vector<ConfigurationTree::RecordField>&      fieldCandidateList,
                                                 std::vector<int>&                                 fieldCount,
                                                 const std::vector<std::string /*relative-path*/>& fieldAcceptList,
                                                 const std::vector<std::string /*relative-path*/>& fieldRejectList,
                                                 unsigned int                                      depth,
                                                 const std::string&                                relativePathBase,
                                                 bool                                              inFirstRecord) const
{
	//__COUT__ << depth << ":relativePathBase " << relativePathBase <<
	//	" + " << inFirstRecord <<__E__;
	--depth;

	// clang-format off
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
	//				if Link Table/UID pair is not found in <field candidates list>
	//			(avoid endless loops through tree)
	//			recursiveGetCommonFields()
	//	=====================
	// clang-format on

	bool         found;                         // used in loops
	auto         tableName = getTableName();    // all fields will share this table name
	auto         uid       = getUIDAsString();  // all fields will share this uid
	unsigned int j;

	auto recordChildren = getChildren();
	for(const auto& fieldNode : recordChildren)
	{
		//__COUT__ << "All... " << fieldNode.second.getNodeType() <<
		//		" -- " << (relativePathBase + fieldNode.first) <<
		//		" + " << inFirstRecord <<__E__;

		if(fieldNode.second.isValueNode() || fieldNode.second.isGroupLinkNode())
		{
			// skip author and record insertion time
			if(fieldNode.second.isValueNode())
			{
				if(fieldNode.second.getColumnInfo().getType() == TableViewColumnInfo::TYPE_AUTHOR ||
				   fieldNode.second.getColumnInfo().getType() == TableViewColumnInfo::TYPE_TIMESTAMP)
					continue;

				//__COUT__ << "isValueNode " << fieldNode.first << __E__;
			}

			if(inFirstRecord)  // first uid record
			{
				//__COUT__ << "Checking... " << fieldNode.second.getNodeType() <<
				//		" -- " << (relativePathBase + fieldNode.first) <<
				//		"-- depth=" << depth << __E__;

				// check field accept filter list
				found = fieldAcceptList.size() ? false : true;  // accept if no filter
				                                                // list
				for(const auto& fieldFilter : fieldAcceptList)
					if(fieldFilter.find('/') != std::string::npos)
					{
						// filter is for full path, so add relative path base
						if(StringMacros::wildCardMatch(fieldFilter, relativePathBase + fieldNode.first))
						{
							found = true;
							break;
						}
					}
					else if(StringMacros::wildCardMatch(fieldFilter, fieldNode.first))
					{
						found = true;
						break;
					}

				if(found)
				{
					// check field reject filter list

					found = true;  // accept if no filter list
					for(const auto& fieldFilter : fieldRejectList)
						if(fieldFilter.find('/') != std::string::npos)
						{
							// filter is for full path, so add relative path base
							if(StringMacros::wildCardMatch(fieldFilter, relativePathBase + fieldNode.first))
							{
								found = false;  // reject if match
								break;
							}
						}
						else if(StringMacros::wildCardMatch(fieldFilter, fieldNode.first))
						{
							found = false;  // reject if match
							break;
						}
				}

				// if found, new field (since this is first record)
				if(found)
				{
					//__COUT__ << "FOUND field " <<
					//		(relativePathBase + fieldNode.first) << __E__;

					if(fieldNode.second.isGroupLinkNode())
					{
						//__COUT__ << "isGroupLinkNode " << fieldNode.first << __E__;

						// must get column info differently for group link column

						std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
						bool                                                               isGroupLink;
						tableView_->getChildLink(tableView_->findCol(fieldNode.first), isGroupLink, linkPair);

						// add both link columns

						fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
						                                                            uid,
						                                                            tableView_->getColumnInfo(linkPair.first).getName(),
						                                                            relativePathBase,  // relative path, not including columnName_
						                                                            &tableView_->getColumnInfo(linkPair.first)));
						fieldCount.push_back(1);  // init count to 1

						fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
						                                                            uid,
						                                                            tableView_->getColumnInfo(linkPair.second).getName(),
						                                                            relativePathBase,  // relative path, not including columnName_
						                                                            &tableView_->getColumnInfo(linkPair.second)));
						fieldCount.push_back(1);  // init count to 1
					}
					else  // value node
					{
						fieldCandidateList.push_back(ConfigurationTree::RecordField(tableName,
						                                                            uid,
						                                                            fieldNode.first,
						                                                            relativePathBase,  // relative path, not including columnName_
						                                                            &fieldNode.second.getColumnInfo()));
						fieldCount.push_back(1);  // init count to 1
					}
				}
			}
			else  // not first record
			{
				// if field is in <field candidates list>, increment <field count>
				//	else ignore
				for(j = 0; j < fieldCandidateList.size(); ++j)
				{
					if((relativePathBase + fieldNode.first) == (fieldCandidateList[j].relativePath_ + fieldCandidateList[j].columnName_))
					{
						//__COUT__ << "incrementing " << j <<
						//		" " << fieldCandidateList[j].relativePath_ << __E__;
						// found, so increment <field count>
						++fieldCount[j];
						if(fieldNode.second.isGroupLinkNode() && j + 1 < fieldCandidateList.size())
							++fieldCount[j + 1];  // increment associated link index too!
						break;
					}
				}
			}
		}  // end value and group link node handling
		else if(fieldNode.second.isUIDLinkNode())
		{
			//__COUT__ << "isUIDLinkNode " << (relativePathBase + fieldNode.first) <<
			//		" + " << inFirstRecord << __E__;

			if(inFirstRecord)  // first uid record
			{
				// check field accept filter list
				found = fieldAcceptList.size() ? false : true;  // accept if no filter list
				for(const auto& fieldFilter : fieldAcceptList)
					if(fieldFilter.find('/') != std::string::npos)
					{
						// filter is for full path, so add relative path base
						if(StringMacros::wildCardMatch(fieldFilter, relativePathBase + fieldNode.first))
						{
							found = true;
							break;
						}
					}
					else if(StringMacros::wildCardMatch(fieldFilter, fieldNode.first))
					{
						found = true;
						break;
					}

				if(found)
				{
					// check field reject filter list

					found = true;  // accept if no filter list
					for(const auto& fieldFilter : fieldRejectList)
						if(fieldFilter.find('/') != std::string::npos)
						{
							// filter is for full path, so add relative path base
							if(StringMacros::wildCardMatch(fieldFilter, relativePathBase + fieldNode.first))
							{
								found = false;  // reject if match
								break;
							}
						}
						else if(StringMacros::wildCardMatch(fieldFilter, fieldNode.first))
						{
							found = false;  // reject if match
							break;
						}
				}

				//__COUTV__(found);

				// if found, new field (since this is first record)
				if(found)
				{
					std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
					bool                                                               isGroupLink;

					//__COUTV__(fieldNode.first);
					tableView_->getChildLink(tableView_->findCol(fieldNode.first), isGroupLink, linkPair);

					// add both link columns

					fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
					                                                            uid,
					                                                            tableView_->getColumnInfo(linkPair.first).getName(),
					                                                            relativePathBase,  // relative path, not including columnName_
					                                                            &tableView_->getColumnInfo(linkPair.first)));
					fieldCount.push_back(1);  // init count to 1

					fieldCandidateList.push_back(ConfigurationTree::RecordField(table_->getTableName(),
					                                                            uid,
					                                                            tableView_->getColumnInfo(linkPair.second).getName(),
					                                                            relativePathBase,  // relative path, not including columnName_
					                                                            &tableView_->getColumnInfo(linkPair.second)));
					fieldCount.push_back(1);  // init count to 1
				}
			}
			else  // not first record
			{
				// if link fields (MUST BE 2) is in <field candidates list>, increment <field count>
				//	else ignore
				for(j = 0; j < fieldCandidateList.size() - 1; ++j)
				{
					if((relativePathBase + fieldNode.first) == (fieldCandidateList[j].relativePath_ + fieldCandidateList[j].columnName_))
					{
						//__COUT__ << "incrementing " << j <<
						//		" " << fieldCandidateList[j].relativePath_ << __E__;
						// found, so increment <field count>
						++fieldCount[j];
						++fieldCount[j + 1];  // increment associated link index too!
						break;
					}
				}
			}

			// if depth remaining, then follow link, recursively!
			if(depth > 0 && !fieldNode.second.isDisconnected())
				fieldNode.second.recursiveGetCommonFields(fieldCandidateList,
				                                          fieldCount,
				                                          fieldAcceptList,
				                                          fieldRejectList,
				                                          depth,
				                                          (relativePathBase + fieldNode.first) + "/",  // relativePathBase
				                                          inFirstRecord                                // continue inFirstRecord (or not) depth search
				);
		}  // end handle unique link node
	}      // end field node loop
}  // end recursiveGetCommonFields()

//==============================================================================
// getChildrenByPriority
//	returns them in order encountered in the table
//	if filterMap criteria, then rejects any that do not meet all criteria
//
//	value can be comma-separated for OR of multiple values
std::vector<std::vector<std::pair<std::string, ConfigurationTree>>> ConfigurationTree::getChildrenByPriority(
    std::map<std::string /*relative-path*/, std::string /*value*/> filterMap, bool onlyStatusTrue) const
{
	std::vector<std::vector<std::pair<std::string, ConfigurationTree>>> retVector;

	//__COUT__ << "Children of node: " << getValueAsString() << __E__;

	bool        filtering = filterMap.size();
	std::string fieldValue;

	bool createContainer;

	std::vector<std::vector<std::string>> childrenNamesByPriority = getChildrenNamesByPriority(onlyStatusTrue);

	for(auto& childNamesAtPriority : childrenNamesByPriority)
	{
		createContainer = true;

		for(auto& childName : childNamesAtPriority)
		{
			//__COUT__ << "\tChild: " << childName << __E__;

			if(filtering) // if all criteria are not met, then skip
				if(!passFilterMap(childName,filterMap)) continue;			
			
			if(createContainer)
			{
				retVector.push_back(std::vector<std::pair<std::string, ConfigurationTree>>());
				createContainer = false;
			}

			retVector[retVector.size() - 1].push_back(std::pair<std::string, ConfigurationTree>(childName, this->getNode(childName, true)));
		}  // end children within priority loop
	}      // end children by priority loop

	//__COUT__ << "Done w/Children of node: " << getValueAsString() << __E__;
	return retVector;
}  // end getChildrenByPriority()

//==============================================================================
// passFilterMap
//		returns true if childName meets all critera
bool ConfigurationTree::passFilterMap(const std::string& childName, std::map<std::string /*relative-path*/, std::string /*value*/> filterMap) const
{
	// if all criteria are not met, then skip
	bool skip = false;

	// for each filter, check value
	for(const auto& filterPair : filterMap)
	{
		std::string filterPath = childName + "/" + filterPair.first;
		__COUTV__(filterPath);

		ConfigurationTree childNode = this->getNode(filterPath);
		try
		{
			// extract field value list
			std::vector<std::string> fieldValues;
			StringMacros::getVectorFromString(filterPair.second, fieldValues, std::set<char>({','}) /*delimiters*/);

			__COUTV__(fieldValues.size());

			skip = true;
			// for each field check if any match
			for(const auto& fieldValue : fieldValues)
			{
				// Note: that ConfigurationTree maps both fields associated with a
				// link 	to the same node instance.  The behavior is likely not
				// expected as response for this function.. 	so for links
				// return
				// actual value for field name specified 	i.e. if Table of link
				// is requested give that; if linkID is requested give that.  use
				// TRUE in getValueAsString for proper behavior

				if(childNode.isGroupIDNode())
				{
					// handle groupID node special, check against set of groupIDs

					bool                  groupIdFound  = false;
					std::set<std::string> setOfGroupIDs = childNode.getSetOfGroupIDs();

					for(auto& groupID : setOfGroupIDs)
					{
						__COUT__ << "\t\tGroupID Check: " << filterPair.first << " == " << fieldValue << " => "
									<< StringMacros::decodeURIComponent(fieldValue) << " ??? " << groupID << __E__;

						if(StringMacros::wildCardMatch(StringMacros::decodeURIComponent(fieldValue), groupID))
						{
							// found a match for the field/groupId pair
							__COUT__ << "Found match" << __E__;
							groupIdFound = true;
							break;
						}
					}  // end groupID search

					if(groupIdFound)
					{
						// found a match for the field/groupId-set pair
						__COUT__ << "Found break match" << __E__;
						skip = false;
						break;
					}
				}
				else  // normal child node, check against value
				{
					__COUT__ << "\t\tCheck: " << filterPair.first << " == " << fieldValue << " => " << StringMacros::decodeURIComponent(fieldValue)
								<< " ??? " << childNode.getValueAsString(true) << __E__;

					if(StringMacros::wildCardMatch(StringMacros::decodeURIComponent(fieldValue), childNode.getValueAsString(true)))
					{
						// found a match for the field/value pair
						skip = false;
						break;
					}
				}
			}
		}
		catch(...)
		{
			__SS__ << "Failed to access filter path '" << filterPath << "' - aborting." << __E__;

			ss << nodeDump() << __E__;
			__SS_THROW__;
		}

		if(skip)
			break;  // no match for this field, so stop checking and skip this
					// record
	}
	return !skip;
} //end passFilterMap()

//==============================================================================
// getChildren
//	returns them in order encountered in the table
//	if filterMap criteria, then rejects any that do not meet all criteria
//		filterMap-value can be comma-separated for OR of multiple values
//
//	Note: filterMap is handled special for groupID fields
//		matches are considered after parsing | for set of groupIDs
//
std::vector<std::pair<std::string, ConfigurationTree>> ConfigurationTree::getChildren(std::map<std::string /*relative-path*/, std::string /*value*/> filterMap,
                                                                                      bool                                                           byPriority,
                                                                                      bool 															 onlyStatusTrue) const
{
	std::vector<std::pair<std::string, ConfigurationTree>> retVector;

	//__COUT__ << "Children of node: " << getValueAsString() << __E__;

	bool        filtering = filterMap.size();
	// bool        skip;
	std::string fieldValue;

	std::vector<std::string> childrenNames = getChildrenNames(byPriority, onlyStatusTrue);
	for(auto& childName : childrenNames)
	{
		if(filtering && // if all criteria are not met, then skip
			!passFilterMap(childName,filterMap)) continue;		

		retVector.push_back(std::pair<std::string, ConfigurationTree>(childName, this->getNode(childName, true)));
	}

	//__COUT__ << "Done w/Children of node: " << getValueAsString() << __E__;
	return retVector;
}  // end getChildren()

//==============================================================================
// getChildrenMap
//	does not return them in order encountered in the table,
//	instead, in alphabetical order of map
std::map<std::string, ConfigurationTree> ConfigurationTree::getChildrenMap(std::map<std::string /*relative-path*/, std::string /*value*/> filterMap,
																			bool onlyStatusTrue) const
{
	std::map<std::string, ConfigurationTree> retMap;

	bool        filtering = filterMap.size();

	//__COUT__ << "Children of node: " << getValueAsString() << __E__;
	std::vector<std::string> childrenNames = getChildrenNames(false /* byPriority */, onlyStatusTrue);
	for(auto& childName : childrenNames)
	{
		//__COUT__ << "\tChild: " << childName << __E__;

		// if all criteria are not met, then skip
		if(filtering && !passFilterMap(childName,filterMap)) continue;
		
		retMap.insert(std::pair<std::string, ConfigurationTree>(childName, this->getNode(childName)));
	}

	//__COUT__ << "Done w/Children of node: " << getValueAsString() << __E__;
	return retMap;
}  // end getChildrenMap()

//==============================================================================
// same as status()
bool ConfigurationTree::isEnabled(void) const
{
	if(!isUIDNode())
	{
		__SS__ << "Can not get status of '" << getValueAsString() << ".' Can only check the status of a UID/Record node!" << __E__;
		__SS_THROW__;
	}

	bool tmpStatus = true;
	try
	{
		tableView_->getValue(tmpStatus, row_, tableView_->getColStatus());
	}
	catch(const std::runtime_error& e)
	{
		//ignore error, assuming does not have a status column
		//default to enabled if no status
	}	
	return tmpStatus;
}  // end isEnabled()

//==============================================================================
bool ConfigurationTree::isStatusNode(void) const
{
	if(!isValueNode())
		return false;

	return col_ == tableView_->getColStatus();
}  // end isStatusNode()

//==============================================================================
// getChildrenNamesByPriority
//	returns them in priority order encountered in the table
std::vector<std::vector<std::string>> ConfigurationTree::getChildrenNamesByPriority(bool onlyStatusTrue) const
{
	std::vector<std::vector<std::string /*child name*/>> retVector;


	if(!tableView_)
	{
		__SS__ << "Can not get children names of '" << getValueAsString() << "' with null configuration view pointer!" << __E__;
		if(isLinkNode() && isDisconnected())
			ss << " This node is a disconnected link to " << getDisconnectedTableName() << __E__;

		ss << nodeDump() << __E__;
		__SS_ONLY_THROW__;
	}

	if(row_ == TableView::INVALID && col_ == TableView::INVALID)
	{
		// this node is table node
		// so return all uid node strings that match groupId

		// bool tmpStatus;

		std::vector<std::vector<unsigned int /*group row*/>> groupRowsByPriority = tableView_->getGroupRowsByPriority(
		    groupId_ == "" ? TableView::INVALID :  // if no group ID, take all rows and ignore column, do not attempt link lookup
		        tableView_->getLinkGroupIDColumn(childLinkIndex_),
		    groupId_,
		    onlyStatusTrue);

		// now build vector of vector names by priority
		for(const auto& priorityChildRowVector : groupRowsByPriority)
		{
			retVector.push_back(std::vector<std::string /*child name*/>());
			for(const auto& priorityChildRow : priorityChildRowVector)
				retVector[retVector.size() - 1].push_back(tableView_->getDataView()[priorityChildRow][tableView_->getColUID()]);
		}
	}
	else if(row_ == TableView::INVALID)
	{
		__SS__ << "Malformed ConfigurationTree" << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
	else if(col_ == TableView::INVALID)
	{
		// this node is uid node
		// so return all link and value nodes

		for(unsigned int c = 0; c < tableView_->getNumberOfColumns(); ++c)
			if(c == tableView_->getColUID() ||  // skip UID and linkID columns (only show
			                                    // link column, to avoid duplicates)
			   tableView_->getColumnInfo(c).isChildLinkGroupID() || tableView_->getColumnInfo(c).isChildLinkUID())
				continue;
			else
			{
				retVector.push_back(std::vector<std::string /*child name*/>());
				retVector[retVector.size() - 1].push_back(tableView_->getColumnInfo(c).getName());
			}
	}
	else  // this node is value node, so has no node to choose from
	{
		// this node is value node, cant go any deeper!
		__SS__ << "\n\nError occurred looking for children of nodeName=" << getValueName() << "\n\n"
		       << "Invalid depth! getChildrenValues() called from a value point in the "
		          "Configuration Tree."
		       << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}

	return retVector;
}  // end getChildrenNamesByPriority()

//==============================================================================
// getChildrenNames
//	returns them in order encountered in the table
std::vector<std::string> ConfigurationTree::getChildrenNames(bool byPriority, bool onlyStatusTrue) const
{
	std::vector<std::string /*child name*/> retVector;

	if (isRootNode())
	{
		for(auto& configPair : configMgr_->getActiveVersions())
		{
			//__GEN_COUT__ << configPair.first <<  " " << (int)(configPair.second?1:0) <<
			// __E__;			
			retVector.push_back(configPair.first);
		}
		return retVector;
	}

	if(!tableView_)
	{
		__SS__ << "Can not get children names of '" << getFieldName() << ":" << getValueAsString() << "' with null configuration view pointer!" << __E__;
		if(isLinkNode() && isDisconnected())
			ss << " This node is a disconnected link to " << getDisconnectedTableName() << "(" << getDisconnectedLinkID() << ")" << __E__;
		__SS_ONLY_THROW__;
	}

	if(row_ == TableView::INVALID && col_ == TableView::INVALID)
	{
		// this node is table node
		// so return all uid node strings that match groupId
		std::vector<unsigned int /*group row*/> groupRows =
		    tableView_->getGroupRows((groupId_ == "" ? TableView::INVALID :  // if no group ID, take all rows, do not attempt link lookup
		                                  tableView_->getLinkGroupIDColumn(childLinkIndex_)),
		                             groupId_,
		                             onlyStatusTrue,
		                             byPriority);

		// now build vector of vector names by priority
		for(const auto& groupRow : groupRows)
			retVector.push_back(tableView_->getDataView()[groupRow][tableView_->getColUID()]);

		//		bool tmpStatus;
		//
		//		if(byPriority)  // reshuffle by priority
		//		{
		//			try
		//			{
		//				std::map<uint64_t /*priority*/, std::vector<unsigned int /*child row*/>> orderedByPriority;
		//				std::vector<std::string /*child name*/>                                  retPrioritySet;
		//
		//				unsigned int col = tableView_->getColPriority();
		//
		//				uint64_t tmpPriority;
		//
		//				for(unsigned int r = 0; r < tableView_->getNumberOfRows(); ++r)
		//					if(groupId_ == "" || tableView_->isEntryInGroup(r, childLinkIndex_, groupId_))
		//					{
		//						// check status if needed
		//						if(onlyStatusTrue)
		//						{
		//							tableView_->getValue(tmpStatus, r, tableView_->getColStatus());
		//
		//							if(!tmpStatus)
		//								continue;  // skip those with status false
		//						}
		//
		//						tableView_->getValue(tmpPriority, r, col);
		//						// do not accept DEFAULT value of 0.. convert to 100
		//						orderedByPriority[tmpPriority ? tmpPriority : 100].push_back(r);
		//					}
		//
		//				// at this point have priority map
		//				// now build return vector
		//
		//				for(const auto& priorityChildRowVector : orderedByPriority)
		//					for(const auto& priorityChildRow : priorityChildRowVector.second)
		//						retVector.push_back(tableView_->getDataView()[priorityChildRow][tableView_->getColUID()]);
		//
		//				__COUT__ << "Returning priority children list." << __E__;
		//				return retVector;
		//			}
		//			catch(std::runtime_error& e)
		//			{
		//				__COUT_WARN__ << "Priority configuration not found. Assuming all "
		//						"children have equal priority. "
		//						<< __E__;
		//				retVector.clear();
		//			}
		//		}
		//		// else not by priority
		//
		//		for(unsigned int r = 0; r < tableView_->getNumberOfRows(); ++r)
		//			if(groupId_ == "" || tableView_->isEntryInGroup(r, childLinkIndex_, groupId_))
		//			{
		//				// check status if needed
		//				if(onlyStatusTrue)
		//				{
		//					tableView_->getValue(tmpStatus, r, tableView_->getColStatus());
		//
		//					if(!tmpStatus)
		//						continue;  // skip those with status false
		//				}
		//
		//				retVector.push_back(tableView_->getDataView()[r][tableView_->getColUID()]);
		//			}
	}
	else if(row_ == TableView::INVALID)
	{
		__SS__ << "Malformed ConfigurationTree" << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}
	else if(col_ == TableView::INVALID)
	{
		// this node is uid node
		// so return all link and value nodes

		for(unsigned int c = 0; c < tableView_->getNumberOfColumns(); ++c)
			if(c == tableView_->getColUID() ||  // skip UID and linkID columns (only show
			                                    // link column, to avoid duplicates)
			   tableView_->getColumnInfo(c).isChildLinkGroupID() || tableView_->getColumnInfo(c).isChildLinkUID())
				continue;
			else
				retVector.push_back(tableView_->getColumnInfo(c).getName());
	}
	else  // this node is value node, so has no node to choose from
	{
		// this node is value node, cant go any deeper!
		__SS__ << "\n\nError occurred looking for children of nodeName=" << getValueName() << "\n\n"
		       << "Invalid depth! getChildrenValues() called from a value point in the "
		          "Configuration Tree."
		       << __E__;

		ss << nodeDump() << __E__;
		__SS_THROW__;
	}

	return retVector;
}  // end getChildrenNames()

//==============================================================================
// getValueAsTreeNode
//	returns tree node for value of this node, treating the value
//		as a string for the absolute path string from root of tree
ConfigurationTree ConfigurationTree::getValueAsTreeNode(void) const
{
	// check if first character is a /, .. if so try to get value in tree
	//	if exception, just take value
	// note: this call will throw an error, in effect, if not a "value" node
	if(!tableView_)
	{
		__SS__ << "Invalid node for get value." << __E__;
		__SS_THROW__;
	}

	std::string valueString = tableView_->getValueAsString(row_, col_, true /* convertEnvironmentVariables */);
	//__COUT__ << valueString << __E__;
	if(valueString.size() && valueString[0] == '/')
	{
		//__COUT__ << "Starts with '/' - check if valid tree path: " << valueString <<
		// __E__;
		try
		{
			ConfigurationTree retNode = configMgr_->getNode(valueString);
			__COUT__ << "Found a valid tree path in value!" << __E__;
			return retNode;
		}
		catch(...)
		{
			__SS__ << "Invalid tree path." << __E__;
			__SS_ONLY_THROW__;
		}
	}

	{
		__SS__ << "Invalid value string '" << valueString << "' - must start with a '/' character." << __E__;
		__SS_ONLY_THROW__;
	}
}  // end getValueAsTreeNode()
