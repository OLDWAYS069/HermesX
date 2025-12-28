#include "LoFS.h"
#include "SPILock.h"
#include "configuration.h"
#include <string.h>
#include <stdlib.h>
#include <string>

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#include <SD.h>
#include <SPI.h>

// Use the same SPI handler setup as FSCommon.cpp
#ifdef SDCARD_USE_SPI1
extern SPIClass SPI_HSPI;
#define SDHandler SPI_HSPI
#else
#define SDHandler SPI
#endif

#ifndef SD_SPI_FREQUENCY
#define SD_SPI_FREQUENCY 4000000U
#endif
#endif

bool LoFS::isSDCardAvailable()
{
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    // Check current card type
    uint8_t cardType = SD.cardType();
    
    // If card type is NONE, try to initialize the SD card
    if (cardType == CARD_NONE) {
        concurrency::LockGuard g(spiLock);
        SDHandler.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
        if (SD.begin(SDCARD_CS, SDHandler, SD_SPI_FREQUENCY)) {
            cardType = SD.cardType();
        }
    }
    
    return (cardType != CARD_NONE);
#else
    return false; // SD card support not compiled in or disabled
#endif
}

// Helper to convert mode to SD library mode
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
// ESP32/RP2040: SD library uses string modes
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
static const char *convertToSDMode(const char *modeStr)
{
    // Already a string, return as-is
    return modeStr;
}

static const char *convertToSDMode(uint8_t mode)
{
    // Convert uint8_t to string: 0 = read, non-zero = write
    return (mode == 0) ? "r" : "w";
}
#else
// STM32WL/NRF52: SD library uses uint8_t modes
static uint8_t convertToSDMode(const char *modeStr)
{
    if (strcmp(modeStr, "r") == 0) {
        return FILE_READ;
    }
    return FILE_WRITE;
}

static uint8_t convertToSDMode(uint8_t mode)
{
    // STM32WL/NRF52: FILE_O_READ=0, FILE_O_WRITE=1
    return (mode == 0) ? FILE_READ : FILE_WRITE;
}
#endif
#endif

LoFS::FSType LoFS::parsePath(const char *filepath, char *strippedPath, size_t bufferSize)
{
    if (!filepath || !strippedPath || bufferSize == 0) {
        return FSType::INVALID;
    }

    // Check for /internal/ prefix
    if (strncmp(filepath, "/internal/", 10) == 0) {
        size_t len = strlen(filepath + 10);
        if (len + 1 > bufferSize) {
            return FSType::INVALID;
        }
        strcpy(strippedPath, filepath + 10);
        // Ensure leading slash for internal filesystem
        if (strippedPath[0] != '/') {
            memmove(strippedPath + 1, strippedPath, len + 1);
            strippedPath[0] = '/';
        }
        return FSType::INTERNAL;
    }

    // Check for /sd/ prefix
    if (strncmp(filepath, "/sd/", 4) == 0) {
        // Check if SD card is actually available (compile-time or runtime)
        if (!LoFS::isSDCardAvailable()) {
            return FSType::INVALID; // SD card not available
        }
        size_t len = strlen(filepath + 4);
        if (len + 1 > bufferSize) {
            return FSType::INVALID;
        }
        strcpy(strippedPath, filepath + 4);
        // SD card paths typically don't need leading slash
        if (strippedPath[0] == '/') {
            memmove(strippedPath, strippedPath + 1, len);
        }
        return FSType::SD;
    }

    // Default to internal filesystem if no prefix (backward compatibility)
    size_t len = strlen(filepath);
    if (len + 1 > bufferSize) {
        return FSType::INVALID;
    }
    strcpy(strippedPath, filepath);
    return FSType::INTERNAL;
}

LoFS::FSType LoFS::parsePath(const char *filepath, char **strippedPath)
{
    if (!filepath) {
        *strippedPath = nullptr;
        return FSType::INVALID;
    }

    size_t maxLen = strlen(filepath) + 10; // Extra space for path manipulation
    *strippedPath = (char *)malloc(maxLen);
    if (!*strippedPath) {
        return FSType::INVALID;
    }

    return parsePath(filepath, *strippedPath, maxLen);
}

File LoFS::open(const char *filepath, uint8_t mode)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return File();
    }

    File result;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        // ESP32/RP2040: SD library uses string modes
        const char *sdMode = convertToSDMode(mode);
        result = SD.open(strippedPath, sdMode);
#else
        // STM32WL/NRF52: SD library uses uint8_t modes
        uint8_t sdMode = convertToSDMode(mode);
        result = SD.open(strippedPath, sdMode);
#endif
    } else
