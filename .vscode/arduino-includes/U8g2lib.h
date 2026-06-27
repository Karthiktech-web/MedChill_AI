#ifndef U8G2LIB_H
#define U8G2LIB_H

#include "Arduino.h"

typedef int U8g2Rotation;
#define U8G2_R0 0
#define U8X8_PIN_NONE -1

class U8G2 {
public:
  U8G2(U8g2Rotation rotation, int reset) {}
  void begin() {}
  void clearBuffer() {}
  void setFont(const char *font) {}
  void drawStr(int x, int y, const char *s) {}
  void drawLine(int x0, int y0, int x1, int y1) {}
  void sendBuffer() {}
};

class U8G2_SH1106_128X64_NONAME_F_HW_I2C : public U8G2 {
public:
  U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8g2Rotation rotation, int reset) : U8G2(rotation, reset) {}
};

#endif // U8G2LIB_H
