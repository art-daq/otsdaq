#ifndef _ots_ConfigurationTree_h_
#define _ots_ConfigurationTree_h_

#include "otsdaq/TableCore/TableView.h"

namespace ots
{
class ConfigurationManager;
class TableBase;

template<typename T>
struct identity
{
	typedef T type;
};

class ConfigurationTree
{
	friend class ConfigurationGUISupervisor;
	friend class Iterator;
	friend class ARTDAQTableBase;

	// clang-format off
  public:
	// Note: due to const members, implicit copy constructor exists, but NOT assignment
	// operator=
	//	... so ConfigurationTree t = mytree.GetNode(nodeString); //OK
	//	... or ConfigurationTree t(mytree.GetNode(nodeString)); //OK
	//	... but mytree = mytree.GetNode(nodeString); //does NOT work
	ConfigurationTree();
	//	ConfigurationTree(const ConfigurationTree& a)
	//	:
	//		configMgr_				(a.configMgr_),
	//	  table_			(a.table_),
	//	  groupId_					(a.groupId_),
	//	  linkColName_				(a.linkColName_),
	//	  disconnectedTargetName_ 	(a.disconnectedTargetName_),
	//	  childLinkIndex_			(a.childLinkIndex_),
	//	  row_						(a.row_),
	//	  col_						(a.col_),
	//	  tableView_				(a.tableView_)
	//	{
	//		__COUT__ << std::endl;
	//		//return *this;
	//	}

	ConfigurationTree(const ConfigurationManager* const& configMgr,
	                  const TableBase* const&            table);
	~ConfigurationTree(void);

	ConfigurationTree(ConfigurationTree const& a) = default;

	ConfigurationTree& operator=(const ConfigurationTree& a)
	{
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR ConfigManager: " << configMgr_
		         << " configuration: " << table_ << std::endl;
		// Note: Members of the ConfigurationTree are declared constant.
		//	(Refer to comments at top of class declaration for solutions)
		//	So this operator cannot work.. SO I am going to crash just in case it is
		// called by mistake
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR CANNOT BE USED - ConfigurationTree is a "
		            "const class. SO YOUR CODE IS WRONG! You should probably instantiate "
		            "and initialize another ConfigurationTree, rather than assigning to "
		            "an existing ConfigurationTree. Crashing now."
		         << std::endl;
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR CANNOT BE USED - ConfigurationTree is a "
		            "const class. SO YOUR CODE IS WRONG! You should probably instantiate "
		            "and initialize another ConfigurationTree, rather than assigning to "
		            "an existing ConfigurationTree. Crashing now."
		         << std::endl;
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR CANNOT BE USED - ConfigurationTree is a "
		            "const class. SO YOUR CODE IS WRONG! You should probably instantiate "
		            "and initialize another ConfigurationTree, rather than assigning to "
		            "an existing ConfigurationTree. Crashing now."
		         << std::endl;
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR CANNOT BE USED - ConfigurationTree is a "
		            "const class. SO YOUR CODE IS WRONG! You should probably instantiate "
		            "and initialize another ConfigurationTree, rather than assigning to "
		            "an existing ConfigurationTree. Crashing now."
		         << std::endl;
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR CANNOT BE USED - ConfigurationTree is a "
		            "const class. SO YOUR CODE IS WRONG! You should probably instantiate "
		            "and initialize another ConfigurationTree, rather than assigning to "
		            "an existing ConfigurationTree. Crashing now."
		         << std::endl;
		__COUT__ << "OPERATOR= COPY CONSTRUCTOR CANNOT BE USED - ConfigurationTree is a "
		            "const class. SO YOUR CODE IS WRONG! You should probably instantiate "
		            "and initialize another ConfigurationTree, rather than assigning to "
		            "an existing ConfigurationTree. Crashing now."
		         << std::endl;
		         
		StringMacros::stackTrace();         
		exit(0);

		// copy to const members is not allowed.. but would look like this:

		configMgr_ = a.configMgr_;
		table_     = a.table_;
		// groupId_		= a.groupId_;
		// linkColName_	= a.linkColName_;
		// childLinkIndex_ = a.childLinkIndex_;
		// row_			= a.row_;
		// col_			= a.col_;
		tableView_ = a.tableView_;
		__COUT__ << "OPERATOR COPY CONSTRUCTOR" << std::endl;
		return *this;
	}; //end operator=

