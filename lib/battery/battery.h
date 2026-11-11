#pragma once

#include <Arduino.h>

// Battery voltage monitor. The battery goes through a voltage divider
// (R_TOP from battery+ to the pin, R_BOTTOM from the pin to GND) so the
// ESP32 pin only ever sees a fraction of the battery voltage.
// Non-blocking: call update() every loop()
class Battery {
public:
    // rTop / rBottom in ohms, must match the resistors on the PCB
    Battery(uint8_t adcPin, float rTop, float rBottom)
        : adcPin_(adcPin), ratio_((rTop + rBottom) / rBottom) {}

    void begin() {
        analogReadResolution(12);
        analogSetPinAttenuation(adcPin_, ADC_11db);   // pin range up to ~3.1 V
        voltage_ = readBatteryVoltage();              // start the filters at the real value
        restVoltage_ = voltage_;
    }

    // tell the monitor whether the motors are drawing current. The battery sags a
    // lot under motor load (internal resistance), so low/empty are judged on the
    // resting voltage, not the sagged one. Call every loop() before update()
    void setLoaded(bool loaded) {
        if (loaded_ && !loaded) unloadedSince_ = millis();   // motors just stopped
        loaded_ = loaded;
    }

    // takes a reading every SAMPLE_MS; returns true when a new reading was taken
    bool update() {
        if (millis() - lastSample_ < SAMPLE_MS) return false;
        lastSample_ = millis();

        float raw = readBatteryVoltage();

        // live voltage (sags while driving): slow moving average to smooth motor noise
        voltage_ += FILTER * (raw - voltage_);

        // resting voltage: only updated once the motors have been off for REST_MS,
        // so the battery has recovered from the sag
        if (!loaded_ && millis() - unloadedSince_ >= REST_MS)
            restVoltage_ += REST_FILTER * (raw - restVoltage_);

        // near 0 V means no battery (e.g. running off USB on the bench): don't flag it
        if (!present()) {
            low_ = false;
            empty_ = false;
            return true;
        }

        const bool wasEmpty = empty_;
        const float restBefore = restVoltage_;

        // lowest live voltage seen while driving: shows how far the battery sags
        if (loaded_ && voltage_ < minLoaded_) minLoaded_ = voltage_;

        // hysteresis: once low/empty, the voltage has to recover a bit to clear it
        if (restVoltage_ < LOW_V) low_ = true;
        else if (restVoltage_ > LOW_V + HYSTERESIS_V) low_ = false;

        if (restVoltage_ < EMPTY_V) empty_ = true;
        else if (restVoltage_ > EMPTY_V + HYSTERESIS_V) empty_ = false;

        // emergency: even under load the (2 s filtered) voltage shouldn't drop this far.
        // Also drag the resting value down to it, so empty stays set until a real
        // resting reading (motors off 1 s) shows the battery recovered above 7.4 V
        const bool floorHit = loaded_ && voltage_ < LOADED_FLOOR_V;
        if (floorHit) {
            empty_ = true;
            restVoltage_ = voltage_;
        }

        // remember why and at what voltages the empty state just turned on
        if (!wasEmpty && empty_) {
            tripped_ = true;
            tripReason_ = floorHit ? UNDER_LOAD : RESTING;
            tripLive_ = voltage_;
            tripRest_ = restBefore;
        }

        return true;
    }

    float voltage() const { return voltage_; }          // live filtered voltage (sags under load)
    float restVoltage() const { return restVoltage_; }  // last voltage with the motors off
    bool present() const { return voltage_ > NO_BATTERY_V; }
    bool low() const { return low_; }            // warn: replace soon
    bool empty() const { return empty_; }        // replace now: regulator is near dropout

    // why the empty state last turned on, and the voltages at that moment
    enum Reason { NONE = 0, RESTING = 1, UNDER_LOAD = 2 };
    Reason tripReason() const { return tripReason_; }
    float tripLive() const { return tripLive_; }
    float tripRest() const { return tripRest_; }
    float minLoaded() const { return minLoaded_; }   // lowest live voltage while driving
    // true once per new trip (then clears), so the caller can log it
    bool takeTrip() {
        bool t = tripped_;
        tripped_ = false;
        return t;
    }

    // 0-100 from the resting voltage, using the lookup table below
    // (straight lines between the points)
    int percent() const {
        const float v = restVoltage_;
        struct Point { float volts; int pct; };
        // approximate 9 V alkaline curve. 0% is set at 7.0 V, where the L7805
        // (needs about 2 V of headroom above 5 V) stops regulating, not where the
        // battery is truly dead. Refine these points by logging a real discharge
        static const Point table[] = {
            {9.5f, 100}, {9.0f, 85}, {8.6f, 70}, {8.3f, 55},
            {8.0f, 40},  {7.7f, 25}, {7.4f, 10}, {7.0f, 0},
        };
        const int n = sizeof(table) / sizeof(table[0]);

        if (v >= table[0].volts) return 100;
        for (int i = 1; i < n; i++) {
            if (v >= table[i].volts) {
                const Point &hi = table[i - 1];
                const Point &lo = table[i];
                float frac = (v - lo.volts) / (hi.volts - lo.volts);
                return lroundf(lo.pct + frac * (hi.pct - lo.pct));
            }
        }
        return 0;
    }

    // two-point correction from multimeter checks: true = gain * measured + offset
    // measure at two voltages (uncalibrated reading vs meter), then
    // gain = (meter1 - meter2) / (read1 - read2), offset = meter1 - gain * read1
    void setCalibration(float gain, float offset = 0.0f) {
        gain_ = gain;
        offset_ = offset;
    }

private:

    static constexpr float LOW_V = 7.7f;          // resting, about 25%: warn
    static constexpr float EMPTY_V = 7.2f;        // resting, replace: just above L7805 dropout
    static constexpr float LOADED_FLOOR_V = 6.0f; // while driving: stop if it sags this far
    static constexpr unsigned long REST_MS = 1000;// motors off this long = battery has recovered
    static constexpr float REST_FILTER = 0.2f;    // ~0.5 s time constant for the resting reading
    static constexpr float HYSTERESIS_V = 0.2f;
    static constexpr float NO_BATTERY_V = 3.0f;   // below this, assume no battery is connected
    static constexpr unsigned long SAMPLE_MS = 100;
    static constexpr float FILTER = 0.05f;        // ~2 s time constant at 100 ms samples
    static constexpr int SAMPLES = 16;

    // averaged pin voltage, scaled back up to battery voltage
    float readBatteryVoltage() {
        uint32_t sum = 0;
        for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(adcPin_);
        float pinVolts = sum / (float)SAMPLES / 1000.0f;
        return pinVolts * ratio_ * gain_ + offset_;
    }

    uint8_t adcPin_;
    float ratio_;
    float gain_ = 1.0f;
    float offset_ = 0.0f;
    float voltage_ = 0;
    float restVoltage_ = 0;
    bool loaded_ = false;
    unsigned long unloadedSince_ = 0;
    bool low_ = false;
    bool empty_ = false;
    bool tripped_ = false;
    Reason tripReason_ = NONE;
    float tripLive_ = 0;
    float tripRest_ = 0;
    float minLoaded_ = 99.0f;
    unsigned long lastSample_ = 0;
};
