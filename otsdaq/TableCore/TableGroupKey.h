#ifndef _ots_TableGroupKey_h_
#define _ots_TableGroupKey_h_

#include <ostream>

namespace ots
{
// TableGroupKey is the type used for versions association with global configurations
//(whereas TableVersion is the type used for version associated with a configuration
// table)
class TableGroupKey
{
  public:
	explicit TableGroupKey(unsigned int key = INVALID);
	explicit TableGroupKey(char* const& groupString);
	explicit TableGroupKey(const std::string& groupString);
	virtual ~TableGroupKey(void);

	unsigned int key(void) const;
	bool         isInvalid(void) const;
	std::string  toString(void) const;

	// Operators
	TableGroupKey& operator=(const unsigned int key);
	bool           operator==(unsigned int key) const;
	bool           operator==(const TableGroupKey& key) const;
	bool           operator!=(unsigned int key) const;
	bool           operator!=(const TableGroupKey& key) const;
	bool           operator<(const TableGroupKey& key) const;
	bool           operator>(const TableGroupKey& key) const;
	bool           operator<=(const TableGroupKey& key) const { return !operator>(key); }
	bool           operator>=(const TableGroupKey& key) const { return !operator<(key); }
	TableGroupKey&  operator*=(const unsigned int a);  //to support StringMacros on TableGroupKey types
	TableGroupKey&  operator*=(const TableGroupKey a); //to support StringMacros on TableGroupKey types
	TableGroupKey&  operator+=(const TableGroupKey a); //to support StringMacros on TableGroupKey types
	TableGroupKey&  operator-=(const TableGroupKey a); //to support StringMacros on TableGroupKey types
	TableGroupKey&  operator/=(const TableGroupKey a); //to support StringMacros on TableGroupKey types

	friend std::ostream& operator<<(std::ostream& out, const TableGroupKey& key)
	{
		out << key.toString();
		return out;
	}

	static TableGroupKey getNextKey(const TableGroupKey& key = TableGroupKey());
	static std::string   getFullGroupString(const std::string& groupName, const TableGroupKey& key);
	static void          getGroupNameAndKey(const std::string& fullGroupString, std::string& groupName, TableGroupKey& key);
	static unsigned int  getDefaultKey(void);
	static unsigned int  getInvalidKey(void);

	static const unsigned int INVALID;
  private:
	static const unsigned int DEFAULT;
	unsigned int              key_;
};
}  // namespace ots
#endif
