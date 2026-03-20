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

#line 26 "CRC32UnitTest.ino"

#include <ArduinoUnit.h> // https://github.com/mmurdoch/arduinounit
#include <ErriezCRC32.h> // https://github.com/Erriez/ErriezCRC32

/*!
 * \brief ErriezCRC32 Arduino unittest
 * \details
 *     Verify CRC32 algorithm with online calculators, such as:
 *         https://crccalc.com
 *         http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
 */

typedef struct {
    size_t size;
    const uint8_t *data;
    uint32_t checksum;
} TestBuffer;

const uint8_t ONE_BYTE_ZERO[1] = { 0x00 };
const uint8_t ONE_BYTE_ONE[1] = { 0x01 };
const uint8_t ONE_BYTE_TWO[1] = { 0x02 };
const uint8_t ONE_BYTE_FF[1] = { 0xFF };

TestBuffer testBufferOneByte[] = {
    { 1, ONE_BYTE_ZERO, 0xD202EF8D },
    { 1, ONE_BYTE_ONE, 0xA505DF1B },
    { 1, ONE_BYTE_TWO, 0x3C0C8EA1 },
    { 1, ONE_BYTE_FF, 0xFF000000 }
};

const uint8_t TWO_BYTES_ZERO[2] = { 0x00, 0x00 };
const uint8_t TWO_BYTES_1[2] = { 0x00, 0x01 };
const uint8_t TWO_BYTES_2[2] = { 0x01, 0x00 };
const uint8_t TWO_BYTES_3[2] = { 0x11, 0x11 };
const uint8_t TWO_BYTES_4[2] = { 0x30, 0x00 };
const uint8_t TWO_BYTES_5[2] = { 0x00, 0x30 };
const uint8_t TWO_BYTES_6[2] = { 0x00, 0x80 };
const uint8_t TWO_BYTES_7[2] = { 0x80, 0x00 };
const uint8_t TWO_BYTES_8[2] = { 0x12, 0x34 };
const uint8_t TWO_BYTES_FF[2] = { 0xFF, 0xFF };

TestBuffer testBufferTwoBytes[] = {
    { 2, TWO_BYTES_ZERO, 0x41D912FF },
    { 2, TWO_BYTES_1, 0x36DE2269 },
    { 2, TWO_BYTES_2, 0x58C223BE },
    { 2, TWO_BYTES_3, 0x78B0111D },
    { 2, TWO_BYTES_4, 0x9E9F240C },
    { 2, TWO_BYTES_5, 0x67002253 },
    { 2, TWO_BYTES_6, 0xAC6191DF },
    { 2, TWO_BYTES_7, 0x7A5A8AB4 },
    { 2, TWO_BYTES_8, 0x18999699 },
    { 2, TWO_BYTES_FF, 0xFFFF0000 }
};

const uint8_t THREE_BYTES_ZERO[3] = { 0x00, 0x00, 0x00 };
const uint8_t THREE_BYTES_1[3] = { 0x00, 0x00, 0x01 };
const uint8_t THREE_BYTES_2[3] = { 0x00, 0x01, 0x00 };
const uint8_t THREE_BYTES_3[3] = { 0x01, 0x01, 0x01 };
const uint8_t THREE_BYTES_4[3] = { 0x7C, 0x70, 0x2B };
const uint8_t THREE_BYTES_5[3] = { 0x0F, 0x55, 0x31 };
const uint8_t THREE_BYTES_6[3] = { 0xFE, 0x77, 0x6C };
const uint8_t THREE_BYTES_7[3] = { 0xDA, 0x00, 0x43 };
const uint8_t THREE_BYTES_8[3] = { 0x13, 0x9D, 0xAA };
const uint8_t THREE_BYTES_FF[3] = { 0xFF, 0xFF, 0xFF };

