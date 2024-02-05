#include "otsdaq/TableCore/TableBase.h"

#include <iostream>  // std::cout
#include <typeinfo>

#include "otsdaq/TableCore/TableInfoReader.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "TableBase"
#undef __COUT_HDR__
#define __COUT_HDR__ ("TableBase-" + getTableName() + "\t<> ")

const std::string TableBase::GROUP_CACHE_PREPEND = "GroupCache_";
const std::string TableBase::JSON_DOC_PREPEND = "JSONDoc_";

//==============================================================================
// TableBase
//	If a valid string pointer is passed in accumulatedExceptions
//	then allowIllegalColumns is set for InfoReader
//	If accumulatedExceptions pointer = 0, then illegal columns throw std::runtime_error
// exception
TableBase::TableBase(const std::string& tableName,
                     std::string*       accumulatedExceptions)
    : MAX_VIEWS_IN_CACHE(20)  // This is done, so that inheriting table classes could have
                              // varying amounts of cache
    , tableName_(tableName)
    , activeTableView_(0)
    , mockupTableView_(tableName)
{
	if(tableName == "")
	{
		__SS__ << "Do not allow anonymous table view construction!" << __E__;
		ss << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}

	// December 2021 started seeing an issue where traceTID is found to be cleared to 0
	//	which crashes TRACE if __COUT__ is used in a Table plugin constructor
	//	This check and re-initialization seems to cover up the issue for now.
	//	Why it is cleared to 0 after the constructor sets it to -1 is still unknown.
	//		Note: it seems to only happen on the first alphabetially ARTDAQ Configure Table plugin.
	if(traceTID == 0)
	{
		std::cout << "TableBase Before traceTID=" << traceTID << __E__;
		char buf[40];
		traceInit(trace_name(TRACE_NAME, __TRACE_FILE__, buf, sizeof(buf)), 0);
		std::cout << "TableBase After traceTID=" << traceTID << __E__;
		__COUT__ << "TableBase TRACE reinit and Constructed." << __E__;
	}

	//if special GROUP CACHE table, handle construction in a special way
	if(tableName.substr(0,TableBase::GROUP_CACHE_PREPEND.length()) == TableBase::GROUP_CACHE_PREPEND ||
		tableName.substr(0,TableBase::JSON_DOC_PREPEND.length()) == TableBase::JSON_DOC_PREPEND)
	{
		__COUT__ << "TableBase for '" << tableName << "' constructed." << __E__;
		return;
	} //end special GROUP CACHE table construction

	bool dbg = false;  // tableName == "ARTDAQEventBuilderTable";
	if(dbg)
		__COUTV__(tableName);
	// info reader fills up the mockup view
	TableInfoReader tableInfoReader(accumulatedExceptions);
	if(dbg)
		__COUT__ << "Reading..." << __E__;
	try  // to read info
	{
		std::string returnedExceptions = tableInfoReader.read(this);
		if(dbg)
			__COUT__ << "Read.";
		if(returnedExceptions != "")
			__COUT_ERR__ << returnedExceptions << __E__;

		if(accumulatedExceptions)
			*accumulatedExceptions += std::string("\n") + returnedExceptions;
	}
	catch(...)  // if accumulating exceptions, continue to and return, else throw
	{
		__SS__ << "Failure in tableInfoReader.read(this). "
		       << "Perhaps you need to run otsdaq_convert_config_to_table ?" << __E__;
		__COUT_ERR__ << "\n" << ss.str();
		if(accumulatedExceptions)
			*accumulatedExceptions += std::string("\n") + ss.str();
		else
			throw;
		return;  // do not proceed with mockup check if this failed
	}
	if(dbg)
		__COUT__ << "Initializing..." << __E__;
	// call init on mockup view to verify columns
	try
	{
		getMockupViewP()->init();
		if(dbg)
			__COUT__ << "Init." << __E__;
	}
	catch(std::runtime_error& e)  // if accumulating exceptions, continue to and return, else throw
	{
		if(accumulatedExceptions)
			*accumulatedExceptions += std::string("\n") + e.what();
		else
			throw;
	}
}  // end constructor()

//==============================================================================
// TableBase
//	Default constructor is only used  to create special tables
//		not based on an ...Info.xml file
//	e.g. the TableGroupMetadata table in ConfigurationManager
TableBase::TableBase(bool specialTable, const std::string& specialTableName)
    : MAX_VIEWS_IN_CACHE(1)  // This is done, so that inheriting table classes could have
                             // varying amounts of cache
    , tableName_(specialTableName)
    , activeTableView_(0)
    , mockupTableView_(specialTableName)
{
	__COUT__ << "Special table '" << tableName_ << "' constructed. " << specialTable << __E__;
}  // special table constructor()

////==============================================================================
// TableBase::TableBase(void)
//  : MAX_VIEWS_IN_CACHE(1)
//  {
//	__SS__ << "Should not call void constructor, table type is lost!" << __E__;
//	ss << StringMacros::stackTrace() << __E__;
//	__SS_THROW__;
// }

//==============================================================================
TableBase::~TableBase(void) {}

//==============================================================================
std::string TableBase::getTypeId() { return typeid(this).name(); }

//==============================================================================
void TableBase::init(ConfigurationManager* /*tableManager*/)
{
	//__COUT__ << "Default TableBase::init() called." << __E__;
}

//==============================================================================
void TableBase::reset(bool keepTemporaryVersions)
{
	// std::cout << __COUT_HDR_FL__ << "resetting" << __E__;
	deactivate();
	if(keepTemporaryVersions)
		trimCache(0);
	else  // clear all
		tableViews_.clear();
}

//==============================================================================
void TableBase::print(std::ostream& out) const
{
	// std::cout << __COUT_HDR_FL__ << "activeVersion_ " << activeVersion_ << "
	// (INVALID_VERSION:=" << INVALID_VERSION << ")" << __E__;
	if(!activeTableView_)
	{
		__COUT_ERR__ << "ERROR: No active view set" << __E__;
		return;
	}
	activeTableView_->print(out);
}

//==============================================================================
// makes active version the specified table view version
//  if the version is not already stored, then creates a mockup version
void TableBase::setupMockupView(TableVersion version)
{
	if(!isStored(version))
	{
		tableViews_.emplace(std::make_pair(version, TableView(tableName_)));
		tableViews_.at(version).copy(mockupTableView_, version, mockupTableView_.getAuthor());
		trimCache();
		if(!isStored(version))  // the trim cache is misbehaving!
		{
			__SS__ << "IMPOSSIBLE ERROR: trimCache() is deleting the "
			          "latest view version "
			       << version << "!" << __E__;
			__SS_THROW__;
		}
	}
	else
	{
		__SS__ << "View to fill with mockup already exists: " << version << ". Cannot overwrite!" << __E__;
		ss << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}
}  // end setupMockupView()

//==============================================================================
// trimCache
//	if there are more views than MAX_VIEWS_IN_CACHE, erase them.
//	choose wisely the view to delete
//		(by access time)
void TableBase::trimCache(unsigned int trimSize)
{
	// delete cached views, if necessary

	if(trimSize == (unsigned int)-1)  // if -1, use MAX_VIEWS_IN_CACHE
		trimSize = MAX_VIEWS_IN_CACHE;

	// int i = 0;
	while(getNumberOfStoredViews() > trimSize)
	{
		TableVersion versionToDelete;
		time_t       stalestTime = -1;

		for(auto& viewPair : tableViews_)
			if(!viewPair.first.isTemporaryVersion())
			{
				if(stalestTime == -1 || viewPair.second.getLastAccessTime() < stalestTime)
				{
					versionToDelete = viewPair.first;
					stalestTime     = viewPair.second.getLastAccessTime();
					if(!trimSize)
						break;  // if trimSize is 0, then just take first found
				}
			}

		if(versionToDelete.isInvalid())
		{
			__SS__ << "Can NOT have a stored view with an invalid version!" << __E__;
			__SS_THROW__;
		}

		eraseView(versionToDelete);
	}
}

