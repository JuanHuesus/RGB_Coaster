#include <FastLED.h>
#include <avr/wdt.h>
#include <rgb_lcd.h>

// Define pins for data
// lcd SDA -> A4
// lcd SCL -> A5
#define LED_STRIP_PIN    2  
#define NUM_LEDS         18
#define BRIGHTNESS       20

#define LM35_PIN         6   
#define SWITCH_PIN       3
#define SOUND_PIN        3

#define SIMPLE_ANIMATION_SPEED  10
#define TEMP_UPDATE_INTERVAL    5000
#define AUDIO_SAMPLE_AMOUNT     10

// Define rgb palettes for the LED strip
uint8_t paletteIndex = 0;

// initialize the LED strip and LCD screen
CRGB leds[NUM_LEDS];
rgb_lcd lcd;

// Define palette
DEFINE_GRADIENT_PALETTE(heatmap_gp) {
  0,   0,   0,   0,   //black
  128, 255,   0,   0,   //red
  200, 255, 255,   0,   //bright yellow
  255, 255, 255, 255    //full white 
};

// Set palette
CRGBPalette16 myPal = heatmap_gp;

// Define
DEFINE_GRADIENT_PALETTE(soundPalette_gp) {
  0,    128, 0, 128,  // Purple
  3,   0,   200, 255,  // Cyan
  6,  0,   0,   255,  // Blue
  12,  255, 0,   255   // Pink
};

// Set
CRGBPalette16 soundPalette = soundPalette_gp;

// Switch states
enum AnimationState {
  Basic,
  Temperature,
  Sound
};

// Default led profile
AnimationState currentAnimationState = Basic;

// setup
void setup() {
  wdt_disable();  // Disable the watchdog and wait for more than 2 seconds
  delay(3000);  // Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration
  wdt_enable(WDTO_2S);  // Enable the watchdog with a timeout of 2 seconds

  // Setup input and pullup for button
  DDRD &= ~(1 << SWITCH_PIN); // Set as input (clear the bit)
  PORTD |= (1 << SWITCH_PIN); // Enable pull-up resistor (set the bit)

  // Configure the ADC
  // Set the reference voltage to AVCC (5V)
  ADMUX |= (1 << REFS0);
  
  // Set the prescaler to 128 (ADC clock frequency is 16MHz / 128 = 125kHz)
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  
  // Enable the ADC
  ADCSRA |= (1 << ADEN);

  lcd.begin(16, 2); // 16x2 lcd

  FastLED.addLeds<WS2813, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS); // 0-255
  FastLED.show();  // Initialize all LEDs to 'off'

  // Set up Timer1 to generate an interrupt every 1ms
  cli(); // Disable interrupts
  TCCR1A = 0; // Set Timer1 control register A to 0
  TCCR1B = 0; // Set Timer1 control register B to 0
  TCNT1 = 0;  // Initialize counter value to 0
  OCR1A = 15999; // Set the compare value for a 1ms interrupt (assuming a 16MHz clock)
  TCCR1B |= (1 << WGM12); // Set Timer1 to CTC (Clear Timer on Compare Match) mode
  TCCR1B |= (1 << CS11) | (1 << CS10); // Set prescaler to 64 for 16MHz clock
  TIMSK1 |= (1 << OCIE1A); // Enable Timer1 compare match interrupt
  sei(); // Enable interrupts
}

// High priority button timer interrupt
ISR(TIMER1_COMPA_vect) {
  // Handle button input with debounce
  static unsigned long lastDebounceTime = 0;
  static bool lastSwitchState = HIGH;
  static bool newSwitchState = LOW;
  int debounceDelay = 50;

  // read the state of the switch into a local variable:
  int reading = (PIND & (1 << SWITCH_PIN)) ? HIGH : LOW;
  
  // If the switch changed, due to noise or pressing:
  if (reading != lastSwitchState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  // Compare time
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // if the button state has changed:
    if (reading != newSwitchState) {
      newSwitchState = reading;

      // only toggle the LED if the new button state is LOW after press
      if (reading == LOW) {

        // Cycle the animation state
        currentAnimationState = static_cast<AnimationState>((currentAnimationState + 1) % 3);
      }
    }

    // Togle last state for later comparison
    lastSwitchState = !lastSwitchState;
  }
}