	static const std::string DISCONNECTED_VALUE;
	static const std::string VALUE_TYPE_DISCONNECTED;
	static const std::string VALUE_TYPE_NODE;

	static const std::string NODE_TYPE_GROUP_TABLE;
	static const std::string NODE_TYPE_TABLE;
	static const std::string NODE_TYPE_GROUP_LINK;
	static const std::string NODE_TYPE_UID_LINK;
	static const std::string NODE_TYPE_VALUE;
	static const std::string NODE_TYPE_UID;
	static const std::string NODE_TYPE_ROOT;

	static const std::string ROOT_NAME;

	struct BitMap
	{
		BitMap() : isDefault_(true), zero_(0) {}

		friend ConfigurationTree;  // so ConfigurationTree can access private
		const uint64_t& get(unsigned int row, unsigned int col) const
		{
			return isDefault_ ? zero_ : bitmap_[row][col];
		}
		unsigned int numberOfRows() const { return bitmap_.size(); }
		unsigned int numberOfColumns(unsigned int row) const
		{
			return bitmap_[row].size();
		}

	  private:
		std::vector<std::vector<uint64_t>> bitmap_;
		bool                               isDefault_;  // when default always return 0
		uint64_t                           zero_;
	};

	// Methods

	//==============================================================================
	// getValue (not std::string value)
	//	throw exception unless it value node
	// NOTE: can not overload functions based on return type, so T& passed as value
	template<class T>
	void 										getValue					(T& value) const;  // defined in included .icc source
	// special version of getValue for string type
	//	Note: necessary because types of std::basic_string<char> cause compiler problems
	// if no string specific function
	void 										getValue					(std::string& value) const;
	void 										getValueAsBitMap			(ConfigurationTree::BitMap& value) const;

	//==============================================================================
	// getValue (not std::string value)
	//	throw exception unless it value node
	// NOTE: can not overload functions based on return type, so calls function with T&
	// passed as value
	template<class T>
	T 											getValue					(void) const;  // defined in included .icc source
	template<class T>
	T 											getValueWithDefault			(const T& defaultValue) const;  // defined in included .icc source
	// special version of getValue for string type
	//	Note: necessary because types of std::basic_string<char> cause compiler problems
	// if no string specific function
	std::string               					getValue					(void) const;
	std::string               					getValueWithDefault			(const std::string& defaultValue) const;
	ConfigurationTree::BitMap 					getValueAsBitMap			(void) const;

  private:
	template<typename T>
	T 											handleValidateValueForColumn(
	    const TableView* 				configView,
	    std::string      				value,
	    unsigned int     				col,
	    ots::identity<T>) const;  // defined in included .icc source
	std::string 								handleValidateValueForColumn(
		const TableView* 				configView,
		std::string      				value,
		unsigned int     				col,
		ots::identity<std::string>) const;

  public:
	// navigating between nodes
	ConfigurationTree 							getNode						(const std::string& nodeName, bool doNotThrowOnBrokenUIDLinks = false) const;
	std::map<std::string, ConfigurationTree>	getNodes					(const std::string& nodeString) const;
	ConfigurationTree 							getBackNode					(std::string nodeName, unsigned int backSteps = 1) const;
	ConfigurationTree 							getForwardNode				(std::string  nodeName, unsigned int forwardSteps = 1) const;

