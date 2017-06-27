#include "Arduino.h"
#include "FeedTimer.h"

void FeedTimer::setTime(long portionSize, int h, int m) {
  _portionSize = portionSize;
  _h = h;
  _m = m;
  Serial.println(String(_portionSize) + " " + String(_h) + " " + String(_m));
}

int FeedTimer::portionSize() {
  return _portionSize;
}

int FeedTimer::hour() {
  return _h;
}

int FeedTimer::minute() {
  return _m;
}