// loop through the different profiles with a button switch, print on the LCD
void loop() {
  unsigned long currentMillis = millis();
  
  // Update animation based on the current state
  switch (currentAnimationState) {
    case Basic:
      updateSimple();
      break;

    case Temperature:
      updateTemperature();
      break;

    case Sound:
      updateSound();
      break;
  }
   // Reset watchdog
  wdt_reset();
}

// LED animation logic for the simple profile, which just rotates a palette around the selected LEDs
void updateSimple() {
  fill_palette(leds, NUM_LEDS, paletteIndex, 255 / NUM_LEDS, myPal, 255, LINEARBLEND);

  EVERY_N_MILLISECONDS(SIMPLE_ANIMATION_SPEED) {
    paletteIndex++;
  }
  
  // LCD code to show what lighting profile is applied
  FastLED.show();
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("Lighting: Simple");
}

void updateTemperature() {
  float temp = getTemperature();
  
  // Display temperature on the LCD
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temp);
  lcd.print(" C   ");
  lcd.setCursor(0, 1);
  lcd.print("Lighting: Temp  ");

  // Map the temperature range to the color range
  CRGB color = mapTemperatureToColor(temp);

  // Set the color of the entire LED strip
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

// code for getting the temperature and converting it to Celsius
float getTemperature() {
  static unsigned long lastTemperatureReadTime = 0;
  unsigned long currentMillis = millis();
  int tempADC;

  // Compare time
  if (currentMillis - lastTemperatureReadTime >= TEMP_UPDATE_INTERVAL) {
    lastTemperatureReadTime = currentMillis;
    // Get temperature reading
    tempADC = readADC(LM35_PIN);
    // Convert adc value to temperature in Celsius
    tempADC = (tempADC / 2.048) - 6; // Calibration overshoots about 6c
  }
  return tempADC;
}

// color coding for the LED profile which reacts to the temperature
CRGB mapTemperatureToColor(float temperature) {
  uint8_t r = map(temperature, 10, 50, 0, 255);
  uint8_t g = map(temperature, 10, 50, 255, 0);
  return CRGB(r, g, 0);
}

// Code for processing the detected sound with smoothing
void updateSound() {
  int soundIndex = 0;
  int totalSound = 0;
  int soundReadings[AUDIO_SAMPLE_AMOUNT];

  // Get sound reading
  int soundLevel = readADC(SOUND_PIN);
  
  // Smoothing
  totalSound = totalSound - soundReadings[soundIndex] + soundLevel;
  soundReadings[soundIndex] = soundLevel;
  soundIndex = (soundIndex + 1) % AUDIO_SAMPLE_AMOUNT;

  int averageSound = totalSound / AUDIO_SAMPLE_AMOUNT;

  // LCD for the sound profile
  lcd.setCursor(0, 0);
  lcd.print("Sound level: ");
  lcd.print(averageSound);
  lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print("Lighting: Sound ");

  // Smoothed LED brightness
  int brightness = map(averageSound, 0, 100, 0, 255);

  CRGB color;

  // Logic for going between colors while reactive to sound
  int soundThreshold = 2;
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


int readADC(int pin){
  // Set ADC channel
  ADMUX &= ~((1 << MUX3) | (1 << MUX2) | (1 << MUX1) | (1 << MUX0));
  ADMUX |= pin;

  // Start ADC conversion
  ADCSRA |= (1 << ADSC);

  // Wait for conversion to complete
  while (ADCSRA & (1 << ADSC));
  return ADC;
}
