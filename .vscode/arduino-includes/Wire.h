#ifndef WIRE_H
#define WIRE_H

#include "Arduino.h"

class TwoWire {
public:
  void begin(int sda = -1, int scl = -1);
};

extern TwoWire Wire;

#endif // WIRE_H
