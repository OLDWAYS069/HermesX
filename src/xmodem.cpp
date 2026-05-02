/**
 * @file xmodem.cpp
 * @brief Implementation of XMODEM protocol for Meshtastic devices.
 *
 * This file contains the implementation of the XMODEM protocol for Meshtastic devices. It is based on the XMODEM implementation
 * by Georges Menie (www.menie.org) and has been adapted for protobuf encapsulation.
 *
 * The XMODEM protocol is used for reliable transmission of binary data over a serial connection. This implementation supports
 * both sending and receiving of data.
 *
 * The XModemAdapter class provides the main functionality for the protocol, including CRC calculation, packet handling, and
 * control signal sending.
 *
 * @copyright Copyright (c) 2001-2019 Georges Menie
 * @author
 * @author
 * @date
 */
/***********************************************************************************************************************
 * based on XMODEM implementation by Georges Menie (www.menie.org)
 ***********************************************************************************************************************
 * Copyright 2001-2019 Georges Menie (www.menie.org)
 * All rights reserved.
 *
 * Adapted for protobuf encapsulation. this is not really Xmodem any more.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **********************************************************************************************************************/

#include "xmodem.h"
#include "SPILock.h"
#include "modules/HermesXUpdateManager.h"
#include "platform/esp32/HermesCrashBreadcrumb.h"

#ifdef FSCom

XModemAdapter xModem;

XModemAdapter::XModemAdapter() {}

namespace
{
constexpr const char *kHermesXUpdateFilePath = "/update/firmware.bin";
}

/**
 * Calculates the CRC-16 CCITT checksum of the given buffer.
 *
 * @param buffer The buffer to calculate the checksum for.
 * @param length The length of the buffer.
 * @return The calculated checksum.
 */
unsigned short XModemAdapter::crc16_ccitt(const pb_byte_t *buffer, int length)
{
    unsigned short crc16 = 0;
    while (length != 0) {
        crc16 = (unsigned char)(crc16 >> 8) | (crc16 << 8);
        crc16 ^= *buffer;
        crc16 ^= (unsigned char)(crc16 & 0xff) >> 4;
        crc16 ^= (crc16 << 8) << 4;
        crc16 ^= ((crc16 & 0xff) << 4) << 1;
        buffer++;
        length--;
    }

    return crc16;
}

/**
 * Calculates the checksum of the given buffer and compares it to the given
 * expected checksum. Returns 1 if the checksums match, 0 otherwise.
 *
 * @param buf The buffer to calculate the checksum of.
 * @param sz The size of the buffer.
 * @param tcrc The expected checksum.
 * @return 1 if the checksums match, 0 otherwise.
 */
int XModemAdapter::check(const pb_byte_t *buf, int sz, unsigned short tcrc)
{
    return crc16_ccitt(buf, sz) == tcrc;
}

void XModemAdapter::sendControl(meshtastic_XModem_Control c)
{
    xmodemStore = meshtastic_XModem_init_zero;
    xmodemStore.control = c;
    LOG_DEBUG("XModem: Notify Send control %d", c);
    packetReady.notifyObservers(packetno);
}

meshtastic_XModem XModemAdapter::getForPhone()
{
    return xmodemStore;
}

void XModemAdapter::resetForPhone()
{
    xmodemStore = meshtastic_XModem_init_zero;
}

void XModemAdapter::setDiagnosticHook(DiagnosticHook hook)
{
    diagnosticHook = std::move(hook);
}

void XModemAdapter::emitDiagnostic(const char *message)
{
    if (diagnosticHook) {
        diagnosticHook(message);
    }
}

