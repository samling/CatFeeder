#ifndef FeedTimer_h
#define FeedTimer_h

#include "Arduino.h"

class FeedTimer {
  public:
    void setTime(long portionSize, int h, int m);
    int portionSize();
    int hour();
    int minute();
  private:
    long _portionSize; // Duration in ms to spin the auger
    int _h; // Hour
    int _m; // Minute
};
#endif
