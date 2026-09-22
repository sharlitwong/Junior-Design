/*
 Junior Design
 Blues Clues
*/
#include <Adafruit_NeoPixel.h>
#include <pwm.h>

#define MOTOR_1_PWM    1
#define MOTOR_2_PWM    2
#define MOTOR_1_DIR1   4
#define MOTOR_1_DIR2   5
#define MOTOR_2_DIR1   6
#define MOTOR_2_DIR2   7
#define LED            38
#define NUM_LEDS       1

#define BUTTON_PIN     42

volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

void IRAM_ATTR handleButtonISR() { //Interrupt Service Routine for pushbutton
  unsigned long now = millis();
  if (now - lastInterruptTime > 200) {  // debounce
    buttonPressed = true;
    lastInterruptTime = now;
  }
}

enum State {
    STATE_1,
    STATE_2,
    STATE_3,
    STATE_4,
    STATE_5,
    STATE_6,
    STATE_7,
    NUM_STATES
};
State currentState = STATE_1;

Pwm motor1_pwm(MOTOR_1_PWM, 0);   // channel 0
Pwm motor2_pwm(MOTOR_2_PWM, 1);   // channel 1

Adafruit_NeoPixel rgb(NUM_LEDS, LED, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    delay(1000);

    //MOTOR PINS configure direction pins to be output
    pinMode(MOTOR_1_DIR1, OUTPUT);
    pinMode(MOTOR_1_DIR2, OUTPUT);
    pinMode(MOTOR_2_DIR1, OUTPUT);
    pinMode(MOTOR_2_DIR2, OUTPUT);

    //BUTTON PIN (for arduino state machine)
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonISR, FALLING); //GPIO pin, ISR, event

    motor1_pwm.begin();
    motor2_pwm.begin();

    rgb.begin();
}

void loop() {
    //delay(1000);

    if (buttonPressed) {
        buttonPressed = false;
        // advance state machine 
        Serial.println("button pressed");
        currentState = static_cast<State>((currentState + 1) % NUM_STATES);
    }

    //Motor 1 direction
    //Serial.println("Set direction for Motor 1");
    digitalWrite(MOTOR_1_DIR1, HIGH);
    digitalWrite(MOTOR_1_DIR2, LOW);

    //Motor 2 direction
    //Serial.println("Set direction for Motor 2");
    digitalWrite(MOTOR_2_DIR1, LOW);
    digitalWrite(MOTOR_2_DIR2, HIGH);

    //Serial.println("Writing PWM duty cycles");
    motor1_pwm.writePercent(100);
    motor2_pwm.writePercent(50);

    //STATE MACHINE

    switch (currentState) {
    case STATE_1:
        // e.g. red
        Serial.println("State 1");
        rgb.setPixelColor(0, rgb.Color(150, 0, 0));
        rgb.show();
        break;

    case STATE_2:
        // e.g. orange
        Serial.println("State 2");
        rgb.setPixelColor(0, rgb.Color(255, 60, 0));
        rgb.show();
        break;

    case STATE_3:
        // e.g. yellow
        Serial.println("State 3");
        rgb.setPixelColor(0, rgb.Color(150, 60, 0));
        rgb.show();
        break;

    case STATE_4:
        // e.g. green
        Serial.println("State 4");
        rgb.setPixelColor(0, rgb.Color(0, 150, 0));
        rgb.show();
        break;

    case STATE_5:
        // e.g. blue
        Serial.println("State 5");
        rgb.setPixelColor(0, rgb.Color(0, 0, 150));
        rgb.show();
        break;

    case STATE_6:
        // e.g. purple
        Serial.println("State 6");
        rgb.setPixelColor(0, rgb.Color(100, 0, 150));
        rgb.show();
        break;

    case STATE_7:
        // e.g. purple
        Serial.println("State 7");
        rgb.setPixelColor(0, rgb.Color(150, 150, 150));
        rgb.show();
        break;

    default:
        // shouldn't happen, but good practice to handle it
        break;
    }
}