#endif
    {
        // Internal filesystem - handle platform-specific mode conversion
        concurrency::LockGuard g(spiLock);
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        // ESP32/RP2040: Convert uint8_t mode to string mode
        // FILE_O_READ is "r" (string), FILE_O_WRITE is "w" (string) on these platforms
        // mode is uint8_t: 0 = read, non-zero = write
        const char *modeStr = (mode == 0) ? FILE_O_READ : FILE_O_WRITE;
        result = FSCom.open(strippedPath, modeStr);
#else
        // STM32WL/NRF52: Use uint8_t mode directly
        // FILE_O_READ=0, FILE_O_WRITE=1 on these platforms
        result = FSCom.open(strippedPath, mode);
#endif
    }

    free(strippedPath);
    return result;
}

File LoFS::open(const char *filepath, const char *mode)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return File();
    }

    File result;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        // ESP32/RP2040: SD library uses string modes, pass directly
        result = SD.open(strippedPath, mode);
#else
        // STM32WL/NRF52: SD library uses uint8_t modes
        uint8_t sdMode = convertToSDMode(mode);
        result = SD.open(strippedPath, sdMode);
#endif
    } else
#endif
    {
        // Internal filesystem - handle platform-specific mode
        concurrency::LockGuard g(spiLock);
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        // ESP32/RP2040: Use string mode directly
        result = FSCom.open(strippedPath, mode);
#else
        // STM32WL/NRF52: Convert string mode to uint8_t
        // "r" -> 0 (FILE_O_READ), "w" -> 1 (FILE_O_WRITE)
        uint8_t modeInt = (strcmp(mode, "r") == 0) ? 0 : 1;
        result = FSCom.open(strippedPath, modeInt);
#endif
    }

    free(strippedPath);
    return result;
}

bool LoFS::exists(const char *filepath)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return false;
    }

    bool result = false;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
        result = SD.exists(strippedPath);
    } else
#endif
    {
        // Internal filesystem
        concurrency::LockGuard g(spiLock);
        result = FSCom.exists(strippedPath);
    }

    free(strippedPath);
    return result;
}

bool LoFS::mkdir(const char *filepath)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return false;
    }

    bool result = false;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
        result = SD.mkdir(strippedPath);
    } else
#endif
    {
        // Internal filesystem
        concurrency::LockGuard g(spiLock);
        result = FSCom.mkdir(strippedPath);
    }

    free(strippedPath);
    return result;
}

bool LoFS::remove(const char *filepath)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return false;
    }

    bool result = false;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
        result = SD.remove(strippedPath);
    } else
#endif
    {
        // Internal filesystem
        concurrency::LockGuard g(spiLock);
        result = FSCom.remove(strippedPath);
    }

    free(strippedPath);
    return result;
}

bool LoFS::rename(const char *oldfilepath, const char *newfilepath)
{
    char *oldStripped = nullptr;
    char *newStripped = nullptr;
    FSType oldType = parsePath(oldfilepath, &oldStripped);
    FSType newType = parsePath(newfilepath, &newStripped);

    if (!oldStripped || !newStripped || oldType == FSType::INVALID || newType == FSType::INVALID) {
        if (oldStripped) {
            free(oldStripped);
        }
        if (newStripped) {
            free(newStripped);
        }
        return false;
    }

    bool result = false;

    // If both paths are on the same filesystem, use simple rename
    if (oldType == newType) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        if (oldType == FSType::SD) {
            concurrency::LockGuard g(spiLock);
            result = SD.rename(oldStripped, newStripped);
        } else
#endif
        {
            // Internal filesystem
            concurrency::LockGuard g(spiLock);
            result = FSCom.rename(oldStripped, newStripped);
        }
    } else {
        // Cross-filesystem rename: copy + delete
        // Use a single lock for the entire operation to ensure atomicity
        concurrency::LockGuard g(spiLock);

        // Open source file
        File srcFile;
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        if (oldType == FSType::SD) {
            srcFile = SD.open(oldStripped, FILE_READ);
        } else
#endif
        {
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
            // ESP32/RP2040: FILE_O_READ is "r" (string)
            srcFile = FSCom.open(oldStripped, FILE_O_READ);
#else
            // STM32WL/NRF52: FILE_O_READ is 0 (uint8_t)
            srcFile = FSCom.open(oldStripped, FILE_O_READ);
#endif
        }

        if (!srcFile) {
            free(oldStripped);
            free(newStripped);
            return false;
        }

        // Remove destination if it exists
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        if (newType == FSType::SD) {
            if (SD.exists(newStripped)) {
                SD.remove(newStripped);
            }
        } else
#endif
        {
            if (FSCom.exists(newStripped)) {
                FSCom.remove(newStripped);
            }
        }

        // Open/create destination file
        File dstFile;
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        if (newType == FSType::SD) {
            dstFile = SD.open(newStripped, FILE_WRITE);
        } else
#endif
        {
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
            // ESP32/RP2040: FILE_O_WRITE is "w" (string)
            dstFile = FSCom.open(newStripped, FILE_O_WRITE);
#else
            // STM32WL/NRF52: FILE_O_WRITE is 1 (uint8_t)
            dstFile = FSCom.open(newStripped, FILE_O_WRITE);
#endif
        }

        if (!dstFile) {
            srcFile.close();
            free(oldStripped);
            free(newStripped);
            return false;
        }

        // Copy data
        unsigned char buffer[64]; // Larger buffer for better performance
        size_t bytesRead;
        result = true;

        while ((bytesRead = srcFile.read(buffer, sizeof(buffer))) > 0) {
            if (dstFile.write(buffer, bytesRead) != bytesRead) {
                result = false;
                break;
            }
        }

        dstFile.flush();
        dstFile.close();
        srcFile.close();

        // Delete source file only if copy succeeded
        if (result) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
            if (oldType == FSType::SD) {
                result = SD.remove(oldStripped);
            } else
#endif
            {
                result = FSCom.remove(oldStripped);
            }
        } else {
            // Copy failed, try to clean up destination
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
            if (newType == FSType::SD) {
                SD.remove(newStripped);
            } else
#endif
            {
                FSCom.remove(newStripped);
            }
        }
    }

    free(oldStripped);
    free(newStripped);
    return result;
}

