#include "LoDB.h"
#include "lofs/src/LoFS.h"
#include "configuration.h"
#include "gps/RTC.h"
#include <Arduino.h>
#include <SHA256.h>
#include <algorithm>
#include <cstring>
#include <pb_decode.h>
#include <pb_encode.h>

/**
 * LoDB Implementation - Synchronous Design
 *
 * Threading Model:
 * - All filesystem operations are thread-safe (LoFS handles locking internally)
 * - All operations complete immediately and return results synchronously
 * - SELECT returns complete result sets with optional filtering, sorting, and limiting
 */

// Convert UUID to 16-character hex string
void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17])
{
    snprintf(hex_out, 17, LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
}

// Generate or derive a UUID
lodb_uuid_t lodb_new_uuid(const char *str, uint64_t salt)
{
    char generated_str[32];
    const char *input_str = str;

    // If no string provided, generate one from timestamp and random
    if (str == nullptr) {
        uint32_t timestamp = getTime();
        uint32_t random_val = random(0xFFFFFFFF);
        snprintf(generated_str, sizeof(generated_str), "%u:%u", timestamp, random_val);
        input_str = generated_str;
    }

    // Always hash string with salt
    SHA256 sha256;
    uint8_t hash[32];

    sha256.reset();
    sha256.update(input_str, strlen(input_str));

    // Add salt (always included now)
    uint8_t salt_bytes[8];
    memcpy(salt_bytes, &salt, 8);
    sha256.update(salt_bytes, 8);

    sha256.finalize(hash, 32);

    // Use first 8 bytes as uint64_t
    lodb_uuid_t uuid;
    memcpy(&uuid, hash, sizeof(lodb_uuid_t));
    return uuid;
}

// LoDb Class Implementation

LoDb::LoDb(const char *db_name, LoFS::FSType filesystem) : db_name(db_name)
{
    // Determine filesystem prefix
    if (filesystem == LoFS::FSType::SD) {
        // Explicitly request SD card
        if (LoFS::isSDCardAvailable()) {
            strncpy(fs_prefix, "/sd", sizeof(fs_prefix) - 1);
            fs_prefix[sizeof(fs_prefix) - 1] = '\0';
        } else {
            LOG_WARN("SD card requested but not available, falling back to internal filesystem");
            strncpy(fs_prefix, "/internal", sizeof(fs_prefix) - 1);
            fs_prefix[sizeof(fs_prefix) - 1] = '\0';
        }
    } else if (filesystem == LoFS::FSType::INTERNAL) {
        // Explicitly request internal filesystem
        strncpy(fs_prefix, "/internal", sizeof(fs_prefix) - 1);
        fs_prefix[sizeof(fs_prefix) - 1] = '\0';
    } else {
        // Auto-select: use SD if available, otherwise internal filesystem
        if (LoFS::isSDCardAvailable()) {
            strncpy(fs_prefix, "/sd", sizeof(fs_prefix) - 1);
            fs_prefix[sizeof(fs_prefix) - 1] = '\0';
        } else {
            strncpy(fs_prefix, "/internal", sizeof(fs_prefix) - 1);
            fs_prefix[sizeof(fs_prefix) - 1] = '\0';
        }
    }

    // Build database path with prefix
    snprintf(db_path, sizeof(db_path), "%s/lodb/%s", fs_prefix, db_name);

    // Create directories
    char lodb_base[32];
    snprintf(lodb_base, sizeof(lodb_base), "%s/lodb", fs_prefix);
    LoFS::mkdir(lodb_base);
    if (!LoFS::mkdir(db_path)) {
        LOG_DEBUG("Database directory may already exist or created: %s", db_path);
    }

    LOG_INFO("Initialized LoDB database: %s", db_path);
}

LoDb::~LoDb()
{
    // Nothing to clean up for now
}

LoDbError LoDb::registerTable(const char *table_name, const pb_msgdesc_t *pb_descriptor, size_t record_size)
{
    if (!table_name || !pb_descriptor || record_size == 0) {
        return LODB_ERR_INVALID;
    }

    TableMetadata metadata;
    metadata.table_name = table_name;
    metadata.pb_descriptor = pb_descriptor;
    metadata.record_size = record_size;

    // Build table path: {prefix}/lodb/{db_name}/{table_name}/
    snprintf(metadata.table_path, sizeof(metadata.table_path), "%s/%s", db_path, table_name);

    // Create table directory
    if (!LoFS::mkdir(metadata.table_path)) {
        LOG_DEBUG("Table directory may already exist or created: %s", metadata.table_path);
    }

    tables[table_name] = metadata;
    LOG_INFO("Registered table: %s at %s", table_name, metadata.table_path);
    return LODB_OK;
}

LoDb::TableMetadata *LoDb::getTable(const char *table_name)
{
    auto it = tables.find(table_name);
    if (it == tables.end()) {
        LOG_ERROR("Table not registered: %s", table_name);
        return nullptr;
    }
    return &it->second;
}

// Insert a record with a UUID
LoDbError LoDb::insert(const char *table_name, lodb_uuid_t uuid, const void *record)
{
    if (!table_name || !record) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    // Convert UUID to hex for filename
    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    // Build file path
    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.pr", table->table_path, uuid_hex);

    // Check if file already exists
    auto existing = LoFS::open(file_path, FILE_O_READ);
    if (existing) {
        existing.close();
        LOG_ERROR("UUID already exists: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_INVALID;
    }

    // Encode to buffer
    uint8_t buffer[2048];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, table->pb_descriptor, record)) {
        LOG_ERROR("Failed to encode protobuf for insert");
        return LODB_ERR_ENCODE;
    }

    size_t encoded_size = stream.bytes_written;
    LOG_DEBUG("Encoded record: %d bytes", encoded_size);

    // Write to file
    auto file = LoFS::open(file_path, FILE_O_WRITE);
    if (!file) {
        LOG_ERROR("Failed to open file for writing: %s", file_path);
        return LODB_ERR_IO;
    }

    size_t written = file.write(buffer, encoded_size);
    if (written != encoded_size) {
        LOG_ERROR("Failed to write file, wrote %d of %d bytes", written, encoded_size);
        file.close();
        return LODB_ERR_IO;
    }

    file.flush();
    file.close();
    LOG_DEBUG("Wrote record to: %s (%d bytes)", file_path, encoded_size);

    LOG_INFO("Inserted record with custom UUID: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

// Get a record by UUID
LoDbError LoDb::get(const char *table_name, lodb_uuid_t uuid, void *record_out)
{
    if (!table_name || !record_out) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    // Convert UUID to hex for filename
    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    // Build file path
    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.pr", table->table_path, uuid_hex);
    LOG_DEBUG("file_path: %s", file_path);

    // Read file into buffer
    uint8_t buffer[2048];
    size_t file_size = 0;

    auto file = LoFS::open(file_path, FILE_O_READ);
    if (!file) {
        LOG_DEBUG("Record not found: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }

    file_size = file.read(buffer, sizeof(buffer));
    file.close();

    if (file_size == 0) {
        LOG_ERROR("Record file is empty: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_IO;
    }

    LOG_DEBUG("Read record file: %s (%d bytes)", file_path, file_size);

    // Decode from buffer
    pb_istream_t stream = pb_istream_from_buffer(buffer, file_size);
    memset(record_out, 0, table->record_size);

    if (!pb_decode(&stream, table->pb_descriptor, record_out)) {
        LOG_ERROR("Failed to decode protobuf from " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_DECODE;
    }

    LOG_DEBUG("Retrieved record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

// Update a single record by UUID
LoDbError LoDb::update(const char *table_name, lodb_uuid_t uuid, const void *record)
{
    if (!table_name || !record) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    // Convert UUID to hex for filename
    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    // Build file path
    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.pr", table->table_path, uuid_hex);

    // Check if record exists first
    auto file = LoFS::open(file_path, FILE_O_READ);
    if (!file) {
        LOG_DEBUG("Record not found for update: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }
    file.close();

    // Encode to buffer
    uint8_t buffer[2048];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, table->pb_descriptor, record)) {
        LOG_ERROR("Failed to encode updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_ENCODE;
    }

    size_t encoded_size = stream.bytes_written;

    // Write to file
    LoFS::remove(file_path); // Remove old file
    file = LoFS::open(file_path, FILE_O_WRITE);
    if (!file) {
        LOG_ERROR("Failed to open file for update: %s", file_path);
        return LODB_ERR_IO;
    }

    size_t written = file.write(buffer, encoded_size);
    if (written != encoded_size) {
        LOG_ERROR("Failed to write updated file");
        file.close();
        return LODB_ERR_IO;
    }

    file.flush();
    file.close();

    LOG_INFO("Updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

// Delete a single record by UUID
LoDbError LoDb::deleteRecord(const char *table_name, lodb_uuid_t uuid)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    // Convert UUID to hex for filename
    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.pr", table->table_path, uuid_hex);

    if (LoFS::remove(file_path)) {
        LOG_DEBUG("Deleted record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_OK;
    } else {
        LOG_WARN("Failed to delete record (may not exist): " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }
}

// Select records with optional filtering, sorting, and limiting
std::vector<void *> LoDb::select(const char *table_name, LoDbFilter filter, LoDbComparator comparator, size_t limit)
{
    std::vector<void *> results;

    if (!table_name) {
        LOG_ERROR("Invalid table_name");
        return results;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LOG_ERROR("Table not found: %s", table_name);
        return results;
    }

    // PHASE 1: FILTER - iterate directory and collect matching records
    File dir = LoFS::open(table->table_path, FILE_O_READ);
    if (!dir) {
        LOG_DEBUG("Table directory not found: %s", table->table_path);
        return results; // Empty result set
    }

    if (!dir.isDirectory()) {
        LOG_ERROR("Table path is not a directory: %s", table->table_path);
        dir.close();
        return results;
    }

    // Iterate through all files in directory
    while (true) {
        File file = dir.openNextFile();
        if (!file) {
            break; // No more files
        }

        // Skip directories
        if (file.isDirectory()) {
            file.close();
            continue;
        }

        // Get filename
        std::string pathStr = file.name();
        file.close();

        // Extract just the filename (after last /)
        size_t lastSlash = pathStr.rfind('/');
        std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

        // Extract UUID (remove .pr extension)
        size_t prPos = filename.find(".pr");
        if (prPos == std::string::npos) {
            LOG_DEBUG("Skipped non-.pr file: %s", filename.c_str());
            continue;
        }

        std::string uuid_hex_str = filename.substr(0, prPos);

        // Parse hex string to uint64_t UUID
        lodb_uuid_t uuid;
        uint32_t high, low;
        if (sscanf(uuid_hex_str.c_str(), "%08x%08x", &high, &low) != 2) {
            LOG_WARN("Failed to parse UUID from filename: %s", uuid_hex_str.c_str());
            continue;
        }
        uuid = ((uint64_t)high << 32) | (uint64_t)low;

        // Allocate buffer for record
        uint8_t *record_buffer = new uint8_t[table->record_size];
        if (!record_buffer) {
            LOG_ERROR("Failed to allocate record buffer");
            continue;
        }

        // Read and decode the record
        memset(record_buffer, 0, table->record_size);
        LoDbError err = get(table_name, uuid, record_buffer);

        if (err != LODB_OK) {
            LOG_WARN("Failed to read record " LODB_UUID_FMT " during select", LODB_UUID_ARGS(uuid));
            delete[] record_buffer;
            continue;
        }

        // Apply filter if provided
        if (filter && !filter(record_buffer)) {
            LOG_DEBUG("Record " LODB_UUID_FMT " filtered out", LODB_UUID_ARGS(uuid));
            delete[] record_buffer;
            continue;
        }

        // Record passed filter, add to results
        results.push_back(record_buffer);
        LOG_DEBUG("Added record " LODB_UUID_FMT " to results", LODB_UUID_ARGS(uuid));
    }

    dir.close();

    LOG_INFO("Select from %s: %d records after filtering", table_name, results.size());

    // PHASE 2: SORT - sort results if comparator provided
    if (comparator && !results.empty()) {
        std::sort(results.begin(), results.end(), [comparator](const void *a, const void *b) { return comparator(a, b) < 0; });
        LOG_DEBUG("Sorted %d records", results.size());
    }

    // PHASE 3: LIMIT - apply limit if specified
    if (limit > 0 && results.size() > limit) {
        // Free records beyond limit
        for (size_t i = limit; i < results.size(); i++) {
            delete[] (uint8_t *)results[i];
        }
        results.resize(limit);
        LOG_DEBUG("Limited results to %d records", limit);
    }

    LOG_INFO("Select from %s complete: %d records returned", table_name, results.size());

    return results;
}

// Free a vector of records returned by select()
void LoDb::freeRecords(std::vector<void *> &records)
{
    for (auto *recordPtr : records) {
        delete[] (uint8_t *)recordPtr;
    }
    records.clear();
}

// Count records in a table with optional filtering
int LoDb::count(const char *table_name, LoDbFilter filter)
{
    if (!table_name) {
        LOG_ERROR("Invalid table_name");
        return -1;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LOG_ERROR("Table not found: %s", table_name);
        return -1;
    }

    int count = 0;

    // If no filter, efficiently count files without loading records
    if (!filter) {
        File dir = LoFS::open(table->table_path, FILE_O_READ);
        if (!dir) {
            LOG_DEBUG("Table directory not found: %s", table->table_path);
            return 0; // Empty table
        }

        if (!dir.isDirectory()) {
            LOG_ERROR("Table path is not a directory: %s", table->table_path);
            dir.close();
            return -1;
        }

        // Count .pr files
        while (true) {
            File file = dir.openNextFile();
            if (!file) {
                break; // No more files
            }

            // Skip directories
            if (file.isDirectory()) {
                file.close();
                continue;
            }

            // Get filename
            std::string pathStr = file.name();
            file.close();

            // Extract just the filename (after last /)
            size_t lastSlash = pathStr.rfind('/');
            std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

            // Check if it's a .pr file
            if (filename.find(".pr") != std::string::npos) {
                count++;
            }
        }

        dir.close();
        LOG_DEBUG("Counted %d records in %s (no filter)", count, table_name);
        return count;
    }

    // Filter provided - need to load records to check them
    // Use select() but don't keep the records, just count
    auto results = select(table_name, filter, LoDbComparator(), 0);
    count = results.size();
    freeRecords(results);

    LOG_DEBUG("Counted %d records in %s (with filter)", count, table_name);
    return count;
}

// Truncate a table - delete all records but keep the table registered
LoDbError LoDb::truncate(const char *table_name)
{
    if (!table_name) {
        LOG_ERROR("Invalid table_name");
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LOG_ERROR("Table not registered: %s", table_name);
        return LODB_ERR_INVALID;
    }

    // Open table directory
    File dir = LoFS::open(table->table_path, FILE_O_READ);
    if (!dir) {
        LOG_DEBUG("Table directory not found: %s (already empty)", table->table_path);
        return LODB_OK; // Table is already empty
    }

    if (!dir.isDirectory()) {
        LOG_ERROR("Table path is not a directory: %s", table->table_path);
        dir.close();
        return LODB_ERR_INVALID;
    }

    // Iterate through all files and delete them
    int deletedCount = 0;
    while (true) {
        File file = dir.openNextFile();
        if (!file) {
            break; // No more files
        }

        // Skip directories
        if (file.isDirectory()) {
            file.close();
            continue;
        }

        // Get filename
        std::string pathStr = file.name();
        file.close();

        // Extract just the filename (after last /)
        size_t lastSlash = pathStr.rfind('/');
        std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

        // Build full file path
        char file_path[192];
        snprintf(file_path, sizeof(file_path), "%s/%s", table->table_path, filename.c_str());

        // Delete the file
        if (LoFS::remove(file_path)) {
            deletedCount++;
        } else {
            LOG_WARN("Failed to delete file during truncate: %s", file_path);
        }
    }

    dir.close();

    LOG_INFO("Truncated table %s: deleted %d records", table_name, deletedCount);
    return LODB_OK;
}

// Drop a table - delete all records and unregister the table
LoDbError LoDb::drop(const char *table_name)
{
    if (!table_name) {
        LOG_ERROR("Invalid table_name");
        return LODB_ERR_INVALID;
    }

    // Check if table exists
    TableMetadata *table = getTable(table_name);
    if (!table) {
        LOG_ERROR("Table not registered: %s", table_name);
        return LODB_ERR_INVALID;
    }

    // Truncate first (delete all records)
    LoDbError err = truncate(table_name);
    if (err != LODB_OK) {
        LOG_WARN("Failed to truncate table before drop: %s", table_name);
        // Continue anyway to try to remove directory and unregister
    }

    // Remove table directory (recursively to ensure cleanup even if truncate didn't remove everything)
    if (LoFS::rmdir(table->table_path, true)) {
        LOG_DEBUG("Removed table directory: %s", table->table_path);
    } else {
        LOG_WARN("Failed to remove table directory: %s", table->table_path);
    }

    // Remove from tables map
    tables.erase(table_name);

    LOG_INFO("Dropped table: %s", table_name);
    return LODB_OK;
}
