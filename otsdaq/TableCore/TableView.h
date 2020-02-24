#ifndef _ots_TableView_h_
#define _ots_TableView_h_

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <stdlib.h>
#include <time.h> /* time_t, time, ctime */
#include <cassert>
#include <iostream>
#include <set>
#include <vector>
#include "otsdaq/TableCore/TableVersion.h"
#include "otsdaq/TableCore/TableViewColumnInfo.h"

namespace ots
{
// clang-format off
class TableView
{
  public:
	static const unsigned int                      					INVALID;
	typedef std::vector<std::vector<std::string> > 					DataView;
	typedef DataView::iterator                     					iterator;
	typedef DataView::const_iterator               					const_iterator;

												TableView					(const std::string& name = "");
	virtual 									~TableView					(void);

	void         								init						(void);
	TableView&   								copy						(const TableView&   src,
																			 TableVersion       destinationVersion,
																			 const std::string& author);
	unsigned int 								copyRows					(const std::string& author,
																			 const TableView&   src,
																			 unsigned int       srcOffsetRow              = 0,
																			 unsigned int       srcRowsToCopy             = (unsigned int)-1,
																			 unsigned int       destOffsetRow             = (unsigned int)-1,
																			 unsigned char      generateUniqueDataColumns = false, // leave as unsigned char rather than
																			 // bool, too many things (e.g. strings)
																			 // evaluate successfully to bool values
																			 const std::string& baseNameAutoUID		   = ""
																			);

	template<class T>  // in included .icc source
	unsigned int 								findRow						(unsigned int col,
									                   						 const T&     value,
									                   						 unsigned int offsetRow = 0,
																			 bool			   doNotThrow = false) const;
	unsigned int 								findRow						(unsigned int       col,
																			 const std::string& value,
																			 unsigned int       offsetRow = 0,
																			 bool			   doNotThrow = false) const;

	template<class T>  // in included .icc source
	unsigned int 								findRowInGroup				(unsigned int       col,
																			 const T&           value,
																			 const std::string& groupId,
																			 const std::string& childLinkIndex,
																			 unsigned int       offsetRow = 0) const;
	unsigned int 								findRowInGroup				(unsigned int       col,
																			 const std::string& value,
																			 const std::string& groupId,
																			 const std::string& childLinkIndex,
																			 unsigned int       offsetRow = 0) const;
	unsigned int 								findCol						(const std::string& name) const;
	unsigned int 								findColByType				(const std::string& type, int startingCol = 0) const;

	// Getters
	const std::string&                          getUniqueStorageIdentifier	(void) const;
	const std::string&                          getTableName				(void) const;
	const TableVersion&                         getVersion					(void) const;
	const std::string&                          getComment					(void) const;
	const std::string&                          getAuthor					(void) const;
	const time_t&                               getCreationTime				(void) const;
	const time_t&                               getLastAccessTime			(void) const;
	const bool&                                 getLooseColumnMatching		(void) const;
	const unsigned int                          getDataColumnSize			(void) const;
	const unsigned int&                         getSourceColumnMismatch		(void) const;
	const unsigned int&                         getSourceColumnMissing		(void) const;
	const std::set<std::string /*col name*/>&   getSourceColumnNames		(void) const;
	std::set<std::string /*col name*/>          getColumnNames				(void) const;
	std::map<std::string, unsigned int /*col*/> getColumnNamesMap			(void) const;
	std::set<std::string /*storage name*/>      getColumnStorageNames		(void) const;
	std::vector<std::string /*default value*/>  getDefaultRowValues			(void) const;

	unsigned int       							getNumberOfRows				(void) const;
	unsigned int       							getNumberOfColumns			(void) const;
	const unsigned int 							getColUID					(void) const;
	const unsigned int 							getColStatus				(void) const;
	const unsigned int 							getColPriority				(void) const;

