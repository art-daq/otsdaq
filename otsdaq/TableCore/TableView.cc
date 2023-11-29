#include "otsdaq/TableCore/TableView.h"
#include "otsdaq/TableCore/TableBase.h"

#include <cstdlib>
#include <iostream>
#include <regex>
#include <sstream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "TableView"
#undef __COUT_HDR__
#define __COUT_HDR__ (tableName_ + "v" + version_.toString() + "\t<> ")

const unsigned int TableView::INVALID = -1;

//==============================================================================
TableView::TableView(const std::string& tableName)
    : storageData_(tableName)  // hijack momentarily for convert to caps
    , tableName_(TableBase::convertToCaps(storageData_))
    , version_(TableVersion::INVALID)
    , comment_("")
    , author_("")
    , creationTime_(time(0))
    , lastAccessTime_(0)
    , colUID_(INVALID)
    , colStatus_(INVALID)
    , colPriority_(INVALID)
    , fillWithLooseColumnMatching_(false)
    , getSourceRawData_(false)
    , sourceColumnMismatchCount_(0)
    , sourceColumnMissingCount_(0)
{
	storageData_ = "";  // unhijack

	if(tableName == "")
	{
		__SS__ << "Do not allow anonymous table view construction!" << __E__;
		ss << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}

}  // end constructor

//==============================================================================
TableView::~TableView(void) {}

//==============================================================================
// operator=
//	Do NOT allow!... use TableView::copy
//	copy is used to maintain consistency with version, creationTime, lastAccessTime, etc)
TableView& TableView::operator=(const TableView /*src*/)
{
	__SS__ << "Invalid use of operator=... Should not directly copy a TableView. Please "
	          "use TableView::copy(sourceView,author,comment)";
	ss << StringMacros::stackTrace() << __E__;

	__COUT__ << ss.str() << __E__;
	exit(0);
	__SS_THROW__;
}

//==============================================================================
TableView& TableView::copy(const TableView& src, TableVersion destinationVersion, const std::string& author)
{
	// tableName_ = src.tableName_;
	version_ = destinationVersion;
	comment_ = src.comment_;
	author_  = author;  // take new author
	// creationTime_ 	= time(0); //don't change creation time
	lastAccessTime_ = time(0);

	// can not use operator= for TableViewColumn (it is a const class)
	// columnsInfo_ = src.columnsInfo_;
	columnsInfo_.clear();
	for(auto& c : src.columnsInfo_)
		columnsInfo_.push_back(c);

	theDataView_       = src.theDataView_;
	sourceColumnNames_ = src.sourceColumnNames_;

	// RAR remove init() check, because usually copy() is only the first step
	//	in a series of changes that result in another call to init()
	// init();  // verify consistency

	std::string tmpCachePrepend = TableBase::GROUP_CACHE_PREPEND;
	tmpCachePrepend = TableBase::convertToCaps(tmpCachePrepend);

	//if special GROUP CACHE table, handle construction in a special way
	if(tableName_.substr(0,tmpCachePrepend.length()) == tmpCachePrepend)
	{
		__COUT__ << "TableView copy for '" << tableName_ << "' done." << __E__;
		return *this;
	} //end special GROUP CACHE table construction


	initColUID();  // setup UID column
	initRowDefaults();
	try
	{
		initColStatus();  // setup Status column
	}
	catch(...)
	{
	}  // ignore no Status column
	try
	{
		initColPriority();  // setup Priority column
	}
	catch(...)
	{
	}  // ignore no Priority column

	return *this;
}  // end copy()

//==============================================================================
// copyRows
//	return row offset of first row copied in
unsigned int TableView::copyRows(const std::string& author,
                                 const TableView&   src,
                                 unsigned int       srcOffsetRow /* = 0 */,
                                 unsigned int       srcRowsToCopy /* = -1 */,
                                 unsigned int       destOffsetRow /* = -1 */,
                                 unsigned char      generateUniqueDataColumns /* = false */,
                                 const std::string& baseNameAutoUID /*= "" */)
{
	//__COUTV__(destOffsetRow);
	//__COUTV__(srcOffsetRow);
	//__COUTV__(srcRowsToCopy);

	unsigned int retRow = (unsigned int)-1;

	// check that column sizes match
	if(src.getNumberOfColumns() != getNumberOfColumns())
	{
		__SS__ << "Error! Number of Columns of source view must match destination view."
		       << "Dimension of source is [" << src.getNumberOfColumns() << "] and of destination is [" << getNumberOfColumns() << "]." << __E__;
		__SS_THROW__;
	}

	unsigned int srcRows = src.getNumberOfRows();

	for(unsigned int r = 0; r < srcRowsToCopy; ++r)
	{
		if(r + srcOffsetRow >= srcRows)
			break;  // end when no more source rows to copy (past bounds)

		destOffsetRow = addRow(author,
		                       generateUniqueDataColumns /*incrementUniqueData*/,
		                       baseNameAutoUID /*baseNameAutoUID*/,
		                       destOffsetRow);  // add and get row created

		if(retRow == (unsigned int)-1)
			retRow = destOffsetRow;  // save row of first copied entry

		// copy data
		for(unsigned int col = 0; col < getNumberOfColumns(); ++col)
			if(generateUniqueDataColumns &&
			   (columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_UID || columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_UNIQUE_DATA ||
			    columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_UNIQUE_GROUP_DATA))
				continue;  // if leaving unique data, then skip copy
			else
				theDataView_[destOffsetRow][col] = src.theDataView_[r + srcOffsetRow][col];

		// prepare for next row
		++destOffsetRow;
	}

	return retRow;
}  // end copyRows()

