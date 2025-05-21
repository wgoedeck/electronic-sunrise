/* *******************************************************
Electronic Sunrise
by Walter Goedecke, original creation date: August 18, 2019
Updated Oct. 16, 2022

This Arduino project simulates a sunrise for the purpose of waking up to 
in the morning, by a single LED light-strip in cloud made of a ball paper
lantern with polyfill glued on the outer surface. 

The microprocessor is an Arduino Genuino Uno, with a DS1307 timing circuit 
RTC (Real-Time Clock) module to accurately trigger either a junction 
transistor or a MOSFET that controls a relay, which turns on an LED 
light-strip, with blue, red, orange, yellow, and finally white sequences 
to simulate a sunrise. 

When light sequence is done, the relay cuts power to the light-strip, 
to avoid a latchup. 

Resources:
Wire.h - Arduino I2C Library
SparkFunDS1307RTC.h, Adafruit_NeoPixel.h

Development environment specifics:
Arduino 1.6.8
SparkFun RedBoard or Arduino Uno board.
SparkFun Real Time Clock Module DS1307 (v14)
Adafruit 1 meter light-strip, with 60 LEDs.
******************************************************* */
#include <Arduino.h>
#include "SparkFunDS1307RTC.h"
#include <Wire.h>

#include "Adafruit_NeoPixel.h"
#include <avr/power.h>

/***** Defininitions *****/
// Comment out the line below if you want month printed before date.
// E.g. October 31, 2016: 10/31/16 vs. 31/10/16
#define PRINT_USA_DATE

// Define pin 13, the LED, to indicate light on.
#define ON_CYCLE_PIN 13 // LED to indicate on cycle state.

//Cloud one Data input pin hooked up to pin 6.
#define PINA 6

//Define days of the week.
#define Sun 1
#define Mon 2
#define Tue 3
#define Wed 4
#define Thu 5
#define Fri 6
#define Sat 7

//Define workday, lateStart, and weekend, as integers.
#define workday 1
#define lateStart 2
#define weekend 3
/************************/

/***** Setup constants *****/
// This pin drives the relay circuit; this controls the power to the LEDs, 
// which can latch to an on-state if not interupted. 
const int relayPin = 4;

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip_a = Adafruit_NeoPixel(300, PINA, NEO_GRB + NEO_KHZ800);

/*IMPORTANT: To reduce NeoPixel burnout risk, add large (~100 to 1000 uF) 
  capacitor across light-strip power leads, add 300 - 500 Ohm resistor on first 
  pixel's data input and minimize distance between Arduino and first pixel. 
  Avoid connecting on a live circuit...if you must, connect GND first. */

// Time for each LED to light, in milliseconds.
int led_delay = 6000;
// Time delay between each LED color sequence.
int strip_delay = 5000;

// Number of LEDs in strip.
int num_led = 60;

/************************/
// Initializtion of code. 
void setup() {
  // Initialize digital Arduino pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  //Initialize board on-cycle LED to off.
  pinMode(ON_CYCLE_PIN, OUTPUT);
  digitalWrite(ON_CYCLE_PIN, LOW);

  // Set pin that controls relay as an output.
  pinMode(relayPin, OUTPUT);
  
  // Use the serial monitor to view time/date output
  Serial.begin(9600);

  // Call rtc.begin to initialize the library
  rtc.begin();

  // Set the RTC to the date & time on PC when sketch is compiled.
  // Need do only infrequently, since the RTC module has its own battery. 
  //rtc.autoTime();
  //rtc.set12Hour(); // Use rtc.set12Hour to set to 12-hour mode.

  // Or you can use the rtc.setTime(s, m, h, day, date, month, year)
  // function to explicitly set the time:
  // e.g. 7:32:16 | Monday October 31, 2016, becomes:
  // rtc.setTime(16, 32, 7, 2, 31, 10, 16);

  // Set specific time:
  //rtc.setTime(16, 10, 11, 2, 12, 5, 25);  // Uncomment to manually set time
  //rtc.set12Hour(); // Use rtc.set12Hour to set to 12-hour mode

  Serial.println("\nStarting electronic sunrise");
  // Call rtc.update() to update all rtc.seconds(), rtc.minutes(), ..., functions.
  rtc.update();

  printTime(); // Print the new time.

  // Setup LED strip.
  strip_a.begin(); 
  strip_a.show(); // Initialize all pixels in cloud to 'off'
}

