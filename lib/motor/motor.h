#pragma once

#include <pwm.h>
// #include <Adafruit_NeoPixel.h>

#define MOTOR_1_DIR1   4
#define MOTOR_1_DIR2   5
#define MOTOR_2_DIR1   6
#define MOTOR_2_DIR2   7

class Motor {
public:
    Pwm left_motor;
    Pwm right_motor;

    bool leftMotorForward = true;
    bool rightMotorForward = true;

    Motor(Pwm &leftPwm, Pwm &rightPwm)
        : left_motor(leftPwm), right_motor(rightPwm) {}

    void begin() {
        digitalWrite(MOTOR_1_DIR1, LOW);
        digitalWrite(MOTOR_1_DIR2, HIGH);
        digitalWrite(MOTOR_2_DIR1, LOW);
        digitalWrite(MOTOR_2_DIR2, HIGH);
    }

    void stop() {
        left_motor.writePercent(0);
        right_motor.writePercent(0);
    }
    
    void moveForward(int leftSpeed, int rightSpeed) {
        if (!leftMotorForward || !rightMotorForward) {
            leftMotorForward = true;
            rightMotorForward = true;
            this->switchDirection();
        }
        left_motor.writePercent(leftSpeed);
        right_motor.writePercent(rightSpeed);
    }

    void moveBackward(int leftSpeed, int rightSpeed){
        if (leftMotorForward || rightMotorForward) {
            leftMotorForward = false;
            rightMotorForward = false;
            this->switchDirection();
        }
        left_motor.writePercent(leftSpeed);
        right_motor.writePercent(rightSpeed);
    }
    
    void turn(int speedLeft, int speedRight) {
        left_motor.writePercent(speedLeft);
        right_motor.writePercent(speedRight);
    }

    void pivotCW(int speed) {
        leftMotorForward = true;
        rightMotorForward = false;
        this -> switchDirection();

        left_motor.writePercent(speed);
        right_motor.writePercent(speed);
    }

    void pivotCCW(int speed) {
        leftMotorForward = false;
        rightMotorForward = true;
        this -> switchDirection();

        left_motor.writePercent(speed);
        right_motor.writePercent(speed);
    }

    // signed duty cycle per wheel (in terms of percentage), -100..100 (%)
    // negative = backward; postive = forward
    void setDuty(int leftDuty, int rightDuty) {
        leftDuty = constrain(leftDuty, -100, 100);
        rightDuty = constrain(rightDuty, -100, 100);

        leftMotorForward = leftDuty >= 0;
        rightMotorForward = rightDuty >= 0;
        this->switchDirection();

        left_motor.writePercent(abs(leftDuty));
        right_motor.writePercent(abs(rightDuty));
    }
    
private:
    void switchDirection() {
        this->switchDirectionLeft();
        this->switchDirectionRight();
    }

    void switchDirectionLeft() {
        if (leftMotorForward) {
            digitalWrite(MOTOR_1_DIR1, LOW);
            digitalWrite(MOTOR_1_DIR2, HIGH);
        } else {
            digitalWrite(MOTOR_1_DIR1, HIGH);
            digitalWrite(MOTOR_1_DIR2, LOW);
        }
    }

    void switchDirectionRight() {
        if (rightMotorForward) {
            digitalWrite(MOTOR_2_DIR1, LOW);
            digitalWrite(MOTOR_2_DIR2, HIGH);
        } else {
            digitalWrite(MOTOR_2_DIR1, HIGH);
            digitalWrite(MOTOR_2_DIR2, LOW);
        }
    }
};