	// Note: Group link handling should be done in this TableView class
	//	only by using isEntryInGroup ...
	//	This is so that multiple group handling is consistent
  private:
	bool 										isEntryInGroupCol			(const unsigned int&    row,
																			 const unsigned int&    groupCol,
																			 const std::string&     groupNeedle,
																			 std::set<std::string>* groupIDList = 0) const;

  public:

	std::set<std::string /*groupID*/> 			getSetOfGroupIDs			(const std::string& childLinkIndex, unsigned int row = -1) const;
	std::set<std::string /*groupID*/> 			getSetOfGroupIDs			(const unsigned int& col, unsigned int row = -1) const;
	bool                 						isEntryInGroup				(const unsigned int& row,
																			 const std::string&  childLinkIndex,
																			 const std::string&  groupNeedle) const;
	const bool            						getChildLink				(const unsigned int&                      col,
																			 bool&                                    isGroup,
																			 std::pair<unsigned int /*link col*/,
																			 unsigned int /*link id col*/>& linkPair) const;
	const unsigned int    						getLinkGroupIDColumn		(const std::string& childLinkIndex) const;
	std::vector<unsigned int /*row*/>			getGroupRows				(const unsigned int groupIdCol, const std::string& groupID, bool onlyStatusTrue = false, bool orderedByPriority = false) const;
	std::vector<std::vector<unsigned int /*row*/>> getGroupRowsByPriority	(const unsigned int groupIdCol, const std::string& groupID, bool onlyStatusTrue = false) const;
private:
	std::vector<std::vector<unsigned int /*row*/>> getGroupRowsInVectors	(const unsigned int groupIdCol, const std::string& groupID, bool onlyStatusTrue, bool orderedByPriority) const;
public:
	void                  						addRowToGroup				(const unsigned int& row,
																			 const unsigned int& col,
																			 const std::string&  groupID);  //, const std::string& colDefault);
	bool                  						removeRowFromGroup			(const unsigned int& row,
																			 const unsigned int& col,
																			 const std::string&  groupID,
																			 bool                deleteRowIfNoGroupLeft = false);

	template<class T>  // in included .icc source
	void 										getValue					(T&           value,
																		     unsigned int row,
																		     unsigned int col,
																		     bool         doConvertEnvironmentVariables = true) const;
	// special version of getValue for string type
	//	Note: necessary because types of std::basic_string<char> cause compiler problems
	// if no string specific function
	void 										getValue					(std::string& value,
																		     unsigned int row,
																		     unsigned int col,
																		     bool         doConvertEnvironmentVariables = true) const;

	template<class T>  // in included .icc source
	T 											validateValueForColumn		(const std::string& value,
	                        												 unsigned int       col,
	                        												 bool doConvertEnvironmentVariables = true) const;
	// special version of getValue for string type
	//	Note: necessary because types of std::basic_string<char> cause compiler problems
	// if no string specific function
	std::string 								validateValueForColumn		(const std::string& value,
	                                  										 unsigned int       col,
	                                  										 bool convertEnvironmentVariables = true) const;
	std::string 								getValueAsString			(unsigned int row,
																			 unsigned int col,
																			 bool         convertEnvironmentVariables = true) const;
	std::string 								getEscapedValueAsString		(unsigned int row,
	            								                       		 unsigned int col,
	            								                       		 bool convertEnvironmentVariables = true) const;
	bool        								isURIEncodedCommentTheSame	(const std::string& comment) const;

	const DataView&                         	getDataView					(void) const;
	const std::vector<TableViewColumnInfo>& 	getColumnsInfo				(void) const;
	std::vector<TableViewColumnInfo>*       	getColumnsInfoP				(void);
	const TableViewColumnInfo&             		getColumnInfo				(unsigned int column) const;

	// Setters

	void 										setUniqueStorageIdentifier	(const std::string& storageUID);
	void 										setTableName				(const std::string& name);
	void 										setComment					(const std::string& comment);
	void 										setURIEncodedComment		(const std::string& uriComment);
	void 										setAuthor					(const std::string& author);
	void 										setCreationTime				(time_t t);
	void 										setLastAccessTime			(time_t t = time(0));
	void 										setLooseColumnMatching		(bool setValue);

