/*
 Junior Design
 Blues Clues
FOR RECEIVING WEBSOCKET STATES
*/
#include <Adafruit_NeoPixel.h>
// #include <pwm.h>
#include <esp32_secrets.h>
#include <websocket.h>
#include <motor.h>
#include <encoder.h>
#include <drive.h>
#include <distance.h>
#include <color.h>
#include <battery.h>
#include <Preferences.h>   // saves the learned motor speeds in flash across reboots

#define TEAM_TAG "BLUESCLUES:"
#define MOTOR_1_PWM    1 //motor 1 is left motor
#define MOTOR_2_PWM    2 //motor 2 is right motor
#define MOTOR_1_DIR1   4
#define MOTOR_1_DIR2   5
#define MOTOR_2_DIR1   6
#define MOTOR_2_DIR2   7
#define LED            38
#define NUM_LEDS       1

#define BUTTON_PIN     42

#define ADC_PIN 9
#define LED_EN  46

#define BLINK1_ADC 10
#define BLINK1_R 11
#define BLINK1_G 12
#define BLINK1_B 13

#define BLINK2_ADC 8
#define BLINK2_R 15
#define BLINK2_G 16
#define BLINK2_B 14


// battery monitor: BMS net on the PCB, through a divider
// TODO: set these to the actual resistors on the PCB before powering from the battery
#define BMS_PIN        3
#define BAT_R_TOP      300000.0f   // battery+ to pin, ohms
#define BAT_R_BOTTOM   100000.0f   // pin to GND, ohms

// false = ignore the distance sensor (for testing movement while it isn't wired/aimed);
// set back to true so the bot stops for obstacles
const bool DISTANCE_STOP = false;
const bool TIMED_TEST = false;   // true = run the timed encoder test, false = normal remote control

Encoder rightEnc(17, 18, true);   // right wheel, flipped so forward counts up A, B
Encoder leftEnc(40, 39);          // left wheel

// fixed-radius turn settings
const float TRACK_WIDTH = 16.0;   // cm, between wheel centres (update when the chassis changes)
const float TURN_RADIUS = 10.0;   // cm, measured to the middle of the bot
const int TURN_SPEED = 2000;      // outer wheel speed, counts/s (sets how fast the turn happens)
const int TURN_INNER = TURN_SPEED * (TURN_RADIUS - TRACK_WIDTH / 2) / (TURN_RADIUS + TRACK_WIDTH / 2);

// motor supply: false = L293D motor supply comes from a 5 V regulator (no limit needed)
//               true  = regulator bypassed, L293D gets the battery directly, so duty
//                       is limited to keep the average motor voltage under MOTOR_MAX_V
const bool MOTORS_ON_BATTERY = true;
const float MOTOR_MAX_V = 6.0f;     // Pololu #1520: 4.5 V nominal, 3-6 V recommended
const float L293_DROP_V = 2.0f;     // approximate voltage lost inside the L293D

volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

const char* SERVER_IP = "10.5.9.24";  // IP of server ESP32
const uint16_t SERVER_PORT = 80;
const char* SERVER_PATH = "/ws";
const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PASSWORD = SECRET_PASS;
const char* CLIENT_ID = "BLUZ12CLUZ34"; //receiver ID

enum State {
    FORWARD,
    BACKWARD,
    PIVOTCW,
    PIVOTCCW,
    STOP,
    TURN_RIGHT,
    TURN_LEFT,
    NUM_STATES
};

unsigned long lastSendTime = 0;

// State currentState = FORWARD;
// State currentState = TIMED_TEST ? STOP : FORWARD;
State currentState = STOP;

Websocket webSocket(
    WIFI_SSID,
    WIFI_PASSWORD,
    SERVER_IP,
    SERVER_PORT,
    SERVER_PATH,
    CLIENT_ID
);

Distance distance(ADC_PIN, LED_EN); 
ColorSensor color1(BLINK1_ADC, BLINK1_R, BLINK1_G, BLINK1_B); 
ColorSensor color2(BLINK2_ADC, BLINK2_R, BLINK2_G, BLINK2_B);
ColorSensor::Surface surface1;
ColorSensor::Surface surface2;
Battery battery(BMS_PIN, BAT_R_TOP, BAT_R_BOTTOM);

