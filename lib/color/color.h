#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <stdlib.h> 
#include <cmath>

class ColorSensor {
public:
    enum Surface { RED, YELLOW, BLUE, BLACK};

    ColorSensor(uint8_t adcPin, uint8_t redPin, uint8_t yellowPin, uint8_t bluePin)
        : adcPin_(adcPin) {
        ledPins_[0] = redPin;
        ledPins_[1] = yellowPin;
        ledPins_[2] = bluePin;
    }

    void begin() {
        for (int i = 0; i < NUM_LEDS; i++) {
            pinMode(ledPins_[i], OUTPUT);
            digitalWrite(ledPins_[i], LOW);
        }
        analogReadResolution(12);
        analogSetPinAttenuation(adcPin_, ADC_11db);   // idle voltage is ~3 V, 0 dB would clip
    }

    bool update() {
        allOff();
        delay(50);
        ambient = readVoltage();
        delay(50);

        digitalWrite(ledPins_[0], HIGH);
        delay(50);
        red = readVoltage();
        digitalWrite(ledPins_[0], LOW);
        delay(50);

        digitalWrite(ledPins_[1], HIGH);
        delay(50);
        yellow = readVoltage();
        digitalWrite(ledPins_[1], LOW);
        delay(50);

        digitalWrite(ledPins_[2], HIGH);
        delay(50);
        blue = readVoltage();
        digitalWrite(ledPins_[2], LOW);
        delay(50);

        return true;
    }

    Surface readSurface() {
        float r = scaledRed();
        float y = scaledYellow();
        float b = scaledBlue();

        // NEW: if nothing reflects much light, it's black
        if (r < blackLevel && y < blackLevel && b < blackLevel) {
            return BLACK;
        }

        // your original logic, now using the scaled values
        if (r > y && r > b) {
            return RED;
        } else if (y > r && y > b) {
            return YELLOW;
        } else if (b > y && b > r) {
            return BLUE;
        } else {
            return BLACK;
        }
    }

    void calibrateAll() {
        Serial.println("Put sensor on RED"); delay(5000);
        calibrate(RED);

        Serial.println("Put sensor on YELLOW"); delay(5000);
        calibrate(YELLOW);

        Serial.println("Put sensor on BLUE"); delay(5000);
        calibrate(BLUE);

        Serial.println("Put sensor on BLACK"); delay(5000);
        calibrate(BLACK);   // must be last

        Serial.println("Calibration done");
    }

    // void setCalibration(float gr, float gy, float gb, float black) {
    //     gainRed = gr;
    //     gainYellow = gy;
    //     gainBlue = gb;
    //     blackLevel = black;
    // }

    // void saveCalibration() {
    //     prefs_.begin("colorsensor", false);   // false = we are writing
    //     prefs_.putFloat("gainRed", gainRed);
    //     prefs_.putFloat("gainYellow", gainYellow);
    //     prefs_.putFloat("gainBlue", gainBlue);
    //     prefs_.putFloat("blackLevel", blackLevel);
    //     prefs_.putBool("saved", true);        // marks that real values exist
    //     prefs_.end();
    //     Serial.println("Calibration saved");
    // }

    // NEW: loads saved values. Returns true if there were any, false if not
    // bool loadCalibration() {
    //     if (!prefs_.begin("colorsensor", true)) {   // true = read only
    //         return false;
    //     }
    //     bool saved = prefs_.getBool("saved", false);
    //     if (saved) {
    //         gainRed    = prefs_.getFloat("gainRed", 1.0f);
    //         gainYellow = prefs_.getFloat("gainYellow", 1.0f);
    //         gainBlue   = prefs_.getFloat("gainBlue", 1.0f);
    //         blackLevel = prefs_.getFloat("blackLevel", 0.1f);
    //     }
    //     prefs_.end();
    //     return saved;
    // }

    //For tuning purposes. Print out the calibration value
    void printCalibration() {
        Serial.println("---- Calibration results ----");
        Serial.print("gainRed:    "); Serial.println(gainRed, 4);
        Serial.print("gainYellow: "); Serial.println(gainYellow, 4);
        Serial.print("gainBlue:   "); Serial.println(gainBlue, 4);
        Serial.print("blackLevel: "); Serial.println(blackLevel, 4);
    }

    //Used for tuning/testing, print the numbers while you move over each lane
    void printValues() {
        Serial.print("ambient: "); Serial.print(ambient, 3);
        Serial.print("  red: ");    Serial.print(scaledRed(), 3);
        Serial.print("  yellow: "); Serial.print(scaledYellow(), 3);
        Serial.print("  blue: ");   Serial.println(scaledBlue(), 3);
    }

private:
    static constexpr int NUM_LEDS = 3;
    static constexpr int SAMPLES = 12;
    static constexpr int CAL_ROUNDS = 10; 
    uint8_t adcPin_;
    uint8_t ledPins_[NUM_LEDS]; 
    float ambient = 0, red = 0, yellow = 0, blue = 0;
    Preferences prefs_; 

    //TODO: Calibrate this thorugh testing
    float gainRed = 1.0f, gainYellow = 1.0f, gainBlue = 1.0f;
    float blackLevel = 0.1f;

    // averaged input voltage in volts
    float readVoltage() {
        uint32_t sum = 0;
        for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(adcPin_);
        return sum / (float)SAMPLES / 1000.0f;
    }

    void allOff() {
        for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins_[i], LOW);
    }

    float deltaRed()    const { return abs(ambient - red); }
    float deltaYellow() const { return abs(ambient - yellow); }
    float deltaBlue()   const { return abs(ambient - blue); }

    // Deltas after gain, so every LED is on the same scale
    float scaledRed()    const { return deltaRed() * gainRed; }
    float scaledYellow() const { return deltaYellow() * gainYellow; }
    float scaledBlue()   const { return deltaBlue() * gainBlue; }

    //Calibration. Put the sensor over the given surface, then call this.
    // The matching LED on its own color is defined as 1.0, so the
    // LEDs are no longer unfairly stronger or weaker than each other.
    void calibrate(Surface s) {
        float sumRed = 0, sumYellow = 0, sumBlue = 0;

        // blink all three LEDs CAL_ROUNDS times and add up the deltas
        for (int i = 0; i < CAL_ROUNDS; i++) {
            update();
            sumRed    += deltaRed();
            sumYellow += deltaYellow();
            sumBlue   += deltaBlue();
        }

        // average delta for each LED
        float avgRed    = sumRed / CAL_ROUNDS;
        float avgYellow = sumYellow / CAL_ROUNDS;
        float avgBlue   = sumBlue / CAL_ROUNDS;

        if (s == RED && avgRed > 0.01f) {
            gainRed = 1.0f / avgRed;
        } else if (s == YELLOW && avgYellow > 0.01f) {
            gainYellow = 1.0f / avgYellow;
        } else if (s == BLUE && avgBlue > 0.01f) {
            gainBlue = 1.0f / avgBlue;
        } else if (s == BLACK) {
            // black: find the biggest scaled average, add some margin
            // (call this AFTER red, yellow, blue are calibrated)
            float biggest = avgRed * gainRed;
            if (avgYellow * gainYellow > biggest) biggest = avgYellow * gainYellow;
            if (avgBlue * gainBlue > biggest)     biggest = avgBlue * gainBlue;
            blackLevel = biggest * 1.5f;
        }
    }
};