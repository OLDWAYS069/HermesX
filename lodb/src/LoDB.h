#pragma once

#include "lofs/src/LoFS.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <pb.h>
#include <string>
#include <vector>

/**
 * LoDB - Synchronous Protobuf Database
 *
 * A filesystem-based database for protobuf records stored in {prefix}/lodb/<db_name>/<table_name>/<uuid>.pr files.
 * Uses /sd/lodb/... if SD card is available, otherwise /internal/lodb/...
 *
 * SYNCHRONOUS DESIGN:
 * - Single-record operations (get, insert, update, delete) complete immediately
 * - SELECT returns complete result sets as vectors with optional filtering, sorting, and limiting
 */

// UUID type - 64-bit unsigned integer
typedef uint64_t lodb_uuid_t;

// UUID formatting macros for platforms without %llx support
#define LODB_UUID_FMT "%08x%08x"
#define LODB_UUID_ARGS(uuid) (uint32_t)((uuid) >> 32), (uint32_t)((uuid)&0xFFFFFFFF)

/**
 * Error codes returned by LoDB operations
 */
typedef enum {
    LODB_OK = 0,        // Success
    LODB_ERR_NOT_FOUND, // UUID doesn't exist
    LODB_ERR_IO,        // Filesystem error
    LODB_ERR_DECODE,    // Protobuf decode failed
    LODB_ERR_ENCODE,    // Protobuf encode failed
    LODB_ERR_INVALID    // Invalid parameters
} LoDbError;

/**
 * Filter function: returns true to include record in results
 * Supports lambdas with captures via std::function
 * @param record Pointer to the decoded protobuf record
 * @return true to include record, false to skip
 */
typedef std::function<bool(const void *)> LoDbFilter;

/**
 * Comparator function: returns -1/0/1 for sorting
 * Supports lambdas with captures via std::function
 * @param a Pointer to first record
 * @param b Pointer to second record
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
typedef std::function<int(const void *, const void *)> LoDbComparator;

/**
 * Convert UUID to 16-character hex string for filenames
 * @param uuid UUID to convert
 * @param hex_out Buffer to store hex string (must be at least 17 bytes for null terminator)
 */
void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17]);

/**
 * Generate or derive a UUID
 * @param str String to hash into UUID (NULL for auto-generated)
 * @param salt Optional salt value (0 for none, typically node ID for user-specific UUIDs)
 * @return 64-bit UUID - auto-generated if str is NULL, otherwise SHA256(str + salt)
 */
lodb_uuid_t lodb_new_uuid(const char *str, uint64_t salt);

    /**
     * LoDB Database Class
     *
     * A database instance with a namespace. Tables within this database
     * are stored at {prefix}/lodb/{db_name}/{table_name}/
     * Prefix is /sd if SD card is available, otherwise /internal
     */
    class LoDb
{
  public:
    /**
     * Create a new database instance
     * @param db_name Name of the database (creates {prefix}/lodb/{db_name}/ directory)
     * @param filesystem Filesystem type (LoFS::FSType::INTERNAL, LoFS::FSType::SD, or LoFS::FSType::AUTO for auto-select)
     */
    LoDb(const char *db_name, LoFS::FSType filesystem = LoFS::FSType::AUTO);

    /**
     * Destructor
     */
    ~LoDb();

    /**
     * Register a table with this database
     * @param table_name Name of the table (directory name)
     * @param pb_descriptor Nanopb message descriptor for the protobuf type
     * @param record_size Size of the in-memory struct (sizeof)
     * @return LODB_OK on success, error code otherwise
     */
    LoDbError registerTable(const char *table_name, const pb_msgdesc_t *pb_descriptor, size_t record_size);

    /**
     * Insert a new record with a UUID
     * @param table_name Name of the table to insert into
     * @param uuid UUID to use for this record
     * @param record Pointer to the protobuf record to insert
     * @return LODB_OK on success, LODB_ERR_INVALID if UUID exists or table not registered, error code otherwise
     */
    LoDbError insert(const char *table_name, lodb_uuid_t uuid, const void *record);

