#pragma once

#include <Arduino.h>

// IR distance sensor with an LED we switch on and off. Reads the voltage with
// the LED off (ambient light) and on (ambient + reflection); the difference is
// the reflection alone, which grows as an obstacle gets closer.
// Non-blocking: call update() every loop()
class Distance {
public:
    Distance(uint8_t adcPin, uint8_t ledPin) : adcPin_(adcPin), ledPin_(ledPin) {}

    void begin() {
        pinMode(ledPin_, OUTPUT);
        digitalWrite(ledPin_, LOW);
        analogReadResolution(12);
        analogSetPinAttenuation(adcPin_, ADC_0db);
    }

    // advances the measurement; returns true when a new reading is ready (~every 100 ms)
    bool update() {
        switch (step_) {
        case 0:                                   // start: LED off, let the reading settle
            digitalWrite(ledPin_, LOW);
            stepStart_ = millis();
            step_ = 1;
            break;
        case 1:                                   // after 50 ms: read ambient light, LED on
            if (millis() - stepStart_ >= SETTLE_MS) {
                off_ = readVoltage();
                digitalWrite(ledPin_, HIGH);
                stepStart_ = millis();
                step_ = 2;
            }
            break;
        case 2:                                   // after 50 ms: read with LED on
            if (millis() - stepStart_ >= SETTLE_MS) {
                delta_ = readVoltage() - off_;
                digitalWrite(ledPin_, LOW);
                step_ = 0;
                return true;
            }
            break;
        }
        return false;
    }

    // latest reading: on - off voltage, in volts (bigger = closer)
    float delta() const { return delta_; }

private:
    static constexpr unsigned long SETTLE_MS = 50;
    static constexpr int SAMPLES = 12;

    // averaged input voltage in volts
    float readVoltage() {
        uint32_t sum = 0;
        for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(adcPin_);
        return sum / (float)SAMPLES / 1000.0f;
    }

    uint8_t adcPin_;
    uint8_t ledPin_;
    int step_ = 0;                // 0 = start, 1 = LED off and settling, 2 = LED on and settling
    unsigned long stepStart_ = 0;
    float off_ = 0;
    float delta_ = 0;
};