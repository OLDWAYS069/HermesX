#include "HermesXUpdateManager.h"

#include "FSCommon.h"
#include "configuration.h"
#include "main.h"
#include "modules/HermesXInterfaceModule.h"
#include <memory>
#if defined(ARCH_ESP32)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_app_format.h>
#include <esp_err.h>
#include <esp_ota_ops.h>
#include "esp_task_wdt.h"
#endif

namespace
{
#ifndef HERMESX_UPDATE_URL
#define HERMESX_UPDATE_URL ""
#endif

constexpr const char *kHermesXRemoteUpdateUrl = HERMESX_UPDATE_URL;

bool isAsciiDigits(const String &value)
{
    if (value.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value.charAt(i);
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

String decodePercentEscapes(const String &value)
{
    String decoded;
    decoded.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value.charAt(i);
        if (c == '%' && i + 2 < value.length()) {
            auto hexDigit = [](char v) -> int {
                if (v >= '0' && v <= '9')
                    return v - '0';
                if (v >= 'A' && v <= 'F')
                    return 10 + (v - 'A');
                if (v >= 'a' && v <= 'f')
                    return 10 + (v - 'a');
                return -1;
            };
            const int hi = hexDigit(value.charAt(i + 1));
            const int lo = hexDigit(value.charAt(i + 2));
            if (hi >= 0 && lo >= 0) {
                decoded += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        decoded += c;
    }
    return decoded;
}

String trimFilenameToken(String value)
{
    value.trim();
    if (value.startsWith("\"") && value.endsWith("\"") && value.length() >= 2) {
        value = value.substring(1, value.length() - 1);
    }
    return value;
}

void ensureUpdateDirs()
{
#ifdef FSCom
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (!FSCom.exists("/update")) {
        FSCom.mkdir("/update");
    }
#endif
}

String truncateMessage(const String &value, size_t limit = 48)
{
    if (value.length() <= limit) {
        return value;
    }
    return value.substring(0, limit - 3) + "...";
}

String readStoredUpdateFilename()
{
#ifdef FSCom
    constexpr const char *kStoredUpdateFilenamePath = "/update/firmware.name";
    if (!FSCom.exists(kStoredUpdateFilenamePath)) {
        return "";
    }
    File file = FSCom.open(kStoredUpdateFilenamePath, FILE_O_READ);
    if (!file) {
        return "";
    }
    String value = file.readStringUntil('\n');
    file.close();
    value.trim();
    return value;
#else
    return "";
#endif
}
} // namespace

HermesXUpdateManager &HermesXUpdateManager::instance()
{
    static HermesXUpdateManager manager;
    return manager;
}

void HermesXUpdateManager::begin()
{
    if (initialized) {
        return;
    }
    initialized = true;
    currentVersion = String(optstr(APP_VERSION_DISPLAY));
    currentBuildVersion = String(optstr(APP_HERMES_VERSION));
#if defined(ARCH_ESP32)
    if (const esp_partition_t *running = esp_ota_get_running_partition()) {
        runningPartitionLabel = running->label;
    } else {
        runningPartitionLabel = "unknown";
    }
#else
    runningPartitionLabel = "unsupported";
#endif
    sourceStatus = String(u8"等待檢查: ") + updatePath;
    ensureUpdateDirs();
    handleBootResumeState();
}

const char *HermesXUpdateManager::getStateLabel() const
{
    switch (state) {
    case HermesXUpdateState::Idle:
        return u8"閒置";
    case HermesXUpdateState::AwaitingImage:
        return u8"待更新檔";
    case HermesXUpdateState::Writing:
        return u8"寫入中";
    case HermesXUpdateState::ReadyToApply:
        return u8"可套用";
    case HermesXUpdateState::Failed:
        return u8"失敗";
    default:
        return u8"未知";
    }
}

bool HermesXUpdateManager::canApply() const
{
#if defined(ARCH_ESP32)
    return state == HermesXUpdateState::ReadyToApply && targetPartition != nullptr && targetPartitionLabel.length() > 0;
#else
    return state == HermesXUpdateState::ReadyToApply;
#endif
}

const char *HermesXUpdateManager::getRemoteUpdateUrl() const
{
    return kHermesXRemoteUpdateUrl;
}

bool HermesXUpdateManager::parseHermesVersionFromFilename(const String &filename, String &buildVersionOut,
                                                          String *displayVersionOut)
{
    String haystack = filename;
    haystack.trim();
    for (int start = haystack.indexOf("HXB"); start >= 0; start = haystack.indexOf("HXB", start + 1)) {
        String candidate = haystack.substring(start);
        const int slashForward = candidate.lastIndexOf('/');
        const int slashBackward = candidate.lastIndexOf('\\');
        const int slash = slashForward > slashBackward ? slashForward : slashBackward;
        if (slash >= 0 && slash + 1 < candidate.length()) {
            candidate = candidate.substring(slash + 1);
        }
        const int query = candidate.indexOf('?');
        if (query >= 0) {
            candidate = candidate.substring(0, query);
        }
        const int hash = candidate.indexOf('#');
        if (hash >= 0) {
            candidate = candidate.substring(0, hash);
        }
        const int quote = candidate.indexOf('"');
        if (quote >= 0) {
            candidate = candidate.substring(0, quote);
        }
        const int semi = candidate.indexOf(';');
        if (semi >= 0) {
            candidate = candidate.substring(0, semi);
        }
        candidate.trim();
        if (candidate.endsWith(".factory.bin")) {
            candidate.remove(candidate.length() - strlen(".factory.bin"));
        } else if (candidate.endsWith(".bin")) {
            candidate.remove(candidate.length() - 4);
        }
        if (!candidate.startsWith("HXB")) {
            continue;
        }
        const int firstUnderscore = candidate.indexOf('_');
        const int secondUnderscore = firstUnderscore >= 0 ? candidate.indexOf('_', firstUnderscore + 1) : -1;
        if (firstUnderscore <= 3 || secondUnderscore <= firstUnderscore + 1) {
            continue;
        }
        const String displayVersion = candidate.substring(0, firstUnderscore);
        const String datePart = candidate.substring(firstUnderscore + 1, secondUnderscore);
        const String timePart = candidate.substring(secondUnderscore + 1);
        if (datePart.length() != 8 || timePart.length() != 4 || !isAsciiDigits(datePart) || !isAsciiDigits(timePart)) {
            continue;
        }
        buildVersionOut = candidate;
        if (displayVersionOut) {
            *displayVersionOut = displayVersion;
        }
        return true;
    }
    return false;
}

String HermesXUpdateManager::extractFilenameFromContentDisposition(const String &headerValue)
{
    String header = headerValue;
    if (header.isEmpty()) {
        return "";
    }

    const int star = header.indexOf("filename*=");
    if (star >= 0) {
        String token = header.substring(star + 10);
        const int semi = token.indexOf(';');
        if (semi >= 0) {
            token = token.substring(0, semi);
        }
        token = trimFilenameToken(token);
        const int tick = token.indexOf("''");
        if (tick >= 0) {
            token = token.substring(tick + 2);
        }
        token = decodePercentEscapes(token);
        if (!token.isEmpty()) {
            return token;
        }
    }

    const int plain = header.indexOf("filename=");
    if (plain >= 0) {
        String token = header.substring(plain + 9);
        const int semi = token.indexOf(';');
        if (semi >= 0) {
            token = token.substring(0, semi);
        }
        token = trimFilenameToken(token);
        if (!token.isEmpty()) {
            return token;
        }
    }
    return "";
}

bool HermesXUpdateManager::poll()
{
    begin();
#if !defined(ARCH_ESP32)
    return false;
#else
    if (state != HermesXUpdateState::Writing) {
        return false;
    }

    if (streamWriteActive) {
        return false;
    }

    if (cancelRequested) {
        abortWriting(u8"已取消更新", true);
        return true;
    }

    if (!updateFile) {
        abortWriting(u8"更新檔已關閉", true);
        return true;
    }

    uint8_t buffer[writeChunkSize];
    const int bytesRead = updateFile.read(buffer, sizeof(buffer));
    if (bytesRead < 0) {
        abortWriting(u8"讀取更新檔失敗", true);
        return true;
    }

    if (bytesRead == 0) {
        finishWritingSuccess();
        return true;
    }

    if (esp_ota_write(otaHandle, buffer, static_cast<size_t>(bytesRead)) != ESP_OK) {
        abortWriting(u8"寫入 OTA 分區失敗", true);
        return true;
    }

    bytesWritten += static_cast<size_t>(bytesRead);
    const int nextPercent =
        (candidateSize > 0) ? static_cast<int>((bytesWritten * 100ULL) / candidateSize) : progressPercent;
    if (nextPercent != progressPercent) {
        progressPercent = nextPercent;
        sourceStatus = String(u8"寫入中: ") + targetPartitionLabel;
        return true;
    }
    return false;
#endif
}

bool HermesXUpdateManager::refreshImage()
{
    begin();
    resetCandidate();
    sourceStatus = String(u8"檢查更新檔: ") + updatePath;
    const bool ok = loadImageInfo(true);
    if (ok) {
        state = HermesXUpdateState::AwaitingImage;
        sourceStatus = String(u8"已找到更新檔: ") + candidateVersion;
        lastError = "";
    } else if (state != HermesXUpdateState::Failed) {
        state = HermesXUpdateState::AwaitingImage;
    }
    return ok;
}

bool HermesXUpdateManager::startUpdate()
{
    begin();
#if !defined(ARCH_ESP32)
    return fail(u8"此平台不支援自有 OTA");
#else
    if (state == HermesXUpdateState::Writing) {
        return false;
    }
    if (!candidateValid && !refreshImage()) {
        return false;
    }

    targetPartition = esp_ota_get_next_update_partition(nullptr);
    if (!targetPartition) {
        return fail(u8"找不到可用 OTA 槽位");
    }
    targetPartitionLabel = targetPartition->label;

    if (candidateSize == 0 || candidateSize > targetPartition->size) {
        return fail(u8"更新檔超過 OTA 槽位容量");
    }

    updateFile = FSCom.open(updatePath, FILE_O_READ);
    if (!updateFile) {
        return fail(u8"無法開啟更新檔");
    }

    if (esp_ota_begin(targetPartition, candidateSize, &otaHandle) != ESP_OK) {
        updateFile.close();
        return fail(u8"OTA 初始化失敗");
    }

    cancelRequested = false;
    streamWriteActive = false;
    bytesWritten = 0;
    progressPercent = 0;
    state = HermesXUpdateState::Writing;
    sourceStatus = String(u8"寫入中: ") + targetPartitionLabel;
    lastError = "";
    return true;
#endif
}

void HermesXUpdateManager::markIncomingTransfer()
{
    begin();
    if (state == HermesXUpdateState::Writing) {
        return;
    }
    progressPercent = 0;
    lastError = "";
    state = HermesXUpdateState::AwaitingImage;
    sourceStatus = u8"接收更新中: 準備中";
}

void HermesXUpdateManager::setIncomingTransferProgress(size_t bytesReceived, size_t totalBytes)
{
    begin();
    if (state == HermesXUpdateState::Writing) {
        return;
    }

    int nextPercent = 0;
    if (totalBytes > 0) {
        nextPercent = static_cast<int>((std::min(bytesReceived, totalBytes) * 100ULL) / totalBytes);
    }
    if (nextPercent > 100) {
        nextPercent = 100;
    }
    if (state == HermesXUpdateState::AwaitingImage && nextPercent == progressPercent && lastError.length() == 0) {
        return;
    }
    progressPercent = nextPercent;
    lastError = "";
    state = HermesXUpdateState::AwaitingImage;
    sourceStatus = String(u8"接收更新中: ") + String(progressPercent) + "%";
}

bool HermesXUpdateManager::beginStreamUpdate(size_t imageSize, const String &version, const String &projectName)
{
    begin();
#if !defined(ARCH_ESP32)
    (void)imageSize;
    (void)version;
    (void)projectName;
    return fail(u8"此平台不支援自有 OTA");
#else
    if (state == HermesXUpdateState::Writing) {
        return false;
    }

    resetCandidate();

    targetPartition = esp_ota_get_next_update_partition(nullptr);
    if (!targetPartition) {
        return fail(u8"找不到可用 OTA 槽位");
    }
    targetPartitionLabel = targetPartition->label;

    if (imageSize == 0 || imageSize > targetPartition->size) {
        return fail(u8"更新檔超過 OTA 槽位容量");
    }

    if (esp_ota_begin(targetPartition, imageSize, &otaHandle) != ESP_OK) {
        return fail(u8"OTA 初始化失敗");
    }

    candidateSize = imageSize;
    candidateVersion = version;
    candidateProjectName = projectName;
    candidateValid = true;
    cancelRequested = false;
    streamWriteActive = true;
    bytesWritten = 0;
    progressPercent = 0;
    state = HermesXUpdateState::Writing;
    sourceStatus = String(u8"接收更新中: ") + version;
    lastError = "";
    return true;
#endif
}

bool HermesXUpdateManager::beginHttpClientForUrl(HTTPClient &client, WiFiClientSecure &secureClient, const char *url)
{
    if (!url || !url[0]) {
        return false;
    }

    client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (strncmp(url, "https://", 8) == 0) {
        secureClient.setInsecure();
        return client.begin(secureClient, url);
    }
    return client.begin(url);
}

bool HermesXUpdateManager::checkRemoteImage(const std::function<void()> &pump)
{
#if !defined(ARCH_ESP32)
    (void)pump;
    return fail(u8"此平台不支援 URL 更新");
#else
    begin();
    resetCandidate();
    progressPercent = 0;
    lastError = "";

    const char *url = getRemoteUpdateUrl();
    if (!url || !url[0]) {
        return fail(u8"URL 更新來源未設定");
    }

    sourceStatus = u8"檢查 URL 更新中";
    if (pump) {
        pump();
    }

    HTTPClient client;
    WiFiClientSecure secureClient;
    if (!beginHttpClientForUrl(client, secureClient, url)) {
        return fail(u8"無法建立 HTTP 連線");
    }
    const char *headerKeys[] = {"Content-Range", "Content-Disposition"};
    client.collectHeaders(headerKeys, 2);

    static constexpr size_t kHeaderBytesRequired =
        sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    client.addHeader("Range", String("bytes=0-") + String(kHeaderBytesRequired - 1));
    const int code = client.GET();
    if (code != HTTP_CODE_OK && code != HTTP_CODE_PARTIAL_CONTENT) {
        client.end();
        return fail(String(u8"HTTP 失敗: ") + String(code));
    }

    int totalSize = client.getSize();
    const String contentRange = client.header("Content-Range");
    const String contentDisposition = client.header("Content-Disposition");
    const int slash = contentRange.lastIndexOf('/');
    if (slash >= 0) {
        totalSize = contentRange.substring(slash + 1).toInt();
    }
    if (totalSize <= 0) {
        client.end();
        return fail(u8"遠端檔案缺少 Content-Length");
    }

    WiFiClient *stream = client.getStreamPtr();
    uint8_t headerBuffer[kHeaderBytesRequired] = {0};
    size_t headerLength = 0;
    uint32_t startMs = millis();
    while (headerLength < kHeaderBytesRequired && millis() - startMs < 5000) {
        const size_t availableBytes = stream->available();
        if (availableBytes == 0) {
            if (pump) {
                pump();
            }
            yield();
            delay(1);
            continue;
        }
        const size_t want = std::min(kHeaderBytesRequired - headerLength, availableBytes);
        const size_t got = stream->readBytes(headerBuffer + headerLength, want);
        headerLength += got;
        if (pump) {
            pump();
        }
    }
    client.end();

    if (headerLength < kHeaderBytesRequired) {
        return fail(u8"遠端更新檔標頭不足");
    }

    String versionOut;
    String projectNameOut;
    String errorOut;
    if (!inspectImageBytes(headerBuffer, headerLength, static_cast<size_t>(totalSize), versionOut, projectNameOut,
                           errorOut)) {
        return fail(errorOut);
    }

    String versionFromFilename;
    String displayFromFilename;
    const String remoteFilename = extractFilenameFromContentDisposition(contentDisposition);
    LOG_INFO("URL update header Content-Disposition: %s", contentDisposition.c_str());
    LOG_INFO("URL update derived filename: %s", remoteFilename.c_str());
    if (parseHermesVersionFromFilename(remoteFilename, versionFromFilename, &displayFromFilename) ||
        parseHermesVersionFromFilename(contentDisposition, versionFromFilename, &displayFromFilename) ||
        parseHermesVersionFromFilename(String(url), versionFromFilename, &displayFromFilename)) {
        versionOut = versionFromFilename;
        LOG_INFO("URL update filename version override: %s", versionOut.c_str());
    } else {
        LOG_WARN("URL update filename parse failed, fallback version=%s", versionOut.c_str());
    }

    if (versionOut == currentVersion || versionOut == currentBuildVersion) {
        candidateValid = false;
        candidateSize = 0;
        candidateVersion = "";
        candidateProjectName = "";
        progressPercent = 0;
        state = HermesXUpdateState::Idle;
        sourceStatus = u8"已是最新版本";
        lastError = "";
        return true;
    }

    candidateSize = static_cast<size_t>(totalSize);
    candidateVersion = versionOut;
    candidateProjectName = projectNameOut;
    candidateValid = true;
    state = HermesXUpdateState::AwaitingImage;
    sourceStatus = String(u8"已找到 URL 更新: ") + candidateVersion;
    return true;
#endif
}

bool HermesXUpdateManager::downloadRemoteImage(const std::function<void()> &pump)
{
#if !defined(ARCH_ESP32)
    (void)pump;
    return fail(u8"此平台不支援 URL 更新");
#else
    begin();
    const char *url = getRemoteUpdateUrl();
    if (!url || !url[0]) {
        return fail(u8"URL 更新來源未設定");
    }
    if (!candidateValid) {
        return fail(u8"請先開始檢查", false);
    }
    sourceStatus = String(u8"開始下載 URL 更新: ") + candidateVersion;
    lastError = "";
    progressPercent = 0;
    if (pump) {
        pump();
    }
    if (!beginStreamUpdate(candidateSize, candidateVersion, candidateProjectName)) {
        return false;
    }

    HTTPClient client;
    WiFiClientSecure secureClient;
    if (!beginHttpClientForUrl(client, secureClient, url)) {
        abortWriting(u8"無法建立 HTTP 連線", false);
        return false;
    }

    const int code = client.GET();
    if (code != HTTP_CODE_OK) {
        client.end();
        abortWriting(String(u8"HTTP 下載失敗: ") + String(code), false);
        return false;
    }

    const int totalSize = client.getSize();
    if (totalSize <= 0 || static_cast<size_t>(totalSize) != candidateSize) {
        client.end();
        abortWriting(u8"遠端檔案大小不符", false);
        return false;
    }

    WiFiClient *stream = client.getStreamPtr();
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[4096]);
    if (!buffer) {
        client.end();
        abortWriting(u8"URL 下載緩衝區配置失敗", false);
        return false;
    }
    size_t received = 0;
    uint32_t zeroReadStartMs = 0;
    while (received < candidateSize) {
        esp_task_wdt_reset();
        const size_t availableBytes = stream->available();
        if (availableBytes == 0) {
            if (zeroReadStartMs == 0) {
                zeroReadStartMs = millis();
            } else if (millis() - zeroReadStartMs > 5000) {
                client.end();
                abortWriting(u8"URL 下載逾時", false);
                return false;
            }
            if (pump) {
                pump();
            }
            yield();
            delay(1);
            continue;
        }
        zeroReadStartMs = 0;
        const size_t want = std::min(static_cast<size_t>(4096), std::min(availableBytes, candidateSize - received));
        const size_t got = stream->readBytes(buffer.get(), want);
        if (got == 0) {
            continue;
        }
        if (!writeStreamChunk(buffer.get(), got)) {
            client.end();
            return false;
        }
        received += got;
        if (pump) {
            pump();
        }
        yield();
    }

    client.end();
    return finishStreamUpdate();
#endif
}

bool HermesXUpdateManager::writeStreamChunk(const uint8_t *data, size_t length)
{
#if !defined(ARCH_ESP32)
    (void)data;
    (void)length;
    return false;
#else
    if (!streamWriteActive || state != HermesXUpdateState::Writing || otaHandle == 0) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (esp_ota_write(otaHandle, data, length) != ESP_OK) {
        abortWriting(u8"寫入 OTA 分區失敗", true);
        return false;
    }

    bytesWritten += length;
    const int nextPercent = (candidateSize > 0) ? static_cast<int>((bytesWritten * 100ULL) / candidateSize) : 0;
    if (nextPercent != progressPercent) {
        progressPercent = nextPercent;
        sourceStatus = String(u8"接收更新中: ") + String(progressPercent) + "%";
    }
    return true;
#endif
}

bool HermesXUpdateManager::finishStreamUpdate()
{
#if !defined(ARCH_ESP32)
    return false;
#else
    if (!streamWriteActive) {
        return false;
    }
    streamWriteActive = false;
    finishWritingSuccess();
    return state == HermesXUpdateState::ReadyToApply;
#endif
}

void HermesXUpdateManager::failStreamUpdate(const String &reason, bool keepCandidate)
{
    abortWriting(reason, keepCandidate);
}

bool HermesXUpdateManager::inspectImageBytes(const uint8_t *data, size_t length, size_t imageSize, String &versionOut,
                                             String &projectNameOut, String &errorOut) const
{
#if !defined(ARCH_ESP32)
    (void)data;
    (void)length;
    (void)imageSize;
    (void)versionOut;
    (void)projectNameOut;
    errorOut = u8"此平台不支援 OTA";
    return false;
#else
    if (!data || length < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
        errorOut = u8"更新檔標頭不足";
        return false;
    }
    if (imageSize < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
        errorOut = u8"更新檔太小";
        return false;
    }

    const auto *imageHeader = reinterpret_cast<const esp_image_header_t *>(data);
    if (imageHeader->magic != ESP_IMAGE_HEADER_MAGIC) {
        errorOut = u8"不是有效的 ESP32 app image";
        return false;
    }

    const auto *appDesc = reinterpret_cast<const esp_app_desc_t *>(
        data + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    const esp_app_desc_t *currentDesc = esp_ota_get_app_description();
    if (appDesc->project_name[0] == '\0' || appDesc->version[0] == '\0') {
        errorOut = u8"更新檔缺少版本資訊";
        return false;
    }
    if (strcmp(appDesc->project_name, currentDesc->project_name) != 0) {
        errorOut = u8"更新檔不是 HermesX 主 app image";
        return false;
    }

    versionOut = appDesc->version;
    projectNameOut = appDesc->project_name;
    return true;
#endif
}

bool HermesXUpdateManager::cancelUpdate()
{
    begin();
    if (state == HermesXUpdateState::Writing) {
        cancelRequested = true;
        sourceStatus = u8"取消中";
        return true;
    }
    if (state == HermesXUpdateState::ReadyToApply || state == HermesXUpdateState::Failed ||
        state == HermesXUpdateState::AwaitingImage) {
        resetCandidate();
        state = HermesXUpdateState::Idle;
        sourceStatus = String(u8"等待檢查: ") + updatePath;
        lastError = "";
        return true;
    }
    return false;
}

void HermesXUpdateManager::resetUiSession(bool clearStoredFile)
{
    begin();
#if defined(ARCH_ESP32)
    if (state == HermesXUpdateState::Writing) {
        abortWriting(u8"UI session reset", false);
    } else {
        if (otaHandle != 0) {
            esp_ota_abort(otaHandle);
            otaHandle = 0;
        }
        if (updateFile) {
            updateFile.close();
        }
        cancelRequested = false;
        streamWriteActive = false;
        resetCandidate();
        state = HermesXUpdateState::Idle;
        sourceStatus = String(u8"等待開始更新");
        lastError = "";
    }
#else
    resetCandidate();
    state = HermesXUpdateState::Idle;
    sourceStatus = String(u8"等待開始更新");
    lastError = "";
#endif

#ifdef FSCom
    if (clearStoredFile && FSCom.exists(updatePath)) {
        FSCom.remove(updatePath);
    }
    if (clearStoredFile && FSCom.exists(updateNamePath)) {
        FSCom.remove(updateNamePath);
    }
#else
    (void)clearStoredFile;
#endif
}

bool HermesXUpdateManager::applyUpdate()
{
    begin();
#if !defined(ARCH_ESP32)
    return fail(u8"此平台不支援自有 OTA");
#else
    if (state != HermesXUpdateState::ReadyToApply || !targetPartition) {
        return false;
    }
    if (!savePendingApply()) {
        return fail(u8"無法保存待套用狀態");
    }
    if (esp_ota_set_boot_partition(targetPartition) != ESP_OK) {
        clearPendingApply();
        return fail(u8"設定開機槽位失敗");
    }
    sourceStatus = String(u8"套用更新: ") + targetPartitionLabel;
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playAckSuccess();
    }
    delay(80);
    ESP.restart();
    return true;
#endif
}

bool HermesXUpdateManager::loadImageInfo(bool updateStateOnFailure)
{
#ifndef FSCom
    return fail(u8"檔案系統不可用", updateStateOnFailure);
#else
    if (!FSCom.exists(updatePath)) {
        if (updateStateOnFailure) {
            state = HermesXUpdateState::AwaitingImage;
            sourceStatus = String(u8"找不到更新檔: ") + updatePath;
            lastError = "";
        }
        return false;
    }

    File file = FSCom.open(updatePath, FILE_O_READ);
    if (!file) {
        return fail(u8"無法開啟更新檔", updateStateOnFailure);
    }

    String versionOut;
    String projectNameOut;
    String errorOut;
    size_t imageSize = 0;
    const bool ok = readImageDescriptor(file, imageSize, versionOut, projectNameOut, errorOut);
    file.close();
    if (!ok) {
        return fail(errorOut, updateStateOnFailure);
    }

    String versionFromFilename;
    String displayFromFilename;
    if (parseHermesVersionFromFilename(readStoredUpdateFilename(), versionFromFilename, &displayFromFilename)) {
        versionOut = versionFromFilename;
    }

#if defined(ARCH_ESP32)
    const esp_partition_t *nextPartition = esp_ota_get_next_update_partition(nullptr);
    if (nextPartition && imageSize > nextPartition->size) {
        return fail(u8"更新檔超過 OTA 槽位容量", updateStateOnFailure);
    }
#endif

    candidateSize = imageSize;
    candidateVersion = versionOut;
    candidateProjectName = projectNameOut;
    candidateValid = true;
    return true;
#endif
}

bool HermesXUpdateManager::fail(const String &message, bool setFailedState)
{
    lastError = truncateMessage(message);
    sourceStatus = u8"更新失敗";
    if (setFailedState) {
        state = HermesXUpdateState::Failed;
    }
    return false;
}

bool HermesXUpdateManager::readImageDescriptor(File &file, size_t &imageSize, String &versionOut, String &projectNameOut,
                                               String &errorOut)
{
#if !defined(ARCH_ESP32)
    (void)file;
    (void)imageSize;
    (void)versionOut;
    (void)projectNameOut;
    errorOut = u8"此平台不支援 OTA";
    return false;
#else
    imageSize = static_cast<size_t>(file.size());
    if (imageSize < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
        errorOut = u8"更新檔太小";
        return false;
    }

    static constexpr size_t kHeaderBytesRequired =
        sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    uint8_t inspectBuffer[kHeaderBytesRequired] = {0};
    file.seek(0);
    if (file.read(inspectBuffer, kHeaderBytesRequired) != static_cast<int>(kHeaderBytesRequired)) {
        errorOut = u8"讀取映像檔標頭失敗";
        return false;
    }

    esp_image_header_t imageHeader;
    esp_image_segment_header_t segmentHeader;
    esp_app_desc_t appDesc;

    memcpy(&imageHeader, inspectBuffer, sizeof(imageHeader));
    if (imageHeader.magic != ESP_IMAGE_HEADER_MAGIC) {
        errorOut = u8"不是有效的 ESP32 app image";
        return false;
    }
    memcpy(&segmentHeader, inspectBuffer + sizeof(imageHeader), sizeof(segmentHeader));
    memcpy(&appDesc, inspectBuffer + sizeof(imageHeader) + sizeof(segmentHeader), sizeof(appDesc));

    const esp_app_desc_t *currentDesc = esp_ota_get_app_description();
    if (appDesc.project_name[0] == '\0' || appDesc.version[0] == '\0') {
        errorOut = u8"更新檔缺少版本資訊";
        return false;
    }
    if (strcmp(appDesc.project_name, currentDesc->project_name) != 0) {
        errorOut = u8"更新檔不是 HermesX 主 app image";
        return false;
    }

    versionOut = appDesc.version;
    projectNameOut = appDesc.project_name;
    return true;
#endif
}

void HermesXUpdateManager::resetCandidate()
{
    candidateValid = false;
    candidateSize = 0;
    candidateVersion = "";
    candidateProjectName = "";
    targetPartitionLabel = "";
    progressPercent = 0;
    bytesWritten = 0;
    streamWriteActive = false;
#if defined(ARCH_ESP32)
    targetPartition = nullptr;
#endif
}

void HermesXUpdateManager::finishWritingSuccess()
{
#if defined(ARCH_ESP32)
    if (updateFile) {
        updateFile.close();
    }
    const esp_err_t endResult = esp_ota_end(otaHandle);
    if (endResult != ESP_OK) {
        const char *errName = esp_err_to_name(endResult);
        LOG_WARN("HermesXUpdate: esp_ota_end failed err=0x%x (%s)", static_cast<unsigned>(endResult),
                 errName ? errName : "unknown");
        abortWriting(String("esp_ota_end failed: ") + (errName ? errName : "unknown"), true);
        return;
    }

    esp_app_desc_t desc;
    const esp_err_t descResult = targetPartition ? esp_ota_get_partition_description(targetPartition, &desc) : ESP_FAIL;
    if (!targetPartition || descResult != ESP_OK) {
        const char *errName = esp_err_to_name(descResult);
        LOG_WARN("HermesXUpdate: partition description failed err=0x%x (%s), continuing after esp_ota_end success",
                 static_cast<unsigned>(descResult), errName ? errName : "unknown");
    } else if (candidateVersion.length() > 0 && strcmp(desc.version, candidateVersion.c_str()) != 0) {
        LOG_WARN("HermesXUpdate: target version mismatch expected=%s actual=%s, continuing after esp_ota_end success",
                 candidateVersion.c_str(), desc.version);
    }

    otaHandle = 0;
    streamWriteActive = false;
    state = HermesXUpdateState::ReadyToApply;
    progressPercent = 100;
    sourceStatus = String(u8"寫入完成: ") + targetPartitionLabel;
    lastError = "";
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playAckSuccess();
    }
#endif
}

void HermesXUpdateManager::abortWriting(const String &reason, bool keepCandidate)
{
#if defined(ARCH_ESP32)
    if (otaHandle != 0) {
        esp_ota_abort(otaHandle);
        otaHandle = 0;
    }
    if (updateFile) {
        updateFile.close();
    }
#endif
    cancelRequested = false;
    streamWriteActive = false;
    progressPercent = 0;
    if (!keepCandidate) {
        resetCandidate();
        state = HermesXUpdateState::Failed;
    } else {
        state = HermesXUpdateState::AwaitingImage;
    }
    sourceStatus = String(u8"更新中止: ") + reason;
    lastError = truncateMessage(reason);
    if (HermesXInterfaceModule::instance) {
        HermesXInterfaceModule::instance->playNackFail();
    }
}

void HermesXUpdateManager::clearPendingApply()
{
#ifdef FSCom
    if (FSCom.exists(pendingStatePath)) {
        FSCom.remove(pendingStatePath);
    }
#endif
    pendingApply = false;
}

bool HermesXUpdateManager::savePendingApply()
{
#ifndef FSCom
    return false;
#else
    ensureUpdateDirs();
    PendingApplyState stateData{};
    strlcpy(stateData.targetVersion, candidateVersion.c_str(), sizeof(stateData.targetVersion));
    strlcpy(stateData.targetPartition, targetPartitionLabel.c_str(), sizeof(stateData.targetPartition));
    if (FSCom.exists(pendingStatePath)) {
        FSCom.remove(pendingStatePath);
    }
    File file = FSCom.open(pendingStatePath, FILE_O_WRITE);
    if (!file) {
        return false;
    }
    const size_t written = file.write(reinterpret_cast<const uint8_t *>(&stateData), sizeof(stateData));
    file.flush();
    file.close();
    pendingApply = (written == sizeof(stateData));
    return pendingApply;
#endif
}

void HermesXUpdateManager::handleBootResumeState()
{
#ifndef FSCom
    return;
#else
    if (!FSCom.exists(pendingStatePath)) {
        return;
    }

    File file = FSCom.open(pendingStatePath, FILE_O_READ);
    if (!file) {
        return;
    }

    PendingApplyState stateData{};
    const size_t read = file.read(reinterpret_cast<uint8_t *>(&stateData), sizeof(stateData));
    file.close();
    if (read != sizeof(stateData) || stateData.magic != pendingStateMagic || stateData.version != pendingStateVersion) {
        clearPendingApply();
        return;
    }

    pendingApply = true;
    const String targetVersion = stateData.targetVersion;
    if (targetVersion.length() > 0 && targetVersion == currentBuildVersion) {
        lastUpdatedVersion = targetVersion;
        sourceStatus = String(u8"已套用更新: ") + targetVersion;
        clearPendingApply();
    } else {
        lastError = u8"偵測到未完成套用狀態，已清除";
        sourceStatus = u8"待套用狀態已清除";
        clearPendingApply();
    }
#endif
}