//==============================================================================
// trimCache
//	if there are more views than MAX_VIEWS_IN_CACHE, erase them.
//	choose wisely the view to delete
//		(by access time)
void TableBase::trimTemporary(TableVersion targetVersion)
{
	if(targetVersion.isInvalid())  // erase all temporary
	{
		for(auto it = tableViews_.begin(); it != tableViews_.end(); /*no increment*/)
		{
			if(it->first.isTemporaryVersion())
			{
				//__COUT__ << "Trimming temporary version: " << it->first << __E__;
				if(activeTableView_ && getViewVersion() == it->first)  // if activeVersion is being erased!
					deactivate();                                      // deactivate active view, instead of guessing at next
					                                                   // active view
				tableViews_.erase(it++);
			}
			else
				++it;
		}
	}
	else if(targetVersion.isTemporaryVersion())  // erase target
	{
		//__COUT__ << "Trimming temporary version: " << targetVersion << __E__;
		eraseView(targetVersion);
	}
	else
	{
		// else this is a persistent version!
		__SS__ << "Temporary trim target was a persistent version: " << targetVersion << __E__;
		__SS_THROW__;
	}
}

//==============================================================================
// checkForDuplicate
// look for a duplicate of the needleVersion in the haystack
//	which is the cached views in tableViews_
//
//	Note: ignoreVersion is useful if you know another view is already identical
//		like when converting from temporary to persistent
//
//	Return invalid if no matches
TableVersion TableBase::checkForDuplicate(TableVersion needleVersion, TableVersion ignoreVersion) const
{
	auto needleIt = tableViews_.find(needleVersion);
	if(needleIt == tableViews_.end())
	{
		// else this is a persistent version!
		__SS__ << "needleVersion does not exist: " << needleVersion << __E__;
		__SS_THROW__;
	}

	const TableView* needleView = &(needleIt->second);
	unsigned int     rows       = needleView->getNumberOfRows();
	unsigned int     cols       = needleView->getNumberOfColumns();

	bool         match;
	unsigned int potentialMatchCount = 0;

	// needleView->print();

	// for each table in cache
	//	check each row,col
	auto viewPairReverseIterator = tableViews_.rbegin();
	for(; viewPairReverseIterator != tableViews_.rend(); ++viewPairReverseIterator)
	{
		if(viewPairReverseIterator->first == needleVersion)
			continue;  // skip needle version
		if(viewPairReverseIterator->first == ignoreVersion)
			continue;  // skip ignore version
		if(viewPairReverseIterator->first.isTemporaryVersion())
			continue;  // skip temporary versions

		if(viewPairReverseIterator->second.getNumberOfRows() != rows)
			continue;  // row mismatch

		if(viewPairReverseIterator->second.getDataColumnSize() != cols || viewPairReverseIterator->second.getSourceColumnMismatch() != 0)
			continue;  // col mismatch

		++potentialMatchCount;
		__COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Checking version... " << viewPairReverseIterator->first << __E__;

		// viewPairReverseIterator->second.print();

		// if column source names do not match then skip
		//	source names are potentially different from
		// getColumnsInfo()/getColumnStorageNames

		match = viewPairReverseIterator->second.getSourceColumnNames().size() == needleView->getSourceColumnNames().size();
		if(match)
		{
			for(auto& haystackColName : viewPairReverseIterator->second.getSourceColumnNames())
				if(needleView->getSourceColumnNames().find(haystackColName) == needleView->getSourceColumnNames().end())
				{
					__COUT__ << "Found column name mismatch for '" << haystackColName << "'... So allowing same data!" << __E__;

					match = false;
					break;
				}
		}

		// checking columnsInfo seems to be wrong approach, use getSourceColumnNames
		// (above) 		auto viewColInfoIt =
		// viewPairReverseIterator->second.getColumnsInfo().begin(); 		for(unsigned
		// int col=0; match && //note column size must already match
		//			viewPairReverseIterator->second.getColumnsInfo().size() > 3 &&
		//			col<viewPairReverseIterator->second.getColumnsInfo().size()-3;++col,viewColInfoIt++)
		//			if(viewColInfoIt->getName() !=
		//					needleView->getColumnsInfo()[col].getName())
		//			{
		//				match = false;
		////				__COUT__ << "Column name mismatch " << col << ":" <<
		////						viewColInfoIt->getName() << " vs " <<
		////						needleView->getColumnsInfo()[col].getName() << __E__;
		//			}

		for(unsigned int row = 0; match && row < rows; ++row)
		{
			for(unsigned int col = 0; col < cols - 2; ++col)  // do not consider author and timestamp
				if(viewPairReverseIterator->second.getDataView()[row][col] != needleView->getDataView()[row][col])
				{
					match = false;

					//					__COUT__ << "Value name mismatch " << col << ":"
					//							<<
					//							viewPairReverseIterator->second.getDataView()[row][col]
					//																			   << "[" <<
					//																			   viewPairReverseIterator->second.getDataView()[row][col].size()
					//																			   << "]" << 							" vs " <<
					//																			   needleView->getDataView()[row][col] << "["
					//																			   <<
					//																			   needleView->getDataView()[row][col].size()
					//																			   <<
					//																			   "]"
					//																			   <<
					//																			   __E__;

					break;
				}
		}
		if(match)
		{
			__COUT_INFO__ << "Duplicate version found: " << viewPairReverseIterator->first << __E__;
			return viewPairReverseIterator->first;
		}
	}  // end table version loop

	__COUT__ << "No duplicates found in " << potentialMatchCount << " potential matches." << __E__;
	return TableVersion();  // return invalid if no matches
}  // end checkForDuplicate()