//==============================================================================
// init
//	Should be called after table is filled to setup special members
//		and verify consistency.
//	e.g. identifying the UID column, checking unique data fields, etc.
//
// 	Note: this function also sanitizes yes/no, on/off, and true/false types
void TableView::init(void)
{
	//__COUT__ << "Starting table verification..." << StringMacros::stackTrace() << __E__;

	try
	{
		// verify column names are unique
		//	make set of names,.. and CommentDescription == COMMENT
		std::set<std::string> colNameSet;
		std::string           capsColName, colName;
		for(auto& colInfo : columnsInfo_)
		{
			colName = colInfo.getStorageName();
			if(colName == "COMMENT_DESCRIPTION")
				colName = "COMMENT";
			capsColName = "";
			for(unsigned int i = 0; i < colName.size(); ++i)
			{
				if(colName[i] == '_')
					continue;
				capsColName += colName[i];
			}

			colNameSet.emplace(capsColName);
		}

		if(colNameSet.size() != columnsInfo_.size())
		{
			__SS__ << "Table Error:\t"
			       << " Columns names must be unique! There are " << columnsInfo_.size() << " columns and the unique name count is " << colNameSet.size()
			       << __E__;
			__SS_THROW__;
		}

		initColUID();  // setup UID column
		try
		{
			initColStatus();  // setup Status column
		}
		catch(...)
		{
		}  // ignore no Status column
		try
		{
			initColPriority();  // setup Priority column
		}
		catch(...)
		{
		}  // ignore no Priority column

		// fix source columns if not already populated
		if(sourceColumnNames_.size() == 0)  // setup sourceColumnNames_ to be correct
			for(unsigned int i = 0; i < getNumberOfColumns(); ++i)
				sourceColumnNames_.emplace(getColumnsInfo()[i].getStorageName());

		// require one comment column
		unsigned int colPos;
		if((colPos = findColByType(TableViewColumnInfo::TYPE_COMMENT)) != INVALID)
		{
			if(columnsInfo_[colPos].getName() != TableViewColumnInfo::COL_NAME_COMMENT)
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_COMMENT << " data type column must have name=" << TableViewColumnInfo::COL_NAME_COMMENT
				       << __E__;
				__SS_THROW__;
			}

			if(findColByType(TableViewColumnInfo::TYPE_COMMENT, colPos + 1) != INVALID)  // found two!
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_COMMENT << " data type in column " << columnsInfo_[colPos].getName()
				       << " is repeated. This is not allowed." << __E__;
				__SS_THROW__;
			}

			if(colPos != getNumberOfColumns() - 3)
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_COMMENT << " data type column must be 3rd to last (in column "
				       << getNumberOfColumns() - 3 << ")." << __E__;
				__SS_THROW__;
			}
		}
		else
		{
			__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_COMMENT << " data type column "
			       << " is missing. This is not allowed." << __E__;
			__SS_THROW__;
		}

		// require one author column
		if((colPos = findColByType(TableViewColumnInfo::TYPE_AUTHOR)) != INVALID)
		{
			if(findColByType(TableViewColumnInfo::TYPE_AUTHOR, colPos + 1) != INVALID)  // found two!
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_AUTHOR << " data type in column " << columnsInfo_[colPos].getName()
				       << " is repeated. This is not allowed." << __E__;
				__SS_THROW__;
			}

			if(colPos != getNumberOfColumns() - 2)
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_AUTHOR << " data type column must be 2nd to last (in column "
				       << getNumberOfColumns() - 2 << ")." << __E__;
				__SS_THROW__;
			}
		}
		else
		{
			__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_AUTHOR << " data type column "
			       << " is missing. This is not allowed." << __E__;
			__SS_THROW__;
		}

		// require one timestamp column
		if((colPos = findColByType(TableViewColumnInfo::TYPE_TIMESTAMP)) != INVALID)
		{
			if(findColByType(TableViewColumnInfo::TYPE_TIMESTAMP, colPos + 1) != INVALID)  // found two!
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_TIMESTAMP << " data type in column " << columnsInfo_[colPos].getName()
				       << " is repeated. This is not allowed." << __E__;
				__SS_THROW__;
			}

			if(colPos != getNumberOfColumns() - 1)
			{
				__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_TIMESTAMP << " data type column must be last (in column " << getNumberOfColumns() - 1
				       << ")." << __E__;
				__COUT_ERR__ << "\n" << ss.str();
				__SS_THROW__;
			}
		}
		else
		{
			__SS__ << "Table Error:\t" << TableViewColumnInfo::TYPE_TIMESTAMP << " data type column "
			       << " is missing. This is not allowed." << __E__;
			__SS_THROW__;
		}

		// check that UID is really unique ID (no repeats)
		// and ... allow letters, numbers, dash, underscore
		// and ... force size 1
		std::set<std::string /*uid*/> uidSet;
		for(unsigned int row = 0; row < getNumberOfRows(); ++row)
		{
			if(uidSet.find(theDataView_[row][colUID_]) != uidSet.end())
			{
				__SS__ << ("Entries in UID are not unique. Specifically at row=" + std::to_string(row) + " value=" + theDataView_[row][colUID_]) << __E__;
				__SS_THROW__;
			}

			if(theDataView_[row][colUID_].size() == 0)
			{
				__SS__ << "An invalid UID '" << theDataView_[row][colUID_] << "' "
				       << " was identified. UIDs must contain at least 1 character." << __E__;
				__SS_THROW__;
			}

			for(unsigned int i = 0; i < theDataView_[row][colUID_].size(); ++i)
				if(!((theDataView_[row][colUID_][i] >= 'A' && theDataView_[row][colUID_][i] <= 'Z') ||
				     (theDataView_[row][colUID_][i] >= 'a' && theDataView_[row][colUID_][i] <= 'z') ||
				     (theDataView_[row][colUID_][i] >= '0' && theDataView_[row][colUID_][i] <= '9') ||
				     (theDataView_[row][colUID_][i] == '-' || theDataView_[row][colUID_][i] == '_')))
				{
					__SS__ << "An invalid UID '" << theDataView_[row][colUID_] << "' "
					       << " was identified. UIDs must contain only letters, numbers,"
					       << "dashes, and underscores." << __E__;
					__SS_THROW__;
				}

			uidSet.insert(theDataView_[row][colUID_]);
		}
		if(uidSet.size() != getNumberOfRows())
		{
			__SS__ << "Entries in UID are not unique!"
			       << "There are " << getNumberOfRows() << " records and the unique UID count is " << uidSet.size() << __E__;
			__SS_THROW__;
		}

		// check that any TYPE_UNIQUE_DATA columns are really unique (no repeats)
		colPos = (unsigned int)-1;
		while((colPos = findColByType(TableViewColumnInfo::TYPE_UNIQUE_DATA, colPos + 1)) != INVALID)
		{
			std::set<std::string /*unique data*/> uDataSet;
			for(unsigned int row = 0; row < getNumberOfRows(); ++row)
			{
				if(uDataSet.find(theDataView_[row][colPos]) != uDataSet.end())
				{
					__SS__ << "Entries in Unique Data column " << columnsInfo_[colPos].getName()
					       << (" are not unique. Specifically at row=" + std::to_string(row) + " value=" + theDataView_[row][colPos]) << __E__;
					__SS_THROW__;
				}
				uDataSet.insert(theDataView_[row][colPos]);
			}
			if(uDataSet.size() != getNumberOfRows())
			{
				__SS__ << "Entries in  Unique Data column " << columnsInfo_[colPos].getName() << " are not unique!"
				       << "There are " << getNumberOfRows() << " records and the unique data count is " << uDataSet.size() << __E__;
				__SS_THROW__;
			}
		}

		// check that any TYPE_UNIQUE_GROUP_DATA columns are really unique fpr groups (no
		// repeats)
		colPos = (unsigned int)-1;
		while((colPos = findColByType(TableViewColumnInfo::TYPE_UNIQUE_GROUP_DATA, colPos + 1)) != INVALID)
		{
			// colPos is a unique group data column
			// now, for each groupId column
			//	check that data is unique for all groups
			for(unsigned int groupIdColPos = 0; groupIdColPos < columnsInfo_.size(); ++groupIdColPos)
				if(columnsInfo_[groupIdColPos].isGroupID())
				{
					std::map<std::string /*group name*/, std::pair<unsigned int /*memberCount*/, std::set<std::string /*unique data*/>>> uGroupDataSets;

					for(unsigned int row = 0; row < getNumberOfRows(); ++row)
					{
						auto groupIds = getSetOfGroupIDs(groupIdColPos, row);

						for(const auto& groupId : groupIds)
						{
							uGroupDataSets[groupId].first++;  // add to member count

							if(uGroupDataSets[groupId].second.find(theDataView_[row][colPos]) != uGroupDataSets[groupId].second.end())
							{
								__SS__ << "Entries in Unique Group Data column " << colPos << ":" << columnsInfo_[colPos].getName()
								       << " are not unique for group ID '" << groupId << ".' Specifically at row=" << std::to_string(row)
								       << " value=" << theDataView_[row][colPos] << __E__;
								__SS_THROW__;
							}
							uGroupDataSets[groupId].second.insert(theDataView_[row][colPos]);
						}
					}

					for(const auto& groupPair : uGroupDataSets)
						if(uGroupDataSets[groupPair.first].second.size() != uGroupDataSets[groupPair.first].first)
						{
							__SS__ << "Entries in  Unique Data column " << columnsInfo_[colPos].getName() << " are not unique for group '" << groupPair.first
							       << "!'"
							       << "There are " << uGroupDataSets[groupPair.first].first << " records and the unique data count is "
							       << uGroupDataSets[groupPair.first].second.size() << __E__;
							__SS_THROW__;
						}
				}
		}  // end TYPE_UNIQUE_GROUP_DATA check

		auto rowDefaults = initRowDefaults();  // getDefaultRowValues();

		// check that column types are well behaved
		//	- check that fixed choice data is one of choices
		//	- sanitize booleans
		//	- check that child link I are unique
		//		note: childLinkId refers to childLinkGroupIDs AND childLinkUIDs
		std::set<std::string>                                              groupIdIndexes, childLinkIndexes, childLinkIdLabels;
		unsigned int                                                       groupIdIndexesCount = 0, childLinkIndexesCount = 0, childLinkIdLabelsCount = 0;
		bool                                                               tmpIsGroup;
		std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> tmpLinkPair;

		// check sanity of data view rows x cols (have seen weird out-of-range crashes)
		if(getNumberOfRows() != theDataView_.size())
		{
			__SS__ << "Impossible row mismatch " << getNumberOfRows() << " vs " << theDataView_.size() << "! How did you get here?" << __E__;
			__SS_THROW__;
		}
		for(unsigned int row = 0; row < getNumberOfRows(); ++row)
			if(getNumberOfColumns() != theDataView_[row].size())
			{
				__SS__ << "Impossible col mismatch " << getNumberOfColumns() << " vs [" << row << "]" << theDataView_[row].size() << "! How did you get here?"
				       << __E__;
				__SS_THROW__;
			}
		if(getNumberOfColumns() != columnsInfo_.size())
		{
			__SS__ << "Impossible col info mismatch " << getNumberOfColumns() << " vs " << columnsInfo_.size() << "! How did you get here?" << __E__;
			__SS_THROW__;
		}
		if(getNumberOfColumns() != rowDefaults.size())
		{
			__SS__ << "Impossible col default mismatch " << getNumberOfColumns() << " vs " << rowDefaults.size() << "! How did you get here?" << __E__;
			__SS_THROW__;
		}

		for(unsigned int col = 0; col < getNumberOfColumns(); ++col)
		{
			if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_FIXED_CHOICE_DATA)
			{
				const std::vector<std::string>& theDataChoices = columnsInfo_[col].getDataChoices();

				// check if arbitrary values allowed
				if(theDataChoices.size() && theDataChoices[0] == "arbitraryBool=1")
					continue;  // arbitrary values allowed

				bool found;
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
				{
					found = false;
					// check against default value first
					if(theDataView_[row][col] == rowDefaults[col])
						continue;  // default is always ok

					for(const auto& choice : theDataChoices)
					{
						if(theDataView_[row][col] == choice)
						{
							found = true;
							break;
						}
					}
					if(!found)
					{
						__SS__ << getTableName() << " Error:\t'" << theDataView_[row][col] << "' in column " << columnsInfo_[col].getName()
						       << " is not a valid Fixed Choice option. "
						       << "Possible values are as follows: ";

						for(unsigned int i = 0; i < columnsInfo_[col].getDataChoices().size(); ++i)
						{
							if(i)
								ss << ", ";
							ss << columnsInfo_[col].getDataChoices()[i];
						}
						ss << "." << __E__;
						__SS_THROW__;
					}
				}
			}
			else if(columnsInfo_[col].isChildLink())
			{
				// check if forcing fixed choices

				const std::vector<std::string>& theDataChoices = columnsInfo_[col].getDataChoices();

				// check if arbitrary values allowed
				if(!theDataChoices.size() || theDataChoices[0] == "arbitraryBool=1")
					continue;  // arbitrary values allowed

				// skip one if arbitrary setting is embedded as first value
				bool skipOne = (theDataChoices.size() && theDataChoices[0] == "arbitraryBool=0");
				bool hasSkipped;

				bool found;
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
				{
					found = false;

					hasSkipped = false;
					for(const auto& choice : theDataChoices)
					{
						if(skipOne && !hasSkipped)
						{
							hasSkipped = true;
							continue;
						}

						if(theDataView_[row][col] == choice)
						{
							found = true;
							break;
						}
					}
					if(!found)
					{
						__SS__ << getTableName() << " Error:\t the value '" << theDataView_[row][col] << "' in column " << columnsInfo_[col].getName()
						       << " is not a valid Fixed Choice option. "
						       << "Possible values are as follows: ";

						// ss <<
						// StringMacros::vectorToString(columnsInfo_[col].getDataChoices())
						// << __E__;
						for(unsigned int i = skipOne ? 1 : 0; i < columnsInfo_[col].getDataChoices().size(); ++i)
						{
							if(i > (skipOne ? 1 : 0))
								ss << ", ";
							ss << columnsInfo_[col].getDataChoices()[i];
						}
						ss << "." << __E__;
						__SS_THROW__;
					}
				}
			}
			else if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_ON_OFF)
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
				{
					if(theDataView_[row][col] == "1" || theDataView_[row][col] == "on" || theDataView_[row][col] == "On" || theDataView_[row][col] == "ON")
						theDataView_[row][col] = TableViewColumnInfo::TYPE_VALUE_ON;
					else if(theDataView_[row][col] == "0" || theDataView_[row][col] == "off" || theDataView_[row][col] == "Off" ||
					        theDataView_[row][col] == "OFF")
						theDataView_[row][col] = TableViewColumnInfo::TYPE_VALUE_OFF;
					else
					{
						__SS__ << getTableName() << " Error:\t the value '" << theDataView_[row][col] << "' in column " << columnsInfo_[col].getName()
						       << " is not a valid Type (On/Off) std::string. Possible "
						          "values are 1, on, On, ON, 0, off, Off, OFF."
						       << __E__;
						__SS_THROW__;
					}
				}
			else if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_TRUE_FALSE)
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
				{
					if(theDataView_[row][col] == "1" || theDataView_[row][col] == "true" || theDataView_[row][col] == "True" ||
					   theDataView_[row][col] == "TRUE")
						theDataView_[row][col] = TableViewColumnInfo::TYPE_VALUE_TRUE;
					else if(theDataView_[row][col] == "0" || theDataView_[row][col] == "false" || theDataView_[row][col] == "False" ||
					        theDataView_[row][col] == "FALSE")
						theDataView_[row][col] = TableViewColumnInfo::TYPE_VALUE_FALSE;
					else
					{
						__SS__ << getTableName() << " Error:\t the value '" << theDataView_[row][col] << "' in column " << columnsInfo_[col].getName()
						       << " is not a valid Type (True/False) std::string. "
						          "Possible values are 1, true, True, TRUE, 0, false, "
						          "False, FALSE."
						       << __E__;
						__SS_THROW__;
					}
				}
			else if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_YES_NO)
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
				{
					if(theDataView_[row][col] == "1" || theDataView_[row][col] == "yes" || theDataView_[row][col] == "Yes" || theDataView_[row][col] == "YES")
						theDataView_[row][col] = TableViewColumnInfo::TYPE_VALUE_YES;
					else if(theDataView_[row][col] == "0" || theDataView_[row][col] == "no" || theDataView_[row][col] == "No" || theDataView_[row][col] == "NO")
						theDataView_[row][col] = TableViewColumnInfo::TYPE_VALUE_NO;
					else
					{
						__SS__ << getTableName() << " Error:\t the value '" << theDataView_[row][col] << "' in column " << columnsInfo_[col].getName()
						       << " is not a valid Type (Yes/No) std::string. Possible "
						          "values are 1, yes, Yes, YES, 0, no, No, NO."
						       << __E__;
						__SS_THROW__;
					}
				}
			else if(columnsInfo_[col].isGroupID())  // GroupID type
			{
				colLinkGroupIDs_[columnsInfo_[col].getChildLinkIndex()] = col;  // add to groupid map
				// check uniqueness
				groupIdIndexes.emplace(columnsInfo_[col].getChildLinkIndex());
				++groupIdIndexesCount;
			}
			else if(columnsInfo_[col].isChildLink())  // Child Link type
			{
				// sanitize no link to default
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
					if(theDataView_[row][col] == "NoLink" || theDataView_[row][col] == "No_Link" || theDataView_[row][col] == "NOLINK" ||
					   theDataView_[row][col] == "NO_LINK" || theDataView_[row][col] == "Nolink" || theDataView_[row][col] == "nolink" ||
					   theDataView_[row][col] == "noLink")
						theDataView_[row][col] = TableViewColumnInfo::DATATYPE_LINK_DEFAULT;

				// check uniqueness
				childLinkIndexes.emplace(columnsInfo_[col].getChildLinkIndex());
				++childLinkIndexesCount;

				// force data type to TableViewColumnInfo::DATATYPE_STRING
				if(columnsInfo_[col].getDataType() != TableViewColumnInfo::DATATYPE_STRING)
				{
					__SS__ << getTableName() << " Error:\t"
					       << "Column " << col << " with name '" << columnsInfo_[col].getName() << "' is a Child Link column and has an illegal data type of '"
					       << columnsInfo_[col].getDataType() << "'. The data type for Child Link columns must be " << TableViewColumnInfo::DATATYPE_STRING
					       << __E__;
					__SS_THROW__;
				}

				// check for link mate (i.e. every child link needs link ID)
				getChildLink(col, tmpIsGroup, tmpLinkPair);
			}
			else if(columnsInfo_[col].isChildLinkUID() ||  // Child Link ID type
			        columnsInfo_[col].isChildLinkGroupID())
			{
				// check uniqueness
				childLinkIdLabels.emplace(columnsInfo_[col].getChildLinkIndex());
				++childLinkIdLabelsCount;

				// check that the Link ID is not empty, and force to default
				for(unsigned int row = 0; row < getNumberOfRows(); ++row)
					if(theDataView_[row][col] == "")
						theDataView_[row][col] = rowDefaults[col];

				// check for link mate (i.e. every child link needs link ID)
				getChildLink(col, tmpIsGroup, tmpLinkPair);
			}

			// check if number exist and then if it is limited by min and max, use functions in here to get values different than stof
			if(columnsInfo_[col].isNumberDataType())
			{
				std::string minimumValueString = columnsInfo_[col].getMinValue();
				std::string maximumValueString = columnsInfo_[col].getMaxValue();
				double      minimumValue, maximumValue, valueFromTable;
				bool        minExists = false, maxExists = false;

				if(!minimumValueString.empty())
				{
					minExists = StringMacros::getNumber(StringMacros::convertEnvironmentVariables(minimumValueString), minimumValue);
					if(!minExists)
					{
						__SS__ << "Inavlid user spec'd min value '" << minimumValueString
						       << "' which is not a valid number. The minimum value must be a number (environment variables and math operations are allowed)."
						       << __E__;
						__SS_THROW__;
					}
				}

				if(!maximumValueString.empty())
				{
					maxExists = StringMacros::getNumber(StringMacros::convertEnvironmentVariables(maximumValueString), maximumValue);
					if(!maxExists)
					{
						__SS__ << "Inavlid user spec'd max value '" << maximumValueString
						       << "' which is not a valid number. The maximum value must be a number (environment variables and math operations are allowed)."
						       << __E__;
						__SS_THROW__;
					}
				}

				if(minExists && maxExists && minimumValue > maximumValue)
				{
					__SS__ << "Minimum value is greater than maximum, check table editor to change this" << __E__;
					__SS_THROW__;
				}

				if(minExists || maxExists)
					for(unsigned int row = 0; row < getNumberOfRows(); ++row)
					{
						getValue(valueFromTable, row, col);
						if(minExists && valueFromTable < minimumValue)
						{
							__SS__ << "The value '" << valueFromTable << "' at [row,col]=[" << row << "," << col
							       << "] is outside the established limits: " << valueFromTable << " is lower than the specified minimum " << minimumValue
							       << "." << __E__;
							__SS_THROW__;
						}
						if(maxExists && valueFromTable > maximumValue)
						{
							__SS__ << "This value '" << valueFromTable << "' at [row,col]=[" << row << "," << col
							       << "] is outside the established limits: " << valueFromTable << " is greater than the specified maximum " << maximumValue
							       << "." << __E__;
							__SS_THROW__;
						}
					}
			}  // end handling NUMBER data types
		}      // end column loop

		// verify child link index uniqueness
		if(groupIdIndexes.size() != groupIdIndexesCount)
		{
			__SS__ << ("GroupId Labels are not unique!") << "There are " << groupIdIndexesCount << " GroupId Labels and the unique count is "
			       << groupIdIndexes.size() << __E__;
			__SS_THROW__;
		}
		if(childLinkIndexes.size() != childLinkIndexesCount)
		{
			__SS__ << ("Child Link Labels are not unique!") << "There are " << childLinkIndexesCount << " Child Link Labels and the unique count is "
			       << childLinkIndexes.size() << __E__;
			__SS_THROW__;
		}
		if(childLinkIdLabels.size() != childLinkIdLabelsCount)
		{
			__SS__ << ("Child Link ID Labels are not unique!") << "There are " << childLinkIdLabelsCount << " Child Link ID Labels and the unique count is "
			       << childLinkIdLabels.size() << __E__;
			__SS_THROW__;
		}
	}
	catch(...)
	{
		__COUT__ << "Error occured in TableView::init() for version=" << version_ << __E__;
		throw;
	}
}  // end init()