TestBuffer testBufferThreeBytes[] = {
    { 3, THREE_BYTES_ZERO, 0xFF41D912 },
    { 3, THREE_BYTES_1, 0x8846E984 },
    { 3, THREE_BYTES_2, 0xE65AE853 },
    { 3, THREE_BYTES_3, 0x909FB2F2 },
    { 3, THREE_BYTES_4, 0x2129C890 },
    { 3, THREE_BYTES_5, 0x620F3704 },
    { 3, THREE_BYTES_6, 0x6461458A },
    { 3, THREE_BYTES_7, 0x908A79DE },
    { 3, THREE_BYTES_8, 0x13CD7A9A },
    { 3, THREE_BYTES_FF, 0xFFFFFF00 }
};

const uint8_t FOUR_BYTES_ZERO[4] = { 0x00, 0x00, 0x00, 0x00 };
const uint8_t FOUR_BYTES_1[4] = { 0x22, 0x0C, 0x67, 0xFE };
const uint8_t FOUR_BYTES_2[4] = { 0x5C, 0xF0, 0x80, 0xE6 };
const uint8_t FOUR_BYTES_3[4] = { 0x01, 0x01, 0x01, 0xF1 };
const uint8_t FOUR_BYTES_4[4] = { 0x7C, 0x7F, 0x92, 0xD8 };
const uint8_t FOUR_BYTES_5[4] = { 0x0D, 0x14, 0x41, 0x5A };
const uint8_t FOUR_BYTES_6[4] = { 0xFE, 0x77, 0xED, 0x85 };
const uint8_t FOUR_BYTES_7[4] = { 0x81, 0x5A, 0x53, 0x20 };
const uint8_t FOUR_BYTES_8[4] = { 0xA2, 0xE2, 0xA9, 0x68 };
const uint8_t FOUR_BYTES_FF[4] = { 0xFF, 0xFF, 0xFF, 0xFF };

TestBuffer testBufferFourBytes[] = {
    { 4, FOUR_BYTES_ZERO, 0x2144DF1C },
    { 4, FOUR_BYTES_1, 0x52DC63B6 },
    { 4, FOUR_BYTES_2, 0x67B1C3D0 },
    { 4, FOUR_BYTES_3, 0x4B9B2185 },
    { 4, FOUR_BYTES_4, 0x947F5D2B },
    { 4, FOUR_BYTES_5, 0xAADDCAC3 },
    { 4, FOUR_BYTES_6, 0x60413A53 },
    { 4, FOUR_BYTES_7, 0xBF728B7B },
    { 4, FOUR_BYTES_8, 0x50ECE496 },
    { 4, FOUR_BYTES_FF, 0xFFFFFFFF }
};