	template<class T>  // in included .icc source
	void 										setVersion					(const T& version);
	template<class T>  // in included .icc source
	void 										setValue					(const T& value, unsigned int row, unsigned int col);
	void 										setValue					(const std::string& value, unsigned int row, unsigned int col);
	void 										setValue					(const char* value, unsigned int row, unsigned int col);

	// Careful: The setValueAsString method is used to set the value without any
	// consistency check with the data type
	void 										setValueAsString			(const std::string& value, unsigned int row, unsigned int col);
	const std::string&							setUniqueColumnValue		(unsigned int row, unsigned int col, std::string baseValueAsString = "", bool doMathAppendStrategy = false);

	void         								resizeDataView				(unsigned int nRows, unsigned int nCols);
	unsigned int 								addRow						(const std::string& author = "",
																		     unsigned char      incrementUniqueData = false,  // leave as unsigned char rather than
																		                                                 // bool, too many things (e.g. strings)
																		                                                 // evaluate successfully to bool values
																			 const std::string&  baseNameAutoUID = "",
																		     unsigned int rowToAdd =
																		         (unsigned int)-1);  // returns index of added row, default is last row
	void 										deleteRow					(int r);
	void 										deleteAllRows				(void) {theDataView_.clear();}


	// Lore did not like this.. wants special access through separate Supervisor for
	// "Database Management" int		addColumn(std::string name, std::string viewName,
	// std::string viewType); //returns index of added column, always is last column
	// unless

	iterator       								begin						(void) { return theDataView_.begin(); }
	iterator       								end							(void) { return theDataView_.end(); }
	const_iterator 								begin						(void) const { return theDataView_.begin(); }
	const_iterator 								end							(void) const { return theDataView_.end(); }
	void           								reset						(void);
	void           								print						(std::ostream& out = std::cout) const;
	void           								printJSON					(std::ostream& out = std::cout) const;
	int            								fillFromJSON				(const std::string& json);
	int            								fillFromCSV					(const std::string& data,
																			 const int&         dataOffset = 0,
																			 const std::string& author     = "");
	bool           								setURIEncodedValue			(const std::string&  value,
	               								                  			 const unsigned int& row,
	               								                  			 const unsigned int& col,
	               								                  			 const std::string&  author = "");

  private:
	const unsigned int 							initColUID					(void);
	const unsigned int 							initColStatus				(void);
	const unsigned int 							initColPriority				(void);

	TableView& 									operator=					(const TableView src);  // operator= is purposely undefined and
														                                            // private (DO NOT USE IT!) - should use
														                                            // TableView::copy()

	std::string 													uniqueStorageIdentifier_;  	// starts empty "", used to implement re-writable views ("temporary views") in artdaq db
	std::string  													tableName_;               	// View name (extensionTableName in xml)
	TableVersion 													version_;                 	// Table version
	std::string  													comment_;                 	// Table version comment
	std::string  													author_;
	time_t       													creationTime_;  			// used more like "construction"(constructor) time
	time_t 															lastAccessTime_;  			// last time the ConfigurationInterface:get() retrieved this view
	
	unsigned int 													colUID_, colStatus_, colPriority_;  // special column pointers
	std::map<std::string, unsigned int>								colLinkGroupIDs_;  // map from child link index to column

	bool                  											fillWithLooseColumnMatching_;
	unsigned int          											sourceColumnMismatchCount_, sourceColumnMissingCount_;
	std::set<std::string> 											sourceColumnNames_;

	std::vector<TableViewColumnInfo> 								columnsInfo_;
	DataView                         								theDataView_;
};

#include "otsdaq/TableCore/TableView.icc"  //define template functions

// clang-format on
}  // namespace ots

#endif