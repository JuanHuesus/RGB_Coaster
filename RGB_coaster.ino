#include <FastLED.h>
#include <Wire.h>
#include <rgb_lcd.h>

// Define pins for data

#define LED_STRIP_PIN    2  
#define NUM_LEDS         18
#define LM35_PIN         A6   
#define SWITCH_PIN       3
#define soundSensorPin   A3

// initialize the LED strip and LCD screen
CRGB leds[NUM_LEDS];
rgb_lcd lcd;


// Define rgb palettes for the LED strip
uint8_t paletteIndex = 0;

DEFINE_GRADIENT_PALETTE (heatmap_gp) {
  0,   0,   0,   0,   //black
  128, 255,   0,   0,   //red
  200, 255, 255,   0,   //bright yellow
  255, 255, 255, 255    //full white 
};

CRGBPalette16 myPal = heatmap_gp;

DEFINE_GRADIENT_PALETTE(soundPalette_gp) {
  0,    128, 0, 128,  // Purple
  64,   0,   200, 255,  // Cyan
  128,  0,   0,   255,  // Blue
  255,  255, 0,   255   // Pink
};

CRGBPalette16 soundPalette = soundPalette_gp;

// variables for the button switch
enum AnimationState {
  TEMPERATURE_MAP,
  HeatMap,
  Sounding
};

// variables for reading the temperature etc
AnimationState currentAnimationState = TEMPERATURE_MAP;

const unsigned long temperatureUpdateInterval = 1000;
const unsigned long customAnimationInterval = 5;
const unsigned long soundReadInterval = 10;

unsigned long lastTemperatureUpdate = 0;
unsigned long lastCustomAnimationUpdate = 0;
unsigned long lastSoundReadTime = 0;

unsigned long lastSwitchTime = 0;
unsigned long switchCooldown = 500;  // Reduce the cooldown time

bool lastSwitchState = HIGH;
bool switchState = HIGH;
bool debounceSwitchState();


// setup for Serial Monitor, pins and the LED strip
void setup() {
  Serial.begin(9600);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(soundSensorPin, INPUT);

  lcd.begin(16, 2); 


  FastLED.addLeds<WS2813, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.show();  // Initialize all LEDs to 'off'
}

// loop through the different profiles with a button switch, print on the LCD and serial for debugging
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
      if (currentMillis - lastSoundReadTime >= soundReadInterval) {
        updateSounds();
        lastSoundReadTime = currentMillis;
      }
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

  // LED animation logic for the HeatMap profile, which just rotates a palette around the selected LEDs
void updateCustomAnimation() {
  fill_palette(leds, NUM_LEDS, paletteIndex, 255 / NUM_LEDS, myPal, 255, LINEARBLEND);

  EVERY_N_MILLISECONDS(1) {
    paletteIndex++;
  }
  // LCD code to show what lighting profile is applied
  FastLED.show();
  lcd.setCursor(0, 0);
  lcd.print("Lighting:            ");
  lcd.setCursor(0, 1);
  lcd.print("Heat Map             ");
}

  // code for getting the temperature and converting it to Celsius
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

// color coding for the LED profile which reacts to the temperature
CRGB mapTemperatureToColor(float temperature) {
  uint8_t r = map(temperature, 10, 50, 0, 255);
  uint8_t g = map(temperature, 10, 50, 255, 0);
  return CRGB(r, g, 0);
}

  // sets the color evenly across the appointed LEDs
void setColor(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

   
void toggleLightMode() {
  // Toggle the animation state
  currentAnimationState = static_cast<AnimationState>((currentAnimationState + 1) % 3);
}

  // Code for processing the detected sound with smoothing
void updateSounds() {
  const int numReadings = 10;

  int soundReadings[numReadings];

  int soundIndex = 0;
  int totalSound = 0;

  int soundLevel = analogRead(soundSensorPin);

  // Smoothing
  totalSound = totalSound - soundReadings[soundIndex] + soundLevel;
  soundReadings[soundIndex] = soundLevel;
  soundIndex = (soundIndex + 1) % numReadings;

  int averageSound = totalSound / numReadings;
  
  // Debug
  Serial.print("Sound level: ");
  Serial.println(averageSound);

  // LCD for the sound profile
  lcd.setCursor(0, 0);
  lcd.print("Sound level: ");
  lcd.setCursor(0, 1);
  lcd.print(averageSound);
  lcd.setCursor(2, 1);
  lcd.print("                "); // Clear the line

  // Smoothed LED brightness
  int brightness = map(averageSound, 0, 100, 0, 255);

  CRGB color;

  // Logic for going between colors while reactive to sound
  int soundThreshold = 40;
  if (averageSound > soundThreshold) {
  // If the sound level exceeds the threshold, smoothly transition to a different color
    color = CRGB(0, brightness, 0);
  } else {
  // Otherwise, smoothly transition back to the color based on the sound level
    color = ColorFromPalette(soundPalette, brightness);
  }

  // Set the color and brightness of the LED strip
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

  // debounce for the button
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