//==============================================================================
// diffTwoVersions
//	return a report of differences among two versions
bool TableBase::diffTwoVersions(TableVersion v1, TableVersion v2, 
	std::stringstream* diffReport /* = 0 */,
	std::map<std::string /* uid */, std::vector<std::string /* colName */>>* v1ModifiedRecords /* = 0 */) const
{
	__COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Diffing version... " << v1 << " vs " << v2 << __E__;
	auto v1It = tableViews_.find(v1);
	if(v1It == tableViews_.end())
	{
		// else this is a persistent version!
		__SS__ << "Version v" << v1 << " does not exist." << __E__;		
		__SS_THROW__;
	}
	auto v2It = tableViews_.find(v2);
	if(v2It == tableViews_.end())
	{
		// else this is a persistent version!
		__SS__ << "Version v" << v2 << " does not exist." << __E__;		
		__SS_THROW__;
	}

	const TableView* view1 = &(v1It->second);
	const TableView* view2 = &(v2It->second);
	unsigned int     rows1       = view1->getNumberOfRows();
	unsigned int     cols1       = view1->getNumberOfColumns();

	bool         noDifference = true;

	//	check each row,col

	// if column source names do not match then note
	//	source names are potentially different from
	// getColumnsInfo()/getColumnStorageNames

	if(view1->getSourceColumnNames().size() != view2->getSourceColumnNames().size())
	{
		__COUT__ << "Found column count mismatch for '" << view1->getSourceColumnNames().size() << 
			" vs " << view2->getSourceColumnNames().size() << __E__;

		if(diffReport) *diffReport << "<li>Found column count mismatch. The v" << v1 << " column count is <b>'" << view1->getSourceColumnNames().size() << 
			"'</b> and the v" << v2 << " column count is <b>'" << view2->getSourceColumnNames().size() << "'</b>." << __E__;

		noDifference = false;	
		if(!diffReport) return noDifference; //do not need to continue to create report
	}

	for(auto& colName1 : view1->getSourceColumnNames())
		if(view2->getSourceColumnNames().find(colName1) == view2->getSourceColumnNames().end())
		{
			__COUT__ << "Found column name mismatch for '" << colName1 << __E__;

			if(diffReport) *diffReport << "<li>Found column name mismatch. The v" << v1 << " column <b>'" << colName1 << 
				"'</b> was not found in v" << v2 << "." << __E__;

			noDifference = false;	
			if(!diffReport) return noDifference; //do not need to continue to create report
		}
	for(auto& colName2 : view2->getSourceColumnNames())
		if(view1->getSourceColumnNames().find(colName2) == view1->getSourceColumnNames().end())
		{
			__COUT__ << "Found column name mismatch for '" << colName2 << __E__;

			if(diffReport) *diffReport << "<li>Found column name mismatch. The v" << v1 << " does not have column <b>'" << colName2 << 
				"'</b> that was found in v" << v2 << "." << __E__;

			noDifference = false;	
			if(!diffReport) return noDifference; //do not need to continue to create report
		}

	if(rows1 != view2->getNumberOfRows())
	{
		__COUT__ << "Found row count mismatch for '" << rows1 << 
			" vs " << view2->getNumberOfRows() << __E__;

		if(diffReport) *diffReport << "<li>Found row count mismatch. The v" << v1 << " row count is <b>'" << rows1 << 
			"'</b> and the v" << v2 << " row count is <b>'" << view2->getNumberOfRows() << "'</b>." << __E__;

		noDifference = false;	
		if(!diffReport) return noDifference; //do not need to continue to create report
	}

	//report on missing UIDs
	std::set<std::string /*uid*/> uidSet1,uidSet2;
	for(unsigned int row = 0; row < rows1; ++row)
		uidSet1.insert(view1->getDataView()[row][view1->getColUID()]);
	for(unsigned int row = 0; row < view2->getNumberOfRows(); ++row)
		uidSet2.insert(view2->getDataView()[row][view2->getColUID()]);

	for(auto& uid1 : uidSet1)
		if(uidSet2.find(uid1) == uidSet2.end())
		{
			__COUT__ << "Found record name mismatch for '" << uid1 << __E__;

			if(diffReport) *diffReport << "<li>Found record name mismatch. The v" << v1 << " record <b>'" << uid1 << 
				"'</b> was not found in v" << v2 << "." << __E__;

			noDifference = false;	
			if(!diffReport) return noDifference; //do not need to continue to create report
		}
	for(auto& uid2 : uidSet2)
		if(uidSet1.find(uid2) == uidSet1.end())
		{
			__COUT__ << "Found record name mismatch for '" << uid2 << __E__;

			if(diffReport) *diffReport << "<li>Found record name mismatch. v" << v1 << " does not have record <b>'" << uid2 << 
				"'</b> that was found in v" << v2 << "." << __E__;

			noDifference = false;	
			if(!diffReport) return noDifference; //do not need to continue to create report
		}


	unsigned int row2, col2;
	for(unsigned int row = 0; row < rows1 && row < view2->getNumberOfRows(); ++row)
	{
		//do not evaluate if UIDs do not match
		row2 = row;
		if(view1->getDataView()[row][view1->getColUID()] != 
			view2->getDataView()[row2][view2->getColUID()])
		{
			bool foundUid2 = false;

			for(row2 = 0; row2 < view2->getNumberOfRows(); ++row2)
				if(view1->getDataView()[row][view1->getColUID()] == 
					view2->getDataView()[row2][view2->getColUID()])
				{
					foundUid2 = true;
					break;
				}
			__COUT__ << "Found row ? '" << foundUid2 << " " << row << "," << row2 << __E__;
			if(!foundUid2) continue; //skip view1 record because no matching record found in view2
		}
		
		__COUT__ << "Found row " << " " << row << "," << row2 << __E__;
		for(unsigned int col = 0; col < cols1 - 2 && 
			col < view2->getNumberOfColumns() - 2; ++col)  // do not consider author and timestamp
		{
			//do not evaluate if column names do not match
			col2 = col;
			if(view1->getColumnInfo(col).getName() != 
				view2->getColumnInfo(col2).getName())
			{
				bool foundCol2 = false;

				for(col2 = 0; col2 < view2->getNumberOfColumns() - 2; ++col2)
					if(view1->getColumnInfo(col).getName() == 
						view2->getColumnInfo(col2).getName())
					{
						foundCol2 = true;
						break;
					}
				
				__COUT__ << "Found column ? '" << foundCol2 << " " << col << "," << col2 << __E__;
				if(!foundCol2) continue; //skip view1 column because no matching column name was found in view2
			}

			__COUT__ << "Found column " << " " << col << "," << col2 << __E__;
			if(view1->getDataView()[row][col] != view2->getDataView()[row2][col2])
			{
				__COUT__ << "Found column value mismatch for '" << row << "," << col << " " << 
					view1->getDataView()[row][col] << __E__;

				if(diffReport) *diffReport << "<li><b>" <<
					view1->getColumnInfo(col).getName()
					<< "</b> value mismatch at v" << v1 << " {UID,r,c}:{<b>" <<
					view1->getDataView()[row][view1->getColUID()] << "</b>," <<
					row << "," << col << "}: <b>'"
					<<
					view1->getDataView()[row][col]
					<< "'</b> vs value in v" << v2 << ": <b>'" <<
					view2->getDataView()[row2][col2] << "'</b>." << __E__;

				noDifference = false;	
				if(!diffReport) return noDifference; //do not need to continue to create report

				if(v1ModifiedRecords) //add uid/colName difference
					(*v1ModifiedRecords)[view1->getDataView()[row][view1->getColUID()]].push_back(
						view1->getColumnInfo(col).getName());
			}
		}
	}

	if(noDifference && diffReport) *diffReport << "<li>No difference found between v" << v1 << " and v" << v2 << "." << __E__;

	return noDifference;  
}  // end diffTwoVersions()

//==============================================================================
void TableBase::changeVersionAndActivateView(TableVersion temporaryVersion, TableVersion version)
{
	auto tmpIt = tableViews_.find(temporaryVersion);
	if(tableViews_.find(temporaryVersion) == tableViews_.end())
	{
		__SS__ << "ERROR: Temporary view version " << temporaryVersion << " doesn't exists!" << __E__;
		__SS_THROW__;
	}
	if(version.isInvalid())
	{
		__SS__ << "ERROR: Attempting to create an invalid version " << version << "! Did you really run out of versions? (this should never happen)" << __E__;
		__SS_THROW__;
	}

	if(tableViews_.find(version) != tableViews_.end())
		__COUT_WARN__ << "WARNING: View version " << version << " already exists! Overwriting." << __E__;

	auto emplacePair /*it,bool*/ = tableViews_.emplace(std::make_pair(version, TableView(tableName_)));
	emplacePair.first->second.copy(tmpIt->second, version, tmpIt->second.getAuthor());
	setActiveView(version);
	eraseView(temporaryVersion);  // delete temp version from tableViews_
}

//==============================================================================
bool TableBase::isStored(const TableVersion& version) const { return (tableViews_.find(version) != tableViews_.end()); }

