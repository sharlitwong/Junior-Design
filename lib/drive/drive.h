#pragma once

#include <Arduino.h>
#include <motor.h>
#include <encoder.h>

//HOLDS each wheel at a target speed (encoder counts per second) by adjusting
//the motor duty cycle with the encoders. Call update() every loop()
class Drive {
public:
    Drive(Motor &motors, Encoder &leftEnc, Encoder &rightEnc)
        : motors_(motors), leftEnc_(leftEnc), rightEnc_(rightEnc) {}

    // targets in counts/s (1440 counts = 1 wheel turn), negative = backward
    void setWheelSpeeds(int leftTarget, int rightTarget) {
        if (leftTarget == leftTarget_ && rightTarget == rightTarget_) return;  // already set
        leftTarget_ = leftTarget;
        rightTarget_ = rightTarget;
        // start from a guess so the wheels don't have to ramp up from 0
        leftDuty_ = constrain(feedforward(leftTarget, leftMaxSpeed_), -maxDuty_, maxDuty_);
        rightDuty_ = constrain(feedforward(rightTarget, rightMaxSpeed_), -maxDuty_, maxDuty_);
        leftSettled_ = 0;
        rightSettled_ = 0;
    }

    // like setWheelSpeeds(), but for small continuous changes (e.g. steering): keeps the
    // current duty so the speed loop keeps its correction. Restarts from the feedforward
    // guess only when a wheel starts, stops or reverses.
    void trackWheelSpeeds(int leftTarget, int rightTarget) {
        bool restart = leftTarget == 0 || rightTarget == 0 ||
                       leftTarget_ == 0 || rightTarget_ == 0 ||
                       (leftTarget > 0) != (leftTarget_ > 0) ||
                       (rightTarget > 0) != (rightTarget_ > 0);
        if (restart) { setWheelSpeeds(leftTarget, rightTarget); return; }
        leftTarget_ = leftTarget;
        rightTarget_ = rightTarget;
    }

    // learned speed at 100% duty for each wheel, counts/s. The starting guess in
    // setWheelSpeeds() uses these, so they decide how big the burst is on a state change
    float leftMaxSpeed() const { return leftMaxSpeed_; }
    float rightMaxSpeed() const { return rightMaxSpeed_; }
    void setMaxSpeeds(float left, float right) {   // e.g. restore saved values on boot
        if (left > 100 && left < 20000) leftMaxSpeed_ = left;
        if (right > 100 && right < 20000) rightMaxSpeed_ = right;
    }

    void stop() { setWheelSpeeds(0, 0); }

    // caps the duty % on both wheels (0-100), e.g. to keep the average motor
    // voltage under the motor's rating when the battery is higher than that
    void setMaxDuty(float maxDuty) { maxDuty_ = constrain(maxDuty, 0.0f, 100.0f); }
    float maxDuty() const { return maxDuty_; }

    void update() {
        unsigned long now = millis(); //get current time
        if (now - lastUpdate_ < UPDATE_MS) return; //if it's been 20 ms
        float dt = (now - lastUpdate_) / 1000.0f; //convert elapsed time to seconds
        lastUpdate_ = now; //restart elapsed time timer

        // measured speed = change in count / time
        long left = leftEnc_.count(); //left encoder ticks
        long right = rightEnc_.count(); //right encoder ticks
        leftSpeed_ = (left - lastLeft_) / dt; //ticks / elapsed time
        rightSpeed_ = (right - lastRight_) / dt;
        lastLeft_ = left; //re sample ticks to current tick count
        lastRight_ = right;

        // adjust each duty in whichever direction closes the speed error
        // clamped to the max duty, so the error can't wind the duty up past it
        if (leftTarget_ != 0)
            leftDuty_ = constrain(leftDuty_ + KI * (leftTarget_ - leftSpeed_) * dt, -maxDuty_, maxDuty_);
        if (rightTarget_ != 0)
            rightDuty_ = constrain(rightDuty_ + KI * (rightTarget_ - rightSpeed_) * dt, -maxDuty_, maxDuty_);

        learn(leftTarget_, leftSpeed_, leftDuty_, leftSettled_, leftMaxSpeed_);
        learn(rightTarget_, rightSpeed_, rightDuty_, rightSettled_, rightMaxSpeed_);

        motors_.setDuty(lroundf(leftDuty_), lroundf(rightDuty_));
    }

    float leftSpeed() const { return leftSpeed_; }    // measured, counts/s
    float rightSpeed() const { return rightSpeed_; }
    int leftTarget() const { return leftTarget_; }    // requested, counts/s
    int rightTarget() const { return rightTarget_; }
    float leftDuty() const { return leftDuty_; }      // current duty %, 100 = maxed out
    float rightDuty() const { return rightDuty_; }

private:
    static constexpr unsigned long UPDATE_MS = 20;  // control loop runs at 50 Hz
    static constexpr float MAX_SPEED = 2000.0f;     // starting value until the bot learns its own
    static constexpr float KI = 0.05f;              // duty % added per count of accumulated error
    static constexpr float SETTLED_ERR = 0.10f;     // within 10% of target counts as settled
    static constexpr int SETTLED_SAMPLES = 25;      // ...for 0.5 s (25 x 20 ms) before learning
    static constexpr float LEARN_RATE = 0.02f;      // how fast the learned value moves per sample

    static float feedforward(int target, float maxSpeed) {
        return constrain(target * 100.0f / maxSpeed, -100.0f, 100.0f);
    }

    // once a wheel has held its target for a while, the duty it needed tells us
    // the real speed-per-duty ratio; nudge the learned max speed toward it
    static void learn(int target, float speed, float duty, int &settled, float &maxSpeed) {
        bool steady = target != 0 && fabsf(duty) > 5.0f && fabsf(duty) < 99.0f &&
                      (speed > 0) == (target > 0) &&
                      fabsf(target - speed) < SETTLED_ERR * abs(target);
        if (!steady) { settled = 0; return; }
        if (++settled < SETTLED_SAMPLES) return;
        float measured = fabsf(speed) * 100.0f / fabsf(duty);   // counts/s at 100% duty
        maxSpeed += LEARN_RATE * (measured - maxSpeed);
    }

    Motor &motors_;
    Encoder &leftEnc_;
    Encoder &rightEnc_;

    int leftTarget_ = 0;
    int rightTarget_ = 0;
    float leftDuty_ = 0;
    float rightDuty_ = 0;
    float leftSpeed_ = 0;
    float rightSpeed_ = 0;
    long lastLeft_ = 0;
    long lastRight_ = 0;
    unsigned long lastUpdate_ = 0;
    float maxDuty_ = 100.0f;
    float leftMaxSpeed_ = MAX_SPEED;
    float rightMaxSpeed_ = MAX_SPEED;
    int leftSettled_ = 0;
    int rightSettled_ = 0;
};