//==============================================================================
// getValue
//	string version
//	Note: necessary because types of std::basic_string<char> cause compiler problems if no
// string specific function
void TableView::getValue(std::string& value, unsigned int row, unsigned int col, bool doConvertEnvironmentVariables) const
{
	if(!(row < getNumberOfRows() && col < theDataView_[row].size()))
	{
		__SS__ << "Invalid row col requested " << row << "," << col << " vs " << getNumberOfRows() << "," << columnsInfo_.size() << "/"
		       << theDataView_[row].size() << __E__;
		__SS_THROW__;
	}

	value = validateValueForColumn(theDataView_[row][col], col, doConvertEnvironmentVariables);
}  // end getValue()

//==============================================================================
// validateValueForColumn
//	string version
//	Note: necessary because types of std::basic_string<char>
//	cause compiler problems if no string specific function
std::string TableView::validateValueForColumn(const std::string& value, unsigned int col, bool doConvertEnvironmentVariables) const
{
	if(col >= columnsInfo_.size())
	{
		__SS__ << "Invalid col requested" << __E__;
		__SS_THROW__;
	}

	if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_FIXED_CHOICE_DATA &&
	   // value == columnsInfo_[col].getDefaultValue())
	   value == columnsInfo_[col].getDefaultDefaultValue(columnsInfo_[col].getType(), columnsInfo_[col].getDataType()))
	{
		// if type string, fixed choice and DEFAULT, then return string of first choice

		std::vector<std::string> choices = columnsInfo_[col].getDataChoices();

		// consider arbitrary bool
		bool   skipOne = (choices.size() && choices[0].find("arbitraryBool=") == 0);
		size_t index   = (skipOne ? 1 : 0);
		if(choices.size() > index)
		{
			return doConvertEnvironmentVariables ? StringMacros::convertEnvironmentVariables(choices[index])
			                                     : choices[index];  // handled value from fixed choices
		}
	}  // end handling default to fixed choice conversion

	if(columnsInfo_[col].getDataType() == TableViewColumnInfo::DATATYPE_STRING)
		return doConvertEnvironmentVariables ? StringMacros::convertEnvironmentVariables(value) : value;
	else if(columnsInfo_[col].getDataType() == TableViewColumnInfo::DATATYPE_TIME)
	{
		return StringMacros::getTimestampString(doConvertEnvironmentVariables ? StringMacros::convertEnvironmentVariables(value) : value);

		//		retValue.resize(30); //known fixed size: Thu Aug 23 14:55:02 2001 CST
		//		time_t timestamp(
		//				strtol((doConvertEnvironmentVariables?StringMacros::convertEnvironmentVariables(value):value).c_str(),
		//						0,10));
		//		struct tm tmstruct;
		//		::localtime_r(&timestamp, &tmstruct);
		//		::strftime(&retValue[0], 30, "%c %Z", &tmstruct);
		//		retValue.resize(strlen(retValue.c_str()));
	}
	else
	{
		__SS__ << "\tUnrecognized column data type: " << columnsInfo_[col].getDataType() << " in configuration " << tableName_
		       << " at column=" << columnsInfo_[col].getName() << " for getValue with type '" << StringMacros::demangleTypeName(typeid(std::string).name())
		       << "'" << __E__;
		__SS_THROW__;
	}

	// return retValue;
}  // end validateValueForColumn()

//==============================================================================
// getValueAsString
//	gets the value with the proper data type and converts to string
//	as though getValue was called.
std::string TableView::getValueAsString(unsigned int row, unsigned int col, bool doConvertEnvironmentVariables) const
{
	if(!(col < columnsInfo_.size() && row < getNumberOfRows()))
	{
		__SS__ << ("Invalid row col requested") << __E__;
		__SS_THROW__;
	}

	//__COUT__ << columnsInfo_[col].getType() << " " << col << __E__;

	if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_ON_OFF)
	{
		if(theDataView_[row][col] == "1" || theDataView_[row][col] == "on" || theDataView_[row][col] == "On" || theDataView_[row][col] == "ON")
			return TableViewColumnInfo::TYPE_VALUE_ON;
		else
			return TableViewColumnInfo::TYPE_VALUE_OFF;
	}
	else if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_TRUE_FALSE)
	{
		if(theDataView_[row][col] == "1" || theDataView_[row][col] == "true" || theDataView_[row][col] == "True" || theDataView_[row][col] == "TRUE")
			return TableViewColumnInfo::TYPE_VALUE_TRUE;
		else
			return TableViewColumnInfo::TYPE_VALUE_FALSE;
	}
	else if(columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_YES_NO)
	{
		if(theDataView_[row][col] == "1" || theDataView_[row][col] == "yes" || theDataView_[row][col] == "Yes" || theDataView_[row][col] == "YES")
			return TableViewColumnInfo::TYPE_VALUE_YES;
		else
			return TableViewColumnInfo::TYPE_VALUE_NO;
	}

	//__COUT__ << __E__;
	return doConvertEnvironmentVariables ? StringMacros::convertEnvironmentVariables(theDataView_[row][col]) : theDataView_[row][col];
}

//==============================================================================
// getEscapedValueAsString
//	gets the value with the proper data type and converts to string
//	as though getValue was called.
//	then escapes all special characters with slash.
//	Note: this should be useful for values placed in double quotes, i.e. JSON.
std::string TableView::getEscapedValueAsString(unsigned int row, unsigned int col, bool doConvertEnvironmentVariables) const
{
	std::string val    = getValueAsString(row, col, doConvertEnvironmentVariables);
	std::string retVal = "";
	retVal.reserve(val.size());  // reserve roughly right size
	for(unsigned int i = 0; i < val.size(); ++i)
	{
		if(val[i] == '\n')
			retVal += "\\n";
		else if(val[i] == '\t')
			retVal += "\\t";
		else if(val[i] == '\r')
			retVal += "\\r";
		else
		{
			// escaped characters need a
			if(val[i] == '"' || val[i] == '\\')
				retVal += '\\';
			retVal += val[i];
		}
	}
	return retVal;
}

//==============================================================================
// setValue
//	string version
void TableView::setValue(const std::string& value, unsigned int row, unsigned int col)
{
	if(!(col < columnsInfo_.size() && row < getNumberOfRows()))
	{
		__SS__ << "Invalid row (" << row << ") col (" << col << ") requested!" << __E__;
		__SS_THROW__;
	}

	if(columnsInfo_[col].getDataType() == TableViewColumnInfo::DATATYPE_STRING)
		theDataView_[row][col] = value;
	else  // dont allow TableViewColumnInfo::DATATYPE_TIME to be set as string.. force use
	      // as time_t to standardize string result
	{
		__SS__ << "\tUnrecognized column data type: " << columnsInfo_[col].getDataType() << " in configuration " << tableName_
		       << " at column=" << columnsInfo_[col].getName() << " for setValue with type '" << StringMacros::demangleTypeName(typeid(value).name()) << "'"
		       << __E__;
		__SS_THROW__;
	}
}  // end setValue()

//==============================================================================
void TableView::setValue(const char* value, unsigned int row, unsigned int col) { setValue(std::string(value), row, col); }  // end setValue()

//==============================================================================
// setValueAsString
//	string version
void TableView::setValueAsString(const std::string& value, unsigned int row, unsigned int col)
{
	if(!(col < columnsInfo_.size() && row < getNumberOfRows()))
	{
		__SS__ << "Invalid row (" << row << ") col (" << col << ") requested!" << __E__;
		__SS_THROW__;
	}

	theDataView_[row][col] = value;
}  // end setValueAsString()

//==============================================================================
// setUniqueColumnValue
//	Auto-generates a unique value for the specified column and places
//	 value at row,col position (a la add row unique value handling)
//
//	Note: doMathAppendStrategy enables appending with a match string
//		e.g. ${PORT} + 1 .. then + 2, etc. (i.e. baseValueAsString = "${PORT}")
const std::string& TableView::setUniqueColumnValue(unsigned int row,
                                                   unsigned int col,
                                                   std::string  baseValueAsString /*= "" */,
                                                   bool         doMathAppendStrategy /*= false*/)
{
	if(!(col < columnsInfo_.size() && row < getNumberOfRows()))
	{
		__SS__ << "Invalid row (" << row << ") col (" << col << ") requested!" << __E__;
		__SS_THROW__;
	}

	__COUT__ << "Current unique data entry is data[" << row << "][" << col << "] = '" << theDataView_[row][col] << "' baseValueAsString = " << baseValueAsString
	         << " doMathAppendStrategy = " << doMathAppendStrategy << __E__;

	bool         firstConflict = true;
	int          maxUniqueData = -1;
	std::string  tmpString     = "";
	bool         foundAny;
	unsigned int index;
	std::string  numString;
	std::string  opString;  // for doMathAppendStrategy
	char         indexString[1000];

	// find max in rows

	// this->print();

	for(unsigned int r = 0; r < getNumberOfRows(); ++r)
	{
		if(r == row)
			continue;  // skip row to add

		// find last non numeric character

		foundAny  = false;
		tmpString = theDataView_[r][col];

		//__COUT__ << "tmpString " << tmpString << __E__;

		for(index = tmpString.length() - 1; index < tmpString.length(); --index)
		{
			//__COUT__ << index << " tmpString[index] " << tmpString[index] <<
			//__E__;
			if(!(tmpString[index] >= '0' && tmpString[index] <= '9'))
				break;  // if not numeric, break
			foundAny = true;
		}

		//__COUT__ << "index " << index << " foundAny " << foundAny << __E__;

		if(tmpString.length() && foundAny)  // then found a numeric substring
		{
			// create numeric substring
			numString = tmpString.substr(index + 1);

			// and alpha basestring
			tmpString = tmpString.substr(0, index + 1);

			if(doMathAppendStrategy && tmpString.size())
			{
				// look for op string
				foundAny = false;
				for(index = tmpString.length() - 1; index < tmpString.length(); --index)
				{
					//__COUT__ << index << " tmpString[index] " << tmpString[index] <<
					//__E__;
					if(!(tmpString[index] == '+' || tmpString[index] == ' '))
						break;  // if not plus op, break
					foundAny = true;
				}

				if(foundAny)
				{
					// create numeric substring
					opString = tmpString.substr(index + 1);

					// and alpha basestring
					tmpString = tmpString.substr(0, index + 1);
				}
			}

			//__COUT__ << tmpString << " vs " << baseValueAsString << __E__;

			if(baseValueAsString != "" && tmpString != baseValueAsString)
				continue;  // skip max unique number if basestring does not match

			//__COUT__ << "Found unique data base string '" << tmpString << "' and number string '" << numString << "' in last record '" << theDataView_[r][col]
			//        << "'" << __E__;

			if(firstConflict)
			{
				// if baseValueAsString ends in number, then add _ to keep naming similar
				if(baseValueAsString.size() && baseValueAsString[baseValueAsString.size() - 1] >= '0' && baseValueAsString[baseValueAsString.size() - 1] <= '9')
					baseValueAsString += '_';

				firstConflict = false;
			}

			// extract number
			sscanf(numString.c_str(), "%u", &index);

			if((int)index > maxUniqueData)
			{
				maxUniqueData = (int)index;

				if(baseValueAsString == "")
					baseValueAsString = tmpString;  // assume a value for base string
			}
		}
		else if(maxUniqueData < 0 && (baseValueAsString == "" || tmpString == baseValueAsString))
		{
			if(firstConflict)
			{
				// if baseValueAsString ends in number, then add _ to keep naming similar
				if(baseValueAsString.size() && baseValueAsString[baseValueAsString.size() - 1] >= '0' && baseValueAsString[baseValueAsString.size() - 1] <= '9')
					baseValueAsString += '_';

				firstConflict = false;
			}

			maxUniqueData = 0;  // start a number if basestring conflict
		}
	}

	//__COUTV__(maxUniqueData);

	if(maxUniqueData == -1)  // if no conflicts, then do not add number
		theDataView_[row][col] = baseValueAsString;
	else
	{
		++maxUniqueData;  // increment

		sprintf(indexString, "%u", maxUniqueData);

		//__COUTV__(indexString);
		//__COUTV__(baseValueAsString);

		if(doMathAppendStrategy)
			theDataView_[row][col] = baseValueAsString + " + " + indexString;
		else
			theDataView_[row][col] = baseValueAsString + indexString;
	}

	__COUT__ << "New unique data entry is data[" << row << "][" << col << "] = '" << theDataView_[row][col] << "'" << __E__;

	// this->print();

	return theDataView_[row][col];
}  // end setUniqueColumnValue()

//==============================================================================
// initColUID
//	if column not found throw error
unsigned int TableView::initColUID(void)
{
	if(colUID_ != INVALID)
		return colUID_;

	// if doesn't exist throw error! each view must have a UID column
	colUID_ = findColByType(TableViewColumnInfo::TYPE_UID);
	if(colUID_ == INVALID)
	{
		__COUT__ << "Column Types: " << __E__;
		for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
			std::cout << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;
		__SS__ << "\tMissing UID Column in table named '" << tableName_ << "'" << __E__;
		__SS_THROW__;
	}
	return colUID_;
}
//==============================================================================
// getColOfUID
//	const version, so don't attempt to lookup
//	if column not found throw error
unsigned int TableView::getColUID(void) const
{
	if(colUID_ != INVALID)
		return colUID_;

	__COUT__ << "Column Types: " << __E__;
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		std::cout << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;

	__SS__ << ("Missing UID Column in config named " + tableName_ + ". (Possibly TableView was just not initialized?" +
	           " This is the const call so can not alter class members)")
	       << __E__;

	ss << StringMacros::stackTrace() << __E__;

	__SS_THROW__;
}