//==============================================================================
bool TableBase::eraseView(TableVersion version)
{
	if(!isStored(version))
		return false;

	if(activeTableView_ && getViewVersion() == version)  // if activeVersion is being erased!
		deactivate();                                    // deactivate active view, instead of guessing at next active view

	tableViews_.erase(version);

	return true;
}

//==============================================================================
const std::string& TableBase::getTableName(void) const { return tableName_; }

//==============================================================================
const std::string& TableBase::getTableDescription(void) const { return tableDescription_; }

//==============================================================================
const TableVersion& TableBase::getViewVersion(void) const { return getView().getVersion(); }

//==============================================================================
// latestAndMockupColumnNumberMismatch
//	intended to check if the column count was recently changed
bool TableBase::latestAndMockupColumnNumberMismatch(void) const
{
	std::set<TableVersion> retSet = getStoredVersions();
	if(retSet.size() && !retSet.rbegin()->isTemporaryVersion())
	{
		return tableViews_.find(*(retSet.rbegin()))->second.getNumberOfColumns() != mockupTableView_.getNumberOfColumns();
	}
	// there are no latest non-temporary tables so there is a mismatch (by default)
	return true;
}

//==============================================================================
std::set<TableVersion> TableBase::getStoredVersions(void) const
{
	std::set<TableVersion> retSet;
	for(auto& configs : tableViews_)
		retSet.emplace(configs.first);
	return retSet;
}

//==============================================================================
// getNumberOfStoredViews
//	count number of stored views, not including temporary views
//	(invalid views should be impossible)
unsigned int TableBase::getNumberOfStoredViews(void) const
{
	unsigned int sz = 0;
	for(auto& viewPair : tableViews_)
		if(viewPair.first.isTemporaryVersion())
			continue;
		else if(viewPair.first.isInvalid())
		{
			//__SS__ << "Can NOT have a stored view with an invalid version!" << __E__;
			//__SS_THROW__;

			// NOTE: if this starts happening a lot, could just auto-correct and remove
			// the invalid version
			// but it would be better to fix the cause.

			// FIXME... for now just auto correcting
			__COUT__ << "There is an invalid version now!.. where did it come from?" << __E__;
		}
		else
			++sz;
	return sz;
}  // end getNumberOfStoredViews()

//==============================================================================
const TableView& TableBase::getView(TableVersion version /* = TableVersion::INVALID */) const
{
	try
	{
		if(version != TableVersion::INVALID)
			return tableViews_.at(version);
	}
	catch(...)
	{
		__SS__ << "Table '" << tableName_ << "' does not have version v" << version << 
			" in the cache." << __E__;
		__SS_THROW__;
	}

	if(!activeTableView_)
	{
		__SS__ << "There is no active table view setup! Please check your system configuration." << __E__;
		__SS_ONLY_THROW__;
	}
	return *activeTableView_;
}

//==============================================================================
TableView* TableBase::getViewP(TableVersion version /* = TableVersion::INVALID */)
{
	try
	{
		if(version != TableVersion::INVALID)
			return &tableViews_.at(version);
	}
	catch(...)
	{
		__SS__ << "Table '" << tableName_ << "' does not have version v" << version << 
			" in the cache." << __E__;
		__SS_THROW__;
	}

	if(!activeTableView_)
	{
		__SS__ << "There is no active table view setup! Please check your system configuration." << __E__;
		__SS_ONLY_THROW__;
	}
	return activeTableView_;
}

//==============================================================================
TableView* TableBase::getMockupViewP(void) { return &mockupTableView_; }

//==============================================================================
void TableBase::setTableName(const std::string& tableName) { tableName_ = tableName; }

//==============================================================================
void TableBase::setTableDescription(const std::string& tableDescription) { tableDescription_ = tableDescription; }

//==============================================================================
// deactivate
//	reset the active view
void TableBase::deactivate() { activeTableView_ = 0; }

//==============================================================================
// isActive
bool TableBase::isActive() { return activeTableView_ ? true : false; }

//==============================================================================
bool TableBase::setActiveView(TableVersion version)
{
	if(!isStored(version))
	{  // we don't call else load for the user, because the table manager would lose
	   // track.. (I think?)
		// so load new versions for the first time through the table manager only. (I
		// think??)
		__SS__ << "\nsetActiveView() ERROR: View with version " << version << " has never been stored before!" << __E__;
		__SS_THROW__;
		return false;
	}
	activeTableView_ = &tableViews_.at(version);

	if(tableViews_.at(version).getVersion() != version)
	{
		__SS__ << "Something has gone very wrong with the version handling!" << __E__;
		__SS_THROW__;
	}

	return true;
}

