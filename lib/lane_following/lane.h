#pragma once
#include <Arduino.h>
#include <drive.h>
#include <color.h>

// Simple two-sensor lane follower. Sensors sit ON the lane when centered.
// Only changes the wheel TARGET speeds; Drive's encoder loop does the rest.
class LaneFollower {
public:
    LaneFollower(Drive& drive) : drive_(drive) {}

    void update(ColorSensor::Surface left, ColorSensor::Surface right);
    void reset();   // call when the bot stops

private:
    Drive& drive_;
    int  lastSide_ = 0;   // the sensor that last saw the lane: -1 left, +1 right, 0 = never
    unsigned long lastSeenMs_ = 0;
    unsigned long lastUpdateMs_ = 0;
    float steer_ = 0;        // + = steering right, - = left (counts/s)

    static const int BASE_SPEED = 1000;   // counts/s
    // static const int STEER      = 250;    // counts/s added to one wheel, removed from the other
    // static const int PIVOT_SPEED = 600;  // counts/s added per wheel
    static const int MAX_STEER  = 900;                  // counts/s; > BASE_SPEED lets the inner wheel reverse (pivot-like)
    static constexpr float STEER_RATE  = 1500.0f;       // counts/s per second the steer builds while off-center
    static constexpr float STEER_DECAY = 3000.0f;       // counts/s per second it relaxes when centered
    static const unsigned long LOST_MS = 1500;  // both black for this long = forget the lane
};