//==============================================================================
// initColStatus
//	if column not found throw error
unsigned int TableView::initColStatus(void)
{
	if(colStatus_ != INVALID)
		return colStatus_;

	// if doesn't exist throw error! each view must have a UID column
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		if(columnsInfo_[col].getName() == TableViewColumnInfo::COL_NAME_STATUS)
		{
			colStatus_ = col;
			return colStatus_;
		}
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		if(columnsInfo_[col].getName() == TableViewColumnInfo::COL_NAME_ENABLED)
		{
			colStatus_ = col;
			return colStatus_;
		}

	// at this point not found!

	__SS__ << "\tMissing column named '" << TableViewColumnInfo::COL_NAME_STATUS << "' or '" << TableViewColumnInfo::COL_NAME_ENABLED << "' in table '"
	       << tableName_ << ".'" << __E__;
	ss << "\n\nTable '" << tableName_ << "' Columns: " << __E__;
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		ss << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;

	__SS_ONLY_THROW__;

}  // end initColStatus()

//==============================================================================
// initColPriority
//	if column not found throw error
unsigned int TableView::initColPriority(void)
{
	if(colPriority_ != INVALID)
		return colPriority_;

	// if doesn't exist throw error! each view must have a UID column
	colPriority_ = findCol("*" + TableViewColumnInfo::COL_NAME_PRIORITY);  // wild card search
	if(colPriority_ == INVALID)
	{
		__SS__ << "\tMissing column named '" << TableViewColumnInfo::COL_NAME_PRIORITY << "' in table '" << tableName_ << ".'" << __E__;
		ss << "\n\nTable '" << tableName_ << "' Columns: " << __E__;
		for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
			ss << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;

		__SS_THROW__;
	}
	return colPriority_;
}

//==============================================================================
// getColStatus
//	const version, so don't attempt to lookup
//	if column not found throw error
unsigned int TableView::getColStatus(void) const
{
	if(colStatus_ != INVALID)
		return colStatus_;

	__SS__ << "\tMissing column named '" << TableViewColumnInfo::COL_NAME_STATUS << "' or '" << TableViewColumnInfo::COL_NAME_ENABLED << "' in table '"
	       << tableName_ << ".'"
	       << " (The Status column is identified when the TableView is initialized)" << __E__;

	ss << "\n\nTable '" << tableName_ << "' Columns: " << __E__;
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		ss << "\t" << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;

	ss << __E__;

	ss << StringMacros::stackTrace() << __E__;

	__SS_THROW__;
}  // end getColStatus()

//==============================================================================
// getColPriority
//	const version, so don't attempt to lookup
//	if column not found throw error
//
//	Note: common for Priority column to not exist, so be quiet with printouts
//	 so as to not scare people.
unsigned int TableView::getColPriority(void) const
{
	if(colPriority_ != INVALID)
		return colPriority_;

	__SS__ << "Priority column was not found... \nColumn Types: " << __E__;

	ss << "Missing " << TableViewColumnInfo::COL_NAME_PRIORITY << " Column in table named '" << tableName_
	   << ".' (The Priority column is identified when the TableView is initialized)" << __E__;  // this is the const call, so can not identify the column and
	                                                                                            // set colPriority_ here

	ss << "\n\nTable '" << tableName_ << "' Columns: " << __E__;
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		ss << "\t" << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;
	ss << __E__;

	ss << StringMacros::stackTrace() << __E__;

	__SS_ONLY_THROW__;  // keep it quiet
}  // end getColPriority()

//==============================================================================
// addRowToGroup
//	Group entry can include | to place a record in multiple groups
void TableView::addRowToGroup(const unsigned int& row,
                              const unsigned int& col,
                              const std::string&  groupID)  //,
                                                           // const std::string &colDefault)
{
	if(isEntryInGroupCol(row, col, groupID))
	{
		__SS__ << "GroupID (" << groupID << ") added to row (" << row << " is already present!" << __E__;
		__SS_THROW__;
	}

	// not in group, so
	//	if no groups
	//		set groupid
	//	if other groups
	//		prepend groupId |
	if(getDataView()[row][col] == "" || getDataView()[row][col] == getDefaultRowValues()[col])  // colDefault)
		setValue(groupID, row, col);
	else
		setValue(groupID + " | " + getDataView()[row][col], row, col);

	//__COUT__ << getDataView()[row][col] << __E__;
}  // end addRowToGroup()

//==============================================================================
// getGroupRows
//	Get the rows of all records with groupID
//	with consideration for status on/off and priority, if enabled with flags
//
//	Note: empty group id of "" or '*' will return all rows
std::vector<unsigned int /*group row*/> TableView::getGroupRows(const unsigned int groupIdCol,
                                                                const std::string& groupID,
                                                                bool               onlyStatusTrue /*=false*/,
                                                                bool               orderedByPriority /*=false*/) const
{
	std::vector<unsigned int /*group row*/>              retVector;
	std::vector<std::vector<unsigned int /*group row*/>> groupRowVectors = getGroupRowsInVectors(groupIdCol, groupID, onlyStatusTrue, orderedByPriority);

	for(const auto& groupRowVector : groupRowVectors)
		for(const auto& groupRow : groupRowVector)
			retVector.push_back(groupRow);

	return retVector;
}  // end getGroupRows()

//==============================================================================
// getGroupRowsByPriority
//	Get the rows of all records with groupID
//	with consideration for status on/off and priority, if enabled with flags
//
//	Note: empty group id of "" or '*' will return all rows
std::vector<std::vector<unsigned int /*group row*/>> TableView::getGroupRowsByPriority(const unsigned int groupIdCol,
                                                                                       const std::string& groupID,
                                                                                       bool               onlyStatusTrue /*=false*/) const
{
	return getGroupRowsInVectors(groupIdCol, groupID, onlyStatusTrue, true /*orderedByPriority*/);
}  // end getGroupRowsByPriority()

//==============================================================================
// getGroupRowsInVectors
//	Private function to allows byPriority switch.
//
//	Get the rows of all records with groupID
//	with consideration for status on/off and priority, if enabled with flags
//
//	Note: empty group id of "" or '*' will return all rows
std::vector<std::vector<unsigned int /*group row*/>> TableView::getGroupRowsInVectors(const unsigned int groupIdCol,
                                                                                      const std::string& groupID,
                                                                                      bool               onlyStatusTrue,
                                                                                      bool               orderedByPriority) const
{
	std::map<uint64_t /*priority*/, std::vector<unsigned int /*child row*/>> mapByPriority;
	std::vector<std::vector<unsigned int /*group row*/>>                     retVector;
	uint64_t                                                                 tmpPriority;
	bool                                                                     tmpStatus;

	if(!(orderedByPriority && colPriority_ != INVALID))  // if no priority column, all at same priorty [0]
		retVector.push_back(std::vector<unsigned int /*group row*/>());

	for(unsigned int r = 0; r < getNumberOfRows(); ++r)
		if(groupID == "" || groupID == "*" || groupIdCol == INVALID || isEntryInGroupCol(r, groupIdCol, groupID))
		{
			// check status if needed
			if(onlyStatusTrue && colStatus_ != INVALID)
			{
				getValue(tmpStatus, r, colStatus_);

				if(!tmpStatus)
					continue;  // skip those with status false
			}

			if(orderedByPriority && colPriority_ != INVALID)
			{
				getValue(tmpPriority, r, colPriority_);
				// do not accept DEFAULT value of 0.. convert to 100
				mapByPriority[tmpPriority ? tmpPriority : 100].push_back(r);
			}
			else  // assume equal priority
				retVector[0].push_back(r);
		}

	if(orderedByPriority && colPriority_ != INVALID)
	{
		// at this point have priority map (which automatically sorts by priority)
		// now build return vector
		for(const auto& priorityChildRowVector : mapByPriority)
		{
			retVector.push_back(std::vector<unsigned int /*group row*/>());
			for(const auto& priorityChildRow : priorityChildRowVector.second)
				retVector[retVector.size() - 1].push_back(priorityChildRow);
		}

		__COUT__ << "Returning priority children list." << __E__;
	}
	// else equal priority vector already constructed

	return retVector;
}  // end getGroupRowsInVectors()

//==============================================================================
// removeRowFromGroup
//	Group entry can include | to place a record in multiple groups
//
//	returns true if row was deleted because it had no group left
bool TableView::removeRowFromGroup(const unsigned int& row, const unsigned int& col, const std::string& groupNeedle, bool deleteRowIfNoGroupLeft)
{
	__COUT__ << "groupNeedle " << groupNeedle << __E__;
	std::set<std::string> groupIDList;
	if(!isEntryInGroupCol(row, col, groupNeedle, &groupIDList))
	{
		__SS__ << "GroupID (" << groupNeedle << ") removed from row (" << row << ") was already removed!" << __E__;
		print();
		__SS_THROW__;
	}

	// is in group, so
	//	create new string based on set of groupids
	//	but skip groupNeedle

	std::string  newValue = "";
	unsigned int cnt      = 0;
	for(const auto& groupID : groupIDList)
	{
		//__COUT__ << groupID << " " << groupNeedle << " " << newValue << __E__;
		if(groupID == groupNeedle)
			continue;  // skip group to be removed

		if(cnt)
			newValue += " | ";
		newValue += groupID;
	}

	bool wasDeleted = false;
	if(deleteRowIfNoGroupLeft && newValue == "")
	{
		__COUT__ << "Delete row since it no longer part of any group." << __E__;
		deleteRow(row);
		wasDeleted = true;
	}
	else
		setValue(newValue, row, col);

	//__COUT__ << getDataView()[row][col] << __E__;

	return wasDeleted;
}  // end removeRowFromGroup()

//==============================================================================
// isEntryInGroup
//	All group link checking should use this function
// 	so that handling is consistent
//
//	Group entry can include | to place a record in multiple groups
bool TableView::isEntryInGroup(const unsigned int& r, const std::string& childLinkIndex, const std::string& groupNeedle) const
{
	unsigned int c = getLinkGroupIDColumn(childLinkIndex);  // column in question

	return isEntryInGroupCol(r, c, groupNeedle);
}  // end isEntryInGroup()

//==============================================================================
//	isEntryInGroupCol
//
//	if *groupIDList != 0 return set of groupIDs found
//		useful for removing groupIDs.
//
//	Group entry can include | to place a record in multiple groups
//
// Note: should mirror what happens in TableView::getSetOfGroupIDs
bool TableView::isEntryInGroupCol(const unsigned int& r, const unsigned int& c, const std::string& groupNeedle, std::set<std::string>* groupIDList) const
{
	if(r >= getNumberOfRows() || c >= getNumberOfColumns())
	{
		__SS__ << "Invalid row/col requested!" << __E__;
		ss << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}

	unsigned int i     = 0;
	unsigned int j     = 0;
	bool         found = false;

	//__COUT__ << "groupNeedle " << groupNeedle << __E__;

	// go through the full groupString extracting groups and comparing to groupNeedle
	for(; j < theDataView_[r][c].size(); ++j)
		if((theDataView_[r][c][j] == ' ' ||  // ignore leading white space or |
		    theDataView_[r][c][j] == '|') &&
		   i == j)
			++i;
		else if((theDataView_[r][c][j] == ' ' ||  // trailing white space or | indicates group
		         theDataView_[r][c][j] == '|') &&
		        i != j)  // assume end of group name
		{
			if(groupIDList)
				groupIDList->emplace(theDataView_[r][c].substr(i, j - i));

			//__COUT__ << "Group found to compare: " <<
			//		theDataView_[r][c].substr(i,j-i) << __E__;
			if(groupNeedle == theDataView_[r][c].substr(i, j - i))
			{
				if(!groupIDList)  // dont return if caller is trying to get group list
					return true;
				found = true;
			}
			// if no match, setup i and j for next find
			i = j + 1;
		}

	if(i != j)  // last group check (for case when no ' ' or '|')
	{
		if(groupIDList)
			groupIDList->emplace(theDataView_[r][c].substr(i, j - i));

		//__COUT__ << "Group found to compare: " <<
		//		theDataView_[r][c].substr(i,j-i) << __E__;
		if(groupNeedle == theDataView_[r][c].substr(i, j - i))
			return true;
	}

	return found;
}  // end isEntryInGroupCol()

