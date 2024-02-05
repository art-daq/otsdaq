#ifndef _ots_TableVersion_h_
#define _ots_TableVersion_h_

#include <ostream>

namespace ots
{
// TableVersion is the type used for version associated with a configuration table
//(whereas TableGroupKey is the type used for versions association with global
// configurations)
//
// Designed so that version type could be changed easily, e.g. to string
class TableVersion
{
  public:
	static const unsigned int INVALID;
	static const unsigned int DEFAULT;
	static const unsigned int SCRATCH;

	explicit TableVersion(unsigned int version = INVALID);
	explicit TableVersion(char* const& versionStr);
	explicit TableVersion(const std::string& versionStr);
	virtual ~TableVersion(void);

	unsigned int version(void) const;
	bool         isTemporaryVersion(void) const;
	bool         isScratchVersion(void) const;
	bool         isMockupVersion(void) const;
	bool         isInvalid(void) const;
	std::string  toString(void) const;

	// Operators
	TableVersion& operator=(const unsigned int version);
	bool          operator==(unsigned int version) const;
	bool          operator==(const TableVersion& version) const;
	bool          operator!=(unsigned int version) const;
	bool          operator!=(const TableVersion& version) const;
	bool          operator<(const TableVersion& version) const;
	bool          operator>(const TableVersion& version) const;
	bool          operator<=(const TableVersion& version) const { return !operator>(version); }
	bool          operator>=(const TableVersion& version) const { return !operator<(version); }
	TableVersion& operator*=(const unsigned int a); //to support StringMacros on TableVersion types
	TableVersion& operator*=(const TableVersion a); //to support StringMacros on TableVersion types
	TableVersion& operator+=(const TableVersion a); //to support StringMacros on TableVersion types
	TableVersion& operator-=(const TableVersion a); //to support StringMacros on TableVersion types
	TableVersion& operator/=(const TableVersion a); //to support StringMacros on TableVersion types

	friend std::ostream& operator<<(std::ostream& out, const TableVersion& version)
	{
		if(version.isScratchVersion())
			out << "ScratchVersion";
		else if(version.isMockupVersion())
			out << "Mock-up";
		else if(version.isInvalid())
			out << "InvalidVersion";
		else
			out << version.toString();
		return out;
	}

	static TableVersion getNextVersion(const TableVersion& version = TableVersion());
	static TableVersion getNextTemporaryVersion(const TableVersion& version = TableVersion());

  protected:
	enum
	{
		NUM_OF_TEMP_VERSIONS = 10000
	};

	unsigned int version_;
};
}  // namespace ots
#endif
