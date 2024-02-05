#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/FESlowControlsTable.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"

#include <iostream>

using namespace ots;

//==============================================================================
FESlowControlsTable::FESlowControlsTable(void) : TableBase("FESlowControlsTable") {}

//==============================================================================
FESlowControlsTable::~FESlowControlsTable(void) {}

//==============================================================================
// init
//	Validates user inputs for data type.
void FESlowControlsTable::init(ConfigurationManager* configManager)
{
	// use isFirstAppInContext to only run once per context, for example to avoie
	//	generating files on local disk multiple times.
	bool isFirstAppInContext = configManager->isOwnerFirstAppInContext();

	if(!isFirstAppInContext)
		return;


	std::string                                            childType;
	std::vector<std::pair<std::string, ConfigurationTree>> childrenMap = configManager->__SELF_NODE__.getChildren();
	for(auto& childPair : childrenMap)
	{
		// check each row in table
		__COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << childPair.first << std::endl;
		childPair.second.getNode(colNames_.colDataType_).getValue(childType);
		__COUT_TYPE__(TLVL_DEBUG+12) << "childType=" << childType << std::endl;

		if(childType[childType.size() - 1] == 'b')  // if ends in 'b' then take that many bits
		{
			unsigned int sz;
			sscanf(&childType[0], "%u", &sz);
			if(sz < 1 || sz > 64)
			{
				__SS__ << "Data type '" << childType << "' for UID=" << childPair.first << " is invalid. "
				       << " The bit size given was " << sz << " and it must be between 1 and 64." << std::endl;
				__COUT_ERR__ << "\n" << ss.str();
				__SS_THROW__;
			}
		}
		else if(childType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && childType != "char" && childType != "unsigned char" && childType != "short" &&
		        childType != "unsigned short" && childType != "int" && childType != "unsigned int" && childType != "long long " &&
		        childType != "unsigned long long" && childType != "float" && childType != "double")
		{
			__SS__ << "Data type '" << childType << "' for UID=" << childPair.first << " is invalid. "
			       << "Valid data types (w/size in bytes) are as follows: "
			       << "#b (# bits)"
			       << ", char (" << sizeof(char) << "B), unsigned char (" << sizeof(unsigned char) << "B), short (" << sizeof(short) << "B), unsigned short ("
			       << sizeof(unsigned short) << "B), int (" << sizeof(int) << "B), unsigned int (" << sizeof(unsigned int) << "B), long long ("
			       << sizeof(long long) << "B), unsigned long long (" << sizeof(unsigned long long) << "B), float (" << sizeof(float) << "B), double ("
			       << sizeof(double) << "B)." << std::endl;
			__COUT_ERR__ << "\n" << ss.str();
			__SS_THROW__;
		}
	}
}  // end init()

DEFINE_OTS_TABLE(FESlowControlsTable)