//==============================================================================
// getSetOfGroupIDs
//	if row == -1, then considers all rows
//	else just that row
//	returns unique set of groupIds in GroupID column
//		associate with childLinkIndex
//
// Note: should mirror what happens in TableView::isEntryInGroupCol
std::set<std::string> TableView::getSetOfGroupIDs(const std::string& childLinkIndex, unsigned int r) const
{
	return getSetOfGroupIDs(getLinkGroupIDColumn(childLinkIndex), r);
}
std::set<std::string> TableView::getSetOfGroupIDs(const unsigned int& c, unsigned int r) const
{
	//__COUT__ << "GroupID col=" << (int)c << __E__;

	std::set<std::string> retSet;

	// unsigned int i = 0;
	// unsigned int j = 0;

	if(r != (unsigned int)-1)
	{
		if(r >= getNumberOfRows())
		{
			__SS__ << "Invalid row requested!" << __E__;
			__SS_THROW__;
		}

		StringMacros::getSetFromString(theDataView_[r][c], retSet);
		//		//go through the full groupString extracting groups
		//		//add each found groupId to set
		//		for(;j<theDataView_[r][c].size();++j)
		//			if((theDataView_[r][c][j] == ' ' || //ignore leading white space or |
		//					theDataView_[r][c][j] == '|')
		//					&& i == j)
		//				++i;
		//			else if((theDataView_[r][c][j] == ' ' || //trailing white space or |
		// indicates group 					theDataView_[r][c][j] == '|')
		//					&& i != j) // assume end of group name
		//			{
		//				//__COUT__ << "Group found: " <<
		//				//		theDataView_[r][c].substr(i,j-i) << __E__;
		//
		//
		//				retSet.emplace(theDataView_[r][c].substr(i,j-i));
		//
		//				//setup i and j for next find
		//				i = j+1;
		//			}
		//
		//		if(i != j) //last group check (for case when no ' ' or '|')
		//			retSet.emplace(theDataView_[r][c].substr(i,j-i));
	}
	else
	{
		// do all rows
		for(r = 0; r < getNumberOfRows(); ++r)
		{
			StringMacros::getSetFromString(theDataView_[r][c], retSet);

			//			i=0;
			//			j=0;
			//
			//			//__COUT__ << (int)r << ": " << theDataView_[r][c] << __E__;
			//
			//			//go through the full groupString extracting groups
			//			//add each found groupId to set
			//			for(;j<theDataView_[r][c].size();++j)
			//			{
			//				//__COUT__ << "i:" << i << " j:" << j << __E__;
			//
			//				if((theDataView_[r][c][j] == ' ' || //ignore leading white
			// space  or | 						theDataView_[r][c][j] == '|')
			//						&& i == j)
			//					++i;
			//				else if((theDataView_[r][c][j] == ' ' || //trailing white
			// space  or |  indicates group 						theDataView_[r][c][j]
			// ==
			// '|')
			//						&& i != j) // assume end of group name
			//				{
			//					//__COUT__ << "Group found: " <<
			//					//		theDataView_[r][c].substr(i,j-i) << __E__;
			//
			//					retSet.emplace(theDataView_[r][c].substr(i,j-i));
			//
			//					//setup i and j for next find
			//					i = j+1;
			//				}
			//			}
			//
			//			if(i != j) //last group (for case when no ' ' or '|')
			//			{
			//				//__COUT__ << "Group found: " <<
			//				//		theDataView_[r][c].substr(i,j-i) << __E__;
			//				retSet.emplace(theDataView_[r][c].substr(i,j-i));
			//			}
		}
	}

	return retSet;
}

//==============================================================================
// getColOfLinkGroupID
//	const version, if column not found throw error
unsigned int TableView::getLinkGroupIDColumn(const std::string& childLinkIndex) const
{
	if(!childLinkIndex.size())
	{
		__SS__ << "Empty childLinkIndex string parameter!" << __E__;
		ss << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}

	const char* needleChildLinkIndex = &childLinkIndex[0];

	// allow space syntax to target a childLinkIndex from a different parentLinkIndex
	// e.g. "parentLinkIndex childLinkIndex"
	size_t spacePos = childLinkIndex.find(' ');
	if(spacePos != std::string::npos && spacePos + 1 < childLinkIndex.size())  // make sure there are more characters
	{
		// found space syntax for targeting childLinkIndex
		needleChildLinkIndex = &childLinkIndex[spacePos + 1];
	}

	std::map<std::string, unsigned int>::const_iterator it = colLinkGroupIDs_.find(needleChildLinkIndex);
	if(it !=  // if already known, return it
	   colLinkGroupIDs_.end())
		return it->second;

	// otherwise search (perhaps init() was not called)
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
	{
		// only check columns with link index associated...
		if(columnsInfo_[col].isChildLink() || columnsInfo_[col].isChildLinkUID() || columnsInfo_[col].isChildLinkGroupID() || columnsInfo_[col].isGroupID())
		{
			if(needleChildLinkIndex == columnsInfo_[col].getChildLinkIndex())
				return col;
		}
	}

	__SS__ << "Error! Incompatible table for this group link! Table '" << tableName_ << "' is missing a GroupID column with data type '"
	       << TableViewColumnInfo::TYPE_START_GROUP_ID << "-" << needleChildLinkIndex << "'.\n\n"
	       << "Note: you can separate the child GroupID column data type from "
	       << "the parent GroupLink column data type; this is accomplished by using a space "
	       << "character at the parent level - the string after the space will be treated "
	          "as the "
	       << "child GroupID column data type." << __E__;
	ss << "Existing Column GroupIDs: " << __E__;
	for(auto& groupIdColPair : colLinkGroupIDs_)
		ss << "\t" << groupIdColPair.first << " : col-" << groupIdColPair.second << __E__;

	ss << "Existing Column Types: " << __E__;
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		ss << "\t" << columnsInfo_[col].getType() << "() " << columnsInfo_[col].getName() << __E__;

	ss << StringMacros::stackTrace() << __E__;

	__SS_THROW__;
}  // end getLinkGroupIDColumn()

//==============================================================================
unsigned int TableView::findRow(unsigned int col, const std::string& value, unsigned int offsetRow, bool doNotThrow /*= false*/) const
{
	for(unsigned int row = offsetRow; row < theDataView_.size(); ++row)
	{
		if(theDataView_[row][col] == value)
			return row;
	}
	if(doNotThrow)
		return TableView::INVALID;

	__SS__ << "\tIn view: " << tableName_ << ", Can't find value=" << value << " in column named " << columnsInfo_[col].getName()
	       << " with type=" << columnsInfo_[col].getType() << __E__ << __E__ << StringMacros::stackTrace() << __E__;

	// Note: findRow gets purposely called by configuration GUI a lot looking for
	// exceptions 	so may not want to print out
	//__COUT__ << "\n" << ss.str();
	__SS_ONLY_THROW__;
}  // end findRow()

//==============================================================================
unsigned int TableView::findRowInGroup(
    unsigned int col, const std::string& value, const std::string& groupId, const std::string& childLinkIndex, unsigned int offsetRow) const
{
	unsigned int groupIdCol = getLinkGroupIDColumn(childLinkIndex);
	for(unsigned int row = offsetRow; row < theDataView_.size(); ++row)
	{
		if(theDataView_[row][col] == value && isEntryInGroupCol(row, groupIdCol, groupId))
			return row;
	}

	__SS__ << "\tIn view: " << tableName_ << ", Can't find in group the value=" << value << " in column named '" << columnsInfo_[col].getName()
	       << "' with type=" << columnsInfo_[col].getType() << " and GroupID: '" << groupId << "' in column '" << groupIdCol
	       << "' with GroupID child link index '" << childLinkIndex << "'" << __E__;
	// Note: findRowInGroup gets purposely called by configuration GUI a lot looking for
	// exceptions 	so may not want to print out
	__SS_ONLY_THROW__;
}  // end findRowInGroup()

//==============================================================================
// findCol
//	throws exception if column not found by name
unsigned int TableView::findCol(const std::string& wildCardName) const
{
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		if(StringMacros::wildCardMatch(wildCardName /*needle*/, columnsInfo_[col].getName() /*haystack*/))
			return col;

	__SS__ << "\tIn view: " << tableName_ << ", Can't find column named '" << wildCardName << "'" << __E__;
	ss << "Existing columns:\n";
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		ss << "\t" << columnsInfo_[col].getName() << "\n";

	ss << StringMacros::stackTrace() << __E__;

	// Note: findCol gets purposely called by configuration GUI a lot looking for
	// exceptions 	so may not want to print out
	__SS_ONLY_THROW__;
}  // end findCol()

//==============================================================================
// findColByType
//	return invalid if type not found
unsigned int TableView::findColByType(const std::string& type, int startingCol) const
{
	for(unsigned int col = startingCol; col < columnsInfo_.size(); ++col)
		if(columnsInfo_[col].getType() == type)
			return col;

	return INVALID;
}  // end findColByType()

// Getters:

//==============================================================================
// getDataColumnSize
unsigned int TableView::getDataColumnSize(void) const
{
	// if no data, give benefit of the doubt that phantom data has mockup column size
	if(!getNumberOfRows())
		return getNumberOfColumns();
	return theDataView_[0].size();  // number of columns in first row of data
}

//==============================================================================
std::set<std::string> TableView::getColumnNames(void) const
{
	std::set<std::string> retSet;
	for(auto& colInfo : columnsInfo_)
		retSet.emplace(colInfo.getName());
	return retSet;
}  // end getColumnNames()

//==============================================================================
std::map<std::string, unsigned int /*col*/> TableView::getColumnNamesMap(void) const
{
	std::map<std::string, unsigned int /*col*/> retMap;
	unsigned int                                c = 0;
	for(auto& colInfo : columnsInfo_)
		retMap.emplace(std::make_pair(colInfo.getName(), c++));
	return retMap;
}  // end getColumnNamesMap()

//==============================================================================
std::set<std::string> TableView::getColumnStorageNames(void) const
{
	std::set<std::string> retSet;
	for(auto& colInfo : columnsInfo_)
		retSet.emplace(colInfo.getStorageName());
	return retSet;
}

//==============================================================================
const std::vector<std::string>& TableView::initRowDefaults(void)
{
	std::vector<std::string>& retVec = rowDefaultValues_;
	retVec.clear();

	// fill each col of new row with default values
	for(unsigned int col = 0; col < getNumberOfColumns(); ++col)
	{
		// if this is a fixed choice Link, and NO_LINK is not in list,
		//	take first in list to avoid creating illegal rows.
		// NOTE: this is not a problem for standard fixed choice fields
		//	because the default value is always required.

		if(columnsInfo_[col].isChildLink())
		{
			const std::vector<std::string>& theDataChoices = columnsInfo_[col].getDataChoices();

			// check if arbitrary values allowed
			if(!theDataChoices.size() ||  // if so, use default
			   theDataChoices[0] == "arbitraryBool=1")
				retVec.push_back(columnsInfo_[col].getDefaultValue());
			else
			{
				bool skipOne = (theDataChoices.size() && theDataChoices[0] == "arbitraryBool=0");
				bool hasSkipped;

				// look for default value in list

				bool foundDefault = false;
				hasSkipped        = false;
				for(const auto& choice : theDataChoices)
					if(skipOne && !hasSkipped)
					{
						hasSkipped = true;
						continue;
					}
					else if(choice == columnsInfo_[col].getDefaultValue())
					{
						foundDefault = true;
						break;
					}

				// use first choice if possible
				if(!foundDefault && theDataChoices.size() > (skipOne ? 1 : 0))
					retVec.push_back(theDataChoices[(skipOne ? 1 : 0)]);
				else  // else stick with default
					retVec.push_back(columnsInfo_[col].getDefaultValue());
			}
		}
		else
			retVec.push_back(columnsInfo_[col].getDefaultValue());
	}

	//__COUT__ << StringMacros::stackTrace() << __E__;
	//__COUTV__(StringMacros::vectorToString(rowDefaultValues_));
	return rowDefaultValues_;
}  // end getDefaultRowValues()

//==============================================================================
const TableViewColumnInfo& TableView::getColumnInfo(unsigned int column) const
{
	if(column >= columnsInfo_.size())
	{
		__SS__ << "\nCan't find column " << column
		       << "\n\n\n\nThe column info is likely missing due to incomplete "
		          "Configuration View filling.\n\n"
		       << __E__;
		ss << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}
	return columnsInfo_[column];
}  // end getColumnInfo()

// Setters 

//==============================================================================
void TableView::setURIEncodedComment(const std::string& uriComment) { comment_ = StringMacros::decodeURIComponent(uriComment); }

//==============================================================================
void TableView::setAuthor(const std::string& author) { author_ = author; }

//==============================================================================
void TableView::setCreationTime(time_t t) { creationTime_ = t; }

//==============================================================================
void TableView::setLastAccessTime(time_t t) { lastAccessTime_ = t; }

//==============================================================================
void TableView::setLooseColumnMatching(bool setValue) { fillWithLooseColumnMatching_ = setValue; }

//==============================================================================
void TableView::doGetSourceRawData(bool setValue) { getSourceRawData_ = setValue; }

//==============================================================================
void TableView::reset(void)
{
	version_ = -1;
	comment_ = "";
	author_  = "";
	columnsInfo_.clear();
	theDataView_.clear();
}  // end reset()

//==============================================================================
void TableView::print(std::ostream& out) const
{
	out << "============================================================================="
	       "="
	    << __E__;
	out << "Print: " << tableName_ << " Version: " << version_ << " Comment: " << comment_ << " Author: " << author_
	    << " Creation Time: " << ctime(&creationTime_) << __E__;
	out << "\t\tNumber of Cols " << getNumberOfColumns() << __E__;
	out << "\t\tNumber of Rows " << getNumberOfRows() << __E__;

	out << "Columns:\t";
	for(int i = 0; i < (int)columnsInfo_.size(); ++i)
		out << i << ":" << columnsInfo_[i].getName() << ":" << columnsInfo_[i].getStorageName() << ":" << columnsInfo_[i].getType() << ":"
		    << columnsInfo_[i].getDataType() << "\t ";
	out << __E__;

	out << "Rows:" << __E__;
	// int         num;
	std::string val;
	for(int r = 0; r < (int)getNumberOfRows(); ++r)
	{
		out << (int)r << ":\t";
		for(int c = 0; c < (int)getNumberOfColumns(); ++c)
		{
			out << (int)c << ":";

			// if fixed choice type, print index in choice
			if(columnsInfo_[c].getType() == TableViewColumnInfo::TYPE_FIXED_CHOICE_DATA)
			{
				int                      choiceIndex = -1;
				std::vector<std::string> choices     = columnsInfo_[c].getDataChoices();
				val                                  = StringMacros::convertEnvironmentVariables(theDataView_[r][c]);

				if(val == columnsInfo_[c].getDefaultValue())
					choiceIndex = 0;
				else
				{
					for(int i = 0; i < (int)choices.size(); ++i)
						if(val == choices[i])
							choiceIndex = i + 1;
				}

				out << "ChoiceIndex=" << choiceIndex << ":";
			}

			out << theDataView_[r][c];
			// stopped using below, because it is called sometimes during debugging when
			//	numbers are set to environment variables:
			//			if(columnsInfo_[c].getDataType() == "NUMBER")
			//			{
			//				getValue(num,r,c,false);
			//				out << num;
			//			}
			//			else
			//			{
			//				getValue(val,r,c,false);
			//				out << val;
			//			}
			out << "\t\t";
		}
		out << __E__;
	}
}  // end print()

