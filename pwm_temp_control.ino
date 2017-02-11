// Temp Sensor: DS1820
// https://arduino-info.wikispaces.com/YourDuinoStarter_TemperatureSensor

// PWM Fan
// http://www.uchobby.com/index.php/2007/09/23/arduino-temperature-controlled-pc-fan/
// http://forum.arduino.cc/index.php?topic=18742.0
// http://electronics.stackexchange.com/questions/84912/controlling-a-4-wired-fan-pwm-signal-using-arduino-allows-only-two-settings
// >> http://arduino.stackexchange.com/questions/25609/need-help-to-set-pwm-frequency-to-25khz/25623#25623

// Button read
// https://www.arduino.cc/en/Tutorial/DigitalReadSerial
// http://www.allaboutcircuits.com/technical-articles/using-interrupts-on-arduino/ interrupt


// One Wire Data Pin = 2
// Button pin = 3
// PWM Pins = 9, 10
// LED Pin = 13 (onboard led?)

// --- THERMAL STUFF ---
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

int currentTemp = 0;

// Minimal PWM signal (if too low, fans will spin up)
unsigned int minPWM = 4;

// --- BUTTON STUFF ---
// Setup button and led
const int buttonPin = 7;     // the number of the pushbutton pin
const int ledPin =  13;      // the number of the LED pin

// variables will change:
volatile int buttonState = 0;         // variable for reading the pushbutton status


// --- LOGIC RELATED ---
// TT stands for target temperature
int TTList[] = {10, 20, 30, 40, 50};
int TTAmount = 5;
int currentTTIndex = 2; // set initial temperature to 20 °C

// LED blink stuff
int ledState = LOW;               // ledState used to set the LED
unsigned long ledPreviousMillis = 0; // will store last time LED was updated
const long ledInterval = 200;       // interval at which to blink (milliseconds)
int ledBlinkCount = 0;

void setup(void)
{
  // --- THERMAL STUFF ---
  // start serial port
  Serial.begin(9600);
  sensors.begin();

  // --- PWM STUFF ---
  // Configure Timer 1 for PWM @ 25 kHz.
  TCCR1A = 0;           // undo the configuration done by...
  TCCR1B = 0;           // ...the Arduino core library
  TCNT1  = 0;           // reset timer
  TCCR1A = _BV(COM1A1)  // non-inverted PWM on ch. A
//           | _BV(COM1B1)  // same on ch; B
           | _BV(WGM11);  // mode 10: ph. correct PWM, TOP = ICR1
  TCCR1B = _BV(WGM13)   // ditto
           | _BV(CS10);   // prescaler = 1
  ICR1   = 320;         // TOP = 320
  // Set the PWM pins as output.
  pinMode( 9, OUTPUT);

  // --- BUTTON STUFF ---
  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  // Attach an interrupt to the ISR vector
  // Interrupt 1 is pin 3 on Arduino Uno and Pro Mini
  attachInterrupt(1, pin_ISR, CHANGE);
}


void loop(void)
{
  unsigned long ledCurrentMillis = millis();
  if (ledCurrentMillis - ledPreviousMillis >= ledInterval) {
    // save the last time you blinked the LED
    ledPreviousMillis = ledCurrentMillis;
  
    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      // if the led is off and we are in a normal blink cycle
      if (ledBlinkCount <= currentTTIndex) {
        ledState = HIGH;
        digitalWrite(ledPin, ledState);

      // if the led is off, and we have waitet 3 additional loops
      // do the temperature magic now (and reset the led loop)
      } else if (ledBlinkCount <= currentTTIndex + 2) {
        // Reset the blink count
        ledBlinkCount = -1;

        // --- THERMAL STUFF ---
        sensors.requestTemperatures();
        
        Serial.print("Inside temperatures:         ");
        float currentTemp = sensors.getTempCByIndex(0);
        Serial.println(currentTemp);

        Serial.print("Outside temperature:         ");
        float outsideTemp = sensors.getTempCByIndex(1);
        Serial.println(outsideTemp);
        
        Serial.print("Target temperature:          ");
        Serial.println(TTList[currentTTIndex]);

        // --- LOGIC STUFF ---
        // Set fan output to 1-320:
        unsigned int pwmValue = minPWM;
        float tempDiff = (currentTemp - TTList[currentTTIndex]) * 100;
        Serial.print("Temperatur differencial is:  ");
        Serial.print(tempDiff);
        Serial.println(" (%100)");
     
        if (outsideTemp > currentTemp) {
          Serial.println("> Unable to cool, outside temperature is higher than inside");
        } else {
          int dutyCycle = 0;
          if (tempDiff <= 0) {
            Serial.println("> No cooling is required");
          } else {
            if (tempDiff > 1000) {
              Serial.println("> A lot of cooling is required!");
              dutyCycle = 100;
            } else {
              Serial.println("> Dynamically setting fan speed");
              dutyCycle = map(tempDiff, 0, 1000, 0,99);
            }
            Serial.print("> Cooling is required, calculated duty cycle is ");
            Serial.print(dutyCycle);
            Serial.println("%");
          }
          pwmValue = map(dutyCycle, 0, 100, minPWM, 320);
        }

        // control the fans
        Serial.print("> Setting PWM value to (1-320) ");
        Serial.println(pwmValue);
        analogWrite25k( 9, pwmValue);
      }
      ledBlinkCount++;
    } else {
      ledState = LOW;
      digitalWrite(ledPin, ledState);
    }
    
  }
}

// PWM output @ 25 kHz, only on pins 9 and 10.
// Output value should be between 0 and 320, inclusive.
void analogWrite25k(int pin, int value)
{
  switch (pin) {
    case 9:
      OCR1A = value;
      break;
    default:
      // no other pin will work
      break;
  }
}

// The button changed from low to high
void pin_ISR() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    ledBlinkCount = 0;
    if ((currentTTIndex + 1) >= TTAmount) {
      currentTTIndex = 0;
    } else {
      currentTTIndex++;
    }
    Serial.print("Target temperature changed to ");
    Serial.print(TTList[currentTTIndex]);
    Serial.println(" C");
  }
  last_interrupt_time = interrupt_time;
}
