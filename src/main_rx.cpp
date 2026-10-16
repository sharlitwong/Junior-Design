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
const bool TIMED_TEST = false;   // true = run the timed encoder test, false = normal remote control
Encoder rightEnc(17, 18, true);   // right wheel, flipped so forward counts up
Encoder leftEnc(40, 39);          // left wheel

// fixed-radius turn settings
const float TRACK_WIDTH = 16.0;   // cm, between wheel centres (update when the chassis changes)
const float TURN_RADIUS = 10.0;   // cm, measured to the middle of the bot
const int TURN_SPEED = 2000;      // outer wheel speed, counts/s (sets how fast the turn happens)
const int TURN_INNER = TURN_SPEED * (TURN_RADIUS - TRACK_WIDTH / 2) / (TURN_RADIUS + TRACK_WIDTH / 2);


#define TEAM_TAG "BLUESCLUES:"

#define MOTOR_1_PWM    1
#define MOTOR_2_PWM    2
#define MOTOR_1_DIR1   4
#define MOTOR_1_DIR2   5
#define MOTOR_2_DIR1   6
#define MOTOR_2_DIR2   7
#define LED            38
#define NUM_LEDS       1

#define BUTTON_PIN     42

#define ADC_PIN 9
#define LED_EN  46

volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

const char* SERVER_IP = "10.5.9.24";  // IP of server ESP32
const uint16_t SERVER_PORT = 80;
const char* SERVER_PATH = "/ws";

const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PASSWORD = SECRET_PASS;

const char* CLIENT_ID = "BLUZ12CLUZ34"; //receiver ID

unsigned long lastSendTime = 0;

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

void setup() {
    distance.begin();
    leftEnc.begin();
    rightEnc.begin();

    Serial.begin(115200);
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

    //BUTTON PIN (for arduino state machine)
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonISR, FALLING); //GPIO pin, ISR, event
    rgb.begin();

    // pinMode(LED_EN, OUTPUT);
    // analogReadResolution(12);
    // analogSetPinAttenuation(ADC_PIN, ADC_11db);
}
    
void loop() {

    drive.update(); // every loop()
    distance.update();

    if (!TIMED_TEST) webSocket.loop();

    if (buttonPressed) {
        buttonPressed = false;
        // advance state machine 
        Serial.println("button pressed");
        currentState = static_cast<State>((currentState + 1) % NUM_STATES);
    }

    if(distance.delta() > 0.350) {
        currentState = STOP;
    }

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
        Serial.printf("Distance recorded: %.3f\n", distance.delta());
    }

    static State lastPrinted = NUM_STATES;   // impossible value, so the first pass prints
    if (currentState != lastPrinted) {
        lastPrinted = currentState;
        Serial.print("State ");
        Serial.println(currentState + 1);
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
        drive.setWheelSpeeds(1000, 1000);      // was motors.moveForward(100, 100)
        rgb.setPixelColor(0, rgb.Color(150, 0, 0));
        rgb.show();
        break;

    case BACKWARD:
        // e.g. orange BACKWARD
        // Serial.println("State 2");
        drive.setWheelSpeeds(-1000, -1000);    // was motors.moveBackward(100, 100)
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

    // float v = analogReadMilliVolts(ADC_PIN);
    // Serial.printf("V = %.3f\n", v);
    // delay(100);
}

