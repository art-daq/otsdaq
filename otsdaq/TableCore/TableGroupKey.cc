#include "otsdaq/TableCore/TableGroupKey.h"

#include "otsdaq/Macros/CoutMacros.h"

#include <string.h>   //for strlen
#include <stdexcept>  //for runtime_error

using namespace ots;

const unsigned int TableGroupKey::INVALID = -1;
const unsigned int TableGroupKey::DEFAULT = 0;

//==============================================================================
TableGroupKey::TableGroupKey(unsigned int key) : key_(key) {}

//==============================================================================
// groupString parameter can be the full group name, or just the group key
TableGroupKey::TableGroupKey(char* const& groupString)
{
	if(!groupString)
	{
		key_ = TableGroupKey::INVALID;
		return;
	}

	// find last character that is not part of key
	//	key consists of numeric, dash, underscore, and period
	int i = strlen(groupString) - 1;
	for(; i >= 0; --i)
		if((groupString[i] < '0' || groupString[i] > '9') && groupString[i] != '-' && groupString[i] != '_' && groupString[i] != '.')
			break;  // not part of key,... likely a 'v' if using "_v" syntax for version

	if(i == (int)strlen(groupString) - 1)  // no key characters found
	{
		key_ = TableGroupKey::DEFAULT;
		return;
	}
	else if(i < 0)  // only key characters found, so assume group key string was given
		i = 0;
	else
		++i;

	// at this point, i is start of key sequence
	sscanf(&groupString[i], "%u", &key_);
}

//==============================================================================
TableGroupKey::TableGroupKey(const std::string& groupString) : TableGroupKey((char*)groupString.c_str()) {}

//==============================================================================
TableGroupKey::~TableGroupKey(void) {}

//==============================================================================
unsigned int TableGroupKey::key(void) const { return key_; }

//==============================================================================
// operator==
bool TableGroupKey::operator==(unsigned int key) const { return (key_ == key); }
bool TableGroupKey::operator==(const TableGroupKey& key) const { return (key_ == key.key_); }


//==============================================================================
// operator*=
//	Only implemented to support StringMacros on TableGroupKey types (e.g. getMapFromString in StringMacros.icc)
TableGroupKey& TableGroupKey::operator*=(const unsigned int a) { key_ *= a; return *this; }
//==============================================================================
// operator*=
//	Only implemented to support StringMacros on TableGroupKey types (e.g. getMapFromString in StringMacros.icc)
TableGroupKey& TableGroupKey::operator*=(const TableGroupKey a) { key_ *= a.key_; return *this; }
//==============================================================================
// operator+=
//	Only implemented to support StringMacros on TableGroupKey types (e.g. getMapFromString in StringMacros.icc)
TableGroupKey& TableGroupKey::operator+=(const TableGroupKey a) { key_ += a.key_; return *this; }
//==============================================================================
// operator+=
//	Only implemented to support StringMacros on TableGroupKey types (e.g. getMapFromString in StringMacros.icc)
TableGroupKey& TableGroupKey::operator-=(const TableGroupKey a) { key_ -= a.key_; return *this; }
//==============================================================================
// operator/=
//	Only implemented to support StringMacros on TableGroupKey types (e.g. getMapFromString in StringMacros.icc)
TableGroupKey& TableGroupKey::operator/=(const TableGroupKey a) { key_ /= a.key_; return *this; }

//==============================================================================
// toString
std::string TableGroupKey::toString(void) const
{
	// represent invalid/temporary versions as negative number strings
	return (isInvalid()) ? std::to_string((int)key_) : std::to_string(key_);
}

//==============================================================================
// assignment operator with type int
TableGroupKey& TableGroupKey::operator=(const unsigned int key)
{
	key_ = key;
	return *this;
}

//==============================================================================
bool TableGroupKey::operator!=(unsigned int key) const { return (key_ != key); }
bool TableGroupKey::operator!=(const TableGroupKey& key) const { return (key_ != key.key_); }

//==============================================================================
// operator<
bool TableGroupKey::operator<(const TableGroupKey& key) const { return (key_ < key.key_); }

//==============================================================================
// operator>
bool TableGroupKey::operator>(const TableGroupKey& key) const { return (key_ > key.key_); }

//==============================================================================
// isInvalid
bool TableGroupKey::isInvalid() const { return (key_ == INVALID); }

//==============================================================================
// getNextKey
//	returns next key given the most recent key
//		if given nothing returns DEFAULT as first key
//		if given 0, returns 1, etc.
//	if no available keys left return INVALID
TableGroupKey TableGroupKey::getNextKey(const TableGroupKey& key)
{
	TableGroupKey retKey(key.key_ + 1);  // DEFAULT := INVALID + 1
	return retKey;                       // if retKey is invalid, then INVALID is returned already
}

//==============================================================================
unsigned int TableGroupKey::getDefaultKey(void) { return DEFAULT; }

//==============================================================================
unsigned int TableGroupKey::getInvalidKey(void) { return INVALID; }

//==============================================================================
// getGroupNameWithKey
//	returns next key given the most recent key
//		if given nothing returns DEFAULT as first key
//		if given 0, returns 1, etc.
//	if no available keys left return INVALID
std::string TableGroupKey::getFullGroupString(const std::string& groupName, const TableGroupKey& key)
{
	if(groupName.size() == 0)
	{
		__SS__
		    << ("TableGroupKey::getFullGroupString() Illegal Group Name! The Group Name "
		        "was not provided.\n");
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
	else if(groupName.size() == 1)
	{
		__SS__ << ("TableGroupKey::getFullGroupString() Illegal Group Name! The Group "
		           "Name is too short: \"" +
		           groupName + "\"")
		       << std::endl;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
	else
	{
		for(unsigned int i = 0; i < groupName.size(); ++i)
		{
			if(!(  // alphaNumeric
			       (groupName[i] >= 'A' && groupName[i] <= 'Z') || (groupName[i] >= 'a' && groupName[i] <= 'z') ||
			       (groupName[i] >= '0' && groupName[i] <= '9')))
			{
				__SS__ << ("TableGroupKey::getFullGroupString() Illegal Group Name! "
				           "Group Name must be alpha-numeric: \"" +
				           groupName + "\"")
				       << std::endl;
				__COUT_ERR__ << ss.str();
				__SS_THROW__;
			}
		}
	}

	return groupName + "_v" + std::to_string(key.key_);
}

//==============================================================================
void TableGroupKey::getGroupNameAndKey(const std::string& fullGroupString, std::string& groupName, TableGroupKey& key)
{
	auto i    = fullGroupString.rfind("_v");
	groupName = fullGroupString.substr(0, i);
	key       = TableGroupKey(fullGroupString.substr(i + 2));
}