//==============================================================================
// mergeViews
//	merges source view A and B and places in
//	destination temporary version.
//	if destination version is invalid, then next available temporary version is chosen
//	one error, throw exception
//
//	Returns version of new temporary view that was created.
TableVersion TableBase::mergeViews(
    const TableView&                                                                                                    sourceViewA,
    const TableView&                                                                                                    sourceViewB,
    TableVersion                                                                                                        destinationVersion,
    const std::string&                                                                                                  author,
    const std::string&                                                                                                  mergeApproach /*Rename,Replace,Skip*/,
    std::map<std::pair<std::string /*original table*/, std::string /*original uidB*/>, std::string /*converted uidB*/>& uidConversionMap,
    std::map<std::pair<std::string /*original table*/, std::pair<std::string /*group linkid*/, std::string /*original gidB*/> >,
             std::string /*converted gidB*/>&                                                                           groupidConversionMap,
    bool                                                                                                                fillRecordConversionMaps,
    bool                                                                                                                applyRecordConversionMaps,
    bool                                                                                                                generateUniqueDataColumns /*=false*/,
    std::stringstream*                                                                                                  mergeReport /*=nullptr*/)
{
	__COUT__ << "mergeViews starting..." << __E__;

	// clang-format off
	// There 3 modes:
	//	rename		-- All records from both groups are maintained, but conflicts from B are renamed.
	//					Must maintain a map of UIDs that are remapped to new name for
	//					because linkUID fields must be preserved.
	//	replace		-- Any UID conflicts for a record are replaced by the record from group B.
	//	skip		-- Any UID conflicts for a record are skipped so that group A record remains
	// clang-format on

	// check valid mode
	if(!(mergeApproach == "Rename" || mergeApproach == "Replace" || mergeApproach == "Skip"))
	{
		__SS__ << "Error! Invalid merge approach '" << mergeApproach << ".'" << __E__;
		__SS_THROW__;
	}

	// check that column sizes match
	if(sourceViewA.getNumberOfColumns() != mockupTableView_.getNumberOfColumns())
	{
		__SS__ << "Error! Number of Columns of source view A must match destination "
		          "mock-up view."
		       << "Dimension of source is [" << sourceViewA.getNumberOfColumns() << "] and of destination mockup is [" << mockupTableView_.getNumberOfColumns()
		       << "]." << __E__;
		__SS_THROW__;
	}
	// check that column sizes match
	if(sourceViewB.getNumberOfColumns() != mockupTableView_.getNumberOfColumns())
	{
		__SS__ << "Error! Number of Columns of source view B must match destination "
		          "mock-up view."
		       << "Dimension of source is [" << sourceViewB.getNumberOfColumns() << "] and of destination mockup is [" << mockupTableView_.getNumberOfColumns()
		       << "]." << __E__;
		__SS_THROW__;
	}

	// fill conversion map based on merge approach

	sourceViewA.print();
	sourceViewB.print();

	if(mergeReport)
		(*mergeReport) << "\n'" << mergeApproach << "'-Merging table '" << getTableName() << "' A=v" << sourceViewA.getVersion() << " with B=v"
		               << sourceViewB.getVersion() << __E__;

	if(fillRecordConversionMaps && mergeApproach == "Rename")
	{
		__COUT__ << "Filling record conversion map." << __E__;

		//	rename		-- All records from both groups are maintained, but conflicts from
		// B  are renamed.
		//					Must maintain a map of UIDs that are remapped to new name for
		// groupB, 					because linkUID fields must be preserved.

		// for each B record
		//	if there is a conflict, rename

		unsigned int uniqueId;
		std::string  uniqueIdString, uniqueIdBase;
		char         indexString[1000];
		unsigned int ra;
		unsigned int numericStartIndex;
		bool         found;

		for(unsigned int cb = 0; cb < sourceViewB.getNumberOfColumns(); ++cb)
		{
			// skip columns that are not UID or GroupID columns
			if(!(sourceViewA.getColumnInfo(cb).isUID() || sourceViewA.getColumnInfo(cb).isGroupID()))
				continue;

			__COUT__ << "Have an ID column: " << cb << " " << sourceViewA.getColumnInfo(cb).getType() << __E__;

			// at this point we have an ID column, verify B and mockup are the same
			if(sourceViewA.getColumnInfo(cb).getType() != sourceViewB.getColumnInfo(cb).getType() ||
			   sourceViewA.getColumnInfo(cb).getType() != mockupTableView_.getColumnInfo(cb).getType())
			{
				__SS__ << "Error! " << sourceViewA.getColumnInfo(cb).getType() << " column " << cb
				       << " of source view A must match source B and destination mock-up "
				          "view."
				       << " Column of source B is [" << sourceViewA.getColumnInfo(cb).getType() << "] and of destination mockup is ["
				       << mockupTableView_.getColumnInfo(cb).getType() << "]." << __E__;
				__SS_THROW__;
			}

			// getLinkGroupIDColumn(childLinkIndex)

			std::vector<std::string /*converted uidB*/> localConvertedIds;  // used for conflict completeness check

			if(sourceViewA.getColumnInfo(cb).isGroupID())
			{
				std::set<std::string> aGroupids = sourceViewA.getSetOfGroupIDs(cb);
				std::set<std::string> bGroupids = sourceViewB.getSetOfGroupIDs(cb);

				for(const auto& bGroupid : bGroupids)
				{
					if(aGroupids.find(bGroupid) == aGroupids.end())
						continue;

					// if here, found conflict
					__COUT__ << "found conflict: " << getTableName() << "/" << bGroupid << __E__;

					// extract starting uniqueId number
					{
						const std::string& str = bGroupid;
						numericStartIndex      = str.size();

						// find first non-numeric character
						while(numericStartIndex - 1 < str.size() && str[numericStartIndex - 1] >= '0' && str[numericStartIndex - 1] <= '9')
							--numericStartIndex;

						if(numericStartIndex < str.size())
						{
							uniqueId     = atoi(str.substr(numericStartIndex).c_str()) + 1;
							uniqueIdBase = str.substr(0, numericStartIndex);
						}
						else
						{
							uniqueId     = 0;
							uniqueIdBase = str;
						}

						__COUTV__(uniqueIdBase);
						__COUTV__(uniqueId);
					}  // end //extract starting uniqueId number

					// find unique id string
					{
						sprintf(indexString, "%u", uniqueId);
						uniqueIdString = uniqueIdBase + indexString;
						__COUTV__(uniqueIdString);

						found = false;
						// check converted records and source A and B for conflicts
						if(aGroupids.find(uniqueIdString) != aGroupids.end())
							found = true;
						if(!found && bGroupids.find(uniqueIdString) != bGroupids.end())
							found = true;
						if(!found && bGroupids.find(uniqueIdString) != bGroupids.end())
							found = true;
						for(ra = 0; !found && ra < localConvertedIds.size(); ++ra)
							if(localConvertedIds[ra] == uniqueIdString)
								found = true;

						while(found)  // while conflict, change id
						{
							++uniqueId;
							sprintf(indexString, "%u", uniqueId);
							uniqueIdString = uniqueIdBase + indexString;
							__COUTV__(uniqueIdString);

							found = false;
							// check converted records and source A and B for conflicts
							if(aGroupids.find(uniqueIdString) != aGroupids.end())
								found = true;
							if(!found && bGroupids.find(uniqueIdString) != bGroupids.end())
								found = true;
							if(!found && bGroupids.find(uniqueIdString) != bGroupids.end())
								found = true;
							for(ra = 0; !found && ra < localConvertedIds.size(); ++ra)
								if(localConvertedIds[ra] == uniqueIdString)
									found = true;
						}
					}  // end find unique id string

					// have unique id string now
					__COUTV__(uniqueIdString);

					groupidConversionMap[std::pair<std::string /*original table*/, std::pair<std::string /*group linkid*/, std::string /*original gidB*/> >(
					    getTableName(),
					    std::pair<std::string /*group linkid*/, std::string /*original gidB*/>(sourceViewB.getColumnInfo(cb).getChildLinkIndex(), bGroupid))] =
					    uniqueIdString;
					localConvertedIds.push_back(uniqueIdString);  // save to vector for
					                                              // future conflict
					                                              // checking within table

					if(mergeReport)
						(*mergeReport) << "\t"
						               << "Found conflicting B groupID for linkIndex '" << sourceViewB.getColumnInfo(cb).getChildLinkIndex()
						               << "' and renamed '" << bGroupid << "' to '" << uniqueIdString << "'" << __E__;

				}  // end row find unique id string loop for groupid

				// done creating conversion map
				__COUTV__(StringMacros::mapToString(groupidConversionMap));

			}     // end group id conversion map fill
			else  // start uid conversion map fill
			{
				for(unsigned int rb = 0; rb < sourceViewB.getNumberOfRows(); ++rb)
				{
					found = false;

					for(ra = 0; ra < sourceViewA.getDataView().size(); ++ra)
						if(sourceViewA.getValueAsString(ra, cb) == sourceViewB.getValueAsString(rb, cb))
						{
							found = true;
							break;
						}

					if(!found)
						continue;

					// found conflict
					__COUT__ << "found conflict: " << getTableName() << "/" << sourceViewB.getDataView()[rb][cb] << __E__;

					// extract starting uniqueId number
					{
						const std::string& str = sourceViewB.getDataView()[rb][cb];
						numericStartIndex      = str.size();

						// find first non-numeric character
						while(numericStartIndex - 1 < str.size() && str[numericStartIndex - 1] >= '0' && str[numericStartIndex - 1] <= '9')
							--numericStartIndex;

						if(numericStartIndex < str.size())
						{
							uniqueId     = atoi(str.substr(numericStartIndex).c_str()) + 1;
							uniqueIdBase = str.substr(0, numericStartIndex);
						}
						else
						{
							uniqueId     = 0;
							uniqueIdBase = str;
						}

						__COUTV__(uniqueIdBase);
						__COUTV__(uniqueId);
					}  // end //extract starting uniqueId number

					// find unique id string
					{
						sprintf(indexString, "%u", uniqueId);
						uniqueIdString = uniqueIdBase + indexString;
						__COUTV__(uniqueIdString);

						found = false;
						// check converted records and source A and B for conflicts
						for(ra = 0; !found && ra < sourceViewA.getDataView().size(); ++ra)
							if(sourceViewA.getValueAsString(ra, cb) == uniqueIdString)
								found = true;
						for(ra = 0; !found && ra < sourceViewB.getDataView().size(); ++ra)
							if(ra == rb)
								continue;  // skip record in question
							else if(sourceViewB.getValueAsString(ra, cb) == uniqueIdString)
								found = true;
						for(ra = 0; !found && ra < localConvertedIds.size(); ++ra)
							if(localConvertedIds[ra] == uniqueIdString)
								found = true;

						while(found)  // while conflict, change id
						{
							++uniqueId;
							sprintf(indexString, "%u", uniqueId);
							uniqueIdString = uniqueIdBase + indexString;
							__COUTV__(uniqueIdString);

							found = false;
							// check converted records and source A and B for conflicts
							for(ra = 0; !found && ra < sourceViewA.getDataView().size(); ++ra)
								if(sourceViewA.getValueAsString(ra, cb) == uniqueIdString)
									found = true;
							for(ra = 0; !found && ra < sourceViewB.getDataView().size(); ++ra)
								if(ra == rb)
									continue;  // skip record in question
								else if(sourceViewB.getValueAsString(ra, cb) == uniqueIdString)
									found = true;
							for(ra = 0; !found && ra < localConvertedIds.size(); ++ra)
								if(localConvertedIds[ra] == uniqueIdString)
									found = true;
						}
					}  // end find unique id string

					// have unique id string now
					__COUTV__(uniqueIdString);

					uidConversionMap[std::pair<std::string /*original table*/, std::string /*original uidB*/>(
					    getTableName(), sourceViewB.getValueAsString(rb, cb))] = uniqueIdString;
					localConvertedIds.push_back(uniqueIdString);  // save to vector for
					                                              // future conflict
					                                              // checking within table

					if(mergeReport)
						(*mergeReport) << "\t"
						               << "Found conflicting B UID and renamed '" << sourceViewB.getValueAsString(rb, cb) << "' to '" << uniqueIdString << "'"
						               << __E__;
				}  // end row find unique id string loop

				// done creating conversion map
				__COUTV__(StringMacros::mapToString(uidConversionMap));
			}  /// end uid conversion map

		}  // end column find unique id string loop

	}  // end rename conversion map create
	else
		__COUT__ << "Not filling record conversion map." << __E__;

	if(!applyRecordConversionMaps)
	{
		__COUT__ << "Not applying record conversion map." << __E__;
		return TableVersion();  // return invalid
	}
	else
	{
		__COUT__ << "Applying record conversion map." << __E__;
		__COUTV__(StringMacros::mapToString(uidConversionMap));
		__COUTV__(StringMacros::mapToString(groupidConversionMap));
	}

	// if destinationVersion is INVALID, creates next available temporary version
	destinationVersion = createTemporaryView(TableVersion(), destinationVersion);

	__COUT__ << "Merging from (A) " << sourceViewA.getTableName() << "_v" << sourceViewA.getVersion() << " and (B) " << sourceViewB.getTableName() << "_v"
	         << sourceViewB.getVersion() << "  to " << getTableName() << "_v" << destinationVersion << " with approach '" << mergeApproach << ".'" << __E__;

	// if the merge fails then delete the destinationVersion view
	try
	{
		// start with a copy of source view A

		tableViews_.emplace(std::make_pair(destinationVersion, TableView(getTableName())));
		TableView* destinationView = &(tableViews_.at(destinationVersion).copy(sourceViewA, destinationVersion, author));

		unsigned int destRow, destSize = destinationView->getDataView().size();
		unsigned int cb;
		bool         found;
		std::map<std::pair<std::string /*original table*/, std::string /*original uidB*/>, std::string /*converted uidB*/>::iterator uidConversionIt;
		std::map<std::pair<std::string /*original table*/, std::pair<std::string /*group linkid*/, std::string /*original gidB*/> >,
		         std::string /*converted uidB*/>::iterator                                                                           groupidConversionIt;

		bool                                                               linkIsGroup;
		std::pair<unsigned int /*link col*/, unsigned int /*link id col*/> linkPair;
		std::string                                                        strb;
		size_t                                                             stri;

		unsigned int colUID = mockupTableView_.getColUID();  // setup UID column

		// handle merger with conflicts consideration
		for(unsigned int rb = 0; rb < sourceViewB.getNumberOfRows(); ++rb)
		{
			if(mergeApproach == "Rename")
			{
				//	rename		-- All records from both groups are maintained, but
				// conflicts from B are renamed. 					Must maintain a map of
				// UIDs that are remapped to new name for groupB,
				//					because linkUID fields must be preserved.

				// conflict does not matter (because record conversion map is already
				// created, always take and append the B record  copy row from B to new
				// row
				destRow = destinationView->copyRows(
				    author, sourceViewB, rb, 1 /*srcRowsToCopy*/, -1 /*destOffsetRow*/, generateUniqueDataColumns /*generateUniqueDataColumns*/);

				// check every column and remap conflicting names

				for(cb = 0; cb < sourceViewB.getNumberOfColumns(); ++cb)
				{
					if(sourceViewB.getColumnInfo(cb).isChildLink())
						continue;  // skip link columns that have table name
					else if(sourceViewB.getColumnInfo(cb).isChildLinkUID())
					{
						__COUT__ << "Checking UID link... col=" << cb << __E__;
						sourceViewB.getChildLink(cb, linkIsGroup, linkPair);

						// if table and uid are in conversion map, convert
						if((uidConversionIt = uidConversionMap.find(std::pair<std::string /*original table*/, std::string /*original uidB*/>(
						        sourceViewB.getValueAsString(rb, linkPair.first), sourceViewB.getValueAsString(rb, linkPair.second)))) !=
						   uidConversionMap.end())
						{
							__COUT__ << "Found entry to remap: " << sourceViewB.getDataView()[rb][linkPair.second] << " ==> " << uidConversionIt->second
							         << __E__;

							if(mergeReport)
								(*mergeReport) << "\t\t"
								               << "Found entry to remap [r,c]=[" << rb << "," << cb << "]"
								               << ": " << sourceViewB.getDataView()[rb][linkPair.second] << " ==> [" << destRow << "," << linkPair.second
								               << uidConversionIt->second << __E__;
							destinationView->setValueAsString(uidConversionIt->second, destRow, linkPair.second);
						}
					}
					else if(sourceViewB.getColumnInfo(cb).isChildLinkGroupID())
					{
						__COUT__ << "Checking GroupID link... col=" << cb << __E__;
						sourceViewB.getChildLink(cb, linkIsGroup, linkPair);

						// if table and uid are in conversion map, convert
						if((groupidConversionIt = groupidConversionMap.find(
						        std::pair<std::string /*original table*/, std::pair<std::string /*group linkid*/, std::string /*original gidB*/> >(
						            sourceViewB.getValueAsString(rb, linkPair.first),
						            std::pair<std::string /*group linkid*/, std::string /*original gidB*/>(
						                sourceViewB.getColumnInfo(cb).getChildLinkIndex(), sourceViewB.getValueAsString(rb, linkPair.second))))) !=
						   groupidConversionMap.end())
						{
							__COUT__ << "Found entry to remap: " << sourceViewB.getDataView()[rb][linkPair.second] << " ==> " << groupidConversionIt->second
							         << __E__;

							if(mergeReport)
								(*mergeReport) << "\t\t"
								               << "Found entry to remap [r,c]=[" << rb << "," << cb << "]"
								               << ": " << sourceViewB.getDataView()[rb][linkPair.second] << " ==> [" << destRow << "," << linkPair.second
								               << "] " << groupidConversionIt->second << __E__;
							destinationView->setValueAsString(groupidConversionIt->second, destRow, linkPair.second);
						}
					}
					else if(sourceViewB.getColumnInfo(cb).isUID())
					{
						__COUT__ << "Checking UID... col=" << cb << __E__;
						if((uidConversionIt = uidConversionMap.find(std::pair<std::string /*original table*/, std::string /*original uidB*/>(
						        getTableName(), sourceViewB.getValueAsString(rb, cb)))) != uidConversionMap.end())
						{
							__COUT__ << "Found entry to remap: " << sourceViewB.getDataView()[rb][cb] << " ==> " << uidConversionIt->second << __E__;

							if(mergeReport)
								(*mergeReport) << "\t\t"
								               << "Found entry to remap [r,c]=[" << rb << "," << cb << "]"
								               << ": " << sourceViewB.getDataView()[rb][cb] << " ==> [" << destRow << "," << cb << "] "
								               << uidConversionIt->second << __E__;
							destinationView->setValueAsString(uidConversionIt->second, destRow, cb);
						}
					}
					else if(sourceViewB.getColumnInfo(cb).isGroupID())
					{
						__COUT__ << "Checking GroupID... col=" << cb << __E__;
						if((groupidConversionIt = groupidConversionMap.find(
						        std::pair<std::string /*original table*/, std::pair<std::string /*group linkid*/, std::string /*original gidB*/> >(
						            getTableName(),
						            std::pair<std::string /*group linkid*/, std::string /*original gidB*/>(sourceViewB.getColumnInfo(cb).getChildLinkIndex(),
						                                                                                   sourceViewB.getValueAsString(rb, cb))))) !=
						   groupidConversionMap.end())
						{
							__COUT__ << "Found entry to remap: " << sourceViewB.getDataView()[rb][cb] << " ==> " << groupidConversionIt->second << __E__;

							if(mergeReport)
								(*mergeReport) << "\t\t"
								               << "Found entry to remap [r,c]=[" << rb << "," << cb << "]" << sourceViewB.getDataView()[rb][cb] << " ==> ["
								               << destRow << "," << cb << "] " << groupidConversionIt->second << __E__;
							destinationView->setValueAsString(groupidConversionIt->second, destRow, cb);
						}
					}
					else
					{
						// look for text link to a Table/UID in the map
						strb = sourceViewB.getValueAsString(rb, cb);
						if(strb.size() > getTableName().size() + 2 && strb[0] == '/')
						{
							// check for linked name
							__COUT__ << "Checking col" << cb << " " << strb << __E__;

							// see if there is an entry in p
							for(const auto& mapPairToPair : uidConversionMap)
							{
								if((stri = strb.find(mapPairToPair.first.first + "/" + mapPairToPair.first.second)) != std::string::npos)
								{
									__COUT__ << "Found a text link match (stri=" << stri << ")! "
									         << (mapPairToPair.first.first + "/" + mapPairToPair.first.second) << " ==> " << mapPairToPair.second << __E__;

									// insert mapped substitution into string
									destinationView->setValueAsString(
									    strb.substr(0, stri) + (mapPairToPair.first.first + "/" + mapPairToPair.first.second) +
									        strb.substr(stri + (mapPairToPair.first.first + "/" + mapPairToPair.first.second).size()),
									    destRow,
									    cb);

									__COUT__ << "Found entry to remap: " << sourceViewB.getDataView()[rb][cb] << " ==> "
									         << destinationView->getDataView()[destRow][cb] << __E__;

									if(mergeReport)
										(*mergeReport) << "\t\t"
										               << "Found entry to remap [r,c]=[" << rb << "," << cb << "] " << sourceViewB.getDataView()[rb][cb]
										               << " ==> [" << destRow << "," << cb << "] " << destinationView->getDataView()[destRow][cb] << __E__;
									break;
								}
							}  // end uid conversion map loop
						}
					}
				}  // end column loop over B record

				continue;
			}  // end rename, no-conflict handling

			// if here, then not doing rename, so conflicts matter

			found = false;

			for(destRow = 0; destRow < destSize; ++destRow)
				if(destinationView->getValueAsString(destRow, colUID) == sourceViewB.getValueAsString(rb, colUID))
				{
					found = true;
					break;
				}
			if(!found)  // no conflict
			{
				__COUT__ << "No " << mergeApproach << " conflict: " << __E__;

				if(mergeApproach == "replace" || mergeApproach == "skip")
				{
					// no conflict so append the B record
					// copy row from B to new row
					destinationView->copyRows(author, sourceViewB, rb, 1 /*srcRowsToCopy*/);
				}
				else

					continue;
			}  // end no-conflict handling

			// if here, then there was a conflict

			__COUT__ << "found " << mergeApproach << " conflict: " << sourceViewB.getDataView()[rb][colUID] << __E__;

			if(mergeApproach == "replace")
			{
				if(mergeReport)
					(*mergeReport) << "\t\t"
					               << "Found UID conflict, replacing A with B record row=" << rb << " " << sourceViewB.getDataView()[rb][colUID] << __E__;
				//	replace		-- Any UID conflicts for a record are replaced by the
				// record from group B.

				// delete row in destination
				destinationView->deleteRow(destRow--);  // delete row and back up pointer
				--destSize;

				// append the B record now
				// copy row from B to new row
				destinationView->copyRows(author, sourceViewB, rb, 1 /*srcRowsToCopy*/);
			}
			else if(mergeApproach == "skip")  // then do nothing with conflicting B record
			{
				if(mergeReport)
					(*mergeReport) << "\t\t"
					               << "Found UID conflict, skipping B record row=" << rb << " " << sourceViewB.getDataView()[rb][colUID] << __E__;
			}
		}

		destinationView->print();
	}
	catch(...)  // if the copy fails then delete the destinationVersion view
	{
		__COUT_ERR__ << "Failed to merge " << sourceViewA.getTableName() << "_v" << sourceViewA.getVersion() << " and " << sourceViewB.getTableName() << "_v"
		             << sourceViewB.getVersion() << " into " << getTableName() << "_v" << destinationVersion << __E__;
		__COUT_WARN__ << "Deleting the failed destination version " << destinationVersion << __E__;
		eraseView(destinationVersion);
		throw;  // and rethrow
	}

	return destinationVersion;
}  // end mergeViews