//==============================================================================
void TableView::printJSON(std::ostream& out) const
{

	{ //handle special GROUP CACHE table
		std::string tmpCachePrepend = TableBase::GROUP_CACHE_PREPEND;
		tmpCachePrepend = TableBase::convertToCaps(tmpCachePrepend);
		__COUT__ << " '" << tableName_ << "' vs "  << tmpCachePrepend << __E__;
		//if special GROUP CACHE table, handle construction in a special way
		if(tableName_.substr(0,tmpCachePrepend.length()) == tmpCachePrepend)
		{
			out << getCustomStorageData();
			return;
		} //end special GROUP CACHE table construction
	} //end handle special GROUP CACHE table


	out << "{\n";
	out << "\"NAME\" : \"" << tableName_ << "\",\n";

	// out << "\"VERSION\" : \"" << version_ <<  "\"\n";

	out << "\"COMMENT\" : ";

	// output escaped comment
	std::string val;
	val = comment_;
	out << "\"";
	for(unsigned int i = 0; i < val.size(); ++i)
	{
		if(val[i] == '\n')
			out << "\\n";
		else if(val[i] == '\t')
			out << "\\t";
		else if(val[i] == '\r')
			out << "\\r";
		else
		{
			// escaped characters need a
			if(val[i] == '"' || val[i] == '\\')
				out << '\\';
			out << val[i];
		}
	}
	out << "\",\n";

	out << "\"AUTHOR\" : \"" << author_ << "\",\n";
	out << "\"CREATION_TIME\" : " << creationTime_ << ",\n";

	// USELESS... out << "\"NUM_OF_COLS\" : " << getNumberOfColumns() << ",\n";
	// USELESS... out << "\"NUM_OF_ROWS\" : " <<  getNumberOfRows() << ",\n";

	out << "\"COL_TYPES\" : {\n";
	for(int c = 0; c < (int)getNumberOfColumns(); ++c)
	{
		out << "\t\t\"" << columnsInfo_[c].getStorageName() << "\" : ";
		out << "\"" << columnsInfo_[c].getDataType() << "\"";
		if(c + 1 < (int)getNumberOfColumns())
			out << ",";
		out << "\n";
	}
	out << "},\n";  // close COL_TYPES

	out << "\"DATA_SET\" : [\n";
	// int num;
	for(int r = 0; r < (int)getNumberOfRows(); ++r)
	{
		out << "\t{\n";
		for(int c = 0; c < (int)getNumberOfColumns(); ++c)
		{
			out << "\t\t\"" << columnsInfo_[c].getStorageName() << "\" : ";

			out << "\"" << getEscapedValueAsString(r, c, false) << "\"";  // do not convert env variables

			if(c + 1 < (int)getNumberOfColumns())
				out << ",";
			out << "\n";
		}
		out << "\t}";
		if(r + 1 < (int)getNumberOfRows())
			out << ",";
		out << "\n";
	}
	out << "]\n";  // close DATA_SET

	out << "}";
}  // end printJSON()

//==============================================================================
// restoreJSONStringEntities
//	returns string with literals \n \t \" \r \\ replaced with char
std::string restoreJSONStringEntities(const std::string& str)
{
	unsigned int sz = str.size();
	if(!sz)
		return "";  // empty string, returns empty string

	std::stringstream retStr;
	unsigned int      i = 0;
	for(; i < sz - 1; ++i)
	{
		if(str[i] == '\\')  // if 2 char escape sequence, replace with char
			switch(str[i + 1])
			{
			case 'n':
				retStr << '\n';
				++i;
				break;
			case '"':
				retStr << '"';
				++i;
				break;
			case 't':
				retStr << '\t';
				++i;
				break;
			case 'r':
				retStr << '\r';
				++i;
				break;
			case '\\':
				retStr << '\\';
				++i;
				break;
			default:
				retStr << str[i];
			}
		else
			retStr << str[i];
	}
	if(i == sz - 1)
		retStr << str[sz - 1];  // output last character (which can't escape anything)

	return retStr.str();
}  // end restoreJSONStringEntities()

//==============================================================================
// fillFromJSON
//	Clears and fills the view from the JSON string.
//	Returns -1 on failure
//
//	first level keys:
//		NAME
//		DATA_SET
int TableView::fillFromJSON(const std::string& json)
{
	{
		//handle special GROUP CACHE table
		std::string tmpCachePrepend = TableBase::GROUP_CACHE_PREPEND;
		tmpCachePrepend = TableBase::convertToCaps(tmpCachePrepend);
		
		//if special GROUP CACHE table, handle construction in a special way
		if(tableName_.substr(0,tmpCachePrepend.length()) == tmpCachePrepend)
		{
			//remove json { } and all " characters
			std::string jsonClean = "";
			for(auto& c : json)
				if(c == '{' || c == '}' || c == '"' || c == ' ') continue;
				else jsonClean += c;

			setCustomStorageData(jsonClean);
			return 0; //success
		} //end special GROUP CACHE table construction
	} //end handle special GROUP CACHE table




	bool dbg     = false;  // tableName_ == "ARTDAQEventBuilderTable" || tableName_ == "";
	bool rawData = getSourceRawData_;
	if(getSourceRawData_)
	{  // only get source raw data once, then revert member variable
		__COUTV__(getSourceRawData_);
		getSourceRawData_ = false;
		sourceRawData_    = "";  // clear for this fill
	}

	std::map<std::string /*key*/, unsigned int /*entries/rows*/> keyEntryCountMap;
	std::vector<std::string>                                     keys;
	keys.push_back("NAME");
	keys.push_back("COMMENT");
	keys.push_back("AUTHOR");
	keys.push_back("CREATION_TIME");
	// keys.push_back ("COL_TYPES");
	keys.push_back("DATA_SET");
	enum
	{
		CV_JSON_FILL_NAME,
		CV_JSON_FILL_COMMENT,
		CV_JSON_FILL_AUTHOR,
		CV_JSON_FILL_CREATION_TIME,
		//	CV_JSON_FILL_COL_TYPES,
		CV_JSON_FILL_DATA_SET
	};

	if(dbg)
		__COUTV__(tableName_);
	if(dbg)
		__COUTV__(json);

	sourceColumnMismatchCount_ = 0;
	sourceColumnMissingCount_  = 0;
	sourceColumnNames_.clear();  // reset
	unsigned int colFoundCount = 0;
	unsigned int i             = 0;
	unsigned int row           = -1;
	unsigned int colSpeedup    = 0;
	unsigned int startString, startNumber = 0, endNumber = -1;
	unsigned int bracketCount   = 0;
	unsigned int sqBracketCount = 0;
	bool         inQuotes       = 0;
	bool         newString      = 0;
	bool         newValue       = 0;
	// bool              isDataArray = 0;
	bool              keyIsMatch, keyIsComment;
	unsigned int      keyIsMatchIndex, keyIsMatchStorageIndex, keyIsMatchCommentIndex;
	const std::string COMMENT_ALT_KEY = "COMMENT";

	std::string  extractedString = "", currKey = "", currVal = "";
	unsigned int currDepth = 0;

	std::vector<std::string> jsonPath;
	std::vector<char>        jsonPathType;       // indicator of type in jsonPath: { [ K
	char                     lastPopType = '_';  // either: _ { [ K
	// _ indicates reset pop (this happens when a new {obj} starts)
	unsigned int matchedKey = -1;
	unsigned int lastCol    = -1;

	// find all depth 1 matching keys
	for(; i < json.size(); ++i)
	{
		switch(json[i])
		{
		case '"':
			if(i - 1 < json.size() &&  // ignore if escaped
			   json[i - 1] == '\\')
				break;

			inQuotes = !inQuotes;  // toggle in quotes if not escaped
			if(inQuotes)
				startString = i;
			else
			{
				extractedString = restoreJSONStringEntities(json.substr(startString + 1, i - startString - 1));
				newString       = 1;  // have new string!
			}
			break;
		case ':':
			if(inQuotes)
				break;  // skip if in quote

			// must be a json object level to have a key
			if(jsonPathType[jsonPathType.size() - 1] != '{' || !newString)  // and must have a string for key
			{
				__COUT__ << "Invalid ':' position" << __E__;
				return -1;
			}

			// valid, so take key
			jsonPathType.push_back('K');
			jsonPath.push_back(extractedString);
			startNumber = i;
			newString   = 0;   // clear flag
			endNumber   = -1;  // reset end number index
			break;

			//    		if(isKey ||
			//    			isDataArray)
			//    		{
			//            	std::cout << "Invalid ':' position" << __E__;
			//            	return -1;
			//    		}
			//    		isKey = 1;	//new value is a key
			//    		newValue = 1;
			//    		startNumber = i;
			//    		break;
		case ',':
			if(inQuotes)
				break;              // skip if in quote
			if(lastPopType == '{')  // don't need value again of nested object
			{
				// check if the nested object was the value to a key, if so, pop key
				if(jsonPathType[jsonPathType.size() - 1] == 'K')
				{
					lastPopType = 'K';
					jsonPath.pop_back();
					jsonPathType.pop_back();
				}
				break;  // skip , handling if {obj} just ended
			}

			if(newString)
				currVal = extractedString;
			else  // number value
			{
				if(endNumber == (unsigned int)-1 ||  // take i as end number if needed
				   endNumber <= startNumber)
					endNumber = i;
				// extract number value
				if(endNumber <= startNumber)  // empty data, could be {}
					currVal = "";
				else
					currVal = json.substr(startNumber + 1, endNumber - startNumber - 1);
			}

			currDepth = bracketCount;

			if(jsonPathType[jsonPathType.size() - 1] == 'K')  // this is the value to key
			{
				currKey  = jsonPath[jsonPathType.size() - 1];
				newValue = 1;  // new value to consider!

				// pop key
				lastPopType = 'K';
				jsonPath.pop_back();
				jsonPathType.pop_back();
			}
			else if(jsonPathType[jsonPathType.size() - 1] == '[')  // this is a value in array
			{
				// key is last key
				for(unsigned int k = jsonPathType.size() - 2; k < jsonPathType.size(); --k)
					if(jsonPathType[k] == 'K')
					{
						currKey = jsonPath[k];
						break;
					}
					else if(k == 0)
					{
						__COUT__ << "Invalid array position" << __E__;
						return -1;
					}

				newValue = 1;  // new value to consider!
				               // isDataArray = 1;
			}
			else  // { is an error
			{
				__COUT__ << "Invalid ',' position" << __E__;
				return -1;
			}

			startNumber = i;
			break;

		case '{':
			if(inQuotes)
				break;          // skip if in quote
			lastPopType = '_';  // reset because of new object
			jsonPathType.push_back('{');
			jsonPath.push_back("{");
			++bracketCount;
			break;

			//    		++bracketCount;
			//    		isDataArray = 0;
			//    		isKey = 0;
			//    		endingObject = 0;
			//    		break;
		case '}':
			if(inQuotes)
				break;  // skip if in quote

			if(lastPopType != '{' &&                          // don't need value again of nested object
			   jsonPathType[jsonPathType.size() - 1] == 'K')  // this is the value to key
			{
				currDepth = bracketCount;
				currKey   = jsonPath[jsonPathType.size() - 1];
				if(newString)
					currVal = extractedString;
				else  // number value
				{
					if(endNumber == (unsigned int)-1 ||  // take i as end number if needed
					   endNumber <= startNumber)
						endNumber = i;
					// extract val
					if(endNumber <= startNumber)  // empty data, could be {}
						currVal = "";
					else
						currVal = json.substr(startNumber + 1, endNumber - startNumber - 1);
				}
				newValue = 1;  // new value to consider!
				// pop key
				jsonPath.pop_back();
				jsonPathType.pop_back();
			}
			// pop {
			if(jsonPathType[jsonPathType.size() - 1] != '{')
			{
				__COUT__ << "Invalid '}' position" << __E__;
				return -1;
			}
			lastPopType = '{';
			jsonPath.pop_back();
			jsonPathType.pop_back();
			--bracketCount;
			break;
		case '[':
			if(inQuotes)
				break;  // skip if in quote
			jsonPathType.push_back('[');
			jsonPath.push_back("[");
			++sqBracketCount;
			startNumber = i;
			break;
		case ']':
			if(inQuotes)
				break;  // skip if in quote

			// must be an array at this level (in order to close it)
			if(jsonPathType[jsonPathType.size() - 1] != '[')
			{
				__COUT__ << "Invalid ']' position" << __E__;
				return -1;
			}

			currDepth = bracketCount;

			// This is an array value
			if(newString)
				currVal = extractedString;
			else  // number value
			{
				if(endNumber == (unsigned int)-1 ||  // take i as end number if needed
				   endNumber <= startNumber)
					endNumber = i;
				// extract val
				if(endNumber <= startNumber)  // empty data, could be {}
					currVal = "";
				else
					currVal = json.substr(startNumber + 1, endNumber - startNumber - 1);
			}
			// isDataArray = 1;

			// key is last key
			for(unsigned int k = jsonPathType.size() - 2; k < jsonPathType.size(); --k)
				if(jsonPathType[k] == 'K')
				{
					currKey = jsonPath[k];
					break;
				}
				else if(k == 0)
				{
					__COUT__ << "Invalid array position" << __E__;
					return -1;
				}

			// pop [
			if(jsonPathType[jsonPathType.size() - 1] != '[')
			{
				__COUT__ << "Invalid ']' position" << __E__;
				return -1;
			}
			lastPopType = '[';
			jsonPath.pop_back();
			jsonPathType.pop_back();
			--sqBracketCount;
			break;
		case ' ':  // white space handling for numbers
		case '\t':
		case '\n':
		case '\r':
			if(inQuotes)
				break;  // skip if in quote
			if(startNumber != (unsigned int)-1 && endNumber == (unsigned int)-1)
				endNumber = i;
			startNumber = i;
			break;
		default:;
		}

		// continue;

		// handle a new completed value
		if(newValue)
		{
			if(0 && tableName_ == "ARTDAQ_DATALOGGER_TABLE")  // for debugging
			{
				std::cout << i << ":\t" << json[i] << " - ";

				//    		if(isDataArray)
				//    			std::cout << "Array:: ";
				//    		if(newString)
				//				std::cout << "New String:: ";
				//    		else
				//				std::cout << "New Number:: ";
				//

				std::cout << "ExtKey=";
				for(unsigned int k = 0; k < jsonPath.size(); ++k)
					std::cout << jsonPath[k] << "/";
				std::cout << " - ";
				std::cout << lastPopType << " ";
				std::cout << bracketCount << " ";
				std::cout << sqBracketCount << " ";
				std::cout << inQuotes << " ";
				std::cout << newValue << "-";
				std::cout << currKey << "-{" << currDepth << "}:";
				std::cout << currVal << " ";
				std::cout << startNumber << "-";
				std::cout << endNumber << " ";
				std::cout << "\n";
				__COUTV__(fillWithLooseColumnMatching_);
			}

			// extract only what we care about
			// for TableView only care about matching depth 1

			// handle matching depth 1 keys

			matchedKey = -1;  // init to unfound
			for(unsigned int k = 0; k < keys.size(); ++k)
				if((currDepth == 1 && keys[k] == currKey) || (currDepth > 1 && keys[k] == jsonPath[1]))
					matchedKey = k;

			if(rawData)
			{
				// raw data handling fills raw data string with row/col values

				if(currDepth == 1)
				{
					if(matchedKey == CV_JSON_FILL_COMMENT)
						setComment(currVal);
					else if(matchedKey == CV_JSON_FILL_AUTHOR)
						setAuthor(currVal);
					else if(matchedKey == CV_JSON_FILL_CREATION_TIME)
						setCreationTime(strtol(currVal.c_str(), 0, 10));
				}
				else if(currDepth == 2)
				{
					// encode URI component so commas are surviving delimiter
					sourceRawData_ += StringMacros::encodeURIComponent(currKey) + "," + StringMacros::encodeURIComponent(currVal) + ",";
					sourceColumnNames_.emplace(currKey);
				}
			}
			else if(matchedKey != (unsigned int)-1)
			{
				// std::cout << "New Data for:: key[" << matchedKey << "]-" <<
				//		keys[matchedKey] << "\n";

				switch(matchedKey)
				{
				case CV_JSON_FILL_NAME:
					// table name is now constant, set by parent TableBase
					if(currDepth == 1)
					{
						//	setTableName(currVal);
						// check for consistency, and show warning
						if(currVal != getTableName() &&
						   getTableName() != "TABLE_GROUP_METADATA")  // allow metadata table to be illegal, since it is created by ConfigurationManager.cc
							__COUT_WARN__ << "JSON-fill Table name mismatch: " << currVal << " vs " << getTableName() << __E__;
					}
					break;
				case CV_JSON_FILL_COMMENT:
					if(currDepth == 1)
						setComment(currVal);
					break;
				case CV_JSON_FILL_AUTHOR:
					if(currDepth == 1)
						setAuthor(currVal);
					break;
				case CV_JSON_FILL_CREATION_TIME:
					if(currDepth == 1)
						setCreationTime(strtol(currVal.c_str(), 0, 10));
					break;
					// case CV_JSON_FILL_COL_TYPES:
					//
					// break;
				case CV_JSON_FILL_DATA_SET:
					//					std::cout << "CV_JSON_FILL_DATA_SET New Data for::
					//"
					//<<  matchedKey << "]-" << 						keys[matchedKey]
					//<<
					//"/" << currDepth << ".../" << 						currKey <<
					//"\n";

					if(currDepth == 2)  // second level depth
					{
						// if matches first column name.. then add new row
						// else add to current row
						unsigned int col, ccnt = 0;
						unsigned int noc = getNumberOfColumns();
						for(; ccnt < noc; ++ccnt)
						{
							// use colSpeedup to change the first column we search
							// for each iteration.. since we expect the data to
							// be arranged in column order

							if(fillWithLooseColumnMatching_)
							{
								// loose column matching makes no attempt to
								// match the column names
								// just assumes the data is in the correct order

								col = colSpeedup;

								// auto matched
								if(col <= lastCol)  // add row (use lastCol in case new
								                    // column-0 was added
									row = addRow();
								lastCol = col;
								if(getNumberOfRows() == 1)  // only for first row
									sourceColumnNames_.emplace(currKey);

								// add value to row and column

								if(row >= getNumberOfRows())
								{
									__SS__ << "Invalid row" << __E__;  // should be impossible?
									std::cout << ss.str();
									__SS_THROW__;
									return -1;
								}

								theDataView_[row][col] = currVal;  // THERE IS NO CHECK FOR WHAT IS READ FROM
								                                   // THE DATABASE. IT SHOULD BE ALREADY
								                                   // CONSISTENT
								break;
							}
							else
							{
								col = (ccnt + colSpeedup) % noc;

								// match key by ignoring '_'
								//	also accept COMMENT == COMMENT_DESCRIPTION
								//	(this is for backwards compatibility..)
								keyIsMatch   = true;
								keyIsComment = true;
								for(keyIsMatchIndex = 0, keyIsMatchStorageIndex = 0, keyIsMatchCommentIndex = 0; keyIsMatchIndex < currKey.size();
								    ++keyIsMatchIndex)
								{
									if(columnsInfo_[col].getStorageName()[keyIsMatchStorageIndex] == '_')
										++keyIsMatchStorageIndex;  // skip to next storage
										                           // character
									if(currKey[keyIsMatchIndex] == '_')
										continue;  // skip to next character

									// match to storage name
									if(keyIsMatchStorageIndex >= columnsInfo_[col].getStorageName().size() ||
									   currKey[keyIsMatchIndex] != columnsInfo_[col].getStorageName()[keyIsMatchStorageIndex])
									{
										// size mismatch or character mismatch
										keyIsMatch = false;
										if(!keyIsComment)
											break;
									}

									// check also if alternate comment is matched
									if(keyIsComment && keyIsMatchCommentIndex < COMMENT_ALT_KEY.size())
									{
										if(currKey[keyIsMatchIndex] != COMMENT_ALT_KEY[keyIsMatchCommentIndex])
										{
											// character mismatch with COMMENT
											keyIsComment = false;
										}
									}

									++keyIsMatchStorageIndex;  // go to next character
								}

								if(keyIsMatch || keyIsComment)  // currKey ==
								                                // columnsInfo_[c].getStorageName())
								{
									if(keyEntryCountMap.find(currKey) == keyEntryCountMap.end())
										keyEntryCountMap[currKey] = 0;  // show follow row count
									else
										++keyEntryCountMap.at(currKey);

									// add row (based on entry counts)
									if(keyEntryCountMap.size() == 1 ||
									   (keyEntryCountMap.at(currKey) && keyEntryCountMap.at(currKey) > row))  // if(col <= lastCol)
									{
										if(getNumberOfRows())  // skip first time
											sourceColumnMissingCount_ += getNumberOfColumns() - colFoundCount;

										colFoundCount = 0;  // reset column found count
										row           = addRow();
									}
									lastCol = col;
									++colFoundCount;

									if(getNumberOfRows() == 1)  // only for first row
										sourceColumnNames_.emplace(currKey);

									// add value to row and column

									if(row >= getNumberOfRows())
									{
										__SS__ << "Invalid row" << __E__;  // should be impossible?!
										__COUT__ << "\n" << ss.str();
										__SS_THROW__;
										return -1;  // never gets here
									}

									theDataView_[row][col] = currVal;
									break;
								}
							}
						}

						if(ccnt >= getNumberOfColumns())
						{
							__COUT__ << "Invalid column in JSON source data: " << currKey << " not found in column names of table named " << getTableName()
							         << "." << __E__;  // input data doesn't match config description

							// CHANGED on 11/10/2016
							//	to.. try just not populating data instead of error
							++sourceColumnMismatchCount_;  // but count errors
							if(getNumberOfRows() == 1)     // only for first row, track source column names
								sourceColumnNames_.emplace(currKey);

							//__SS_THROW__;
							__COUT_WARN__ << "Trying to ignore error, and not populating "
							                 "missing column."
							              << __E__;
						}
						else  // short cut to proper column hopefully in next search
							colSpeedup = (colSpeedup + 1) % noc;
					}
					break;
				default:;  // unknown match?
				}          // end switch statement to match json key
			}              // end matched key if statement

			// clean up handling of new value

			newString = 0;  // toggle flag
			newValue  = 0;  // toggle flag
			// isDataArray = 0;
			endNumber = -1;  // reset end number index
		}

		// if(i>200) break; //185
	}

	//__COUT__ << "Done!" << __E__;
	//__COUTV__(fillWithLooseColumnMatching_);
	//__COUTV__(tableName_); //  << "tableName_ = " << tableName_

	if(!fillWithLooseColumnMatching_ && sourceColumnMissingCount_ > 0)
	{
		__COUTV__(sourceColumnMissingCount_);
		__SS__ << "Can not ignore errors because not every column was found in the source data!"
		       << ". Please see the details below:\n\n"
		       << getMismatchColumnInfo() << StringMacros::stackTrace();
		__SS_ONLY_THROW__;
	}

	// print();

	return 0;  // success
}  // end fillFromJSON()

