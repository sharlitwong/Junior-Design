#include <Adafruit_NeoPixel.h>

#define LED_PIN    38
#define NUM_LEDS   1

// Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Running sanity check...");

  // strip.begin();
  // strip.show(); // turn all LEDs off

  // // Blink RGB once each
  // strip.setPixelColor(0, strip.Color(255, 0, 0));
  // strip.show();
  // delay(500);

  // strip.setPixelColor(0, strip.Color(0, 255, 0));
  // strip.show();
  // delay(500);

  // strip.setPixelColor(0, strip.Color(0, 0, 255));
  // strip.show();
  // delay(500);

  // Serial.println("Sanity check complete!");
}

void loop() {
  Serial.println("Still running...");
  delay(1000);
}