bool LoFS::rmdir(const char *filepath, bool recursive)
{
    if (!exists(filepath)) {
        return true; // Already doesn't exist, consider it success
    }

    // If recursive, first remove all contents
    if (recursive) {
        // Use LoFS::open to get a File object that handles prefix correctly
        File dir = open(filepath, FILE_O_READ);
        if (!dir) {
            return false;
        }

        if (!dir.isDirectory()) {
            dir.close();
            // If it's not a directory, try removing as a file
            return remove(filepath);
        }

        bool result = true;
        
        // Recursively remove all files and subdirectories
        while (true) {
            File file = dir.openNextFile();
            if (!file) {
                break;
            }
            
            // Get the name from file.name() - this might be full path or just filename
            std::string pathFromFile = file.name();
            bool isDir = file.isDirectory();
            file.close();
            
            // Always construct full path from parent directory to ensure correctness
            // Extract just the filename/entry name (after last /)
            size_t lastSlash = pathFromFile.rfind('/');
            std::string entryName = (lastSlash != std::string::npos) ? pathFromFile.substr(lastSlash + 1) : pathFromFile;
            
            // Skip "." and ".." entries
            if (entryName == "." || entryName == "..") {
                continue;
            }
            
            // Build full path: filepath/entryName
            char fullPathBuf[256];
            snprintf(fullPathBuf, sizeof(fullPathBuf), "%s/%s", filepath, entryName.c_str());
            std::string fullPath = fullPathBuf;
            
            // Recursively remove subdirectories, or remove files
            if (isDir) {
                // Recursively remove subdirectory
                if (!rmdir(fullPath.c_str(), true)) {
                    result = false;
                }
            } else {
                // Remove file
                if (!remove(fullPath.c_str())) {
                    result = false;
                }
            }
        }
        dir.close();

        if (!result) {
            return false;
        }
    }

    // Now remove the directory itself (or if non-recursive, just try to remove empty directory)
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return false;
    }

    bool result = false;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
        result = SD.rmdir(strippedPath);
    } else
#endif
    {
        // Internal filesystem
        concurrency::LockGuard g(spiLock);
        result = FSCom.rmdir(strippedPath);
    }

    free(strippedPath);
    return result;
}

uint64_t LoFS::totalBytes(const char *filepath)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return 0;
    }

    uint64_t result = 0;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
        result = SD.totalBytes();
    } else
#endif
    {
        // Internal filesystem
        concurrency::LockGuard g(spiLock);
        result = FSCom.totalBytes();
    }

    free(strippedPath);
    return result;
}

uint64_t LoFS::usedBytes(const char *filepath)
{
    char *strippedPath = nullptr;
    FSType fsType = parsePath(filepath, &strippedPath);

    if (!strippedPath || fsType == FSType::INVALID) {
        if (strippedPath) {
            free(strippedPath);
        }
        return 0;
    }

    uint64_t result = 0;

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (fsType == FSType::SD) {
        concurrency::LockGuard g(spiLock);
        result = SD.usedBytes();
    } else
#endif
    {
        // Internal filesystem
        concurrency::LockGuard g(spiLock);
        result = FSCom.usedBytes();
    }

    free(strippedPath);
    return result;
}

uint64_t LoFS::freeBytes(const char *filepath)
{
    uint64_t total = totalBytes(filepath);
    uint64_t used = usedBytes(filepath);
    if (total == 0) {
        return 0; // Invalid filesystem
    }
    return total - used;
}