//==============================================================================
// copyView
//	copies source view (including version) and places in self
//	as destination temporary version.
//	if destination version is invalid, then next available temporary version is chosen
//	if conflict, throw exception
//
//	Returns version of new temporary view that was created.
TableVersion TableBase::copyView(const TableView& sourceView, TableVersion destinationVersion, 
	const std::string& author, bool looseColumnMatching /* = false */)
{
	// check that column sizes match
	if(!looseColumnMatching && sourceView.getNumberOfColumns() != mockupTableView_.getNumberOfColumns())
	{
		__SS__ << "Error! Number of Columns of source view must match destination "
		          "mock-up view."
		       << "Dimension of source is [" << sourceView.getNumberOfColumns() << "] and of destination mockup is [" << mockupTableView_.getNumberOfColumns()
		       << "]." << __E__;
		__SS_THROW__;
	}

	// check for destination version confict
	if(!destinationVersion.isInvalid() && tableViews_.find(destinationVersion) != tableViews_.end())
	{
		__SS__ << "Error! Asked to copy a view with a conflicting version: " << destinationVersion << __E__;
		__SS_THROW__;
	}

	// if destinationVersion is INVALID, creates next available temporary version
	destinationVersion = createTemporaryView(TableVersion(), destinationVersion);

	__COUT__ << "Copying from " << sourceView.getTableName() << "_v" << sourceView.getVersion() << " to " << getTableName() << "_v" << destinationVersion
	         << __E__;

	try
	{
		tableViews_.emplace(std::make_pair(destinationVersion, TableView(tableName_)));
		tableViews_.at(destinationVersion).copy(sourceView, destinationVersion, author);
	}
	catch(...)  // if the copy fails then delete the destinationVersion view
	{
		__COUT_ERR__ << "Failed to copy from " << sourceView.getTableName() << "_v" << sourceView.getVersion() << " to " << getTableName() << "_v"
		             << destinationVersion << __E__;
		__COUT_WARN__ << "Deleting the failed destination version " << destinationVersion << __E__;
		eraseView(destinationVersion);
		throw;  // and rethrow
	}

	return destinationVersion;
}  // end copyView()

