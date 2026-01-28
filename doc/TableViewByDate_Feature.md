# Table View by Date Feature

## Overview

This feature adds the ability to view and filter table versions in the otsdaq configuration system based on their creation date. This is particularly useful for:

- Finding all tables created in a specific time period (e.g., last week, last month)
- Viewing recent changes to configuration tables
- Debugging configuration issues by examining when versions were created
- Auditing configuration changes over time

## New API

### TableVersionMetadata Structure

A new structure has been added to hold version metadata:

```cpp
struct TableVersionMetadata
{
    TableVersion version;       // The version number
    time_t       creationTime;  // Unix timestamp when created
    std::string  author;        // Who created the version
    std::string  comment;       // Version comment
    bool         metadataValid; // true if metadata was successfully loaded
};
```

**Note**: The `metadataValid` field indicates whether the metadata was successfully loaded. Versions with `metadataValid=false` are excluded from date-based filtering operations.

### ConfigurationInterface Methods

Three new methods have been added to `ConfigurationInterface`:

1. **getVersionsWithMetadata()** - Get all versions with their metadata
   ```cpp
   std::vector<TableVersionMetadata> getVersionsWithMetadata(const TableBase* table) const;
   ```

2. **filterVersionsByDateRange()** - Filter versions by time range
   ```cpp
   std::vector<TableVersionMetadata> filterVersionsByDateRange(
       const std::vector<TableVersionMetadata>& versions,
       time_t startTime,
       time_t endTime) const;
   ```

3. **filterVersionsLastNDays()** - Convenience method for last N days
   ```cpp
   std::vector<TableVersionMetadata> filterVersionsLastNDays(
       const std::vector<TableVersionMetadata>& versions,
       unsigned int numDays) const;
   ```

### ConfigurationManagerRW Methods

Four new methods have been added to `ConfigurationManagerRW` for convenient access:

1. **getTableVersionsWithMetadata()** - Get versions with metadata for a specific table
   ```cpp
   std::vector<TableVersionMetadata> getTableVersionsWithMetadata(const std::string& tableName);
   ```

2. **filterTableVersionsByDateRange()** - Filter table versions by date range
   ```cpp
   std::vector<TableVersionMetadata> filterTableVersionsByDateRange(
       const std::string& tableName,
       time_t startTime,
       time_t endTime);
   ```

3. **filterTableVersionsLastNDays()** - Filter table versions from last N days
   ```cpp
   std::vector<TableVersionMetadata> filterTableVersionsLastNDays(
       const std::string& tableName,
       unsigned int numDays);
   ```

4. **getAllTableVersionsWithMetadata()** - Get metadata for all tables
   ```cpp
   std::map<std::string, std::vector<TableVersionMetadata>> 
       getAllTableVersionsWithMetadata(bool onlyActiveTables = false);
   ```

## Usage Examples

### Example 1: Get all versions with metadata

```cpp
ConfigurationManagerRW cfgMgr("username");
std::vector<TableVersionMetadata> versions = 
    cfgMgr.getTableVersionsWithMetadata("MyTableName");

for(const auto& v : versions)
{
    std::cout << "Version " << v.version 
              << " created at " << v.creationTime 
              << " by " << v.author 
              << std::endl;
}
```

### Example 2: Find versions from the last week

```cpp
ConfigurationManagerRW cfgMgr("username");
std::vector<TableVersionMetadata> recentVersions = 
    cfgMgr.filterTableVersionsLastNDays("MyTableName", 7);

std::cout << "Found " << recentVersions.size() 
          << " versions from the last week" << std::endl;
```

### Example 3: Custom date range

```cpp
ConfigurationManagerRW cfgMgr("username");

// Get versions from 30 days ago to 7 days ago
time_t now = time(0);
time_t startTime = now - (30 * 24 * 60 * 60);
time_t endTime = now - (7 * 24 * 60 * 60);

std::vector<TableVersionMetadata> versions = 
    cfgMgr.filterTableVersionsByDateRange("MyTableName", startTime, endTime);
```

### Example 4: Get metadata for all tables

```cpp
ConfigurationManagerRW cfgMgr("username");

// Get metadata for all active tables
auto allMetadata = cfgMgr.getAllTableVersionsWithMetadata(true);

for(const auto& tablePair : allMetadata)
{
    std::cout << "Table: " << tablePair.first 
              << " has " << tablePair.second.size() 
              << " versions" << std::endl;
}
```

## Implementation Details

### Metadata Loading

The implementation loads the table metadata by:
1. Getting the list of version numbers using the existing `getVersions()` method
2. For each version, loading the table to access the metadata stored in the `TableView`
3. Extracting the creation time, author, and comment from each version

This approach reuses the existing metadata storage mechanism where:
- Creation time is stored as a Unix timestamp (`time_t`)
- Author and comment are stored as strings
- This metadata is loaded from the database along with the table data

### Performance Considerations

- **Initial load**: The first call to `getVersionsWithMetadata()` will load all versions to extract metadata. This may take time for tables with many versions as it currently performs full table loads.
- **Caching recommended**: Consider caching the results if you need to perform multiple filtering operations on the same table. Call `getTableVersionsWithMetadata()` once and then apply filters to the cached result.
- **Partial loading**: The current implementation loads full table data to get metadata. Future optimizations could add database-level queries to get metadata without loading full tables, which would significantly improve performance.
- **Parameter validation**: Input parameters are validated to prevent overflow and invalid ranges, throwing exceptions for invalid values.

### Error Handling

- If a version fails to load, the method logs a warning and includes an entry with `metadataValid=false`
- Versions with invalid metadata are excluded from date-based filtering operations
- If a table doesn't exist, an exception is thrown
- Empty results are returned if no versions match the filter criteria
- Invalid parameters (negative times, endTime < startTime, excessive day counts) throw exceptions

## Integration with Configuration GUI

This functionality can be integrated into the ConfigurationGUISupervisor (in otsdaq-utilities) by:

1. Adding a date filter UI component
2. Calling the new API methods to retrieve filtered versions
3. Displaying the results in the existing table view interface

Example request handler in ConfigurationGUISupervisor:
```cpp
if(requestType == "getTableVersionsByDate")
{
    std::string tableName = CgiDataUtilities::getData(cgiIn, "tableName");
    unsigned int numDays = std::stoi(CgiDataUtilities::getData(cgiIn, "numDays"));
    
    auto versions = cfgMgr->filterTableVersionsLastNDays(tableName, numDays);
    
    // Serialize to XML for response
    for(const auto& v : versions)
    {
        xmlOut.addTextElementToData("version", v.version.toString());
        xmlOut.addTextElementToData("creationTime", std::to_string(v.creationTime));
        xmlOut.addTextElementToData("author", v.author);
        xmlOut.addTextElementToData("comment", v.comment);
    }
}
```

## Future Enhancements

Potential improvements to this feature:
1. Add database-level queries to retrieve metadata without loading full tables
2. Add caching layer for frequently accessed metadata
3. Support for more complex filters (e.g., by author, by comment keywords)
4. Sort options (by date, by author, by version number)
5. Pagination for large result sets
6. Export filtered results to CSV or JSON

## See Also

- See `doc/TableViewByDate_Example.cc` for complete working examples
- TableView class for metadata storage details
- ConfigurationInterface for low-level configuration access
- ConfigurationManagerRW for high-level configuration management