void onWebSocketMessage(const String& message) {
    Serial.print("Received: ");
    Serial.print(message);

    if(webSocket.isAuthenticated()) {
        String echo = "\"echo\":\""; // "echo:":"
        int echo_ind = message.indexOf(echo); //where "echo":" starts in message

        if(echo_ind > 0) {
          int message_open = echo_ind + echo.length(); //opening quote of actual message
          int message_end = message.indexOf("\"", message_open);

          if(message_end > message_open) {
            String value = message.substring(message_open, message_end); //inclusive, uninclusive
            const int taglen = strlen(TEAM_TAG);

            if(value.startsWith(TEAM_TAG) && value.length() == taglen + 1) {
              char digit = value[taglen];
              if(digit >= '1' && digit <= '7') {
                State selectedState = static_cast<State>(digit - '1');
                currentState = selectedState;
              }
            }
          }
        }
    }
}

void IRAM_ATTR handleButtonISR() { //Interrupt Service Routine for pushbutton
  unsigned long now = millis();
  if (now - lastInterruptTime > 200) {  // debounce
    buttonPressed = true;
    lastInterruptTime = now;
  }
}

Pwm motor1_pwm(MOTOR_1_PWM, 0);   // channel 0
Pwm motor2_pwm(MOTOR_2_PWM, 1);   // channel 1
Motor motors(motor1_pwm, motor2_pwm);
Drive drive(motors, leftEnc, rightEnc);   

Adafruit_NeoPixel rgb(NUM_LEDS, LED, NEO_GRB + NEO_KHZ800);

Preferences prefs;
float savedLeftMax = 0, savedRightMax = 0;

// writes the learned speeds to flash, only if they moved more than 2% (saves flash wear)
void saveLearnedSpeeds() {
    float l = drive.leftMaxSpeed(), r = drive.rightMaxSpeed();
    if (fabsf(l - savedLeftMax) < 0.02f * savedLeftMax &&
        fabsf(r - savedRightMax) < 0.02f * savedRightMax) return;
    prefs.putFloat("left", l);
    prefs.putFloat("right", r);
    savedLeftMax = l;
    savedRightMax = r;
    Serial.printf("Saved learned max speeds: L=%.0f R=%.0f\n", l, r);
}

// battery trip log in flash: read it over USB on the next boot, since opening the
// serial monitor restarts the ESP32 and clears the blink
void logBatteryTrip() {
    prefs.putUInt("tripCount", prefs.getUInt("tripCount", 0) + 1);
    prefs.putUChar("tripWhy", battery.tripReason());
    prefs.putFloat("tripLive", battery.tripLive());
    prefs.putFloat("tripRest", battery.tripRest());
    Serial.printf("BATTERY EMPTY (%s): live=%.2f V rest=%.2f V\n",
                  battery.tripReason() == Battery::UNDER_LOAD ? "sagged under load" : "resting voltage low",
                  battery.tripLive(), battery.tripRest());
}

// lowest live voltage while driving, saved when it gets meaningfully lower
void saveLowestSag() {
    float lowest = battery.minLoaded();
    if (lowest > 50.0f) return;   // hasn't driven on battery yet
    if (lowest < prefs.getFloat("minLive", 99.0f) - 0.05f) prefs.putFloat("minLive", lowest);
}

void printBatteryLog() {
    uint32_t n = prefs.getUInt("tripCount", 0);
    float minLive = prefs.getFloat("minLive", 0);
    if (n == 0) {
        Serial.println("Battery log: no empty trips recorded");
    } else {
        Serial.printf("Battery log: %u empty trips. Last one: %s, live=%.2f V, rest before=%.2f V\n",
                      n, prefs.getUChar("tripWhy", 0) == Battery::UNDER_LOAD ? "SAGGED UNDER LOAD" : "RESTING VOLTAGE LOW",
                      prefs.getFloat("tripLive", 0), prefs.getFloat("tripRest", 0));
    }
    if (minLive > 0) Serial.printf("Battery log: lowest voltage seen while driving = %.2f V\n", minLive);
}

