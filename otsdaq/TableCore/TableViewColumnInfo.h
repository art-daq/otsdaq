#ifndef _ots_TableTableViewColumnInfo_h_
#define _ots_TableTableViewColumnInfo_h_

#include <map>
#include <memory> /* shared_ptr */
#include <string>
#include <vector>

namespace ots
{
// clang-format off
class TableViewColumnInfo
{
  public:
	TableViewColumnInfo(const std::string& type,
	                    const std::string& name,
	                    const std::string& storageName,
	                    const std::string& dataType,
	                    const std::string* defaultValue,
	                    const std::string& dataChoicesCSV,
						const std::string* minValue,
						const std::string* maxValue,
	                    std::string*       capturedExceptionString);

	TableViewColumnInfo(const TableViewColumnInfo& c);             // copy constructor because of bitmap pointer
	TableViewColumnInfo& operator=(const TableViewColumnInfo& c);  // assignment operator because of bitmap pointer

	virtual ~TableViewColumnInfo(void);

	const std::string&              getType(void) const;
	const std::string&              getName(void) const;
	const std::string&              getStorageName(void) const;
	const std::string&              getDataType(void) const;
	const std::string&              getDefaultValue(void) const;
	const std::string&              getMinValue(void) const;
	const std::string&              getMaxValue(void) const;
	static const std::string&       getDefaultDefaultValue(const std::string& type, const std::string& dataType);
	static const std::string&       getMinDefaultValue(const std::string& dataType);
	static const std::string&       getMaxDefaultValue(const std::string& dataType);
	const std::vector<std::string>& getDataChoices(void) const;

	struct BitMapInfo  // uses dataChoices CSV fields if type is TYPE_BITMAP_DATA
	{
		BitMapInfo() : minColor_(""), midColor_(""), maxColor_("") {}
		unsigned int numOfRows_, numOfColumns_, cellBitSize_;
		uint64_t     minValue_, maxValue_, stepValue_;
		std::string  aspectRatio_;
		std::string  minColor_, midColor_, maxColor_;
		std::string  absMinColor_, absMaxColor_;
		bool         rowsAscending_, colsAscending_, snakeRows_, snakeCols_;
	};
	const BitMapInfo& getBitMapInfo(void) const;  // uses dataChoices CSV fields if type is TYPE_BITMAP_DATA

	static std::vector<std::string>                                   getAllTypesForGUI(void);
	static std::map<std::pair<std::string, std::string>, std::string> getAllDefaultsForGUI(void);
	static std::vector<std::string>                                   getAllDataTypesForGUI(void);

	 bool isChildLink(void) const;
	 static bool isChildLink(const std::string& type);
	 bool isChildLinkUID(void) const;
	 bool isChildLinkGroupID(void) const;
	 bool isGroupID(void) const;
	 bool isUID(void) const;
	 bool isBoolType(void) const;
	 bool isNumberDataType(void) const;

	std::string getChildLinkIndex(void) const;

	static const std::string TYPE_UID;
	static const std::string TYPE_DATA, TYPE_UNIQUE_DATA, TYPE_UNIQUE_GROUP_DATA, TYPE_MULTILINE_DATA, TYPE_FIXED_CHOICE_DATA, TYPE_BITMAP_DATA;
	static const std::string TYPE_ON_OFF, TYPE_TRUE_FALSE, TYPE_YES_NO;
	static const std::string TYPE_COMMENT, TYPE_AUTHOR, TYPE_TIMESTAMP;
	static const std::string TYPE_START_CHILD_LINK, TYPE_START_CHILD_LINK_UID, TYPE_START_CHILD_LINK_GROUP_ID, TYPE_START_GROUP_ID;
	static const std::string DATATYPE_NUMBER, DATATYPE_STRING, DATATYPE_TIME;

	static const std::string TYPE_VALUE_YES;
	static const std::string TYPE_VALUE_NO;
	static const std::string TYPE_VALUE_TRUE;
	static const std::string TYPE_VALUE_FALSE;
	static const std::string TYPE_VALUE_ON;
	static const std::string TYPE_VALUE_OFF;

	static const std::string DATATYPE_STRING_DEFAULT;
	static const std::string DATATYPE_COMMENT_DEFAULT;
	static const std::string DATATYPE_BOOL_DEFAULT;
	static const std::string DATATYPE_NUMBER_DEFAULT;
	static const std::string DATATYPE_NUMBER_MIN_DEFAULT;
	static const std::string DATATYPE_NUMBER_MAX_DEFAULT;
	static const std::string DATATYPE_TIME_DEFAULT;
	static const std::string DATATYPE_LINK_DEFAULT;

	static const std::string COL_NAME_STATUS, COL_NAME_ENABLED, COL_NAME_PRIORITY, COL_NAME_COMMENT, COL_NAME_AUTHOR, COL_NAME_CREATION;

  private:
	TableViewColumnInfo();  // private constructor, only used in assignment operator
	void extractBitMapInfo();

	std::vector<std::string>        getDataChoicesFromString(const std::string& dataChoicesCSV) const;

  protected:
	const std::string        	   type_;
	const std::string              name_;
	const std::string              storageName_;
	const std::string              dataType_;
	const std::string              defaultValue_;
	const std::vector<std::string> dataChoices_;
	const std::string              minValue_;
	const std::string              maxValue_;
	BitMapInfo*              	   bitMapInfoP_;
}; //end TableViewColumnInfo class
// clang-format on
}  // namespace ots

#endif
