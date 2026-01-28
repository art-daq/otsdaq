// Example usage of table view by date functionality
// This demonstrates how to filter table versions by creation date

#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"
#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include <iostream>
#include <iomanip>
#include <ctime>

using namespace ots;

// Helper function to format time_t as readable string
std::string formatTime(time_t t)
{
	if(t == 0)
		return "Unknown";
	
	std::tm* tm = std::localtime(&t);
	char buffer[100];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
	return std::string(buffer);
}

// Example 1: Get all versions of a table with metadata
void example1_getAllVersionsWithMetadata(ConfigurationManagerRW* cfgMgr, 
                                          const std::string& tableName)
{
	std::cout << "\n=== Example 1: Get all versions with metadata ===" << std::endl;
	
	try
	{
		std::vector<TableVersionMetadata> versions = 
		    cfgMgr->getTableVersionsWithMetadata(tableName);
		
		std::cout << "Found " << versions.size() << " versions of table '" 
		          << tableName << "':" << std::endl;
		
		for(const auto& versionMeta : versions)
		{
			std::cout << "  Version: " << versionMeta.version 
			          << " | Created: " << formatTime(versionMeta.creationTime)
			          << " | Author: " << versionMeta.author
			          << " | Comment: " << versionMeta.comment
			          << std::endl;
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

// Example 2: Filter versions created in the last week
void example2_filterLastWeek(ConfigurationManagerRW* cfgMgr, 
                              const std::string& tableName)
{
	std::cout << "\n=== Example 2: Filter versions from last 7 days ===" << std::endl;
	
	try
	{
		std::vector<TableVersionMetadata> recentVersions = 
		    cfgMgr->filterTableVersionsLastNDays(tableName, 7);
		
		std::cout << "Found " << recentVersions.size() 
		          << " versions created in the last week:" << std::endl;
		
		for(const auto& versionMeta : recentVersions)
		{
			std::cout << "  Version: " << versionMeta.version 
			          << " | Created: " << formatTime(versionMeta.creationTime)
			          << " | Author: " << versionMeta.author
			          << std::endl;
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

// Example 3: Filter versions by specific date range
void example3_filterByDateRange(ConfigurationManagerRW* cfgMgr, 
                                 const std::string& tableName)
{
	std::cout << "\n=== Example 3: Filter by custom date range ===" << std::endl;
	
	try
	{
		// Create time range: from 30 days ago to 7 days ago
		time_t now = time(0);
		time_t startTime = now - (30 * 24 * 60 * 60);  // 30 days ago
		time_t endTime = now - (7 * 24 * 60 * 60);     // 7 days ago
		
		std::vector<TableVersionMetadata> filteredVersions = 
		    cfgMgr->filterTableVersionsByDateRange(tableName, startTime, endTime);
		
		std::cout << "Found " << filteredVersions.size() 
		          << " versions between " << formatTime(startTime)
		          << " and " << formatTime(endTime) << ":" << std::endl;
		
		for(const auto& versionMeta : filteredVersions)
		{
			std::cout << "  Version: " << versionMeta.version 
			          << " | Created: " << formatTime(versionMeta.creationTime)
			          << std::endl;
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

// Example 4: Get metadata for all tables
void example4_getAllTablesMetadata(ConfigurationManagerRW* cfgMgr)
{
	std::cout << "\n=== Example 4: Get metadata for all tables ===" << std::endl;
	
	try
	{
		std::map<std::string, std::vector<TableVersionMetadata>> allMetadata = 
		    cfgMgr->getAllTableVersionsWithMetadata(false /* include all tables */);
		
		std::cout << "Found metadata for " << allMetadata.size() << " tables:" << std::endl;
		
		for(const auto& tablePair : allMetadata)
		{
			const std::string& tableName = tablePair.first;
			const auto& versions = tablePair.second;
			
			std::cout << "\nTable: " << tableName 
			          << " (" << versions.size() << " versions)" << std::endl;
			
			// Show only the 3 most recent versions
			int count = 0;
			for(auto it = versions.rbegin(); 
			    it != versions.rend() && count < 3; 
			    ++it, ++count)
			{
				std::cout << "  Version: " << it->version 
				          << " | Created: " << formatTime(it->creationTime)
				          << std::endl;
			}
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

// Main demonstration
int main()
{
	std::cout << "==================================================" << std::endl;
	std::cout << "Table View by Date - Usage Examples" << std::endl;
	std::cout << "==================================================" << std::endl;
	
	try
	{
		// Initialize configuration manager
		ConfigurationManagerRW cfgMgr("ExampleUser");
		
		// Replace "YourTableName" with an actual table name in your system
		std::string exampleTableName = "YourTableName";
		
		// Run examples
		example1_getAllVersionsWithMetadata(&cfgMgr, exampleTableName);
		example2_filterLastWeek(&cfgMgr, exampleTableName);
		example3_filterByDateRange(&cfgMgr, exampleTableName);
		example4_getAllTablesMetadata(&cfgMgr);
		
		std::cout << "\n==================================================" << std::endl;
		std::cout << "Examples completed successfully!" << std::endl;
		std::cout << "==================================================" << std::endl;
	}
	catch(const std::exception& e)
	{
		std::cerr << "\nFatal error: " << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}
