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

Pwm motor1_pwm(MOTOR_1_PWM, 0);   // channel 0
Pwm motor2_pwm(MOTOR_2_PWM, 1);   // channel 1

void setup() {
  Serial.begin(115200);
  delay(1000);

  //configure direction pins to be output
  Serial.println("Setting Motor Directions");

  pinMode(MOTOR_1_DIR1, OUTPUT);
  pinMode(MOTOR_1_DIR2, OUTPUT);
  pinMode(MOTOR_2_DIR1, OUTPUT);
  pinMode(MOTOR_2_DIR2, OUTPUT);
  
  Serial.println("Beginning PWM");
  Serial.println(motor1_pwm.begin() ? "Motor1 PWM OK" : "Motor1 PWM FAILED");
  Serial.println(motor2_pwm.begin() ? "Motor2 PWM OK" : "Motor2 PWM FAILED");
}

void loop() {
  //delay(1000);

  //Motor 1 direction
  Serial.println("Set direction for Motor 1");
  digitalWrite(MOTOR_1_DIR1, HIGH);
  digitalWrite(MOTOR_1_DIR2, LOW);

  //Motor 2 direction
  Serial.println("Set direction for Motor 2");
  digitalWrite(MOTOR_2_DIR1, LOW);
  digitalWrite(MOTOR_2_DIR2, HIGH);

  Serial.println("Writing PWM duty cycles");
  motor1_pwm.writePercent(100);
  motor2_pwm.writePercent(50);
}