// *******************************************************
// This is the main electronic sunrise program.
// * * * * * * * * * * * * * * * * * * * * * * * * * * * *
void loop() {
  // Initialize time elements. Within loop, 'static' sets variable to persistent. 
  static int8_t currentMinute = -1;
  static int8_t currentHour = -1;
  static int8_t currentDay = -1;

  // Set flag indicating on-cycle. 
  static bool flgOnCycle = false;
  
  // Set time for wakeing up, when cycle should be in on-cycle mode.
  // **Important - remember to set the PM flag; either "false" or "true" for 
  // morning or afternoon!**
  static int alarmHour = 7;
  static int alarmMinute = 0;
  static bool alarmPM = false; //Set to AM. 

  // Set the alarm for a weekday late-start, e.g., one hour later. 
  static int lateStartAlarmHour = 8;
  static int lateStartAlarmMinute = 0;
  static bool lateStartAlarmPM = false; //Set to AM. 
    
  // Weekend alarm time. 
  static int wkndAlarmHour = 9;
  static int wkndAlarmMinute = 0;
  static bool wkndAlarmPM = false; //Set to AM. 
    
  // This is an on-cycle test, usually set for the evening to see if the 
  // unit is working and the time is correct.
  static int eveAlarmHour = 7;
  static int eveAlarmMinute = 0;
  static bool eveAlarmPM = true; //Set to PM. 

  // Set the LED light-strip on-duration, once they are all on.
  static int durationMinutes = 45;
  // Initiate the minute duration counter to null.
  static int durationCounter = 0;
    
  //Set a particular workday to true for a late morning; all others remain false.
  // The late-start will set the alarm later than a typical workday.
  static bool mon_late = false;
  static bool tue_late = false;
  static bool wed_late = true;
  static bool thu_late = false;
  static bool fri_late = false;

  // Declare persistent integer variable "day_status." This will be set to either 
  // "workday," "lateStart", or "weekend."
  static int8_t day_status;

  // Call rtc.update() to update all rtc functions, e.g., rtc.seconds(), 
  // rtc.minutes(), etc.
  rtc.update();

  // Check if minute has changed - if so, then update currentMinute value. 
  if (currentMinute != rtc.minute()) {
    currentMinute = rtc.minute();
    // Check if hour has changed - if so, then update currentHour value. 
    if (currentHour != rtc.hour()) {
      currentHour = rtc.hour();
      // Check if day has changed - if so, then update currentDay value. 
      if (currentDay != rtc.day()) {
        currentDay = rtc.day();

        // Check if day is either a workday, a late-start day, or a weekend day. 
        if (currentDay == Mon) {
          if (mon_late == true) {
            day_status = lateStart;
          } else {
            day_status = workday;
          }
        }
        else if (currentDay == Tue) {
          if (tue_late == true) {
            day_status = lateStart;
          } else {
            day_status = workday;
          }
        }
        else if (currentDay == Wed) {
          if (wed_late == true) {
            day_status = lateStart;
          } else {
            day_status = workday;
          }
        }
        else if (currentDay == Thu) {
          if (thu_late == true) {
            day_status = lateStart;
          } else {
            day_status = workday;
          }
        }
        else if (currentDay == Fri) {
          if (fri_late == true) {
            day_status = lateStart;
          } else {
            day_status = workday;
          }
        }
        // Else, must be a weekend day, i.e., if currentDay is Sunday or Saturday.
        else { 
          day_status = weekend;
        }
      }
    }

    // Print the date and time every 5 minutes when flgOnCycle is false, i.e., LED 
    // strip is off.
    if (flgOnCycle == false) {
      if (currentMinute % 5 == 0) {
        Serial.print("Off: ");
        printTime(); // Print the new time.
      }

      // Turn on light-strip between alarm time-set and when it should go off. 
      // There is a weekday alarm, a late-start alarm, a weekend alarm, and an evening test alarm. 
      if ( //Weekday alarm.
          ( day_status = workday && 
            60*currentHour + currentMinute >= 60*alarmHour + alarmMinute && 
            60*currentHour + currentMinute + 5 < 60*alarmHour + alarmMinute + durationMinutes && 
            rtc.pm() == alarmPM )
          || //Late-start day alarm.
          ( day_status = lateStart && 
            60*currentHour + currentMinute >= 60*lateStartAlarmHour + lateStartAlarmMinute && 
            60*currentHour + currentMinute + 5 < 60*lateStartAlarmHour + lateStartAlarmMinute + durationMinutes && 
            rtc.pm() == lateStartAlarmPM ) 
          || //Weekend alarm.
          ( day_status = weekend && 
            60*currentHour + currentMinute >= 60*wkndAlarmHour + wkndAlarmMinute && 
            60*currentHour + currentMinute + 5 < 60*wkndAlarmHour + wkndAlarmMinute + durationMinutes && 
            rtc.pm() == wkndAlarmPM ) 
          || //Evening test.
          ( 60*currentHour + currentMinute >= 60*eveAlarmHour + eveAlarmMinute && 
            60*currentHour + currentMinute + 5 < 60*eveAlarmHour + eveAlarmMinute + durationMinutes && 
            rtc.pm() == eveAlarmPM ) 
         ) {
        // Turn the relay on by activating the transisitor circuit.
        digitalWrite(relayPin, HIGH);

        // Indicate the on-cycle state as on, thru pin 13, the Arduino board LED.
        // (HIGH is the voltage level)
        digitalWrite(ON_CYCLE_PIN, HIGH);
        digitalWrite(LED_BUILTIN, HIGH);

        // Print "On cycle" to show system state. 
        Serial.println("On cycle");
        //Set on-cycle flag to true.
        flgOnCycle = true;

        // Function activates LED strip, making the cloud blue, red, reddish, 
        // orange-red, then daylight, simulating a sunrise.
        sunRise();

        /* Set duration counter to the on-cycle duration. The duration counter will 
          decrement a minute for each cycle in the loop, and turn the system off 
          when null. */
        durationCounter = durationMinutes;
      }
    }
    else { 
      /* When flgOnCycle is true, indicating on-cycle, print time every 5 minutes 
        during on cycle, beginning each line with "On: " e.g., 
        "On: 1:30:00 PM | Thursday - 5/15/25" */
      if (rtc.getMinute() % 5 == 0) {
        Serial.print("On: ");
        printTime(); // Print the new time.
      }
      // If at end of cycle, when durationCounter reaches null, turn off.
      if (durationCounter <= 0) {
        // Turn the relay off.
        digitalWrite(relayPin, LOW); 
        //Set boolean flag to off.
        flgOnCycle = false;

        // Turn the onboard LED off to indicate that the on-cycle state is off.
        digitalWrite(ON_CYCLE_PIN, LOW);
        digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW

        // Turn off LED cloud, by function that turns all the LEDs in the clouds off.
        clearCloud();

        Serial.print('\n'); //Print linefeed.
      } else { 
        // Keep decrementing the duration counter by a minute. 
        durationCounter--;
      }
    }
  }
  //delay(1000); //Delay a second. 
  // *******************************************************
}

