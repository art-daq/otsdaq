#ifndef _ots_TableBase_h_
#define _ots_TableBase_h_

#include <list>
#include <map>
#include <string>

#include "otsdaq/TableCore/TableVersion.h"
#include "otsdaq/TableCore/TableView.h"

namespace ots
{

class ConfigurationManager;

/// e.g. configManager->__SELF_NODE__;  //to get node referring to this table
#define __SELF_NODE__ getNode(getTableName())

class TableBase
{
  public:
	const unsigned int MAX_VIEWS_IN_CACHE;  ///< Each inheriting table class could have
	                                        ///< varying amounts of cache
	//TableBase(void); //should not be used
	TableBase(bool specialTable, const std::string& specialTableName);
	TableBase(const std::string& tableName, std::string* accumulatedExceptions = 0);

	virtual ~TableBase(void);

	/// Methods
	void         specialMetaTableConstructor(void);
	virtual void init(ConfigurationManager* configManager);

	void destroy(void) { ; }
	void reset(bool keepTemporaryVersions = false);
	void deactivate(void);
	bool isActive(void);

	void print(std::ostream& out = std::cout) const;  ///< always prints active view

	std::string getTypeId(void);

	void         setupMockupView(TableVersion version);
	void         changeVersionAndActivateView(TableVersion temporaryVersion,
	                                          TableVersion version);
	bool         isStored(const TableVersion& version) const;
	bool         eraseView(TableVersion version);
	void         trimCache(unsigned int trimSize = -1);
	void         trimTemporary(TableVersion targetVersion = TableVersion());
	TableVersion checkForDuplicate(TableVersion needleVersion,
	                               TableVersion ignoreVersion = TableVersion()) const;
	bool         diffTwoVersions(
	            TableVersion       v1,
	            TableVersion       v2,
	            std::stringstream* diffReport = 0,
	            std::map<std::string /* uid */, std::vector<std::string /* colName */>>*
	                v1ModifiedRecords = 0) const;

	/// Getters
	const std::string&     getTableName(void) const;
	const std::string&     getTableDescription(void) const;
	std::set<TableVersion> getStoredVersions(void) const;

	const TableView& getView(
	    TableVersion version = TableVersion(TableVersion::INVALID)) const;
	TableView* getViewP(TableVersion version = TableVersion(TableVersion::INVALID));
	TableView* getMockupViewP(void);
	const TableVersion& getViewVersion(void) const;  ///< always the active one

	TableView*   getTemporaryView(TableVersion temporaryVersion);
	TableVersion getNextTemporaryVersion(void) const;
	TableVersion getNextVersion(void) const;

	virtual std::string getStructureAsJSON(
	    const ConfigurationManager* /* configManager */)
	{
		__SS__ << "getStructureAsJSON() is not implemented for this table '"
		       << getTableName() << "'" << __E__;
		__SS_ONLY_THROW__;
	};
	virtual void initPrereqsForARTDAQ(const ConfigurationManager* /* configManager */)
	{
		__SS__ << "initPrereqsForARTDAQ() is not implemented for this table '"
		       << getTableName() << "'" << __E__;
		__SS_ONLY_THROW__;
	};

	/// Setters
	void         setTableName(const std::string& tableName);
	void         setTableDescription(const std::string& tableDescription);
	bool         setActiveView(TableVersion version);
	TableVersion copyView(const TableView&   sourceView,
	                      TableVersion       destinationVersion,
	                      const std::string& author,
	                      bool               looseColumnMatching = false);
	TableVersion mergeViews(
	    const TableView&                          sourceViewA,
	    const TableView&                          sourceViewB,
	    TableVersion                              destinationVersion,
	    const std::string&                        author,
	    const std::string&                        mergeApproach /*Rename,Replace,Skip*/,
	    std::map<std::pair<std::string /*original table*/, std::string /*original uidB*/>,
	             std::string /*converted uidB*/>& uidConversionMap,
	    std::map<std::pair<std::string /*original table*/,
	                       std::pair<std::string /*group linkid*/,
	                                 std::string /*original gidB*/>>,
	             std::string /*converted gidB*/>& groupidConversionMap,
	    bool                                      fillRecordConversionMaps,
	    bool                                      applyRecordConversionMaps,
	    bool                                      generateUniqueDataColumns = false,
	    std::stringstream*                        mergeRepoert              = nullptr);

	TableVersion createTemporaryView(
	    TableVersion sourceViewVersion        = TableVersion(),
	    TableVersion destTemporaryViewVersion = TableVersion::
	        getNextTemporaryVersion());  ///< source of -1, from MockUp, else from valid view version

	static std::string convertToCaps(std::string& str, bool isConfigName = false);

	bool latestAndMockupColumnNumberMismatch(void) const;

	unsigned int getNumberOfStoredViews(void) const;

	/// output table name for ostream operator
	friend std::ostream& operator<<(std::ostream& out, const TableBase& table)
	{
		out << table.getTableName();
		return out;
	}

	// ----- member variables

  public:
	static const std::string GROUP_CACHE_PREPEND;
	static const std::string JSON_DOC_PREPEND;
	static const std::string GROUP_METADATA_TABLE_NAME;

  protected:
	std::string tableName_;
	std::string tableDescription_;

	TableView* activeTableView_;
	TableView  mockupTableView_;

	/// Version and data associated to make it work like a cache.
	/// It will be very likely just 1 version
	/// NOTE: must be very careful to setVersion of view after manipulating (e.g. copy from different version view)
	std::map<TableVersion, TableView> tableViews_;
	bool                              isFirstAppInContext_ =
	    false;  ///<for managing things that should only happen once per node (e.g., write files). If used, should be set in init() to configManager->isOwnerFirstAppInContext();
};

}  // namespace ots

#endif
