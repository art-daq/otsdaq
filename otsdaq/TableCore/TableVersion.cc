#include "otsdaq/TableCore/TableVersion.h"

using namespace ots;

const unsigned int TableVersion::INVALID = -1;
const unsigned int TableVersion::DEFAULT = 0;
const unsigned int TableVersion::SCRATCH = ~(1 << 31);  // highest positive integer

//==============================================================================
TableVersion::TableVersion(unsigned int version) : version_(version) {}
TableVersion::TableVersion(char* const& versionStr)
{
	if(!versionStr)
		version_ = TableVersion::INVALID;
	else
		sscanf(versionStr, "%u", &version_);
}
TableVersion::TableVersion(const std::string& versionStr) : TableVersion((char*)versionStr.c_str()) {}

//==============================================================================
TableVersion::~TableVersion(void) {}

//==============================================================================
// version
//	get version as integer
unsigned int TableVersion::version(void) const { return version_; }

//==============================================================================
// toString
std::string TableVersion::toString(void) const
{	
	// represent invalid/temporary versions as negative number strings
	return (isTemporaryVersion() || isInvalid()) ? std::to_string((int)version_) : std::to_string(version_);
}

//==============================================================================
// assignment operator with type int
TableVersion& TableVersion::operator=(const unsigned int version)
{
	version_ = version;
	return *this;
}

//==============================================================================
// operator*=
//	Only implemented to support StringMacros on TableVersion types (e.g. getMapFromString in StringMacros.icc)
TableVersion& TableVersion::operator*=(const unsigned int a) { version_ *= a; return *this; }
//==============================================================================
// operator*=
//	Only implemented to support StringMacros on TableVersion types (e.g. getMapFromString in StringMacros.icc)
TableVersion& TableVersion::operator*=(const TableVersion a) { version_ *= a.version_; return *this; }
//==============================================================================
// operator+=
//	Only implemented to support StringMacros on TableVersion types (e.g. getMapFromString in StringMacros.icc)
TableVersion& TableVersion::operator+=(const TableVersion a) { version_ += a.version_; return *this; }
//==============================================================================
// operator+=
//	Only implemented to support StringMacros on TableVersion types (e.g. getMapFromString in StringMacros.icc)
TableVersion& TableVersion::operator-=(const TableVersion a) { version_ -= a.version_; return *this; }
//==============================================================================
// operator/=
//	Only implemented to support StringMacros on TableVersion types (e.g. getMapFromString in StringMacros.icc)
TableVersion& TableVersion::operator/=(const TableVersion a) { version_ /= a.version_; return *this; }

//==============================================================================
// operator==
bool TableVersion::operator==(unsigned int version) const { return (version_ == version); }
bool TableVersion::operator==(const TableVersion& version) const { return (version_ == version.version_); }

//==============================================================================
// operator!=
bool TableVersion::operator!=(unsigned int version) const { return (version_ != version); }
bool TableVersion::operator!=(const TableVersion& version) const { return (version_ != version.version_); }

//==============================================================================
// operator<
bool TableVersion::operator<(const TableVersion& version) const { return (version_ < version.version_); }

//==============================================================================
// operator>
bool TableVersion::operator>(const TableVersion& version) const { return (version_ > version.version_); }

//==============================================================================
// isInvalid
bool TableVersion::isInvalid() const
{
	return (version_ == TableVersion::INVALID || (version_ > TableVersion::SCRATCH && version_ < INVALID - NUM_OF_TEMP_VERSIONS));  // unused reserved block
}

//==============================================================================
// isTemporaryVersion
//	claim a block of unsigned ints to designate as temporary versions
bool TableVersion::isTemporaryVersion() const { return (version_ >= INVALID - NUM_OF_TEMP_VERSIONS && version_ != INVALID); }

//==============================================================================
// isScratchVersion
//	claim the most positive signed integer as the scratch version
bool TableVersion::isScratchVersion() const { return (version_ == TableVersion::SCRATCH); }

//==============================================================================
// isMockupVersion
//	the INVALID index represents the mockup version
bool TableVersion::isMockupVersion() const { return (version_ == TableVersion::INVALID); }

//==============================================================================
// getNextVersion
//	returns next version given the most recent version
//		if given nothing returns DEFAULT as first version
//		if given 0, returns 1, etc.
//	if no available versions left return INVALID
TableVersion TableVersion::getNextVersion(const TableVersion& version)
{
	TableVersion retVersion(version.version_ + 1);  // DEFAULT := INVALID + 1
	return (!retVersion.isInvalid() && !retVersion.isTemporaryVersion()) ? retVersion : TableVersion();
}

//==============================================================================
// getNextTemporaryVersion
//	returns next Temporary version given the most recent temporary version
//		if given nothing returns -2 as first temporary version
//		if given -2, returns -3, etc.
//	if no available temporary versions left return INVALID
TableVersion TableVersion::getNextTemporaryVersion(const TableVersion& version)
{
	TableVersion retVersion(version.isTemporaryVersion() ? (version.version_ - 1) : (INVALID - 1));
	return retVersion.isTemporaryVersion() ? retVersion : TableVersion();
}