// Function that makes the cloud or clouds blue, red, reddish, orange-red, then 
// daylight. Light-strip pixel color map is red, green, blue.
void sunRise(){
  // Phase 1: set LEDs to low blue. 
  Serial.println("Blue phase");
  for(int i = 0; i < num_led; i++) { 
    //Serial.println(i);
    strip_a.setPixelColor(i, 0, 0, 10);
    strip_a.show();
    delay(led_delay);
  }
  delay(strip_delay);

  // Phase 2: set LEDs low red. 
  Serial.println("Red phase");
  for(int i = 0; i < num_led; i++) { 
    strip_a.setPixelColor(i, 30, 0, 0);
    strip_a.show();
    delay(led_delay);
  }
  delay(strip_delay);

  // Phase 3: set LEDs low red-ish.
  Serial.println("Orange phase");
  for(int i = 0; i < num_led; i++) { 
    strip_a.setPixelColor(i, 40, 10, 0);
    strip_a.show();
    delay(led_delay);
  }
  delay(strip_delay);

  // Phase 4: set LEDs orange-red.
  Serial.println("Yellow phase");
  for(int i = 0; i < num_led; i++) { 
    strip_a.setPixelColor(i, 80, 30, 0);   //set LEDs orange-red
    strip_a.show();
    delay(led_delay);
  }
  delay(strip_delay);

  // Phase 5: set LEDs daylight-white.
  Serial.println("White phase");
  for(int i = 0; i < num_led; i++) { 
    strip_a.setPixelColor(i, 120, 84, 60);   //set LEDs white
    strip_a.show();
    delay(led_delay);
  }
  delay(strip_delay);
  delay(strip_delay);
}

//Clear cloud function:
void clearCloud(){
  for (int i = 0; i < num_led; i++){ //for all the LEDs
      strip_a.setPixelColor(i, 0, 0, 0); //turn off in cloud one   
  }
  strip_a.show(); //show what was set in cloud one
}

void printTime()
{
  // Print hour
  Serial.print(String(rtc.hour()) + ":");

  // Print minute
  if (rtc.minute() < 10)
    Serial.print('0'); // Print leading '0' for minute
  Serial.print(String(rtc.minute()) + ":");

  // Print second
  if (rtc.second() < 10)
    Serial.print('0'); // Print leading '0' for second
  Serial.print(String(rtc.second()));

  if (rtc.is12Hour()) // If we're in 12-hour mode
  { 
    // Use rtc.pm() to read the AM/PM state of the hour
    if (rtc.pm()) Serial.print(" PM"); // Returns true if PM
    else Serial.print(" AM");
  }
  
  Serial.print(" | ");

  // A few options for printing the day, pick one:
  Serial.print(rtc.dayStr()); // Print day string
  //Serial.print(rtc.dayC()); // Print day character
  //Serial.print(rtc.day()); // Print day integer (1-7, Sun-Sat)

  Serial.print(" - ");

#ifdef PRINT_USA_DATE
  Serial.print(String(rtc.month()) + "/" +   // Print month
                 String(rtc.date()) + "/");  // Print date
#else
  Serial.print(String(rtc.date()) + "/" +    // (or) print date
                 String(rtc.month()) + "/"); // Print month
#endif
  Serial.println(String(rtc.year()));        // Print year
}