void setup() {
    Serial.begin(115200);
    distance.begin();
    color1.begin();
    color2.begin();
    leftEnc.begin();
    rightEnc.begin();

    battery.begin();
    // bench supply, breadboard resistors (redo with the PCB resistors):
    // uncalibrated 8.82 V read vs 9.058 V meter, 7.18 V read vs 7.39 V meter
    // both give meter/read = 1.028, so a plain gain fits, no offset needed
    battery.setCalibration(1.028f);

    // restore what the bot learned last time it drove (0 = nothing saved yet, keeps default)
    prefs.begin("drive", false);
    drive.setMaxSpeeds(prefs.getFloat("left", 0), prefs.getFloat("right", 0));
    savedLeftMax = drive.leftMaxSpeed();
    savedRightMax = drive.rightMaxSpeed();
    Serial.printf("Loaded max speeds: L=%.0f R=%.0f\n", savedLeftMax, savedRightMax);
    printBatteryLog();
    // setup(): replace the two webSocket lines
    if (!TIMED_TEST) {
        webSocket.onMessage(onWebSocketMessage);
        webSocket.begin();
    }

    delay(1000);

    //MOTOR PINS configure direction pins to be output
    pinMode(MOTOR_1_DIR1, OUTPUT);
    pinMode(MOTOR_1_DIR2, OUTPUT);
    pinMode(MOTOR_2_DIR1, OUTPUT);
    pinMode(MOTOR_2_DIR2, OUTPUT);
    motor1_pwm.begin();
    motor2_pwm.begin();

    motors.begin();

    // BUTTON PIN (for arduino state machine)
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonISR, FALLING); //GPIO pin, ISR, event
    rgb.begin();

    color1.setDuty(15,255,25);
    color2.setDuty(15,255,25);

/****************COLOR SENSOR TESTING*******************************************/
    // color1.calibrateAll();
    // Serial.println("Calibration done");
    // color1.printCalibration(); 
/*******************************************************************************/
}
    