    /**
     * Get a record by UUID
     * @param table_name Name of the table to read from
     * @param uuid UUID of the record to retrieve
     * @param record_out Buffer to store decoded record (must be at least record_size bytes)
     * @return LODB_OK on success, LODB_ERR_NOT_FOUND if UUID doesn't exist, error code otherwise
     */
    LoDbError get(const char *table_name, lodb_uuid_t uuid, void *record_out);

    /**
     * Update a single record by UUID
     * @param table_name Name of the table to update
     * @param uuid UUID of the record to update
     * @param record Pointer to the updated protobuf record
     * @return LODB_OK on success, LODB_ERR_NOT_FOUND if UUID doesn't exist, error code otherwise
     */
    LoDbError update(const char *table_name, lodb_uuid_t uuid, const void *record);

    /**
     * Delete a single record by UUID
     * @param table_name Name of the table to delete from
     * @param uuid UUID of the record to delete
     * @return LODB_OK on success, LODB_ERR_NOT_FOUND if UUID doesn't exist, error code otherwise
     */
    LoDbError deleteRecord(const char *table_name, lodb_uuid_t uuid);

    /**
     * Select records from a table with optional filtering, sorting, and limiting
     *
     * Operation order: FILTER → SORT → LIMIT
     *
     * @param table_name Name of the table to query
     * @param filter Optional filter function (NULL to select all records)
     * @param comparator Optional comparator for sorting (NULL for no sorting)
     * @param limit Optional result limit (0 for no limit)
     * @return Vector of heap-allocated record pointers (caller must free each with delete[])
     *
     * USAGE:
     *   auto filter = [](const void* rec) -> bool {
     *       auto* user = (const User*)rec;
     *       return user->active;
     *   };
     *
     *   auto comparator = [](const void* a, const void* b) -> int {
     *       auto* u1 = (const User*)a;
     *       auto* u2 = (const User*)b;
     *       return strcmp(u1->name, u2->name);
     *   };
     *
     *   auto results = db->select("users", filter, comparator, 10);
     *   for (auto* rec : results) {
     *       auto* user = (User*)rec;
     *       // ... use user
     *       delete[] (uint8_t*)rec;  // Free the record
     *   }
     */
    std::vector<void *> select(const char *table_name, LoDbFilter filter = LoDbFilter(),
                               LoDbComparator comparator = LoDbComparator(), size_t limit = 0);

    /**
     * Free a vector of records returned by select()
     * @param records Vector of record pointers to free
     */
    static void freeRecords(std::vector<void *> &records);

    /**
     * Count records in a table with optional filtering
     * 
     * If no filter is provided, this efficiently counts files without loading records.
     * If a filter is provided, records are loaded and filtered (less efficient).
     * 
     * @param table_name Name of the table to count
     * @param filter Optional filter function (NULL to count all records)
     * @return Number of matching records, or -1 on error
     */
    int count(const char *table_name, LoDbFilter filter = LoDbFilter());

    /**
     * Truncate a table - delete all records but keep the table registered
     * @param table_name Name of the table to truncate
     * @return LODB_OK on success, LODB_ERR_INVALID if table not registered, error code otherwise
     */
    LoDbError truncate(const char *table_name);

    /**
     * Drop a table - delete all records and unregister the table
     * @param table_name Name of the table to drop
     * @return LODB_OK on success, LODB_ERR_INVALID if table not registered, error code otherwise
     */
    LoDbError drop(const char *table_name);

  private:
    /**
     * Table metadata
     */
    struct TableMetadata {
        std::string table_name;
        const pb_msgdesc_t *pb_descriptor;
        size_t record_size;
        char table_path[160]; // Full path: {prefix}/lodb/{db_name}/{table_name}/
    };

    std::string db_name;
    char fs_prefix[10]; // "/sd" or "/internal"
    char db_path[128]; // {prefix}/lodb/{db_name}/
    std::map<std::string, TableMetadata> tables;

    /**
     * Get table metadata by name
     * @param table_name Name of the table
     * @return Pointer to table metadata, NULL if not found
     */
    TableMetadata *getTable(const char *table_name);
};
