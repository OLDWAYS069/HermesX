#include "LoDB.h"
#include "lofs/src/LoFS.h"
#include "DebugConfiguration.h"
#include "gps/RTC.h"
#include "diagnostics.pb.h"
#include <algorithm>
#include <cstring>
#include <string>

void lodb_diagnostics()
{
    LOG_INFO("=== LoDB Comprehensive Test Suite ===");
    LOG_INFO("");

    // Cleanup: Remove any leftover test data from previous runs
    const char *cleanupDirs[] = {
        "/sd/lodb/test_db_1",
        "/sd/lodb/test_db_2",
        "/sd/lodb/test_db_3",
        "/sd/lodb/test_db_4",
        "/internal/lodb/test_db_1",
        "/internal/lodb/test_db_2",
        "/internal/lodb/test_db_3",
        "/internal/lodb/test_db_4",
    };
    size_t numCleanupDirs = sizeof(cleanupDirs) / sizeof(cleanupDirs[0]);
    for (size_t i = 0; i < numCleanupDirs; i++) {
        LoFS::rmdir(cleanupDirs[i], true);
    }

    // Test 1: Filesystem Availability and Database Initialization
    LOG_INFO("--- Test 1: Filesystem Availability and Database Initialization ---");
    bool sdAvailable = LoFS::isSDCardAvailable();
    LOG_INFO("SD Card available: %s", sdAvailable ? "YES" : "NO");
    LOG_INFO("Expected database location: %s", sdAvailable ? "/sd/lodb/" : "/internal/lodb/");
    LOG_INFO("");

    // Test 2: Create Multiple Databases with Filesystem Selection
    LOG_INFO("--- Test 2: Create Multiple Databases with Filesystem Selection ---");
    
    // Test auto-selection (default behavior)
    LoDb *db1 = new LoDb("test_db_1");
    LOG_INFO("Created db1 with auto-selection: %s", sdAvailable ? "/sd/lodb/test_db_1" : "/internal/lodb/test_db_1");
    
    // Test explicit internal filesystem
    LoDb *db2 = new LoDb("test_db_2", LoFS::FSType::INTERNAL);
    LOG_INFO("Created db2 with explicit LoFS::FSType::INTERNAL: /internal/lodb/test_db_2");
    
    // Test explicit SD (if available)
    LoDb *db3 = nullptr;
    if (sdAvailable) {
        db3 = new LoDb("test_db_3", LoFS::FSType::SD);
        LOG_INFO("Created db3 with explicit LoFS::FSType::SD: /sd/lodb/test_db_3");
    } else {
        LOG_INFO("Skipping db3 with LoFS::FSType::SD (SD card not available)");
    }
    
    // Test SD request when SD not available (should fall back to INTERNAL)
    LoDb *db4 = nullptr;
    if (!sdAvailable) {
        db4 = new LoDb("test_db_4", LoFS::FSType::SD);
        LOG_INFO("Created db4 with LoFS::FSType::SD (fallback to internal filesystem): /internal/lodb/test_db_4");
    }
    
    LOG_INFO("");

    // Test 3: Register Multiple Tables
    LOG_INFO("--- Test 3: Register Multiple Tables ---");
    LoDbError err;

    // Register tables in db1
    err = db1->registerTable("users", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("db1->registerTable(\"users\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    err = db1->registerTable("messages", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("db1->registerTable(\"messages\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    err = db1->registerTable("logs", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("db1->registerTable(\"logs\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Register tables in db2
    err = db2->registerTable("items", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("db2->registerTable(\"items\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    err = db2->registerTable("orders", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("db2->registerTable(\"orders\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Register tables in db3 if available
    if (db3) {
        err = db3->registerTable("events", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
        LOG_INFO("db3->registerTable(\"events\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    }
    LOG_INFO("");

    // Test 4: Insert Operations
    LOG_INFO("--- Test 4: Insert Operations ---");
    meshtastic_LoDBDiagnosticsTest record = meshtastic_LoDBDiagnosticsTest_init_zero;
    lodb_uuid_t uuid;

    // Insert with auto-generated UUID
    record.id = 1;
    strncpy(record.value, "test_value_1", sizeof(record.value) - 1);
    record.timestamp = getTime();
    record.active = true;
    uuid = lodb_new_uuid(nullptr, 0);
    err = db1->insert("users", uuid, &record);
    LOG_INFO("db1->insert(\"users\", auto-uuid): %s (UUID: " LODB_UUID_FMT ")", err == LODB_OK ? "SUCCESS" : "FAILED", LODB_UUID_ARGS(uuid));
    lodb_uuid_t uuid1 = uuid;

    // Insert with custom UUID from string
    record.id = 2;
    strncpy(record.value, "test_value_2", sizeof(record.value) - 1);
    record.timestamp = getTime() + 1;
    record.active = false;
    uuid = lodb_new_uuid("custom_string_1", 12345);
    err = db1->insert("users", uuid, &record);
    LOG_INFO("db1->insert(\"users\", custom-uuid): %s (UUID: " LODB_UUID_FMT ")", err == LODB_OK ? "SUCCESS" : "FAILED", LODB_UUID_ARGS(uuid));
    lodb_uuid_t uuid2 = uuid;

    // Insert duplicate UUID (should fail)
    record.id = 3;
    strncpy(record.value, "duplicate", sizeof(record.value) - 1);
    err = db1->insert("users", uuid1, &record);
    LOG_INFO("db1->insert(\"users\", duplicate-uuid): %s (should be FAILED)", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Insert into different table
    record.id = 10;
    strncpy(record.value, "message_1", sizeof(record.value) - 1);
    uuid = lodb_new_uuid(nullptr, 0);
    err = db1->insert("messages", uuid, &record);
    LOG_INFO("db1->insert(\"messages\", auto-uuid): %s (UUID: " LODB_UUID_FMT ")", err == LODB_OK ? "SUCCESS" : "FAILED", LODB_UUID_ARGS(uuid));
    lodb_uuid_t uuid3 = uuid;

    // Insert into different database
    record.id = 100;
    strncpy(record.value, "item_1", sizeof(record.value) - 1);
    uuid = lodb_new_uuid(nullptr, 0);
    err = db2->insert("items", uuid, &record);
    LOG_INFO("db2->insert(\"items\", auto-uuid): %s (UUID: " LODB_UUID_FMT ")", err == LODB_OK ? "SUCCESS" : "FAILED", LODB_UUID_ARGS(uuid));
    lodb_uuid_t uuid4 = uuid;

    // Insert multiple records for select/count tests
    for (int i = 0; i < 5; i++) {
        record.id = 20 + i;
        char value[32];
        snprintf(value, sizeof(value), "bulk_test_%d", i);
        strncpy(record.value, value, sizeof(record.value) - 1);
        record.timestamp = getTime() + i;
        record.active = (i % 2 == 0);
        uuid = lodb_new_uuid(nullptr, i);
        err = db1->insert("users", uuid, &record);
        if (err != LODB_OK) {
            LOG_INFO("db1->insert(\"users\", bulk-%d): FAILED", i);
        }
    }
    LOG_INFO("Inserted 5 additional records for bulk operations");
    LOG_INFO("");

    // Test 5: Get Operations
    LOG_INFO("--- Test 5: Get Operations ---");
    meshtastic_LoDBDiagnosticsTest retrieved = meshtastic_LoDBDiagnosticsTest_init_zero;

    // Get existing record
    err = db1->get("users", uuid1, &retrieved);
    LOG_INFO("db1->get(\"users\", uuid1): %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    if (err == LODB_OK) {
        LOG_INFO("  Retrieved: id=%u, value=\"%s\", timestamp=%u, active=%s", retrieved.id, retrieved.value, retrieved.timestamp, retrieved.active ? "true" : "false");
    }

    // Get non-existent record
    lodb_uuid_t fakeUuid = lodb_new_uuid("nonexistent", 99999);
    err = db1->get("users", fakeUuid, &retrieved);
    LOG_INFO("db1->get(\"users\", nonexistent-uuid): %s (should be NOT_FOUND)", err == LODB_ERR_NOT_FOUND ? "NOT_FOUND (expected)" : "FOUND (unexpected)");

    // Get from different table
    err = db1->get("messages", uuid3, &retrieved);
    LOG_INFO("db1->get(\"messages\", uuid3): %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    if (err == LODB_OK) {
        LOG_INFO("  Retrieved: id=%u, value=\"%s\"", retrieved.id, retrieved.value);
    }

    // Get from different database
    err = db2->get("items", uuid4, &retrieved);
    LOG_INFO("db2->get(\"items\", uuid4): %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    if (err == LODB_OK) {
        LOG_INFO("  Retrieved: id=%u, value=\"%s\"", retrieved.id, retrieved.value);
    }
    LOG_INFO("");

    // Test 6: Update Operations
    LOG_INFO("--- Test 6: Update Operations ---");

    // Update existing record
    record.id = 999;
    strncpy(record.value, "updated_value", sizeof(record.value) - 1);
    record.timestamp = getTime() + 1000;
    record.active = false;
    err = db1->update("users", uuid1, &record);
    LOG_INFO("db1->update(\"users\", uuid1): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Verify update
    err = db1->get("users", uuid1, &retrieved);
    if (err == LODB_OK) {
        bool match = (retrieved.id == 999 && strcmp(retrieved.value, "updated_value") == 0);
        LOG_INFO("  Update verification: %s (id=%u, value=\"%s\")", match ? "MATCH" : "MISMATCH", retrieved.id, retrieved.value);
    }

    // Update non-existent record (should fail)
    err = db1->update("users", fakeUuid, &record);
    LOG_INFO("db1->update(\"users\", nonexistent-uuid): %s (should be NOT_FOUND)", err == LODB_ERR_NOT_FOUND ? "NOT_FOUND (expected)" : "SUCCESS (unexpected)");

    // Update in different table
    record.id = 888;
    strncpy(record.value, "updated_message", sizeof(record.value) - 1);
    err = db1->update("messages", uuid3, &record);
    LOG_INFO("db1->update(\"messages\", uuid3): %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    LOG_INFO("");

    // Test 7: Select Operations
    LOG_INFO("--- Test 7: Select Operations ---");

    // Select all records
    auto results1 = db1->select("users", LoDbFilter(), LoDbComparator(), 0);
    LOG_INFO("db1->select(\"users\", no-filter): %d records", results1.size());

    // Select with filter (active records only)
    auto filterActive = [](const void *rec) -> bool {
        const meshtastic_LoDBDiagnosticsTest *r = (const meshtastic_LoDBDiagnosticsTest *)rec;
        return r->active;
    };
    auto results2 = db1->select("users", filterActive, LoDbComparator(), 0);
    LOG_INFO("db1->select(\"users\", filter-active): %d records", results2.size());
    LoDb::freeRecords(results2);

    // Select with filter (id > 20)
    auto filterId = [](const void *rec) -> bool {
        const meshtastic_LoDBDiagnosticsTest *r = (const meshtastic_LoDBDiagnosticsTest *)rec;
        return r->id > 20;
    };
    auto results3 = db1->select("users", filterId, LoDbComparator(), 0);
    LOG_INFO("db1->select(\"users\", filter-id>20): %d records", results3.size());
    LoDb::freeRecords(results3);

    // Select with sorting (by id descending)
    auto comparatorId = [](const void *a, const void *b) -> int {
        const meshtastic_LoDBDiagnosticsTest *ra = (const meshtastic_LoDBDiagnosticsTest *)a;
        const meshtastic_LoDBDiagnosticsTest *rb = (const meshtastic_LoDBDiagnosticsTest *)b;
        if (ra->id > rb->id) return -1;
        if (ra->id < rb->id) return 1;
        return 0;
    };
    auto results4 = db1->select("users", LoDbFilter(), comparatorId, 0);
    LOG_INFO("db1->select(\"users\", sorted-by-id-desc): %d records", results4.size());
    if (results4.size() > 0) {
        const meshtastic_LoDBDiagnosticsTest *first = (const meshtastic_LoDBDiagnosticsTest *)results4[0];
        LOG_INFO("  First record: id=%u", first->id);
    }
    LoDb::freeRecords(results4);

    // Select with limit
    auto results5 = db1->select("users", LoDbFilter(), LoDbComparator(), 3);
    LOG_INFO("db1->select(\"users\", limit=3): %d records", results5.size());
    LoDb::freeRecords(results5);

    // Select with filter, sort, and limit
    auto results6 = db1->select("users", filterActive, comparatorId, 2);
    LOG_INFO("db1->select(\"users\", filter-active+sorted+limit=2): %d records", results6.size());
    LoDb::freeRecords(results6);

    // Clean up results1
    LoDb::freeRecords(results1);
    LOG_INFO("");

    // Test 8: Count Operations
    LOG_INFO("--- Test 8: Count Operations ---");

    // Count all records
    int count1 = db1->count("users");
    LOG_INFO("db1->count(\"users\"): %d records", count1);

    // Count with filter
    int count2 = db1->count("users", filterActive);
    LOG_INFO("db1->count(\"users\", filter-active): %d records", count2);

    // Count in different table
    int count3 = db1->count("messages");
    LOG_INFO("db1->count(\"messages\"): %d records", count3);

    // Count in different database
    int count4 = db2->count("items");
    LOG_INFO("db2->count(\"items\"): %d records", count4);

    // Count non-existent table (should return -1)
    int count5 = db1->count("nonexistent");
    LOG_INFO("db1->count(\"nonexistent\"): %d (should be -1)", count5);
    LOG_INFO("");

    // Test 9: Delete Operations
    LOG_INFO("--- Test 9: Delete Operations ---");

    // Delete existing record
    err = db1->deleteRecord("users", uuid2);
    LOG_INFO("db1->deleteRecord(\"users\", uuid2): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Verify deletion
    err = db1->get("users", uuid2, &retrieved);
    LOG_INFO("  Verification: get after delete: %s (should be NOT_FOUND)", err == LODB_ERR_NOT_FOUND ? "NOT_FOUND (expected)" : "FOUND (unexpected)");

    // Delete non-existent record (should fail gracefully)
    err = db1->deleteRecord("users", fakeUuid);
    LOG_INFO("db1->deleteRecord(\"users\", nonexistent-uuid): %s (should be NOT_FOUND)", err == LODB_ERR_NOT_FOUND ? "NOT_FOUND (expected)" : "SUCCESS (unexpected)");

    // Delete from different table
    err = db1->deleteRecord("messages", uuid3);
    LOG_INFO("db1->deleteRecord(\"messages\", uuid3): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Delete from different database
    err = db2->deleteRecord("items", uuid4);
    LOG_INFO("db2->deleteRecord(\"items\", uuid4): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Verify count after deletions
    int countAfterDelete = db1->count("users");
    LOG_INFO("  Count after deletions: %d records (was %d)", countAfterDelete, count1);
    LOG_INFO("");

    // Test 10: Truncate Operation
    LOG_INFO("--- Test 10: Truncate Operation ---");

    // Insert some test records first
    for (int i = 0; i < 3; i++) {
        record.id = 100 + i;
        char value[32];
        snprintf(value, sizeof(value), "truncate_test_%d", i);
        strncpy(record.value, value, sizeof(record.value) - 1);
        record.timestamp = getTime() + i;
        record.active = true;
        uuid = lodb_new_uuid(nullptr, 1000 + i);
        db1->insert("logs", uuid, &record);
    }

    int countBeforeTruncate = db1->count("logs");
    LOG_INFO("Records before truncate: %d", countBeforeTruncate);

    // Truncate the table
    err = db1->truncate("logs");
    LOG_INFO("db1->truncate(\"logs\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Verify truncate
    int countAfterTruncate = db1->count("logs");
    LOG_INFO("Records after truncate: %d (should be 0)", countAfterTruncate);

    // Verify table is still registered
    err = db1->registerTable("logs", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("Table still registered after truncate: %s (should be SUCCESS, already registered)", err == LODB_OK ? "YES" : "NO");
    LOG_INFO("");

    // Test 11: Drop Operation
    LOG_INFO("--- Test 11: Drop Operation ---");

    // Insert some test records into a table we'll drop
    for (int i = 0; i < 2; i++) {
        record.id = 200 + i;
        char value[32];
        snprintf(value, sizeof(value), "drop_test_%d", i);
        strncpy(record.value, value, sizeof(record.value) - 1);
        record.timestamp = getTime() + i;
        record.active = true;
        uuid = lodb_new_uuid(nullptr, 2000 + i);
        db1->insert("logs", uuid, &record);
    }

    int countBeforeDrop = db1->count("logs");
    LOG_INFO("Records before drop: %d", countBeforeDrop);

    // Drop the table
    err = db1->drop("logs");
    LOG_INFO("db1->drop(\"logs\"): %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Verify table is unregistered
    err = db1->insert("logs", uuid, &record);
    LOG_INFO("Table unregistered after drop: %s (should be INVALID)", err == LODB_ERR_INVALID ? "YES (expected)" : "NO (unexpected)");

    // Re-register the table
    err = db1->registerTable("logs", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    LOG_INFO("Re-registered table: %s", err == LODB_OK ? "SUCCESS" : "FAILED");

    // Verify table is empty after re-registration
    int countAfterReRegister = db1->count("logs");
    LOG_INFO("Records after re-register: %d (should be 0)", countAfterReRegister);
    LOG_INFO("");

    // Test 12: Error Cases
    LOG_INFO("--- Test 12: Error Cases ---");

    // Insert into non-existent table
    err = db1->insert("nonexistent", uuid1, &record);
    LOG_INFO("db1->insert(\"nonexistent\", ...): %s (should be INVALID)", err == LODB_ERR_INVALID ? "INVALID (expected)" : "SUCCESS (unexpected)");

    // Get from non-existent table
    err = db1->get("nonexistent", uuid1, &retrieved);
    LOG_INFO("db1->get(\"nonexistent\", ...): %s (should be INVALID)", err == LODB_ERR_INVALID ? "INVALID (expected)" : "SUCCESS (unexpected)");

    // Update in non-existent table
    err = db1->update("nonexistent", uuid1, &record);
    LOG_INFO("db1->update(\"nonexistent\", ...): %s (should be INVALID)", err == LODB_ERR_INVALID ? "INVALID (expected)" : "SUCCESS (unexpected)");

    // Delete from non-existent table
    err = db1->deleteRecord("nonexistent", uuid1);
    LOG_INFO("db1->deleteRecord(\"nonexistent\", ...): %s (should be INVALID)", err == LODB_ERR_INVALID ? "INVALID (expected)" : "SUCCESS (unexpected)");

    // Select from non-existent table (should return empty vector)
    auto errorResults = db1->select("nonexistent", LoDbFilter(), LoDbComparator(), 0);
    LOG_INFO("db1->select(\"nonexistent\", ...): %d records (should be 0)", errorResults.size());

    // Truncate non-existent table
    err = db1->truncate("nonexistent");
    LOG_INFO("db1->truncate(\"nonexistent\", ...): %s (should be INVALID)", err == LODB_ERR_INVALID ? "INVALID (expected)" : "SUCCESS (unexpected)");

    // Drop non-existent table
    err = db1->drop("nonexistent");
    LOG_INFO("db1->drop(\"nonexistent\", ...): %s (should be INVALID)", err == LODB_ERR_INVALID ? "INVALID (expected)" : "SUCCESS (unexpected)");
    LOG_INFO("");

    // Test 12: Cross-Database Operations
    LOG_INFO("--- Test 12: Cross-Database Operations ---");

    // Verify databases are isolated
    int db1UsersCount = db1->count("users");
    int db2UsersCount = db2->count("users");
    LOG_INFO("db1->count(\"users\"): %d records", db1UsersCount);
    LOG_INFO("db2->count(\"users\"): %d records (should be -1, table not registered)", db2UsersCount);

    // Register users table in db2 and verify isolation
    db2->registerTable("users", &meshtastic_LoDBDiagnosticsTest_msg, sizeof(meshtastic_LoDBDiagnosticsTest));
    record.id = 200;
    strncpy(record.value, "db2_user", sizeof(record.value) - 1);
    uuid = lodb_new_uuid(nullptr, 2000);
    db2->insert("users", uuid, &record);
    int db2UsersCountAfter = db2->count("users");
    LOG_INFO("db2->count(\"users\") after insert: %d records (should be 1)", db2UsersCountAfter);
    LOG_INFO("db1->count(\"users\") unchanged: %d records", db1->count("users"));
    LOG_INFO("");

    // Test 14: Cleanup
    LOG_INFO("--- Test 14: Cleanup ---");

    // Truncate test tables to clean up
    db1->truncate("users");
    db1->truncate("messages");
    db2->truncate("items");
    db2->truncate("orders");
    if (db2->count("users") > 0) {
        db2->truncate("users");
    }
    LOG_INFO("Truncated all test tables");

    // Delete databases (clean up filesystem directories)
    delete db1;
    delete db2;
    if (db3) {
        delete db3;
    }
    if (db4) {
        delete db4;
    }
    LOG_INFO("Deleted database objects");

    // Clean up filesystem directories (reuse cleanupDirs from start of function)
    for (size_t i = 0; i < numCleanupDirs; i++) {
        if (LoFS::exists(cleanupDirs[i])) {
            if (LoFS::rmdir(cleanupDirs[i], true)) {
                LOG_INFO("Removed directory: %s", cleanupDirs[i]);
            }
        }
    }
    LOG_INFO("");

    // Final Summary
    LOG_INFO("=== Test Suite Complete ===");
    LOG_INFO("All tests finished. Check logs above for detailed results.");
}
