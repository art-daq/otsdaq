#include "otsdaq/ConfigurationInterface/Database_configInterface.h"

#include <algorithm>
#include <iterator>

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

// #include "ConfigurationInterface.h"
#include "artdaq-database/ConfigurationDB/configurationdbifc.h"

#include "artdaq-database/BasicTypes/basictypes.h"
using artdaq::database::basictypes::FhiclData;
using artdaq::database::basictypes::JsonData;

namespace artdaq
{
namespace database
{
namespace configuration
{
using ots::TableBase;

template<>
template<>
bool MakeSerializable<TableBase const*>::writeDocumentImpl<JsonData>(JsonData& data) const
{
	std::stringstream ss;

	_conf->getView().printJSON(ss);
	// std::cout << "!!!!!\n" << ss.str() << std::endl; //for debugging
	data.json_buffer = ss.str();

	return true;
}

template<>
std::string MakeSerializable<TableBase const*>::configurationNameImpl() const
{
	return _conf->getTableName();
}

template<>
template<>
bool MakeSerializable<TableBase*>::readDocumentImpl<JsonData>(JsonData const& data)
{
	int retVal = _conf->getViewP()->fillFromJSON(data.json_buffer);

	return (retVal >= 0);
}

template<>
std::string MakeSerializable<TableBase*>::configurationNameImpl() const
{
	return _conf->getTableName();
}

}  // namespace configuration
}  // namespace database
}  // namespace artdaq
