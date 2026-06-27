#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define HIGH 0x1
#define LOW  0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define U8X8_PIN_NONE -1

typedef uint8_t byte;

typedef unsigned long millis_t;
unsigned long millis();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
void delay(unsigned long ms);

class HardwareSerial {
public:
  void begin(unsigned long baud);
  void println(const char *s);
  void println(const char c[]);
  void println(unsigned long n);
  void println();
  int printf(const char *format, ...);
};

extern HardwareSerial Serial;

#endif // ARDUINO_H
