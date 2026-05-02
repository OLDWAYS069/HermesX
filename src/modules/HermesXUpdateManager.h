#pragma once

#include "configuration.h"
#include <Arduino.h>
#include <functional>

#if defined(ARCH_ESP32)
#include "FSCommon.h"
#include <FS.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#endif

enum class HermesXUpdateState : uint8_t {
    Idle,
    AwaitingImage,
    Writing,
    ReadyToApply,
    Failed,
};

class HermesXUpdateManager
{
  public:
    static HermesXUpdateManager &instance();

    void begin();
    bool poll();

    bool refreshImage();
    bool startUpdate();
    void markIncomingTransfer();
    void setIncomingTransferProgress(size_t bytesReceived, size_t totalBytes);
    bool beginStreamUpdate(size_t imageSize, const String &version, const String &projectName);
    bool writeStreamChunk(const uint8_t *data, size_t length);
    bool finishStreamUpdate();
    bool checkRemoteImage(const std::function<void()> &pump = {});
    bool downloadRemoteImage(const std::function<void()> &pump = {});
    void failStreamUpdate(const String &reason, bool keepCandidate = false);
    bool cancelUpdate();
    void resetUiSession(bool clearStoredFile = false);
    bool applyUpdate();
    bool inspectImageBytes(const uint8_t *data, size_t length, size_t imageSize, String &versionOut, String &projectNameOut,
                           String &errorOut) const;

    HermesXUpdateState getState() const { return state; }
    const char *getStateLabel() const;

    const String &getCurrentVersion() const { return currentVersion; }
    const String &getCurrentBuildVersion() const { return currentBuildVersion; }
    const String &getCandidateVersion() const { return candidateVersion; }
    const String &getSourceStatus() const { return sourceStatus; }
    const String &getLastError() const { return lastError; }
    const String &getRunningPartitionLabel() const { return runningPartitionLabel; }
    const String &getTargetPartitionLabel() const { return targetPartitionLabel; }
    const String &getLastUpdatedVersion() const { return lastUpdatedVersion; }
    int getProgressPercent() const { return progressPercent; }
    bool hasCandidate() const { return candidateValid; }
    bool isBusy() const { return state == HermesXUpdateState::Writing; }
    bool canApply() const;
    bool hasPendingApply() const { return pendingApply; }
    const char *getUpdatePath() const { return updatePath; }
    const char *getRemoteUpdateUrl() const;
    static bool parseHermesVersionFromFilename(const String &filename, String &buildVersionOut,
                                               String *displayVersionOut = nullptr);
    static String extractFilenameFromContentDisposition(const String &headerValue);

  private:
    HermesXUpdateManager() = default;

    bool loadImageInfo(bool updateStateOnFailure);
    bool fail(const String &message, bool setFailedState = true);
    bool readImageDescriptor(File &file, size_t &imageSize, String &versionOut, String &projectNameOut,
                             String &errorOut);
    bool beginHttpClientForUrl(class HTTPClient &client, class WiFiClientSecure &secureClient, const char *url);
    void resetCandidate();
    void finishWritingSuccess();
    void abortWriting(const String &reason, bool keepCandidate);
    void clearPendingApply();
    bool savePendingApply();
    void handleBootResumeState();

    static constexpr const char *updatePath = "/update/firmware.bin";
    static constexpr const char *updateNamePath = "/update/firmware.name";
    static constexpr const char *pendingStatePath = "/prefs/hermesx_update_pending.bin";
    static constexpr uint32_t pendingStateMagic = 0x48585550; // HXUP
    static constexpr uint32_t pendingStateVersion = 1;
    static constexpr size_t writeChunkSize = 4096;

    struct PendingApplyState {
        uint32_t magic = pendingStateMagic;
        uint32_t version = pendingStateVersion;
        char targetVersion[32] = {0};
        char targetPartition[16] = {0};
    };

    bool initialized = false;
    HermesXUpdateState state = HermesXUpdateState::Idle;
    String currentVersion;
    String currentBuildVersion;
    String candidateVersion;
    String candidateProjectName;
    String sourceStatus;
    String lastError;
    String runningPartitionLabel;
    String targetPartitionLabel;
    String lastUpdatedVersion;
    int progressPercent = 0;
    bool candidateValid = false;
    bool pendingApply = false;
    bool cancelRequested = false;
    bool streamWriteActive = false;
    size_t candidateSize = 0;
    size_t bytesWritten = 0;

#if defined(ARCH_ESP32)
    File updateFile;
    const esp_partition_t *targetPartition = nullptr;
    esp_ota_handle_t otaHandle = 0;
#endif
};
