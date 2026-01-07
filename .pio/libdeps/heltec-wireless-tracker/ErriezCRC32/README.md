# Optimized CRC32 library for Arduino
[![Build Status](https://travis-ci.org/Erriez/ErriezCRC32.svg?branch=master)](https://travis-ci.org/Erriez/ErriezCRC32)

This is a target independent, flash and RAM size optimized CRC32 library for Arduino without CRC tables.

![CRC32](https://raw.githubusercontent.com/Erriez/ErriezCRC32/master/extras/CRC32.png)


## Library features

- Compatible with standard CRC32 algorithms
- Small flash footprint
- No RAM buffer allocations
- 32-bit table-less CRC32 calculation on:
  - Arduino String object
  - Single null-terminated character array
  - Single buffer
  - Multiple null-terminated character arrays
  - Multiple buffers
- No buffer alignment needed
- Buffer input length in Bytes
- Big and little endian input buffers
- Arduino C library, compatible with C and C++ applications
- CRC32 polynomial: 0xEDB88320


## Hardware

This library can be used on any 8 or 32-bit target and is optimized for small flash/RAM footprints
without CRC table allocations in RAM.


## Examples

Arduino IDE | Examples | Erriez CRC32:

* [CRC32](https://github.com/Erriez/ErriezCRC32/blob/master/examples/CRC32/CRC32.ino)
* tests | [CRC32UnitTest](https://github.com/Erriez/ErriezCRC32/blob/master/tests/CRC32UnitTest/CRC32UnitTest.ino)


## Benchmarks

Arduino IDE | Examples | Erriez CRC32:

* tests | [CRC32Benchmark](https://github.com/Erriez/ErriezCRC32/blob/master/tests/CRC32Benchmark/CRC32Benchmark.ino)


#### Benchmark results on buffers

The benchmark results below may be different for each target and run. Lower duration means faster CRC calculation.

| Board                       | F_CPU  | 4 Bytes | 64 Bytes | 512 Bytes | 1024 Bytes |
| --------------------------- | :----: | :-----: | :------: | :-------: | :--------: |
| Arduino UNO (ATMega328)     | 16MHz  |  80us   |  1172us  |  9000us   |  17112us   |
| ESP8266 (WeMOS D1 R2)       | 160MHz |   3us   |   35us   |   279us   |   557us    |
| ESP32 (WeMOS Lolin32)       | 80MHz  |   2us   |   15us   |   111us   |   223us    |
| DUE (SAM3X8E ARM Cortex-M3) | 84MHz  |   6us   |   70us   |   547us   |   1103us   |

Benchmarks performed with the following versions:

* Arduino IDE v1.8.5
* Arduino AVR Boards v1.6.21 (Arduino UNO)
* [esp8266](http://arduino.esp8266.com/stable/package_esp8266com_index.json) v2.4.2
* Arduino SAM Boards (32-bits ARM Cortex-M3) v1.6.11

## Unit tests

This library contains extensive unit tests on a various number of buffer types and lengths:

* tests | [CRC32UnitTest](https://github.com/Erriez/ErriezCRC32/blob/master/tests/CRC32UnitTest/CRC32UnitTest.ino)


## Library documentation

* [Doxygen online HTML](https://erriez.github.io/ErriezCRC32)
* [Doxygen PDF](https://github.com/Erriez/ErriezCRC32/raw/gh-pages/latex/ErriezCRC32.pdf)


## Usage

**Introduction**

The library consists of four C-functions for CRC calculation:

```c++
// Calculate CRC32 on null-terminated character array (string)
uint32_t crc32String(const char *buffer);
// Calculate CRC32 on one single buffer
uint32_t crc32Buffer(const void *buffer, size_t bufferLength);

// Calculate CRC32 on multiple buffers
// First crc32Update() call should start with argument crc = CRC32_INITIAL
// Next crc32Update() calls should set argument crc = previousCRC
uint32_t crc32Update(const void *buffer, size_t bufferLength, uint32_t crc);
// Call crc32Update() at the end after the last crc32Update()
uint32_t crc32Final(uint32_t crc);
```

**Initialization**

Only an include file is required. No object allocation or library initialization required.

```c++
#include <ErriezCRC32.h>
```

**Calculate CRC32 on Arduino String**

```c++
String msg = "Hello world String!";
uint32_t crc = crc32String(msg.c_str());
```

**Calculate CRC32 on null-terminated character array**

```c++
char msg[] = "Hello world char array!";
uint32_t crc = crc32String(msg);
```

**Calculate CRC32 on single character buffer**

```c++
char msg[] = "Hello world single buffer!";
uint32_t crc = crc32Buffer(msg, strlen(msg));
```

**Calculate CRC32 on multiple characters**

```c++
uint32_t crc;

crc = crc32Update("Hello ", 6, CRC32_INITIAL);
crc = crc32Update("world ", 6, crc);
crc = crc32Update("multiple ", 9, crc);
crc = crc32Update("buffers!", 8, crc);
crc = crc32Final(crc);
```

**Calculate CRC32 on multiple buffers**

```c++
const uint8_t testBuffer1[3] = { 0xEB, 0xE5, 0x51 };
const uint8_t testBuffer2[5] = { 0x87, 0x7F, 0xB8, 0x18, 0x4E };
uint32_t crc;

crc = crc32Update(testBuffer1, sizeof(testBuffer1), CRC32_INITIAL);
crc = crc32Update(testBuffer2, sizeof(testBuffer2), crc);
crc = crc32Final(crc);
```

**Check CRC**

```c++
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
```

**Print 32-bit CRC value**

Printing a ```uint32_t``` value is not implemented on some targets which is a limitation of ```Serial.print()``` or ```sprintf()```. 
This is a workaround to print a 32-bit hex value:

```c++
void printCRC(uint32_t crc)
{
	Serial.print("0x");
    for (int8_t i = 3; i >= 0; i--) {
        uint8_t c = crc >> (i * 8);
        if (c < 0x10) {
            Serial.print("0");
        }
        Serial.print(c, HEX);
    }
}
```

**CRC-calculation with Python**

```python
>>> import binascii
>>> hex(binascii.crc32(b'Hello world String!') & 0xffffffff)
'0x55df869b'
```



## Library dependencies

* CRC32Benchmark.ino: [ErriezTimestamp.h](https://github.com/Erriez/ErriezTimestamp.git)
* CRC32UnitTest.ino: [ArduinoUnit.h](https://github.com/mmurdoch/arduinounit)


## Library installation

Please refer to the [Wiki](https://github.com/Erriez/ErriezArduinoLibrariesAndSketches/wiki) page.


## Other Arduino Libraries and Sketches from Erriez

* [Erriez Libraries and Sketches](https://github.com/Erriez/ErriezArduinoLibrariesAndSketches)