//==============================================================================
std::string TableView::getMismatchColumnInfo(void) const
{
	const std::set<std::string>& srcColNames  = getSourceColumnNames();
	std::set<std::string>        destColNames = getColumnStorageNames();

	__SS__ << "The source column size was found to be " << srcColNames.size() << ", and the current number of columns for this table is "
	       << getNumberOfColumns() << ". This resulted in a count of " << getSourceColumnMismatch() << " source column mismatches, and a count of "
	       << getSourceColumnMissing() << " table entries missing in " << getNumberOfRows() << " row(s) of data." << __E__;

	ss << "\n\n" << srcColNames.size() << " Source column names in ALPHABETICAL order were as follows:\n";
	char        index       = 'a';
	std::string preIndexStr = "";
	for(auto& srcColName : srcColNames)
	{
		if(destColNames.find(srcColName) == destColNames.end())
			ss << "\n\t*** " << preIndexStr << index << ". " << srcColName << " ***";
		else
			ss << "\n\t" << preIndexStr << index << ". " << srcColName;

		if(index == 'z')  // wrap-around
		{
			preIndexStr += 'a';  // keep adding index 'digits' for wrap-around
			index = 'a';
		}
		else
			++index;
	}
	ss << __E__;

	ss << "\n\n" << destColNames.size() << " Current table column names in ALPHABETICAL order are as follows:\n";
	index       = 'a';
	preIndexStr = "";
	for(auto& destColName : destColNames)
	{
		if(srcColNames.find(destColName) == srcColNames.end())
			ss << "\n\t*** " << preIndexStr << index << ". " << destColName << " ***";
		else
			ss << "\n\t" << preIndexStr << index << ". " << destColName;

		if(index == 'z')  // wrap-around
		{
			preIndexStr += 'a';  // keep adding index 'digits' for wrap-around
			index = 'a';
		}
		else
			++index;
	}
	ss << __E__;
	return ss.str();
}  // end getMismatchColumnInfo()

//==============================================================================
bool TableView::isURIEncodedCommentTheSame(const std::string& comment) const
{
	std::string compareStr = StringMacros::decodeURIComponent(comment);
	return comment_ == compareStr;
}
//
////==============================================================================
// bool TableView::isValueTheSame(const std::string &valueStr,
//		unsigned int r, unsigned int c) const
//{
//	__COUT__ << "valueStr " << valueStr << __E__;
//
//	if(!(c < columnsInfo_.size() && r < getNumberOfRows()))
//	{
//		__SS__ << "Invalid row (" << (int)r << ") col (" << (int)c << ") requested!" <<
//__E__;
//		__SS_THROW__;
//	}
//
//	__COUT__ << "originalValueStr " << theDataView_[r][c] << __E__;
//
//	if(columnsInfo_[c].getDataType() == TableViewColumnInfo::DATATYPE_TIME)
//	{
//		time_t valueTime(strtol(valueStr.c_str(),0,10));
//		time_t originalValueTime;
//		getValue(originalValueTime,r,c);
//		__COUT__ << "time_t valueStr " << valueTime << __E__;
//		__COUT__ << "time_t originalValueStr " << originalValueTime << __E__;
//		return valueTime == originalValueTime;
//	}
//	else
//	{
//		return valueStr == theDataView_[r][c];
//	}
//}