	// extracting information from node
	const ConfigurationManager* 				getConfigurationManager		(void) const { return configMgr_; }
	const std::string&          				getTableName				(void) const;
	const std::string&          				getParentTableName			(void) const;
	const std::string&          				getFieldTableName			(void) const;
	const TableVersion&         				getTableVersion				(void) const;
	const time_t&               				getTableCreationTime		(void) const;
	std::vector<std::vector<std::string>> 		getChildrenNamesByPriority	(bool onlyStatusTrue = false) const;
	std::vector<std::string> 					getChildrenNames			(bool byPriority = false, bool onlyStatusTrue = false) const;
	std::vector<std::vector<std::pair<
		std::string, ConfigurationTree>>>		getChildrenByPriority		(std::map<std::string /*relative-path*/, 
																				std::string /*value*/> filterMap 	= std::map<std::string /*relative-path*/, std::string /*value*/>(),
	    																		bool onlyStatusTrue 				= false) const;
	std::vector<std::pair<std::string, 
		ConfigurationTree>> 					getChildren					(std::map<std::string /*relative-path*/, 
																				std::string /*value*/> filterMap 	= std::map<std::string /*relative-path*/, std::string /*value*/>(),
																			    bool byPriority     				= false,
																			    bool onlyStatusTrue 				= false) const;
	std::map<std::string, ConfigurationTree> 	getChildrenMap				(std::map<std::string /*relative-path*/, 
																				std::string /*value*/> filterMap 	= std::map<std::string /*relative-path*/, std::string /*value*/>(),
																			    bool onlyStatusTrue 				= false) const;
	bool 										passFilterMap				(const std::string& childName, std::map<std::string /*relative-path*/, std::string /*value*/> filterMap) const;
	std::string                              	getEscapedValue				(void) const;
	const std::string&       					getValueAsString			(bool returnLinkTableValue = false) const;
	const std::string&       					getUIDAsString				(void) const;
	const std::string&       					getValueDataType			(void) const;
	const std::string&       					getValueType				(void) const;
	const std::string&       					getValueName				(void) const;
	const std::string&							getDefaultValue				(void) const;
	inline const std::string&					getFieldName				(void) const { return getValueName(); } //alias for getValueName
	std::string              					getNodeType					(void) const;
	const unsigned int&        					getNodeRow					(void) const;
	const std::string&       					getDisconnectedTableName	(void) const;
	const std::string&       					getDisconnectedLinkID		(void) const;
	const std::string&       					getChildLinkIndex			(void) const;
	std::vector<std::string> 					getFixedChoices				(void) const;
	std::set<std::string> 						getSetOfGroupIDs			(void) const;

  public:
	// boolean info
	bool 										isDefaultValue				(void) const;
	inline bool									isRootNode					(void) const { return (!isLinkNode() && !table_); }
	inline bool									isTableNode					(void) const { return (table_ && row_ == TableView::INVALID && col_ == TableView::INVALID); }
	bool 										isValueNode					(void) const;
	bool 										isValueBoolType				(void) const;
	bool 										isValueNumberDataType		(void) const;
	bool 										isDisconnected				(void) const;
	bool 										isLinkNode					(void) const;
	bool 										isGroupLinkNode				(void) const;
	bool 										isUIDLinkNode				(void) const;
	bool 										isGroupIDNode				(void) const;
	bool 										isUIDNode					(void) const;
	bool 										isStatusNode				(void) const;
	bool										isEnabled					(void) const; //same as status()
	inline bool									status						(void) const { return isEnabled(); } //same as isEnabled()

	void 										print						(const unsigned int& depth = -1, std::ostream& out = std::cout) const;
	std::string 								nodeDump					(void) const;  // used for debugging (when throwing exception)

	// make stream output easy
	friend std::ostream& 						operator<<					(
		std::ostream& out, const ConfigurationTree& t)
	{
		out << t.getValueAsString();
		return out;
	}

  protected:
	const unsigned int&        					getRow						(void) const;
	const unsigned int&        					getColumn					(void) const;
	const unsigned int&        					getFieldRow					(void) const;
	const unsigned int&        					getFieldColumn				(void) const;
	const TableViewColumnInfo& 					getColumnInfo				(void) const;

	// extracting information from a list of records
	struct RecordField
	{
		RecordField(const std::string&         table,
		            const std::string&         /*uid*/,
		            const std::string&         columnName,
		            const std::string&         relativePath,
		            const TableViewColumnInfo* columnInfo)
		    : tableName_(table)
		    , columnName_(columnName)
		    , relativePath_(relativePath)
		    , columnInfo_(columnInfo)
		{
		}