void XModemAdapter::handlePacket(meshtastic_XModem xmodemPacket)
{
    hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemPacketRx, xmodemPacket.control);
    switch (xmodemPacket.control) {
    case meshtastic_XModem_Control_SOH:
    case meshtastic_XModem_Control_STX:
        if ((xmodemPacket.seq == 0) && !isReceiving && !isTransmitting) {
            emitDiagnostic("Xmodem start packet seq0");
            // NULL packet has the destination filename
            memset(filename, 0, sizeof(filename));
            size_t filenameLen = xmodemPacket.buffer.size;
            if (filenameLen > sizeof(filename) - 1) {
                filenameLen = sizeof(filename) - 1;
            }
            memcpy(filename, &xmodemPacket.buffer.bytes, filenameLen);

            expectedBytes = 0;
            receivedBytes = 0;
            isUpdateFirmwareReceive = false;
            isUpdateFirmwareStream = false;
            String versionFromFilename;

            char *separator = strchr(filename, '|');
            if (separator) {
                *separator = '\0';
                char *filenameHint = strchr(separator + 1, '|');
                if (filenameHint) {
                    *filenameHint = '\0';
                    ++filenameHint;
                    String displayFromFilename;
                    HermesXUpdateManager::parseHermesVersionFromFilename(String(filenameHint), versionFromFilename,
                                                                        &displayFromFilename);
                }
                expectedBytes = strtoul(separator + 1, nullptr, 10);
            }
            isUpdateFirmwareReceive = strcmp(filename, kHermesXUpdateFilePath) == 0;
            hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemStartMeta,
                                        isUpdateFirmwareReceive ? 1U : 0U);
            emitDiagnostic(isUpdateFirmwareReceive ? "Xmodem target update file" : "Xmodem target other file");

            if (xmodemPacket.control == meshtastic_XModem_Control_SOH) { // Receive this file and put to Flash
                hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemOpenWrite,
                                            static_cast<uint16_t>(expectedBytes & 0xFFFFu));
                emitDiagnostic("Xmodem about to open write");
                if (isUpdateFirmwareReceive) {
                    const String streamVersion = versionFromFilename.length() > 0 ? versionFromFilename : String("USB");
                    if (HermesXUpdateManager::instance().beginStreamUpdate(expectedBytes, streamVersion, "HermesX")) {
                        hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemOpenWriteOk,
                                                    static_cast<uint16_t>(expectedBytes & 0xFFFFu));
                        emitDiagnostic("Xmodem ota stream begin ok");
                        isUpdateFirmwareStream = true;
                        sendControl(meshtastic_XModem_Control_ACK);
                        emitDiagnostic("Xmodem sent start ACK");
                        isReceiving = true;
                        packetno = 1;
                        break;
                    }
                    hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemOpenWriteFail,
                                                static_cast<uint16_t>(expectedBytes & 0xFFFFu));
                    emitDiagnostic("Xmodem ota stream begin fail");
                    sendControl(meshtastic_XModem_Control_NAK);
                    isReceiving = false;
                    break;
                }

                spiLock->lock();
                file = FSCom.open(filename, FILE_O_WRITE);
                spiLock->unlock();
                if (file) {
                    hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemOpenWriteOk,
                                                static_cast<uint16_t>(expectedBytes & 0xFFFFu));
                    emitDiagnostic("Xmodem open write ok");
                    sendControl(meshtastic_XModem_Control_ACK);
                    emitDiagnostic("Xmodem sent start ACK");
                    isReceiving = true;
                    packetno = 1;
                    break;
                }
                hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId::XmodemOpenWriteFail,
                                            static_cast<uint16_t>(expectedBytes & 0xFFFFu));
                emitDiagnostic("Xmodem open write fail");
                sendControl(meshtastic_XModem_Control_NAK);
                isReceiving = false;
                break;
            } else { // Transmit this file from Flash
                emitDiagnostic("Xmodem transmit path");
                LOG_INFO("XModem: Transmit file %s", filename);
                spiLock->lock();
                file = FSCom.open(filename, FILE_O_READ);
                spiLock->unlock();
                if (file) {
                    emitDiagnostic("Xmodem open read ok");
                    packetno = 1;
                    isTransmitting = true;
                    xmodemStore = meshtastic_XModem_init_zero;
                    xmodemStore.control = meshtastic_XModem_Control_SOH;
                    xmodemStore.seq = packetno;
                    spiLock->lock();
                    xmodemStore.buffer.size = file.read(xmodemStore.buffer.bytes, sizeof(meshtastic_XModem_buffer_t::bytes));
                    spiLock->unlock();
                    xmodemStore.crc16 = crc16_ccitt(xmodemStore.buffer.bytes, xmodemStore.buffer.size);
                    LOG_DEBUG("XModem: STX Notify Send packet %d, %d Bytes", packetno, xmodemStore.buffer.size);
                    if (xmodemStore.buffer.size < sizeof(meshtastic_XModem_buffer_t::bytes)) {
                        isEOT = true;
                        // send EOT on next Ack
                    }
                    packetReady.notifyObservers(packetno);
                    break;
                }
                emitDiagnostic("Xmodem open read fail");
                sendControl(meshtastic_XModem_Control_NAK);
                isTransmitting = false;
                break;
            }
        } else {
            if (isReceiving) {
                // normal file data packet
                if ((xmodemPacket.seq == packetno) &&
                    check(xmodemPacket.buffer.bytes, xmodemPacket.buffer.size, xmodemPacket.crc16)) {
                    // valid packet
                    if (isUpdateFirmwareStream) {
                        if (!HermesXUpdateManager::instance().writeStreamChunk(xmodemPacket.buffer.bytes,
                                                                               xmodemPacket.buffer.size)) {
                            sendControl(meshtastic_XModem_Control_CAN);
                            isReceiving = false;
                            break;
                        }
                    } else {
                        spiLock->lock();
                        file.write(xmodemPacket.buffer.bytes, xmodemPacket.buffer.size);
                        spiLock->unlock();
                    }
                    receivedBytes += xmodemPacket.buffer.size;
                    if (isUpdateFirmwareReceive && !isUpdateFirmwareStream) {
                        HermesXUpdateManager::instance().setIncomingTransferProgress(receivedBytes, expectedBytes);
                    }
                    sendControl(meshtastic_XModem_Control_ACK);
                    packetno++;
                    break;
                }
                // invalid packet
                sendControl(meshtastic_XModem_Control_NAK);
                emitDiagnostic("Xmodem data NAK");
                break;
            } else if (isTransmitting) {
                // just received something weird.
                emitDiagnostic("Xmodem unexpected packet while transmitting");
                sendControl(meshtastic_XModem_Control_CAN);
                isTransmitting = false;
                break;
            }
        }
        break;
    case meshtastic_XModem_Control_EOT:
        // End of transmission
        emitDiagnostic("Xmodem EOT");
        sendControl(meshtastic_XModem_Control_ACK);
        if (isUpdateFirmwareStream) {
            HermesXUpdateManager::instance().finishStreamUpdate();
        } else {
            spiLock->lock();
            file.flush();
            file.close();
            spiLock->unlock();
        }
        isReceiving = false;
        if (isUpdateFirmwareReceive && !isUpdateFirmwareStream) {
            auto &updateManager = HermesXUpdateManager::instance();
            if (updateManager.refreshImage()) {
                if (!updateManager.startUpdate()) {
                    LOG_WARN("XModem: update file received but OTA start failed: %s", updateManager.getLastError().c_str());
                }
            } else {
                LOG_WARN("XModem: update file received but refresh failed: %s", updateManager.getLastError().c_str());
            }
        }
        break;
    case meshtastic_XModem_Control_CAN:
        // Cancel transmission and remove file
        emitDiagnostic("Xmodem CAN");
        sendControl(meshtastic_XModem_Control_ACK);
        if (isUpdateFirmwareStream) {
            HermesXUpdateManager::instance().failStreamUpdate(u8"USB 更新已取消", false);
        } else {
            spiLock->lock();
            file.flush();
            file.close();
            FSCom.remove(filename);
            spiLock->unlock();
        }
        isReceiving = false;
        if (isUpdateFirmwareReceive && !isUpdateFirmwareStream) {
            HermesXUpdateManager::instance().failStreamUpdate(u8"USB 更新已取消", false);
        }
        break;
    case meshtastic_XModem_Control_ACK:
        // Acknowledge Send the next packet
        if (isTransmitting) {
            if (isEOT) {
                sendControl(meshtastic_XModem_Control_EOT);
                spiLock->lock();
                file.close();
                spiLock->unlock();
                LOG_INFO("XModem: Finished send file %s", filename);
                isTransmitting = false;
                isEOT = false;
                break;
            }
            retrans = MAXRETRANS; // reset retransmit counter
            packetno++;
            xmodemStore = meshtastic_XModem_init_zero;
            xmodemStore.control = meshtastic_XModem_Control_SOH;
            xmodemStore.seq = packetno;
            spiLock->lock();
            xmodemStore.buffer.size = file.read(xmodemStore.buffer.bytes, sizeof(meshtastic_XModem_buffer_t::bytes));
            spiLock->unlock();
            xmodemStore.crc16 = crc16_ccitt(xmodemStore.buffer.bytes, xmodemStore.buffer.size);
            LOG_DEBUG("XModem: ACK Notify Send packet %d, %d Bytes", packetno, xmodemStore.buffer.size);
            if (xmodemStore.buffer.size < sizeof(meshtastic_XModem_buffer_t::bytes)) {
                isEOT = true;
                // send EOT on next Ack
            }
            packetReady.notifyObservers(packetno);
        } else {
            // just received something weird.
            sendControl(meshtastic_XModem_Control_CAN);
        }
        break;
    case meshtastic_XModem_Control_NAK:
        // Negative acknowledge. Send the same buffer again
        if (isTransmitting) {
            if (--retrans <= 0) {
                sendControl(meshtastic_XModem_Control_CAN);
                spiLock->lock();
                file.close();
                spiLock->unlock();
                LOG_INFO("XModem: Retransmit timeout, cancel file %s", filename);
                isTransmitting = false;
                break;
            }
            xmodemStore = meshtastic_XModem_init_zero;
            xmodemStore.control = meshtastic_XModem_Control_SOH;
            xmodemStore.seq = packetno;
            spiLock->lock();
            file.seek((packetno - 1) * sizeof(meshtastic_XModem_buffer_t::bytes));

            xmodemStore.buffer.size = file.read(xmodemStore.buffer.bytes, sizeof(meshtastic_XModem_buffer_t::bytes));
            spiLock->unlock();
            xmodemStore.crc16 = crc16_ccitt(xmodemStore.buffer.bytes, xmodemStore.buffer.size);
            LOG_DEBUG("XModem: NAK Notify Send packet %d, %d Bytes", packetno, xmodemStore.buffer.size);
            if (xmodemStore.buffer.size < sizeof(meshtastic_XModem_buffer_t::bytes)) {
                isEOT = true;
                // send EOT on next Ack
            }
            packetReady.notifyObservers(packetno);
        } else {
            // just received something weird.
            sendControl(meshtastic_XModem_Control_CAN);
        }
        break;
    default:
        // Unknown control character
        emitDiagnostic("Xmodem default handler");
        break;
    }
}
#endif
