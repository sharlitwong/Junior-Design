#pragma once

#include <Arduino.h>

// Battery voltage reader. The battery goes through a voltage divider
// (R_TOP from battery+ to the pin, R_BOTTOM from the pin to the ESP32's GND)
// so the pin only ever sees a fraction of the battery voltage.
// Non-blocking: call update() every loop()
class Battery {
public:
    // rTop / rBottom in ohms, must match the actual resistors
    Battery(uint8_t adcPin, float rTop, float rBottom)
        : adcPin_(adcPin), ratio_((rTop + rBottom) / rBottom) {}

    void begin() {
        analogReadResolution(12);
        analogSetPinAttenuation(adcPin_, ADC_11db);   // pin range up to ~3.1 V
        voltage_ = readBatteryVoltage();              // start the average at the real value
    }

    // takes a reading every 100 ms
    void update() {
        if (millis() - lastSample_ < 100) return;
        lastSample_ = millis();
        // moving average (~0.5 s) so electrical noise doesn't make it jump
        voltage_ += 0.2f * (readBatteryVoltage() - voltage_);
    }

    float voltage() const { return voltage_; }      // battery voltage, volts
    bool present() const { return voltage_ > 3.0f; } // below 3 V = no battery (e.g. on USB)

    // multimeter reading / ESP32 reading, measured once
    void setCalibration(float gain) { gain_ = gain; }

    // 0-100, approximate 9 V alkaline curve; 0% = 6.5 V, the replacement threshold
    int percent() const {
        struct Point { float volts; int pct; };
        static const Point table[] = {
            {9.5f, 100}, {9.0f, 85}, {8.6f, 70}, {8.3f, 55},
            {8.0f, 40},  {7.7f, 25}, {7.4f, 10}, {6.5f, 0},
        };
        const int n = sizeof(table) / sizeof(table[0]);
        if (voltage_ >= table[0].volts) return 100;
        for (int i = 1; i < n; i++) {
            if (voltage_ >= table[i].volts) {
                const Point &hi = table[i - 1];
                const Point &lo = table[i];
                float frac = (voltage_ - lo.volts) / (hi.volts - lo.volts);
                return lroundf(lo.pct + frac * (hi.pct - lo.pct));
            }
        }
        return 0;
    }

private:
    // average of 16 readings at the pin, scaled back up to battery voltage
    float readBatteryVoltage() {
        uint32_t sum = 0;
        for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(adcPin_);
        return sum / 16.0f / 1000.0f * ratio_ * gain_;
    }

    uint8_t adcPin_;
    float ratio_;
    float gain_ = 1.0f;
    float voltage_ = 0;
    unsigned long lastSample_ = 0;
};
