#include <FastLED.h>
#include <Wire.h>
#include <rgb_lcd.h>

#define LED_STRIP_PIN    6    
#define NUM_LEDS         30   
#define LM35_PIN         A6   

CRGB leds[NUM_LEDS];
rgb_lcd lcd;

void setup() {
  Serial.begin(9600);
  
  lcd.begin(16, 2);
  lcd.setRGB(255, 0, 0);  // Set LCD backlight color (R, G, B)

  lcd.print("Temperature:");
  lcd.setCursor(0, 1);
  lcd.print("LED Color:");

  FastLED.addLeds<NEOPIXEL, LED_STRIP_PIN>(leds, NUM_LEDS);
  FastLED.show();  // Initialize all LEDs to 'off'
}

void loop() {
  static uint32_t lastUpdate = 0;
  static const uint32_t updateInterval = 1000; // Update every 1 second

  // Check if it's time to update
  if (millis() - lastUpdate >= updateInterval) {
    int temp_adc_val = analogRead(LM35_PIN);
    float temp_val = (temp_adc_val * 4.88) / 10.0;  // Convert adc value to temperature in Celsius

    Serial.print("Temperature = ");
    Serial.print(temp_val);
    Serial.println(" Degree Celsius");

    // Display temperature on the LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp_val);
    lcd.print(" C  ");

    // Map the temperature range to the color range
    CRGB color = mapTemperatureToColor(temp_val);

    // Set the color of the entire LED strip
    setColor(color);

    lastUpdate = millis(); // Update the last update time
  }

}

CRGB mapTemperatureToColor(float temperature) {
  // Map your temperature range to the color range
  // For example, you can use a gradient from blue to red

  // Assuming temperature range is from 0 to 100 degrees Celsius
  uint8_t r = map(temperature, 0, 100, 0, 255);
  uint8_t b = 255 - r;
  CRGB color = CRGB(r, 0, b);

  return color;
}

void setColor(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}
