/*
 * MIT License
 *
 * Copyright (c) 2018 Erriez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*!
 * \brief CRC32 example for Arduino
 * \details
 *      Source:         https://github.com/Erriez/ErriezCRC32
 *      Documentation:  https://erriez.github.io/ErriezCRC32
 */

#include <ErriezCRC32.h>
#include <ErriezTimestamp.h> // https://github.com/Erriez/ErriezTimestamp.git

// Create timestamp with microseconds resolution
TimestampMicros timestamp;

// Function prototypes
void benchmarkCharArray();
void benchmarkBuffer(uint16_t bufferLength, uint32_t expectedCRC);
void checkCRC(uint32_t crcCalculated, uint32_t crcExpected);
void printCRC32(uint32_t crc);


void setup()
{
    Serial.begin(115200);
    while(!Serial) {
        ;
    }
    Serial.println(F("ErriezCRC32 benchmark"));

    Serial.println(F("\nBenchmark char arrays:"));
    benchmarkCharArray("0", 0xF4DBDF21UL);
    benchmarkCharArray("1234", 0x9BE3E0A3UL);
    benchmarkCharArray(
            "01234567890123456789012345678901234567890123456789012345678901234567890123456789",
            0x4D684BB6UL
    );

    Serial.println(F("\nBenchmark buffers:"));
    benchmarkBuffer(1, 0xD202EF8DUL);
    benchmarkBuffer(4, 0x8BB98613UL);
    benchmarkBuffer(64, 0x100ECE8CUL);
    benchmarkBuffer(512, 0x1C613576UL);
    benchmarkBuffer(1024, 0xB70B4C26UL);
}

void loop()
{
    // Empty
}

void benchmarkCharArray(const char *msg, uint32_t expectedCRC)
{
    uint32_t crc;

    // Print array length
    Serial.print(F("  "));
    Serial.print(strlen(msg));
    Serial.print(F(" Bytes: "));

    // Calculate CRC null-terminated character array
    timestamp.start();
    crc = crc32String(msg);
    Serial.print(timestamp.delta());
    Serial.print(F("us "));

    // Check calculated CRC
    checkCRC(crc, expectedCRC);
}

void benchmarkBuffer(uint16_t bufferLength, uint32_t expectedCRC)
{
    uint8_t buffer[1024];
    uint32_t crc;

    // Check buffer length
    if (bufferLength > sizeof(buffer)) {
        bufferLength = sizeof(buffer);
    }

    // Fill buffer
    for (uint16_t i = 0; i < bufferLength; i++) {
        buffer[i] = i;
    }

    // Print buffer length
    Serial.print(F("  "));
    Serial.print(bufferLength);
    Serial.print(F(" Bytes: "));

    // Calculate CRC buffer
    timestamp.start();
    crc = crc32Buffer(buffer, bufferLength);
    Serial.print(timestamp.delta());
    Serial.print(F("us "));

    // Check calculated CRC
    checkCRC(crc, expectedCRC);
}

void checkCRC(uint32_t crcCalculated, uint32_t crcExpected)
{
    // Print calculated CRC
    printCRC(crcCalculated);
    Serial.print(F("..."));

    // Verify CRC
    if (crcCalculated == crcExpected) {
        Serial.println(F("OK"));
    } else {
        Serial.print(F("FAILED! Expected: "));
        printCRC(crcExpected);
        Serial.println(F(""));
    }
}

void printCRC(uint32_t crc)
{
    // Some targets do not support 32-bit Serial.print or sprintf, so print in a for loop as
    // workaround
    Serial.print("0x");
    for (int8_t i = 3; i >= 0; i--) {
        uint8_t c = crc >> (i * 8);
        if (c < 0x10) {
            Serial.print("0");
        }
        Serial.print(c, HEX);
    }
}