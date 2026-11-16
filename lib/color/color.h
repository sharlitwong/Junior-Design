#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <stdlib.h> 
#include <cmath>

class ColorSensor {
public:
    enum Surface { RED, YELLOW, BLUE, BLACK};
    
    //Construction function
    ColorSensor(uint8_t adcPin, uint8_t redPin, uint8_t yellowPin, uint8_t bluePin): adcPin_(adcPin) {
        ledPins_[0] = redPin;
        ledPins_[1] = yellowPin;
        ledPins_[2] = bluePin;
    }

    //Setting up color sensor
    void begin() {
        analogWriteFrequency(20000); 
        for (int i = 0; i < NUM_LEDS; i++) {
            pinMode(ledPins_[i], OUTPUT);
            // analogWriteFrequency(ledPins_[i], 20000);   // NEW: fast PWM so the sensor sees a steady average
            ledOff(i);
        }
        analogReadResolution(12);
        analogSetPinAttenuation(adcPin_, ADC_11db);   // idle voltage is ~3 V, 0 dB would clip
    }

    struct Thresholds {
        float blueMin;      // fb above this -> BLUE
        float blackMax;     // max(fr, fy) below this -> BLACK
        float yellowRatio;  // dy > yellowRatio * dr -> YELLOW, else RED
    };

    void setThresholds(float blueMin, float blackMax, float yellowRatio) {
        th_ = { blueMin, blackMax, yellowRatio };
    }
   
    // Update readings for each LED (non-blocking, call every loop)
    bool update() {
        switch (step_) {
        case 0:                                   // start: all LEDs off, let ambient settle
            allOff();
            stepStart_ = millis();
            step_ = 1;
            break;
        case 1:                                   // after 50 ms: read ambient, red LED on
            if (millis() - stepStart_ >= SETTLE_MS) {
                newAmbient_ = readVoltage();
                allOff();
                ledOn(0);
                stepStart_ = millis();
                step_ = 2;
            }
            break;
        case 2:                                   // after 50 ms: read red, yellow LED on
            if (millis() - stepStart_ >= SETTLE_MS) {
                newRed_ = readVoltage();
                allOff();
                ledOn(1);
                stepStart_ = millis();
                step_ = 3;
            }
            break;
        case 3:                                   // after 50 ms: read yellow, blue LED on
            if (millis() - stepStart_ >= SETTLE_MS) {
                newYellow_ = readVoltage();
                allOff();
                ledOn(2);
                stepStart_ = millis();
                step_ = 4;
            }
            break;
        case 4:                                   // after 50 ms: read blue, publish all four
            if (millis() - stepStart_ >= SETTLE_MS) {
                newBlue_ = readVoltage();
                allOff();
                ambient = newAmbient_;            // copy together so readSurface() never
                red     = newRed_;                // mixes old and new readings
                yellow  = newYellow_;
                blue    = newBlue_;
                step_ = 0;
                return true;                      // a full set of readings is ready
            }
            break;
        }
        return false;
    }

    Surface readSurface() {
        float amb = max(ambient, 0.05f);
        float dr = deltaRed();
        float dy = deltaYellow();
        float db = deltaBlue();

        float fr = dr / amb;   // fractional drops
        float fy = dy / amb;
        float fb = db / amb;

        // blue: only a blue surface pulls the blue reading down by most of ambient
        if (fb > th_.blueMin) return BLUE;

        // black: red and yellow LEDs barely change the reading (blue channel leaks, so ignored)
        if (max(fr, fy) < th_.blackMax) return BLACK;

        // yellow reflects the yellow LED about as well as red; red reflects yellow poorly
        if (dy > th_.yellowRatio * dr) return YELLOW;

        return RED;
    }
    
    //Calibrate the scale factor for each LED color
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

    //Set the scale factor for each LED color
    void setCalibration(float gr, float gy, float gb, float black) {
        gainRed = gr;
        gainYellow = gy;
        gainBlue = gb;
        blackLevel = black;
    }

    // Set LED brightness (0-255) from main, so you can tune without editing the class
    void setDuty(uint8_t r, uint8_t y, uint8_t b) {
        duty_[0] = r;
        duty_[1] = y;
        duty_[2] = b;
    }

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

    // Raw voltages with each LED on. Use this while tuning duty_
    void printRaw() {
        Serial.print("ambient: "); Serial.print(ambient, 3);
        Serial.print("  red: ");    Serial.print(red, 3);
        Serial.print("  yellow: "); Serial.print(yellow, 3);
        Serial.print("  blue: ");   Serial.println(blue, 3);
    }

private:
    static constexpr int NUM_LEDS = 3;
    static constexpr int SAMPLES = 12;
    static constexpr int CAL_ROUNDS = 10; 
    uint8_t adcPin_;
    uint8_t ledPins_[NUM_LEDS]; 
    float ambient = 0, red = 0, yellow = 0, blue = 0;
    Preferences prefs_; 

    Thresholds th_ = { 0.5f, 0.06f, 0.5f };

    static constexpr unsigned long SETTLE_MS = 15;
    uint8_t step_ = 0;                 // 0 = start, 1 = ambient, 2 = red, 3 = yellow, 4 = blue
    unsigned long stepStart_ = 0;
    float newAmbient_ = 0, newRed_ = 0, newYellow_ = 0, newBlue_ = 0;   // cycle in progress

    // NEW: LED brightness, 0-255. Red, yellow, blue. Start red/blue at ~10%
    uint8_t duty_[NUM_LEDS] = {25, 255, 25};

    //Calibration values
    float gainRed = 1.0f, gainYellow = 1.0f, gainBlue = 1.0f;
    float blackLevel = 0.1f;

    // Set duty cycles for each LED
    void ledOn(int i)  { analogWrite(ledPins_[i], duty_[i]); }
    void ledOff(int i) { analogWrite(ledPins_[i], 0); }

    // averaged input voltage in volts
    float readVoltage() {
        uint32_t sum = 0;
        for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(adcPin_);
        return sum / (float)SAMPLES / 1000.0f;
    }

    //Turn all LEDs off
    void allOff() {
        for (int i = 0; i < NUM_LEDS; i++) ledOff(i);  
    }

    //Calculate the delta for each LED color
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
            // update();
            step_ = 0;                // restart the cycle
            while (!update()) { }     // wait for one full fresh reading (blocking is fine here)
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