//==============================================================================
// createTemporaryView
//	-1, from MockUp, else from valid view version
//	destTemporaryViewVersion is starting point for search for available temporary
// versions. 	if destTemporaryViewVersion is invalid, starts search at
// TableVersion::getNextTemporaryVersion().
// 	returns new temporary version number (which is always negative)
TableVersion TableBase::createTemporaryView(TableVersion sourceViewVersion, TableVersion destTemporaryViewVersion)
{
	__COUT_TYPE__(TLVL_DEBUG + 20) << __COUT_HDR__ << "Table: " << getTableName() << __E__ <<
		 "Num of Views: " << tableViews_.size()
		<< " (Temporary Views: " << (tableViews_.size() - getNumberOfStoredViews())
		<< ")" << __E__;

	TableVersion tmpVersion = destTemporaryViewVersion;
	if(tmpVersion.isInvalid())
		tmpVersion = TableVersion::getNextTemporaryVersion();
	while(isStored(tmpVersion) &&  // find a new valid temporary version
	      !(tmpVersion = TableVersion::getNextTemporaryVersion(tmpVersion)).isInvalid())
		;
	if(isStored(tmpVersion) || tmpVersion.isInvalid())
	{
		__SS__ << "Invalid destination temporary version: " << destTemporaryViewVersion << ". Expected next temporary version < " << tmpVersion << __E__;
		__SS_THROW__;
	}

	if(sourceViewVersion == TableVersion::INVALID ||  // use mockup if sourceVersion is -1 or not found
	   tableViews_.find(sourceViewVersion) == tableViews_.end())
	{
		if(sourceViewVersion != -1)
		{
			__SS__ << "ERROR: sourceViewVersion " << sourceViewVersion << " not found. "
			       << "Invalid source version. Version requested is not stored (yet?) or "
			          "does not exist."
			       << __E__;
			__SS_THROW__;
		}
		__COUT_TYPE__(TLVL_DEBUG + 20) << __COUT_HDR__ << "Using Mock-up view" << __E__;
		tableViews_.emplace(std::make_pair(tmpVersion, TableView(tableName_)));
		tableViews_.at(tmpVersion).copy(mockupTableView_, tmpVersion, mockupTableView_.getAuthor());
	}
	else
	{
		try  // do not allow init to throw an exception here..
		{    // it's ok to copy invalid data, the user may be trying to change it
			tableViews_.emplace(std::make_pair(tmpVersion, TableView(tableName_)));
			tableViews_.at(tmpVersion).copy(tableViews_.at(sourceViewVersion), tmpVersion, tableViews_.at(sourceViewVersion).getAuthor());
		}
		catch(...)
		{
			__COUT_WARN__ << "createTemporaryView() Source view failed init(). "
			              << "This is being ignored (hopefully the new copy is being fixed)." << __E__;
		}
	}

	return tmpVersion;
}  // end createTemporaryView()