void loop() {
    drive.update(); // every loop()
    distance.update();
    color1.update();
    color2.update();
    battery.setLoaded(drive.leftTarget() != 0 || drive.rightTarget() != 0);   // motors running?
    if (battery.update() && MOTORS_ON_BATTERY) {
        // duty x (battery - L293D drop) = average motor voltage, so cap the duty to
        // hit MOTOR_MAX_V. Fresh 9.5 V -> 80%; the cap rises to 100% as the battery drains.
        // No battery reading (USB only): use the fresh-battery cap to be safe
        float available = (battery.present() ? battery.voltage() : 9.6f) - L293_DROP_V;
        drive.setMaxDuty(available > 0 ? 100.0f * MOTOR_MAX_V / available : 100.0f);
    }
    if (battery.takeTrip()) logBatteryTrip();

    if (!TIMED_TEST) webSocket.loop();

    if (buttonPressed) {
        buttonPressed = false;
        // advance state machine 
        Serial.println("button pressed");
        currentState = static_cast<State>((currentState + 1) % NUM_STATES);
    }

    if(DISTANCE_STOP && distance.delta() > 0.350) {
        currentState = STOP;
    }

    Serial.println("Raw Values (color 1):");
    color1.printRaw();
    Serial.println("Raw Values (color 2):");
    color2.printRaw();
    
    surface1 = color1.readSurface();
    surface2 = color2.readSurface();

    Serial.println("Color 1 surface:");
    if (surface1 == color1.RED)         Serial.println("RED");
    else if (surface1 == color1.YELLOW) Serial.println("YELLOW");
    else if (surface1 == color1.BLUE)   Serial.println("BLUE");
    else                                Serial.println("BLACK");

    Serial.println("Color 2 surface:");
    if (surface2 == color2.RED)         Serial.println("RED");
    else if (surface2 == color2.YELLOW) Serial.println("YELLOW");
    else if (surface2 == color2.BLUE)   Serial.println("BLUE");
    else                                Serial.println("BLACK");
    

    delay(200);
    // battery too low to keep the 5 V regulator working: stop and wait for a swap
    if (battery.empty()) {
        currentState = STOP;
    }

    static bool lowWarned = false;
    if (battery.low() && !lowWarned) {
        Serial.printf("BATTERY LOW: %.2f V at rest (%d%%), replace soon\n", battery.restVoltage(), battery.percent());
    }
    lowWarned = battery.low();

    // if (distance.update()) {
    //     // A new reading is ready
    //     Serial.printf("Distance delta: %.3f V\n", distance.delta());
    // }

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 50) {   // print 20x per second, not thousands
        lastPrint = millis();
        //Serial.printf("A=%d B=%d\n", digitalRead(17), digitalRead(18));
        // Serial.printf("A=%d B=%d\n", digitalRead(39), digitalRead(40));   // prints B's value under "A"
        // Serial.printf("L=%ld R=%ld\n", (long)leftEnc.count(), (long)rightEnc.count());
        // Serial.printf("L=%ld R=%ld  Lspd=%.0f Rspd=%.0f\n",
        //               (long)leftEnc.count(), (long)rightEnc.count(),
        //               drive.leftSpeed(), drive.rightSpeed());
        // target vs measured speed (counts/s) and duty %; duty stuck at 100 = motors maxed out
        Serial.printf("L tgt=%d spd=%.0f duty=%.0f | R tgt=%d spd=%.0f duty=%.0f | dist=%.3f | bat=%.2fV rest=%.2fV %d%% max=%.0f | learned L=%.0f R=%.0f\n",
                      drive.leftTarget(), drive.leftSpeed(), drive.leftDuty(),
                      drive.rightTarget(), drive.rightSpeed(), drive.rightDuty(),
                      distance.delta(), battery.voltage(), battery.restVoltage(),
                      battery.percent(), drive.maxDuty(),
                      drive.leftMaxSpeed(), drive.rightMaxSpeed());
    }

    static State lastPrinted = NUM_STATES;   // impossible value, so the first pass prints
    if (currentState != lastPrinted) {
        lastPrinted = currentState;
        Serial.print("State ");
        Serial.println(currentState + 1);
        if (currentState == STOP) {
            saveLearnedSpeeds();
            saveLowestSag();
        }
    }

    if (TIMED_TEST) {
        static unsigned long testStart = millis();   // set on the first pass through loop()
        static int phase = 0;                        // 0 = waiting, 1 = driving, 2 = coasting, 3 = done
        unsigned long t = millis() - testStart;

        if (phase == 0 && t >= 2000) {               // after 2 s: zero counts and go
            leftEnc.reset();
            rightEnc.reset();
            currentState = FORWARD;
            phase = 1;
        } else if (phase == 1 && t >= 5000) {        // after 3 s of driving: stop
            currentState = STOP;
            phase = 2;
        } else if (phase == 2 && t >= 6000) {        // after 1 s of coasting: report
            long l = leftEnc.count();
            long r = rightEnc.count();
            Serial.printf("FINAL  L=%ld  R=%ld  R-L=%ld\n", l, r, r - l);
            phase = 3;
        }
    }

    switch (currentState) {
    case FORWARD:
        // e.g. red FORWARD
        // Serial.println("State 1");
        drive.setWheelSpeeds(1500, 1500);     // was motors.moveForward(100, 100)
        rgb.setPixelColor(0, rgb.Color(150, 0, 0));
        rgb.show();
        break;

    case BACKWARD:
        // e.g. orange BACKWARD
        // Serial.println("State 2");
        drive.setWheelSpeeds(-1500, -1500);   // was motors.moveBackward(100, 100)
        rgb.setPixelColor(0, rgb.Color(255, 60, 0));
        rgb.show();
        break;

    case PIVOTCW:
        // e.g. yellow PIVOT CW
        // Serial.println("State 3");
        drive.setWheelSpeeds(800, -800);       // was motors.pivotCW(100)
        rgb.setPixelColor(0, rgb.Color(150, 60, 0));
        rgb.show();
        break;

    case PIVOTCCW:
        // e.g. green PIVOT CCW
        // Serial.println("State 4");
        drive.setWheelSpeeds(-800, 800);       // was motors.pivotCCW(100)
        rgb.setPixelColor(0, rgb.Color(0, 150, 0));
        rgb.show();
        break;

    case STOP:
        // e.g. blue STOP 
        // Serial.println("State 5");
        rgb.setPixelColor(0, rgb.Color(0, 0, 150));
        drive.stop();
        rgb.show();
        break;

    case TURN_LEFT:
        // e.g. purple TURN LEFT
        // Serial.println("State 6");
        drive.setWheelSpeeds(TURN_INNER, TURN_SPEED);   // left wheel on the inside of the curve
        rgb.setPixelColor(0, rgb.Color(100, 0, 150));
        rgb.show();
        break;

    case TURN_RIGHT:
        // e.g. purple TURN RIGHT 
        // Serial.println("State 7");
        drive.setWheelSpeeds(TURN_SPEED, TURN_INNER);   // right wheel on the inside of the curve
        rgb.setPixelColor(0, rgb.Color(150, 150, 150));
        rgb.show();
        break;

    default:
        // shouldn't happen
        break;
    }

    // empty battery: blink red over the state color so it's obvious why the bot stopped
    // FAST blink (100 ms) = battery sagged below 6 V while driving
    // SLOW blink (500 ms) = resting voltage below 7.2 V
    if (battery.empty()) {
        unsigned long period = battery.tripReason() == Battery::UNDER_LOAD ? 100 : 500;
        bool on = (millis() / period) % 2;
        rgb.setPixelColor(0, on ? rgb.Color(255, 0, 0) : rgb.Color(0, 0, 0));
        rgb.show();
    }
}

