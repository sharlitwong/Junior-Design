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

    static const int BASE_SPEED = 1000;
    static const int MIN_STEER  = 200;                  // ~0.2 x BASE_SPEED
    static const int MAX_STEER  = 600;                  // ~0.6 x BASE_SPEED
    static constexpr float STEER_RATE = 1100.0f;        // ~1.1 x BASE_SPEED
    static const unsigned long LOST_MS = 300;      
};