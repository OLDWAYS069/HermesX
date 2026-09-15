#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t *)(p))
class String : public std::string {
public:
 using std::string::string;
 String()=default;
 String(const std::string &s):std::string(s){}
 String(char c):std::string(1,c){}
 String(int n):std::string(std::to_string(n)){}
 String(unsigned n):std::string(std::to_string(n)){}
};
inline uint32_t previewTime = 0;
inline uint32_t millis() { return previewTime; }