//==============================================================================
// getNextAvailableTemporaryView
//	TableVersion::INVALID is always MockUp
// returns next available temporary version number (which is always negative)
TableVersion TableBase::getNextTemporaryVersion() const
{
	TableVersion tmpVersion;

	// std::map guarantees versions are in increasing order!
	if(tableViews_.size() != 0 && tableViews_.begin()->first.isTemporaryVersion())
		tmpVersion = TableVersion::getNextTemporaryVersion(tableViews_.begin()->first);
	else
		tmpVersion = TableVersion::getNextTemporaryVersion();

	// verify tmpVersion is ok
	if(isStored(tmpVersion) || tmpVersion.isInvalid() || !tmpVersion.isTemporaryVersion())
	{
		__SS__ << "Invalid destination temporary version: " << tmpVersion << __E__;
		__SS_THROW__;
	}
	return tmpVersion;
} //end getNextTemporaryVersion()

//==============================================================================
// getNextVersion
// 	returns next available new version
//	the implication is any version number equal or greater is available.
TableVersion TableBase::getNextVersion() const
{
	TableVersion tmpVersion;

	// std::map guarantees versions are in increasing order!
	if(tableViews_.size() != 0 && !tableViews_.rbegin()->first.isTemporaryVersion())
		tmpVersion = TableVersion::getNextVersion(tableViews_.rbegin()->first);
	else
		tmpVersion = TableVersion::getNextVersion();

	// verify tmpVersion is ok
	if(isStored(tmpVersion) || tmpVersion.isInvalid() || tmpVersion.isTemporaryVersion())
	{
		__SS__ << "Invalid destination next version: " << tmpVersion << __E__;
		__SS_THROW__;
	}
	return tmpVersion;
} //end getNextVersion()

//==============================================================================
// getTemporaryView
//	must be a valid temporary version, and the view must be stored in table.
// 	temporary version indicates it has not been saved to database and assigned a version
// number
TableView* TableBase::getTemporaryView(TableVersion temporaryVersion)
{
	if(!temporaryVersion.isTemporaryVersion() || !isStored(temporaryVersion))
	{
		__SS__ << getTableName() << ":: Error! Temporary version not found!" << __E__;
		__SS_THROW__;
	}
	return &tableViews_.at(temporaryVersion);
} //end getTemporaryView()

//==============================================================================
// convertToCaps
//	static utility for converting table and column names to the caps version
//	throw std::runtime_error if not completely alpha-numeric input
std::string TableBase::convertToCaps(std::string& str, bool isTableName)
{
	// append Table to be nice to user
	unsigned int configPos = (unsigned int)std::string::npos;
	if(isTableName && (configPos = str.find("Table")) != str.size() - strlen("Table"))
		str += "Table";

	// create all caps name and validate
	//	only allow alpha names with Table at end
	std::string capsStr = "";
	for(unsigned int c = 0; c < str.size(); ++c)
		if(str[c] >= 'A' && str[c] <= 'Z')
		{
			// add _ before table and if lower case to uppercase
			if(c == configPos || (c && str[c - 1] >= 'a' && str[c - 1] <= 'z') ||  // if this is a new start of upper case
			   (c && str[c - 1] >= 'A' && str[c - 1] <= 'Z' &&                     // if this is a new start from running caps
			    c + 1 < str.size() && str[c + 1] >= 'a' && str[c + 1] <= 'z'))
				capsStr += "_";
			capsStr += str[c];
		}
		else if(str[c] >= 'a' && str[c] <= 'z')
			capsStr += char(str[c] - 32);  // capitalize
		else if(str[c] >= '0' && str[c] <= '9')
			capsStr += str[c];  // allow numbers
		else                    // error! non-alpha
		{
			//allow underscores for group cache document name
			if((str.substr(0,TableBase::GROUP_CACHE_PREPEND.length()) == TableBase::GROUP_CACHE_PREPEND ||
				str.substr(0,TableBase::JSON_DOC_PREPEND.length()) == TableBase::JSON_DOC_PREPEND) &&
			 	str[c] == '_') 
			{
				capsStr += '-';
				continue;
			}

			std::stringstream ss;
			ss << __COUT_HDR_FL__ << "TableBase::convertToCaps: Invalid character found in name (allowed: A-Z, a-z, 0-9) '" << str << "'" << __E__;
			TLOG(TLVL_ERROR) << ss.str();
			__SS_ONLY_THROW__;
		}

	return capsStr;
} //end convertToCaps()
