#include <FastLED.h>
#include <Wire.h>
#include <rgb_lcd.h>


#define LED_STRIP_PIN    2  
#define NUM_LEDS         18
#define LM35_PIN         A6   
#define SWITCH_PIN       3

CRGB leds[NUM_LEDS];
rgb_lcd lcd;

uint8_t paletteIndex = 0;

DEFINE_GRADIENT_PALETTE (heatmap_gp) {
  0,   0,   0,   0,   //black
  128, 255,   0,   0,   //red
  200, 255, 255,   0,   //bright yellow
  255, 255, 255, 255    //full white 
};

CRGBPalette16 myPal = heatmap_gp;

enum AnimationState {
  TEMPERATURE_MAP,
  HeatMap,
  Sounding
};

AnimationState currentAnimationState = TEMPERATURE_MAP;

const unsigned long temperatureUpdateInterval = 1000;
const unsigned long customAnimationInterval = 5;
unsigned long lastSwitchTime = 0;
unsigned long switchCooldown = 500;  // Reduce the cooldown time
unsigned long lastTemperatureUpdate = 0;
unsigned long lastCustomAnimationUpdate =0;

bool lastSwitchState = HIGH;
bool switchState = HIGH;
bool debounceSwitchState();

int soundSensorPin = A3;

void setup() {
  Serial.begin(9600);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(soundSensorPin, INPUT);

  lcd.begin(16, 2);
  lcd.setRGB(255, 0, 0);  // Set LCD backlight color (R, G, B)


  FastLED.addLeds<WS2813, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(155);
  FastLED.show();  // Initialize all LEDs to 'off'
}

void loop() {
  unsigned long currentMillis = millis();
  int soundLevel = analogRead(soundSensorPin);

  // Read the switch state with debounce
  switchState = debounceSwitchState();

  // Update animation based on the current state
  switch (currentAnimationState) {
  case TEMPERATURE_MAP:
     if (currentMillis - lastTemperatureUpdate >= temperatureUpdateInterval) {
        updateTemperatureMap();
        lastTemperatureUpdate = currentMillis;
        }
    break;

  case HeatMap:
     if (currentMillis - lastCustomAnimationUpdate >= customAnimationInterval) {
        updateCustomAnimation();
        lastCustomAnimationUpdate = currentMillis;
     }
    break;

  case Sounding:
    updateSounding();
    break;
     
}

  // Check for switch press and update state
  handleSwitchPress();

  // Display information in the Serial Monitor at a slower rate
  if (currentMillis % 500 == 0) {
    Serial.print("Temperature: ");
    Serial.print(getTemperature());
    Serial.print(" C | Button State: ");
    Serial.println(switchState);
  }
}



void handleSwitchPress() {
  static bool buttonWasPressed = false;
  if (switchState == LOW && !buttonWasPressed) {
    toggleLightMode();
    buttonWasPressed = true;
  } else if (switchState == HIGH) {
    buttonWasPressed = false;
  }
}


void updateTemperatureMap() {

  float temp_val = getTemperature();
  Serial.print("Temperature = ");
  Serial.print(temp_val);
  Serial.println(" Degree Celsius");

  // Display temperature on the LCD
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temp_val);
  lcd.print(" C  ");
  lcd.setCursor(0, 1);
  lcd.print("Lighting: Temp");

  // Map the temperature range to the color range
  CRGB color = mapTemperatureToColor(temp_val);

  // Set the color of the entire LED strip
  setColor(color);
}

void updateCustomAnimation() {
  // Your custom animation logic here
  CRGB color = customHeatMap();

  // Set the color of the entire LED strip
  setColor(color);

  lcd.setCursor(0, 0);
  lcd.print("Lighting:            ");
  lcd.setCursor(0, 1);
  lcd.print("Heat Map             ");
}

float getTemperature() {

  static unsigned long lastTemperatureReadTime = 50;
  static const unsigned long temperatureReadInterval = 500;

  unsigned long currentMillis = millis();
  if (currentMillis - lastTemperatureReadTime >= temperatureReadInterval) {
    lastTemperatureReadTime = currentMillis;
  }

  int temp_adc_val = analogRead(LM35_PIN);
  return (temp_adc_val * 4.88) / 10.0;  // Convert adc value to temperature in Celsius
}

CRGB mapTemperatureToColor(float temperature) {
  uint8_t r = map(temperature, 10, 50, 0, 255);
  uint8_t g = map(temperature, 10, 50, 255, 0);
  return CRGB(r, g, 0);
}

void setColor(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void toggleLightMode() {
  // Toggle the animation state
  currentAnimationState = static_cast<AnimationState>((currentAnimationState + 1) % 3);
}

CRGB customHeatMap() {
  CRGB color;

  // Example: Fill LEDs with a gradient palette
  fill_palette(leds, NUM_LEDS, paletteIndex, 255 / NUM_LEDS, myPal, 255, LINEARBLEND);

  EVERY_N_MILLISECONDS(5) {
    paletteIndex++;
  }
  FastLED.show();

  return color;
}

void updateSounding() {

  const int numReadings = 10;
  int soundReadings[numReadings];
  static unsigned long lastSoundReadTime = 0;
  const unsigned long soundReadInterval = 10;
  int soundIndex = 0;
  int totalSound = 0;

  



  unsigned long currentMillis = millis();
  if(currentMillis - lastSoundReadTime >= soundReadInterval) {
    int soundLevel = analogRead(soundSensorPin);
  

// LCD for the sound profile
      lcd.setCursor(0, 0);
      lcd.print("Decibels:       ");


 // smoothing    
      totalSound = totalSound - soundReadings[soundIndex] + soundLevel;
      soundReadings[soundIndex] = soundLevel;
      soundIndex = (soundIndex + 1) % numReadings;

  int averageSound = totalSound / numReadings;  

  lastSoundReadTime = currentMillis;  

  //debug
  Serial.print("Sound level:    ");
  Serial.println(averageSound);

// smoothed LED brightness
  int brightness = map(averageSound, 0, 1023, 0, 255);
       CRGB color = CRGB(brightness, 0, 0);

  // Set the color of the entire LED strip
      setColor(color);

    if (averageSound > 500) {
      // if high sound, change to a diff color
      color = CRGB(0, brightness, 0);
      setColor(color);
    }
  }
}

bool debounceSwitchState() {
  static unsigned long lastDebounceTime = 0;
  static unsigned long debounceDelay = 50;

  bool reading = digitalRead(SWITCH_PIN);

  if (reading != lastSwitchState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != switchState) {
      switchState = reading;
    }
  }

  lastSwitchState = reading;
  return switchState;
}