		std::string tableName_, columnName_, relativePath_;
		// relativePath_ is relative to record uid node, not including columnName_

		const TableViewColumnInfo* columnInfo_;
	};
	std::vector<ConfigurationTree::RecordField> getCommonFields				(
	    const std::vector<std::string /*relative-path*/>& recordList,
	    const std::vector<std::string /*relative-path*/>& fieldAcceptList,
	    const std::vector<std::string /*relative-path*/>& fieldRejectList,
	    unsigned int                                      depth = -1,
	    bool autoSelectFilterFields                             = false) const;
	std::set<std::string /*unique-value*/> 		getUniqueValuesForField		(
	    const std::vector<std::string /*relative-path*/>& recordList,
	    const std::string&                                fieldName,
		std::string*									  fieldGroupIDChildLinkIndex = 0) const;

  private:
	// private constructor: ONLY privately allow full access to member variables through constructor
	ConfigurationTree(const ConfigurationManager* const& configMgr,
	                  const TableBase* const&            config,
	                  const std::string&                 groupId,
	                  const TableBase* const&            linkParentConfig,
	                  const std::string&                 linkColName,
	                  const std::string&                 linkColValue,
	                  const unsigned int                 linkBackRow,
	                  const unsigned int                 linkBackCol,
	                  const std::string&                 disconnectedTargetName,
	                  const std::string&                 disconnectedLinkID,
	                  const std::string&                 childLinkIndex,
	                  const unsigned int                 row = TableView::INVALID,
	                  const unsigned int                 col = TableView::INVALID);

	static ConfigurationTree 					recurse						(const ConfigurationTree& t, const std::string& childPath, bool doNotThrowOnBrokenUIDLinks, const std::string& originalNodeString);
	ConfigurationTree        					recursiveGetNode			(const std::string& nodeName, bool doNotThrowOnBrokenUIDLinks, const std::string& originalNodeString) const;
	static void              					recursivePrint				(const ConfigurationTree& t, unsigned int depth, std::ostream& out, std::string space);

	void 										recursiveGetCommonFields	(
																			    std::vector<ConfigurationTree::RecordField>&      fieldCandidateList,
																			    std::vector<int>&                                 fieldCount,
																			    const std::vector<std::string /*relative-path*/>& fieldAcceptList,
																			    const std::vector<std::string /*relative-path*/>& fieldRejectList,
																			    unsigned int                                      depth,
																			    const std::string&                                relativePathBase,
																			    bool                                              inFirstRecord) const;
	ConfigurationTree 							getValueAsTreeNode			(void) const;

	// Any given ConfigurationTree is either a config, uid, or value node:
	//	- config node is a pointer to a config table
	//	- uid node is a pointer to a row in a config table
	//	- value node is a pointer to a cell in a config table
	//
	// Assumption: uid column is present
	const ConfigurationManager* 			configMgr_;  		// root node
	const TableBase*            			table_;      		// config node
	const std::string           			groupId_;    		// group config node
	const TableBase* 						linkParentTable_;  // link node parent config pointer (could be used
	                                     						// to traverse backwards through tree)
	const std::string  						linkColName_;     	// link node field name
	const std::string  						linkColValue_;    	// link node field value
	const unsigned int 						linkBackRow_;     	// source table link row
	const unsigned int 						linkBackCol_;     	// source table link col
	const std::string  						disconnectedTargetName_;  	// only used if disconnected to determine
	                   						                          	// target table name
	const std::string  						disconnectedLinkID_;  		// only used if disconnected to determine target link ID
	const std::string  						childLinkIndex_;  			// child link index
	const unsigned int 						row_;             			// uid node
	const unsigned int 						col_;             			// value node
	const TableView*   						tableView_;
};

#include "otsdaq/ConfigurationInterface/ConfigurationTree.icc"  //define template functions

// clang-format on
}  // namespace ots

#endif
