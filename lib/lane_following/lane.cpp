#include "lane.h"

void LaneFollower::reset() {
    steer_ = 0;
    lastSide_ = 0;
    lastUpdateMs_ = millis();
    drive_.stop();
}

void LaneFollower::update(ColorSensor::Surface left, ColorSensor::Surface right) {
    unsigned long now = millis();
    float dt = (now - lastUpdateMs_) / 1000.0f;
    lastUpdateMs_ = now;
    if (dt > 0.1f) dt = 0.1f;               // ignore long gaps
 
    bool leftOn  = (left  != ColorSensor::BLACK);   // any non-black color = lane
    bool rightOn = (right != ColorSensor::BLACK);
 
    int dir = 0;   // which way the bot needs to turn: +1 right, -1 left, 0 = centered
 
    if (leftOn && rightOn) {
        dir = 0;
        lastSeenMs_ = now;
    } else if (rightOn) {                   // left is black: lane is to the right
        dir = +1;
        lastSide_ = +1;
        lastSeenMs_ = now;
    } else if (leftOn) {                    // right is black: lane is to the left
        dir = -1;
        lastSide_ = -1;
        lastSeenMs_ = now;
    } else if (lastSide_ != 0 && now - lastSeenMs_ < LOST_MS) {
        dir = lastSide_;                    // both black: lane is on the side that saw it last
    } else {
        lastSide_ = 0;                      // never saw the lane, or lost it: go straight
        steer_ = 0;
        drive_.trackWheelSpeeds(BASE_SPEED, BASE_SPEED);
        return;
    }
 
    if (dir == 0) {
        steer_ = 0;                          // centered: stop turning immediately
    } else {
        if ((dir > 0 && steer_ < 0) || (dir < 0 && steer_ > 0)) steer_ = 0;
        if (steer_ == 0) steer_ = dir * MIN_STEER;       // start with a small correction
        steer_ += dir * STEER_RATE * dt;                 // escalate only if it persists
        steer_ = constrain(steer_, (float)-MAX_STEER, (float)MAX_STEER);
    }
 
    // steer right = left wheel faster, right wheel slower
    drive_.trackWheelSpeeds(BASE_SPEED + (int)steer_, BASE_SPEED - (int)steer_);
}