const uint8_t FIVE_BYTES_ZERO[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t FIVE_BYTES_1[5] = { 0x00, 0x00, 0x00, 0x00, 0x01 };
const uint8_t FIVE_BYTES_2[5] = { 0x80, 0x00, 0x00, 0x00, 0x00 };
const uint8_t FIVE_BYTES_3[5] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
const uint8_t FIVE_BYTES_4[5] = { 0x44, 0x5B, 0x66, 0x80, 0x4B };
const uint8_t FIVE_BYTES_5[5] = { 0x03, 0xFA, 0xE4, 0x18, 0x30 };
const uint8_t FIVE_BYTES_6[5] = { 0xE3, 0xAA, 0x0D, 0x2E, 0x6C };
const uint8_t FIVE_BYTES_7[5] = { 0xE8, 0xE3, 0x94, 0x81, 0xD8 };
const uint8_t FIVE_BYTES_8[5] = { 0xEF, 0x79, 0x1A, 0x93, 0xBD };
const uint8_t FIVE_BYTES_FF[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

TestBuffer testBufferFiveBytes[] = {
    { 5, FIVE_BYTES_ZERO, 0xC622F71D },
    { 5, FIVE_BYTES_1, 0xB125C78B },
    { 5, FIVE_BYTES_2, 0x77C4478F },
    { 5, FIVE_BYTES_3, 0x470B99F4 },
    { 5, FIVE_BYTES_4, 0xE18A36F6 },
    { 5, FIVE_BYTES_5, 0x6270AB87 },
    { 5, FIVE_BYTES_6, 0xCC5B047F },
    { 5, FIVE_BYTES_7, 0x4AC67615 },
    { 5, FIVE_BYTES_8, 0x849C784B },
    { 5, FIVE_BYTES_FF, 0xD2FD1072 }
};

const uint8_t SIX_BYTES_ZERO[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t SIX_BYTES_1[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const uint8_t SIX_BYTES_2[6] = { 0xF9, 0xBB, 0xC5, 0x98, 0x12, 0x84 };
const uint8_t SIX_BYTES_FF[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

TestBuffer testBufferSixBytes[] = {
    { 6, SIX_BYTES_ZERO,  0xB1C2A1A3 },
    { 6, SIX_BYTES_1, 0xC6C59135 },
    { 6, SIX_BYTES_2, 0xC4FA10E1 },
    { 6, SIX_BYTES_FF, 0x41D9ED00 }
};

const uint8_t SEVEN_BYTES_ZERO[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t SEVEN_BYTES_1[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const uint8_t SEVEN_BYTES_2[7] = { 0xE1, 0x97, 0xF7, 0x5D, 0xA0, 0x50, 0x7D };
const uint8_t SEVEN_BYTES_FF[7] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

TestBuffer testBufferSevenBytes[] = {
    { 7, SEVEN_BYTES_ZERO, 0x9D6CDF7E },
    { 7, SEVEN_BYTES_1, 0xEA6BEFE8 },
    { 7, SEVEN_BYTES_2, 0xB6DC02A1 },
    { 7, SEVEN_BYTES_FF, 0xFF41D9ED }
};

const uint8_t EIGHT_BYTES_ZERO[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t EIGHT_BYTES_1[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const uint8_t EIGHT_BYTES_2[8] = { 0xF9, 0xBB, 0xC5, 0x98, 0x12, 0x36, 0x34, 0xA2 };
const uint8_t EIGHT_BYTES_FF[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

TestBuffer testBufferEightBytes[] = {
    { 8, EIGHT_BYTES_ZERO, 0x6522DF69 },
    { 8, EIGHT_BYTES_1, 0x1225EFFF },
    { 8, EIGHT_BYTES_2, 0x5E231F81 },
    { 8, EIGHT_BYTES_FF,  0x2144DF1C }
};

const uint8_t NINE_BYTES_ZERO[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t NINE_BYTES_1[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const uint8_t NINE_BYTES_2[9] = { 0x42, 0x15, 0xB9, 0x42, 0xBE, 0xAE, 0xE1, 0xFC, 0x74 };
const uint8_t NINE_BYTES_FF[9] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

TestBuffer testBufferNineBytes[] = {
    { 9, NINE_BYTES_ZERO, 0xE60914AE },
    { 9, NINE_BYTES_1, 0x910E2438 },
    { 9, NINE_BYTES_2, 0x9049D834 },
    { 9, NINE_BYTES_FF,  0xEB201890 }
};


typedef struct {
    const char *str;
    uint32_t checksum;
} TestString;

TestString testString[] {
    { "", 0 },
    { "a", 0xE8B7BE43 },
    { "aa", 0x078A19D7 },
    { "hello", 0x3610A686 },
    { "hello world!", 0x03B4C26D }
};

static void testSingleBuffers()
{
    uint32_t crc32;

    // Test 1 Byte buffer
    for (uint8_t i = 0; i < (sizeof(testBufferOneByte) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferOneByte[i].data, testBufferOneByte[i].size);
        assertEqual(crc32, testBufferOneByte[i].checksum);
    }

    // Test 2 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferTwoBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferTwoBytes[i].data, testBufferTwoBytes[i].size);
        assertEqual(crc32, testBufferTwoBytes[i].checksum);
    }

    // Test 3 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferThreeBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferThreeBytes[i].data, testBufferThreeBytes[i].size);
        assertEqual(crc32, testBufferThreeBytes[i].checksum);
    }

    // Test 4 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferFourBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferFourBytes[i].data, testBufferFourBytes[i].size);
        assertEqual(crc32, testBufferFourBytes[i].checksum);
    }

    // Test 5 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferFiveBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferFiveBytes[i].data, testBufferFiveBytes[i].size);
        assertEqual(crc32, testBufferFiveBytes[i].checksum);
    }

    // Test 6 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferSixBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferSixBytes[i].data, testBufferSixBytes[i].size);
        assertEqual(crc32, testBufferSixBytes[i].checksum);
    }

    // Test 7 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferSevenBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferSevenBytes[i].data, testBufferSevenBytes[i].size);
        assertEqual(crc32, testBufferSevenBytes[i].checksum);
    }
    
    // Test 8 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferEightBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferEightBytes[i].data, testBufferEightBytes[i].size);
        assertEqual(crc32, testBufferEightBytes[i].checksum);
    }

    // Test 9 Bytes buffer
    for (uint8_t i = 0; i < (sizeof(testBufferNineBytes) / sizeof(TestBuffer)); i++) {
        crc32 = crc32Buffer(testBufferNineBytes[i].data, testBufferNineBytes[i].size);
        assertEqual(crc32, testBufferNineBytes[i].checksum);
    }

    // Test 16 Bytes buffer
    const uint8_t testBuffer16[16] = { 
        0x01, 0x36, 0x1E, 0xF0, 0x1C, 0x8D, 0x31, 0x97, 
        0xA3, 0x07, 0x81, 0x63, 0x0F, 0x7C, 0x84, 0xBA 
    };
    crc32 = crc32Buffer(testBuffer16, sizeof(testBuffer16));
    assertEqual(crc32, 0x1C531332);

    // Test 31 Bytes buffer
    const uint8_t testBuffer31[31] = { 
        0x44, 0x03, 0x61, 0x0F, 0x39, 0xDB, 0x20, 0x8C, 
        0xE9, 0xB3, 0x7B, 0x92, 0xF5, 0x20, 0x50, 0xFB, 
        0x05, 0x26, 0x18, 0xD2, 0x5C, 0x01, 0x73, 0x6E,
        0x8A, 0xA2, 0x6B, 0x39, 0x8B, 0x25, 0x51
    };
    crc32 = crc32Buffer(testBuffer31, sizeof(testBuffer31));
    assertEqual(crc32, 0x6EF4772E);

    // Test 256 Bytes buffer
    const uint8_t testBuffer256[256] = { 
        0x03, 0x6D, 0x7F, 0x7D, 0x7E, 0xDD, 0xD4, 0xCC, 
        0xFA, 0x67, 0x30, 0x37, 0xCC, 0x5D, 0x26, 0x56, 
        0x52, 0x4F, 0xF8, 0x3C, 0x93, 0x5E, 0x06, 0x4F,
        0xE6, 0xB3, 0x25, 0xA5, 0x2F, 0xE3, 0x86, 0x41, 
        0x83, 0x3D, 0x1C, 0xC8, 0x2F, 0x36, 0xB8, 0x99, 
        0x7E, 0x75, 0x27, 0x4B, 0xC7, 0x38, 0x2E, 0x08,
        0x87, 0x13, 0x19, 0x4C, 0x9A, 0x3C, 0x40, 0xF1, 
        0xDF, 0xE8, 0xBE, 0x9C, 0xCD, 0x72, 0x28, 0xEB, 
        0xD1, 0x4A, 0x1E, 0x01, 0x6E, 0xE1, 0xE8, 0x4B,
        0x83, 0x1C, 0x8C, 0x93, 0x28, 0xF5, 0x06, 0xCB, 
        0x0E, 0x61, 0x8C, 0x20, 0x48, 0x9A, 0x16, 0x35, 
        0xF1, 0x60, 0x80, 0xA5, 0x15, 0xE5, 0x10, 0x22,
        0xCC, 0x55, 0x2B, 0xD3, 0x74, 0x52, 0x80, 0xCA, 
        0x6E, 0xB8, 0x04, 0xAC, 0xDC, 0x90, 0x77, 0xE9, 
        0x31, 0x0C, 0x54, 0x24, 0x9F, 0xDB, 0x5C, 0xA3,
        0xAB, 0x48, 0x2B, 0xDD, 0x7D, 0xE7, 0x81, 0x90, 
        0xA8, 0xC6, 0x22, 0xED, 0x89, 0x5E, 0x8E, 0xC3, 
        0x79, 0xC8, 0xE7, 0xB5, 0x62, 0xE7, 0xBC, 0xEC,
        0x90, 0x8C, 0xA6, 0xC9, 0xB3, 0xC8, 0xDB, 0x89,
        0x72, 0xF0, 0x33, 0xE8, 0x0E, 0x0C, 0x2F, 0x24, 
        0xEE, 0xA4, 0x12, 0x02, 0x0E, 0x3F, 0x17, 0xA3,
        0xB1, 0xEE, 0x45, 0x52, 0xCE, 0xBE, 0x86, 0xA6,
        0x1C, 0xFD, 0xED, 0x84, 0xAE, 0x8D, 0x52, 0xF9, 
        0x7B, 0xCE, 0xBF, 0xEA, 0x67, 0xC7, 0x46, 0x11,
        0x7A, 0x9F, 0x40, 0xC9, 0x75, 0x94, 0x10, 0x9F, 
        0xF7, 0xF1, 0xDE, 0xAC, 0xC8, 0xBC, 0xF9, 0x2B, 
        0x1D, 0x1E, 0xC8, 0xCE, 0xCB, 0xBB, 0x71, 0xC7,
        0xD4, 0x79, 0xAA, 0x8E, 0xB9, 0x72, 0x63, 0xCD, 
        0x7D, 0x07, 0x25, 0xFD, 0x45, 0x5F, 0x65, 0xAE, 
        0x01, 0xBC, 0x28, 0x6E, 0x8C, 0x6B, 0xAD, 0xD3,
        0x2B, 0x52, 0x0A, 0x24, 0x5C, 0x45, 0xDC, 0x8D, 
        0x5A, 0xAF, 0x85, 0x02, 0xCB, 0x4A, 0x99, 0x11 
    };
    crc32 = crc32Buffer(testBuffer256, sizeof(testBuffer256));
    assertEqual(crc32,  0xC52F3441);
}

static void testMultipleBuffers()
{
    uint32_t crc32;

    const uint8_t testVal1 = 0x37;
    const uint8_t testVal2 = 0x2F;
    crc32 = crc32Update(&testVal1, sizeof(testVal1), CRC32_INITIAL);
    crc32 = crc32Update(&testVal2, sizeof(testVal2), crc32);
    crc32 = crc32Final(crc32);
    assertEqual(crc32, 0x7A0F8F92);

    const uint8_t testBuffer3[3] = { 0xEB, 0xE5, 0x51 };
    const uint8_t testBuffer5[5] = { 0x87, 0x7F, 0xB8, 0x18, 0x4E };
    crc32 = crc32Update(testBuffer3, sizeof(testBuffer3), CRC32_INITIAL);
    crc32 = crc32Update(testBuffer5, sizeof(testBuffer5), crc32);
    crc32 = crc32Final(crc32);
    assertEqual(crc32, 0xC57A3392);
}

static void testStrings()
{
    uint32_t crc32;
    
    // Test strings
    for (uint8_t i = 0; i < (sizeof(testString) / sizeof(TestString)); i++) {
        crc32 = crc32String(testString[i].str);
        assertEqual(crc32, testString[i].checksum);
    }

    // Test longer string
    assertEqual(crc32String("thequickbrownfoxjumpsoverthelazydog"), 0x604A1A5F);
}

test(correct)
{
    Serial.println(F("Starting test correct..."));
    
    Serial.println(F("  Single buffers..."));
    testSingleBuffers();

    Serial.println(F("  Multiple buffers..."));
    testMultipleBuffers();

    Serial.println(F("  Strings..."));
    testStrings();
}

test(incorrect)
{
    Serial.println(F("Starting test incorrect..."));
    assertNotEqual(crc32Buffer(testBufferOneByte[0].data, testBufferOneByte[0].size), 0);
}

void setup()
{
    Serial.begin(115200);
    while(!Serial) {
        ;
    }
    Serial.println(F("ErriezCRC32 unittest"));
}

void loop()
{
    Test::run();
}