//==============================================================================
// fillFromCSV
//	Fills the view from the CSV string.
//
//	Note: converts all %## to the ascii character, # is hex nibble
// (e.g. '%' must be represented as "%25")
//
//	dataOffset := starting destination row
//
//	while there are row entries in the data.. replace
// data range from [dataOffset, dataOffset+chunkSize-1]
// ... note if less rows, this means rows were deleted
// ... if more, then rows were added.
//
//	',' next cell delimiter
//  ';' next row delimiter
//
//
//	if author == "", do nothing special for author and timestamp column
//	if author != "", assign author for any row that has been modified, and assign now as
// timestamp
//
//	Returns -1 if data was same and pre-existing content
//	Returns 1 if data was same, but columns are different
//	otherwise 0
//
int TableView::fillFromCSV(const std::string& data, const int& dataOffset, const std::string& author)
{
	int retVal = 0;

	int r = dataOffset;
	int c = 0;

	int i = 0;                  // use to parse data std::string
	int j = data.find(',', i);  // find next cell delimiter
	int k = data.find(';', i);  // find next row delimiter

	bool         rowWasModified;
	unsigned int countRowsModified = 0;
	int          authorCol         = findColByType(TableViewColumnInfo::TYPE_AUTHOR);
	int          timestampCol      = findColByType(TableViewColumnInfo::TYPE_TIMESTAMP);
	// std::string valueStr, tmpTimeStr, originalValueStr;

	while(k != (int)(std::string::npos))
	{
		rowWasModified = false;
		if(r >= (int)getNumberOfRows())
		{
			addRow();
			//__COUT__ << "Row added" << __E__;
			rowWasModified = true;
		}

		while(j < k && j != (int)(std::string::npos))
		{
			//__COUT__ << "Col " << (int)c << __E__;

			// skip last 2 columns
			if(c >= (int)getNumberOfColumns() - 2)
			{
				i = j + 1;
				j = data.find(',', i);  // find next cell delimiter
				++c;
				continue;
			}

			if(setURIEncodedValue(data.substr(i, j - i), r, c))
				rowWasModified = true;

			i = j + 1;
			j = data.find(',', i);  // find next cell delimiter
			++c;
		}

		// if row was modified, assign author and timestamp
		if(author != "" && rowWasModified)
		{
			__COUT__ << "Row=" << (int)r << " was modified!" << __E__;
			setValue(author, r, authorCol);
			setValue(time(0), r, timestampCol);
		}

		if(rowWasModified)
			++countRowsModified;

		++r;
		c = 0;

		i = k + 1;
		j = data.find(',', i);  // find next cell delimiter
		k = data.find(';', i);  // find new row delimiter
	}

	// delete excess rows
	while(r < (int)getNumberOfRows())
	{
		deleteRow(r);
		__COUT__ << "Row deleted: " << (int)r << __E__;
		++countRowsModified;
	}

	__COUT_INFO__ << "countRowsModified=" << countRowsModified << __E__;

	if(!countRowsModified)
	{
		// check that source columns match storage name
		// otherwise allow same data...

		bool match = getColumnStorageNames().size() == getSourceColumnNames().size();
		if(match)
		{
			for(auto& destColName : getColumnStorageNames())
				if(getSourceColumnNames().find(destColName) == getSourceColumnNames().end())
				{
					__COUT__ << "Found column name mismach for '" << destColName << "'... So allowing same data!" << __E__;

					match = false;
					break;
				}
		}
		// if still a match, do not allow!
		if(match)
		{
			__SS__ << "No rows were modified! No reason to fill a view with same content." << __E__;
			__COUT__ << "\n" << ss.str();
			return -1;
		}
		// else mark with retVal
		retVal = 1;
	}  // end same check

	// print(); //for debugging

	// setup sourceColumnNames_ to be correct
	sourceColumnNames_.clear();
	for(unsigned int i = 0; i < getNumberOfColumns(); ++i)
		sourceColumnNames_.emplace(getColumnsInfo()[i].getStorageName());

	init();  // verify new table (throws runtime_errors)

	// printout for debugging
	//	__SS__ << "\n";
	//	print(ss);
	//	__COUT__ << "\n" << ss.str() << __E__;

	return retVal;
}  // end fillFromCSV()

//==============================================================================
// setURIEncodedValue
//	converts all %## to the ascii character
//	returns true if value was different than original value
//
//
//	if author == "", do nothing special for author and timestamp column
//	if author != "", assign author for any row that has been modified, and assign now as
// timestamp
bool TableView::setURIEncodedValue(const std::string& value, const unsigned int& r, const unsigned int& c, const std::string& author)
{
	if(!(c < columnsInfo_.size() && r < getNumberOfRows()))
	{
		__SS__ << "Invalid row (" << (int)r << ") col (" << (int)c << ") requested!"
		       << "Number of Rows = " << getNumberOfRows() << "Number of Columns = " << columnsInfo_.size() << __E__;
		print(ss);
		__SS_THROW__;
	}

	std::string valueStr         = StringMacros::decodeURIComponent(value);
	std::string originalValueStr = getValueAsString(r, c, false);  // do not convert env variables

	//__COUT__ << "valueStr " << valueStr << __E__;
	//__COUT__ << "originalValueStr " << originalValueStr << __E__;

	if(columnsInfo_[c].getDataType() == TableViewColumnInfo::DATATYPE_NUMBER)
	{
		// check if valid number
		std::string convertedString = StringMacros::convertEnvironmentVariables(valueStr);
		// do not check here, let init check
		//	if this is a link to valid number, then this is an improper check.
		//		if(!StringMacros::isNumber(convertedString))
		//		{
		//			__SS__ << "\tIn configuration " << tableName_
		//			       << " at column=" << columnsInfo_[c].getName() << " the value
		// set
		//("
		//			       << convertedString << ")"
		//			       << " is not a number! Please fix it or change the column
		// type..."
		//			       << __E__;
		//			__SS_THROW__;
		//		}
		theDataView_[r][c] = valueStr;

		// is it here that a new exception should be added to enforce min and max, given that they only appear with number type?
	}
	else if(columnsInfo_[c].getDataType() == TableViewColumnInfo::DATATYPE_TIME)
	{
		//				valueStr = StringMacros::decodeURIComponent(data.substr(i,j-i));
		//
		//				getValue(tmpTimeStr,r,c);
		//				if(valueStr != tmpTimeStr)//theDataView_[r][c])
		//				{
		//					__COUT__ << "valueStr=" << valueStr <<
		//							" theDataView_[r][c]=" << tmpTimeStr << __E__;
		//					rowWasModified = true;
		//				}

		setValue(time_t(strtol(valueStr.c_str(), 0, 10)), r, c);
	}
	else
		theDataView_[r][c] = valueStr;

	bool rowWasModified = (originalValueStr != getValueAsString(r, c, false));  // do not convert env variables

	// if row was modified, assign author and timestamp
	if(author != "" && rowWasModified)
	{
		__COUT__ << "Row=" << (int)r << " was modified!" << __E__;
		int authorCol    = findColByType(TableViewColumnInfo::TYPE_AUTHOR);
		int timestampCol = findColByType(TableViewColumnInfo::TYPE_TIMESTAMP);
		setValue(author, r, authorCol);
		setValue(time(0), r, timestampCol);
	}

	return rowWasModified;
}  // end setURIEncodedValue()

//==============================================================================
void TableView::resizeDataView(unsigned int nRows, unsigned int nCols)
{
	// FIXME This maybe should disappear but I am using it in ConfigurationHandler
	// still...
	theDataView_.resize(nRows, std::vector<std::string>(nCols));
}

//==============================================================================
// addRow
//	returns index of added row, always is last row
//	return -1 on failure (throw error)
//
//	if baseNameAutoUID != "", creates a UID based on this base name
//		and increments and appends an integer relative to the previous last row
unsigned int TableView::addRow(const std::string& author,
                               unsigned char      incrementUniqueData /*= false */,  // leave as unsigned char rather than
                               // bool, too many things (e.g. strings)
                               // evaluate successfully to bool values
                               const std::string& baseNameAutoUID /*= "" */,
                               unsigned int       rowToAdd /*= -1 */)
{
	// default to last row
	if(rowToAdd == (unsigned int)-1)
		rowToAdd = getNumberOfRows();

	theDataView_.resize(getNumberOfRows() + 1, std::vector<std::string>(getNumberOfColumns()));

	// shift data down the table if necessary
	for(unsigned int r = getNumberOfRows() - 2; r >= rowToAdd; --r)
	{
		if(r == (unsigned int)-1)
			break;  // quit wrap around case
		for(unsigned int col = 0; col < getNumberOfColumns(); ++col)
			theDataView_[r + 1][col] = theDataView_[r][col];
	}

	std::vector<std::string> defaultRowValues = getDefaultRowValues();

	// char         indexString[1000];
	std::string tmpString, baseString;
	// bool         foundAny;
	// unsigned int index;
	// unsigned int maxUniqueData;
	std::string numString;

	// fill each col of new row with default values
	//	if a row is a unique data row, increment last row in attempt to make a legal
	// column
	for(unsigned int col = 0; col < getNumberOfColumns(); ++col)
	{
		// if(incrementUniqueData)
		// 	__COUT__ << col << " " << columnsInfo_[col].getType() << " basename= " <<
		// 		baseNameAutoUID << __E__;

		// baseNameAutoUID indicates to attempt to make row unique
		//	add index to max number
		if(incrementUniqueData && (col == getColUID() || columnsInfo_[col].isChildLinkGroupID() ||
		                           (getNumberOfRows() > 1 && (columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_UNIQUE_DATA ||
		                                                      columnsInfo_[col].getType() == TableViewColumnInfo::TYPE_UNIQUE_GROUP_DATA))))
		{
			if(col == getColUID() || columnsInfo_[col].isChildLinkGroupID())
				setUniqueColumnValue(rowToAdd, col, baseNameAutoUID /*baseValueAsString*/);
			else
				setUniqueColumnValue(rowToAdd, col);
		}
		else
			theDataView_[rowToAdd][col] = defaultRowValues[col];
	}

	if(author != "")
	{
		__COUT__ << "Row=" << rowToAdd << " was created!" << __E__;

		int authorCol    = findColByType(TableViewColumnInfo::TYPE_AUTHOR);
		int timestampCol = findColByType(TableViewColumnInfo::TYPE_TIMESTAMP);
		setValue(author, rowToAdd, authorCol);
		setValue(time(0), rowToAdd, timestampCol);
	}

	return rowToAdd;
}  // end addRow()

//==============================================================================
// deleteRow
//	throws exception on failure
void TableView::deleteRow(int r)
{
	if(r >= (int)getNumberOfRows())
	{
		// out of bounds
		__SS__ << "Row " << (int)r << " is out of bounds (Row Count = " << getNumberOfRows() << ") and can not be deleted." << __E__;
		__SS_THROW__;
	}

	theDataView_.erase(theDataView_.begin() + r);
}  // end deleteRow()

//==============================================================================
// getChildLink ~
//	find the pair of columns associated with a child link.
//
//	c := a member column of the pair
//
//	returns:
//		isGroup := indicates pair found is a group link
//		linkPair := pair of columns that are part of the link
//
//	a unique link is defined by two columns: TYPE_START_CHILD_LINK,
// TYPE_START_CHILD_LINK_UID
//  a group link is defined by two columns: TYPE_START_CHILD_LINK,
//  TYPE_START_CHILD_LINK_GROUP_ID
//
//	returns true if column is member of a group or unique link.
bool TableView::getChildLink(const unsigned int& c, bool& isGroup, std::pair<unsigned int /*link col*/, unsigned int /*link id col*/>& linkPair) const
{
	if(!(c < columnsInfo_.size()))
	{
		__SS__ << "Invalid col (" << (int)c << ") requested for child link!" << __E__;
		__SS_THROW__;
	}

	//__COUT__ << "getChildLink for col: " << (int)c << "-" <<
	//		columnsInfo_[c].getType() << "-" << columnsInfo_[c].getName() << __E__;

	// check if column is a child link UID
	if((isGroup = columnsInfo_[c].isChildLinkGroupID()) || columnsInfo_[c].isChildLinkUID())
	{
		// must be part of unique link, (or invalid table?)
		//__COUT__ << "col: " << (int)c << __E__;
		linkPair.second   = c;
		std::string index = columnsInfo_[c].getChildLinkIndex();

		//__COUT__ << "index: " << index << __E__;

		// find pair link
		for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
		{
			//__COUT__ << "try: " << col << "-" << columnsInfo_[col].getType() << "-" <<
			// columnsInfo_[col].getName() << __E__;
			if(col == c)
				continue;  // skip column c that we know
			else if(columnsInfo_[col].isChildLink() && index == columnsInfo_[col].getChildLinkIndex())
			{
				// found match!
				//__COUT__ << "getChildLink Found match for col: " << (int)c << " at " <<
				// col << __E__;
				linkPair.first = col;
				return true;
			}
		}

		// if here then invalid table!
		__SS__ << "\tIn view: " << tableName_ << ", Can't find complete child link for column name " << columnsInfo_[c].getName() << __E__;
		__SS_THROW__;
	}

	if(!columnsInfo_[c].isChildLink())
		return false;  // cant be unique link

	// this is child link, so find pair link uid or gid column
	linkPair.first    = c;
	std::string index = columnsInfo_[c].getChildLinkIndex();

	//__COUT__ << "index: " << index << __E__;

	// find pair link
	for(unsigned int col = 0; col < columnsInfo_.size(); ++col)
	{
		//__COUT__ << "try: " << col << "-" << columnsInfo_[col].getType() << "-" <<
		// columnsInfo_[col].getName() << __E__;
		if(col == c)
			continue;  // skip column c that we know
		//		__COUT__ << "try: " << col << "-" << columnsInfo_[col].getType() <<
		//				"-" << columnsInfo_[col].getName() <<
		//				"-u" << columnsInfo_[col].isChildLinkUID() <<
		//				"-g" << columnsInfo_[col].isChildLinkGroupID() << __E__;
		//
		//		if(columnsInfo_[col].isChildLinkUID())
		//			__COUT__ << "-L" << columnsInfo_[col].getChildLinkIndex() << __E__;
		//
		//		if(columnsInfo_[col].isChildLinkGroupID())
		//			__COUT__ << "-L" << columnsInfo_[col].getChildLinkIndex() << __E__;

		if(((columnsInfo_[col].isChildLinkUID() && !(isGroup = false)) || (columnsInfo_[col].isChildLinkGroupID() && (isGroup = true))) &&
		   index == columnsInfo_[col].getChildLinkIndex())
		{
			// found match!
			//__COUT__ << "getChildLink Found match for col: " << (int)c << " at " << col
			//<< __E__;
			linkPair.second = col;
			return true;
		}
	}

	// if here then invalid table!
	__SS__ << "\tIn view: " << tableName_ << ", Can't find complete child link id for column name " << columnsInfo_[c].getName() << __E__;
	__SS_THROW__;
}  // end getChildLink()
