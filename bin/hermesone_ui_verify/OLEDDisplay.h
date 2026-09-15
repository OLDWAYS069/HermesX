#pragma once
#include "Arduino.h"
#include <array>
#include <fstream>
#include <cassert>
#include "OLEDDisplayFonts.h"
enum OLEDDISPLAY_COLOR { BLACK, WHITE };
enum OLEDDISPLAY_TEXT_ALIGNMENT {TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER};
class OLEDDisplay {
public:
 std::array<uint8_t,8192> pixels{};
 const uint8_t *font = ArialMT_Plain_10;
 OLEDDISPLAY_COLOR color=WHITE;
 OLEDDISPLAY_TEXT_ALIGNMENT alignment=TEXT_ALIGN_LEFT;
 int outside=0;
 int getWidth() const {return 128;} int getHeight() const {return 64;}
 int width() const {return 128;} int height() const {return 64;}
 void setFont(const uint8_t *f) {font=f;}
 void setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT a) {alignment=a;}
 void setColor(OLEDDISPLAY_COLOR c) {color=c;}
 OLEDDISPLAY_COLOR getColor() {return color;}
 void setPixelColor(int x,int y,OLEDDISPLAY_COLOR c) {if(x<0||x>=128||y<0||y>=64){outside++; return;} pixels[y*128+x]=c?255:0;}
 void setPixel(int x,int y){setPixelColor(x,y,color);}
 void fillRect(int x,int y,int w,int h){for(int j=0;j<h;j++) for(int i=0;i<w;i++)setPixel(x+i,y+j);}
 void drawHorizontalLine(int x,int y,int w){fillRect(x,y,w,1);}
 void drawVerticalLine(int x,int y,int h){fillRect(x,y,1,h);}
 void drawRect(int x,int y,int w,int h){drawHorizontalLine(x,y,w);drawHorizontalLine(x,y+h-1,w);drawVerticalLine(x,y,h);drawVerticalLine(x+w-1,y,h);}
 void drawLine(int x,int y,int tx,int ty){int dx=abs(tx-x),sx=x<tx?1:-1,dy=-abs(ty-y),sy=y<ty?1:-1,e=dx+dy;for(;;){setPixel(x,y);if(x==tx&&y==ty)break;int t=e*2;if(t>=dy){e+=dy;x+=sx;}if(t<=dx){e+=dx;y+=sy;}}}
 int getStringWidth(const String &s){int n=0;for(unsigned char c:s)if(c>=font[2])n+=font[4+(c-font[2])*4+3];return n;}
 void drawString(int x,int y,const String &s){if(alignment==TEXT_ALIGN_CENTER)x-=getStringWidth(s)/2;for(unsigned char c:s){if(c<font[2])continue;const auto *j=font+4+(c-font[2])*4;int off=j[0]*256+j[1];if(off!=65535){const auto *b=font+4+font[3]*4+off;int rh=(font[1]+7)/8;for(int k=0;k<j[2];k++)for(int bit=0;bit<8;bit++){int yy=(k%rh)*8+bit;if(yy<font[1]&&(b[k]&(1<<bit)))setPixel(x+k/rh,y+yy);}}x+=j[3];}}
 void save(const char *name){assert(outside==0);std::ofstream f(std::string(name)+".pgm",std::ios::binary);f<<"P5\n128 64\n255\n";f.write((char*)pixels.data(),pixels.size());}
};
