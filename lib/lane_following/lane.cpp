#include "lane.h"

void LaneFollower::reset() {
    lastSide_ = 0;
    drive_.stop();
}

void LaneFollower::update(ColorSensor::Surface left, ColorSensor::Surface right) {
    bool leftOn  = (left  != ColorSensor::BLACK);   // any non-black color = lane
    bool rightOn = (right != ColorSensor::BLACK);
    unsigned long now = millis();

    int steer = 0;   // + = steer right, - = steer left

    if (leftOn && rightOn) {        // both on lane: straight
        steer = 0;                  
        lastSeenMs_ = now;
    } else if (rightOn) {           // left is black: bot is left of the lane
        steer = +STEER;
        lastSide_ = +1;
        lastSeenMs_ = now;
    } else if (leftOn) {            // right is black: bot is right of the lane
        steer = -STEER;
        lastSide_ = -1;
        lastSeenMs_ = now;
    } else {                    // both black
        if (lastSide_ != 0 && now - lastSeenMs_ < LOST_MS) {
            // overshot: steer back toward the side that saw the lane last
            steer = lastSide_ * STEER;
        } else {
            lastSide_ = 0;      // never saw the lane (or lost it): go straight
            steer = 0;
        }
    }

    // steer right = left wheel faster, right wheel slower
    drive_.trackWheelSpeeds(BASE_SPEED + steer, BASE_SPEED - steer);
}