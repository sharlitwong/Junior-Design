#include "lane.h"

// void LaneFollower::reset() {
//     lastSide_ = 0;
//     drive_.stop();
// }

void LaneFollower::reset() {
    steer_ = 0;
    lastSide_ = 0;
    lastUpdateMs_ = millis();
    drive_.stop();
}

// void LaneFollower::update(ColorSensor::Surface left, ColorSensor::Surface right) {
//     bool leftOn  = (left  != ColorSensor::BLACK);   // any non-black color = lane
//     bool rightOn = (right != ColorSensor::BLACK);
//     unsigned long now = millis();

//     int steer = 0;   // + = steer right, - = steer left

//     if (leftOn && rightOn) {        // both on lane: straight
//         steer = 0;                  
//         lastSeenMs_ = now;
//     } else if (rightOn) {           // left is black: bot is left of the lane
//         steer = +STEER;
//         lastSide_ = +1;
//         lastSeenMs_ = now;
//     } else if (leftOn) {            // right is black: bot is right of the lane
//         steer = -STEER;
//         lastSide_ = -1;
//         lastSeenMs_ = now;
//     } else {                                // both black
//         if (lastSide_ != 0 && now - lastSeenMs_ < LOST_MS) {
//             // steering wasn't enough: pivot in place toward the side that saw the lane last
//             drive_.setWheelSpeeds(lastSide_ * PIVOT_SPEED, -lastSide_ * PIVOT_SPEED);
//             return;                         // skip the steer-based line below
//         }
//         lastSide_ = 0;                      // never saw the lane (or lost it): go straight
//         steer = 0;
//     }

//     // steer right = left wheel faster, right wheel slower
//     drive_.trackWheelSpeeds(BASE_SPEED + steer, BASE_SPEED - steer);
// }

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
        // centered: relax steering back to 0
        if (steer_ > 0)      steer_ = max(0.0f, steer_ - STEER_DECAY * dt);
        else if (steer_ < 0) steer_ = min(0.0f, steer_ + STEER_DECAY * dt);
    } else {
        // error flipped sign (overshoot): drop the old steering and build the other way
        if ((dir > 0 && steer_ < 0) || (dir < 0 && steer_ > 0)) steer_ = 0;
        // still off-center (undershoot): keep building steering
        steer_ += dir * STEER_RATE * dt;
        steer_ = constrain(steer_, (float)-MAX_STEER, (float)MAX_STEER);
    }
 
    // steer right = left wheel faster, right wheel slower
    drive_.trackWheelSpeeds(BASE_SPEED + (int)steer_, BASE_SPEED - (int)steer_);
}