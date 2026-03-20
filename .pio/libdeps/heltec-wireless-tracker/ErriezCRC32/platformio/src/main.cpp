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
 *
 *      Verify CRC32 algorithm with online calculators, such as:
 *         https://crccalc.com
 *         http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
 */

#include <Arduino.h>
#include <ErriezCRC32.h>

// Function prototypes
void testString();
void testCharArray();
void testSingleBuffer();
void testMultipleStrings();
void testMultipleBuffers();
void checkCRC(uint32_t crcCalculated, uint32_t crcExpected);
void printCRC(uint32_t crc);


void setup()
{
    // Initialize serial port
    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    Serial.println(F("ErriezCRC32 example\n"));

    // Run tests
    testString();
    testCharArray();
    testSingleBuffer();
    testMultipleStrings();
    testMultipleBuffers();
}

void loop()
{
    // Empty
}

void testString()
{
    uint32_t crc;

    // Calculate CRC String
    String msg = "Hello world String!";
    crc = crc32String(msg.c_str());
    Serial.print(msg);
    Serial.print(F(": "));
    checkCRC(crc, 0x55DF869BUL);
}

void testCharArray()
{
    char msg[] = "Hello world char array!";
    uint32_t crc;

    // Calculate CRC null-terminated character array
    crc = crc32String(msg);
    Serial.print(msg);
    Serial.print(F(": "));
    checkCRC(crc, 0x95119850UL);
}

void testSingleBuffer()
{
    char msg[] = "Hello world single buffer!";
    uint32_t crc;

    // Calculate CRC single buffer
    crc = crc32Buffer(msg, strlen(msg));
    Serial.print(msg);
    Serial.print(F(": "));
    checkCRC(crc, 0x9C3B392AUL);
}

void testMultipleStrings()
{
    uint32_t crc;

    // Calculate CRC multiple strings
    crc = crc32Update("Hello ", 6, CRC32_INITIAL);
    crc = crc32Update("world ", 6, crc);
    crc = crc32Update("multiple ", 9, crc);
    crc = crc32Update("buffers!", 8, crc);
    crc = crc32Final(crc);
    Serial.print(F("Hello world multiple buffers!: "));
    checkCRC(crc, 0x951E6550UL);
}

void testMultipleBuffers()
{
    const uint8_t testBuffer1[3] = { 0xEB, 0xE5, 0x51 };
    const uint8_t testBuffer2[5] = { 0x87, 0x7F, 0xB8, 0x18, 0x4E };
    uint32_t crc;

    // Calculate CRC multiple buffers
    crc = crc32Update(testBuffer1, sizeof(testBuffer1), CRC32_INITIAL);
    crc = crc32Update(testBuffer2, sizeof(testBuffer2), crc);
    crc = crc32Final(crc);
    Serial.print(F("buffer1 + buffer2: "));
    checkCRC(crc, 0xC57A3392UL);
}

void checkCRC(uint32_t crcCalculated, uint32_t crcExpected)
{
    printCRC(crcCalculated);
    Serial.print(F("..."));

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
        uint8_t c = (uint8_t)(crc >> (i * 8));
        if (c < 0x10) {
            Serial.print("0");
        }
        Serial.print(c, HEX);
    }
}
