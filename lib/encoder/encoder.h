#pragma once

#include <Arduino.h>

// Quadrature encoder (Pololu magnetic encoder, 12 CPR at the motor shaft)
// Counts every edge of A and B, so one motor shaft turn = 12 ticks
// and one wheel turn = 12 * gear ratio ticks
class Encoder {
public:
    Encoder(uint8_t pinA, uint8_t pinB, bool reversed = false)
        : pinA_(pinA), pinB_(pinB), reversed_(reversed) {}

    void begin() {
        pinMode(pinA_, INPUT_PULLUP);
        pinMode(pinB_, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(pinA_), handleA, this, CHANGE);
        attachInterruptArg(digitalPinToInterrupt(pinB_), handleB, this, CHANGE);
    }

    // ticks since begin() or the last reset(), negative when spinning backward
    int32_t count() const { return reversed_ ? -count_ : count_; }

    void reset() { count_ = 0; }

private:
    // ISRs live in encoder.cpp: IRAM_ATTR functions can't be inline in a header
    static void handleA(void* arg);
    static void handleB(void* arg);

    uint8_t pinA_;
    uint8_t pinB_;
    bool reversed_;
    volatile int32_t count_ = 0;
};
