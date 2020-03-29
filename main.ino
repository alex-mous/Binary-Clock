/* 
 * Copyright (c) 2020  Alex Mous
 * 
 * Binary Clock
 * Conversion of a classic two-bell alarm clock (Equity #13014) into a 
 * futuristic binary (base 2) clock using a Circuit Playground Express
 * 
 * Uses an underlying 24 hour system represented by color shown (RGB 
 * for the morning and CMY for the afternoon) and rovides precise 
 * timing using a 1Hz generator circuit from a clock circuit
 * 
 * Also has alarm capabilities, a system to tell AM and PM apart 
 * using different colors, and buttons to make adjusting time easy
 * 
 * Wiring diagram and pictures are provided - see GitHub
 * 
 * Please change the constants in section just below to customize the 
 * clock to your needs. See the Adafruit website on how to upload the 
 * code to your Circuit Playground Express.
 */

#include <Adafruit_Circuit_Playground.h>

#define LED_B 13 //Board LED
#define CLOCK_PULSE A1 //Clock pin
#define BUTTON_LEFT A4 //Left button
#define BUTTON_RIGHT A5 //Right button
#define ALARM_MOTOR 12

#define CAPACITIVE_SENSOR 0 //Capacitive sensor pin

#define CAP_THRESHOLD 950 //Threshold above which a touch is triggered


//Brightness depending on time and state
#define DAY_BRIGHTNESS 200
#define DAY_TAPPED_BRIGHTNESS 255
#define NIGHT_BRIGHTNESS 0
#define NIGHT_TAPPED_BRIGHTNESS 30
#define ALARM_BRIGHTNESS 255

//Time states will exist
#define TAP_ON_TIME 5 //Seconds the LEDs will be tapped brightness when tapped
#define ALARM_ON_TIME 20 //Amount of time the alarm will run on the first time (doubled after snooze)

//Night time start and stop
#define NIGHT_START 23
#define NIGHT_STOP 7

//Color definitions - default, hours, minutes, both (then alternate for afternoon)
const int COLOR_MAP[][3] = { {0,0,0}, {255,0,0}, {0,0,255}, {0,255,0}, {0,0,0}, {0,255,255}, {255,255,0}, {255,0,255} }; //RGB and then CMY

//Used for calculations of above
const int RGB = 0;
const int CMY = 4;

const int LED_QUADRANTS[][2] = {{5,6}, {3,4}, {8,9}, {0,1}}; //Order in which LEDs are lit (little endian)

int led_colors[4][3]; //Current colors (stores values from COLOR_MAP)

int time_now[3] = {0,0,12};
int time_alarm[3] = {0,0,0}; //Time for the alarm - won't run when at {0,0,0}


//Flags used during loop()
boolean alarm_snooze = false;
int alarm_running_c = -1; //Count of the number of seconds left to run alarm
int tapped_brightness_c = -1; //Count of seconds until reset to normal brightness



//----------Setup

void setup() {
  //Setup pin modes and initial states
  pinMode(LED_B, OUTPUT);
  pinMode(CLOCK_PULSE, INPUT);
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);
  pinMode(CAPACITIVE_SENSOR, INPUT);
  digitalWrite(LED_B, LOW);
  pinMode(ALARM_MOTOR, OUTPUT);
  digitalWrite(ALARM_MOTOR, LOW);
  CircuitPlayground.begin(DAY_BRIGHTNESS);
  updateColors(time_now[2], time_now[1]); //Set to the default time
  updateLEDs();
  Serial.begin(9600); //Debugging
}




//----------Main Loop

void loop() {
  int val = analogRead(CLOCK_PULSE);
  if (val > 60) {
    time_now[0]++;
    if (time_now[0] >= 60) { //60 Seconds == 1 Minute
      time_now[1] += 1;
      time_now[0] = 0;
      if (time_now[1] >= 60) { //60 Minutes == 1 Hour
        time_now[2]++;
        time_now[1] = 0;
        if (time_now[2] > 24) { //24 Hours == 1 Day; truncate
          time_now[2] = 1;
        }
      }
      if (time_now[1] % 5 == 0) { //Update clock every five minutes
        updateColors(time_now[2], time_now[1]);
        dimmerTimeCheck(time_now[2]);
        updateLEDs();
        alarmTime(time_now, time_alarm, &alarm_running_c, &alarm_snooze);
      }
    }

    if (alarm_running_c >= 0) { //If there are more alarm loops to complete, run the sequence
      //Serial.print("Alarm running... Seconds left:"); Serial.println(alarm_running_c); //Debugging
      if (alarm_running_c == 0) { //Reset to defaults when zero
        updateColors(time_now[2], time_now[1]);
        dimmerTimeCheck(time_now[2]); //Reset to adjust the brightness based on the time
        updateLEDs();
        digitalWrite(ALARM_MOTOR, LOW); //Turn off the alarm bells
      } else if (alarm_running_c % 2 == 1) {
        for (int i=0; i<4; i++) { //Show nothing
          setQuadrant(i, 0, RGB);
        }
        updateLEDs();
      } else {
        for (int i=0; i<4; i++) { //Show red
          setQuadrant(i, 1, RGB);
        }
        updateLEDs();
      }
      alarm_running_c--;
    }

    if (tapped_brightness_c >= 0) { //Button pressed/tapped - show high brightness
      //Serial.print("Tapped brightness... Seconds left:"); Serial.println(tapped_brightness_c); //Debugging
      if (tapped_brightness_c == 0) { //Reset to defaults
        if (nightTime(time_now[2])) { //Running with night brightness levels
          CircuitPlayground.setBrightness(NIGHT_BRIGHTNESS);
        } else {
          CircuitPlayground.setBrightness(DAY_BRIGHTNESS);
        }
      }
      updateLEDs();
      tapped_brightness_c--;
    }
     //Debugging
    Serial.print("Second:\t"); Serial.print(time_now[0]); Serial.print("\t");
    Serial.print("Minute:\t"); Serial.print(time_now[1]); Serial.print("\t");
    Serial.print("Hour:\t"); Serial.print(time_now[2]); Serial.print("\t"); Serial.println();
    delay(100);
  }
  checkButtons();
}



//----------Global Update Functions

void updateColors(int hours, int minutes) { //Update the global <led_colors> array with new values representative of the time <hours> and <minutes>
  int res[4] = {0, 0, 0, 0};
  minutes /= 5; //Get minutes in fives
  int color_sys;
  if (hours <= 12) { //Before PM, use first color system
    color_sys = RGB;
  } else {
    color_sys = CMY;
    hours -= 12;
  }
  for (int i=3, j=0; i>=0; i--, j++) { //Loop from 3 to 0 (powers of 2) and 0 to 3 (indexes in result)
    int power = 1 << i; //Get 2 to the power i
    if (hours >= power) { //If the value is greater, add a corresponding amount the the res array and subtract the amount from the value
      res[j] += 1;
      hours -= power;
    }
    if (minutes >= power) {
      res[j] += 2;
      minutes -= power;
    }
  }
  for (int i=0; i<4; i++) { //Now set up the array
    setQuadrant(i, res[i], color_sys);
  }
}

void setQuadrant(int quadrant, int color, int color_sys) { //Change the color of the quadrant to a color given in COLOR_MAP
  led_colors[quadrant][0] = COLOR_MAP[color + color_sys][0];
  led_colors[quadrant][1] = COLOR_MAP[color + color_sys][1];
  led_colors[quadrant][2] = COLOR_MAP[color + color_sys][2];
}



//----------Time Functions

void updateTime(int *_time) { //Adjust the time depending on button presses
  //Serial.println("Updating the time..."); //Debugging
	for (int i=0; i<10; i++) {
		if (analogRead(BUTTON_LEFT) > 500) {
      *(_time+2) += 1;
      //Serial.print("New hours: "); Serial.println(*(_time+2)); //Debugging
      i = 0;
		} else if (analogRead(BUTTON_RIGHT) > 500) {
      *(_time+1) += 5;
      //Serial.print("New minutes: "); Serial.println(*(_time+1)); //Debugging
      i = 0;
		}

    if (*(_time+1) >= 60) { //Make sure the minutes are within the correct range and increment hours if not
      *(_time+1) = 0;
      *(_time+2) += 1;
      //Serial.println("Minutes reset"); //Debugging
    }
    if (*(_time+2) > 24) { //Make sure the hours are within the correct range and reset if not
      //Serial.println("Hours reset"); //Debugging
      *(_time+2) = 1;
    }
    updateColors(*(_time+2), *(_time+1));
    updateLEDs();
		delay(250); //Debounce delay
    for (int j=0; j<20; j++) { //Loop for 200ms and break early if a button is pressed
      if (analogRead(BUTTON_LEFT) > 500 || analogRead(BUTTON_RIGHT) > 500) break;
      delay(10);
    }
	}
}

void dimmerTimeCheck(int hours) { //Validate the current <hours> and set the brightness of the LEDs
  if (nightTime(hours)) {
    //Serial.println("Night time - setting brightness"); //Debugging
    CircuitPlayground.setBrightness(NIGHT_BRIGHTNESS);
    updateLEDs();
  } else {
    //Serial.println("Day time - setting brightness"); //Debugging
    CircuitPlayground.setBrightness(DAY_BRIGHTNESS);
    updateLEDs();
  }
}



//----------Buttons

void checkButtons() { //Check if the buttons are being pressed and and call on functions accordingly (set clock time, set alarm time, clear clocks, or light up (only one button))
	if (doubleButtonClick(50)) {
    long start = millis(); //Get the start time so that we can correct for clock drift while adjusting
		delay(2000);
    CircuitPlayground.setBrightness(DAY_BRIGHTNESS);
		if (doubleButtonClick(50)) { //Long press of both buttons
			delay(2000);
			if (doubleButtonClick(50)) { //Long press of both buttons; reset time to zero
        //Serial.println("Resetting timers to zero"); //Debugging
  			time_now[0] = 0; time_now[1] = 0; time_now[2] = 12;
        time_alarm[0] = 0; time_alarm[1] = 0; time_alarm[2] = 0;
  			flashColor(1); //Red
        delayUntilCount();
			} else {
        //Serial.println("Setting alarm"); //Debugging
        flashColor(2); //Blue
        updateTime(time_alarm); //Run the update function
        flashColor(2); //Blue
        correctTime(time_now, start, millis()); //Correct the time for the amount of time spent updating the alarm
			}
		} else { //Short press of both buttons; adjust the time
      //Serial.println("Setting current time"); //Debugging
			flashColor(3); //Green
			updateTime(time_now); //Run the update function
      flashColor(3); //Green
      correctTime(time_now, start, millis());
      dimmerTimeCheck(time_now[2]); //Set brightness to current time
		}
	} else if (eitherButtonClick(50)) { //One of the buttons was pressed
    if (nightTime(time_now[2])) { //If it's night time, show the night tapped brightness
      CircuitPlayground.setBrightness(NIGHT_TAPPED_BRIGHTNESS);
    } else {
      CircuitPlayground.setBrightness(DAY_TAPPED_BRIGHTNESS);
    }
    updateLEDs();
    tapped_brightness_c = TAP_ON_TIME;
	}
}

boolean doubleButtonClick(int delay_time_ms) { //Check if both buttons are pressed within <delay_time_ms>
	if (analogRead(BUTTON_LEFT) > 500) {
		return (buttonClick(BUTTON_RIGHT, delay_time_ms));
	} else if (analogRead(BUTTON_RIGHT)> 500) {
		return (buttonClick(BUTTON_LEFT, delay_time_ms));
	}
	return false;
}

boolean eitherButtonClick(int delay_time_ms) { //Check if either button is pressed within <delay_time_ms>
  boolean on = false;
  if (analogRead(BUTTON_LEFT) > 500) {
    on = (buttonClick(BUTTON_LEFT, delay_time_ms));
  }
  if (!on && analogRead(BUTTON_RIGHT)> 500) {
    on = (buttonClick(BUTTON_RIGHT, delay_time_ms));
  }
  return on;
}

boolean buttonClick(int button, int delay_time_ms) { //Check if the button <button> is pressed in time <delay_time_ms>
	for (int i=0; i<delay_time_ms; i++) {
		if (analogRead(button) > 500)
			return true;
		delay(1);
	}
	return false;
}



//----------LED Set/Change

void flashColor(int color) { //Flash the color for a second; 1 for Red, 2 for Blue, 3 for Gren
  for (int i=0; i<4; i++) { //Set the current color of the LEDs
    setQuadrant(i, color, RGB);
  }
  updateLEDs();
  delay(1000);
  updateColors(time_now[2], time_now[1]);
  updateLEDs();
}

void flashLEDBrightness(int high_brightness, int low_brightness, int time_ms) { //Flash the LEDs to <high_brightness> for <time_ms> and then reset to <low_brightness>
  CircuitPlayground.setBrightness(high_brightness);
  updateLEDs();
  delay(time_ms);
  CircuitPlayground.setBrightness(low_brightness);
  updateLEDs();
}

void updateLEDs() { //Set the LEDs with the values in the led_color array
  for (int i=0; i<4; i++) {
    CircuitPlayground.setPixelColor(LED_QUADRANTS[i][0], led_colors[i][0], led_colors[i][1], led_colors[i][2]);
    CircuitPlayground.setPixelColor(LED_QUADRANTS[i][1], led_colors[i][0], led_colors[i][1], led_colors[i][2]);
  }
}



//----------Alarm

void alarmTime(int *curr, int *alarm, int *a_running, boolean *a_snooze) { //Based on the time <curr> and alarm <alarm>, set how long the alarm should run and whether it should snooze
  if (*(curr+1) == *(alarm+1) && *(curr+2) == *(alarm+2)) { //The times are equal, so start the alarm
    //Serial.println("Alarm triggered!"); //Debugging
    CircuitPlayground.setBrightness(ALARM_BRIGHTNESS); //Change the brightness
    updateLEDs();
    digitalWrite(ALARM_MOTOR, HIGH); //Start up the motor
    *a_running = ALARM_ON_TIME;
    *a_snooze = true; //Set snooze for next run
  } else if (*a_snooze) { //Alarm snoozing, trigger for a last time
    CircuitPlayground.setBrightness(ALARM_BRIGHTNESS); //Change the brightness
    //Serial.println("Snooze finished, now running alarm again"); //Debugging
    digitalWrite(ALARM_MOTOR, HIGH);
    *a_running = ALARM_ON_TIME * 2;
    *a_snooze = false;
  }
}



//----------Helper Methods

void delayUntilCount() { //Synchronize with the clock
  while (analogRead(CLOCK_PULSE) < 150) {
    delay(5);
  }
}

void correctTime(int* curr, long start_ms, long stop_ms) { //Correct time <curr> with the millisecond counts <start_ms> and <stop_ms>
  long total_time = (stop_ms - start_ms)/1000;
  *(time_now+1) = *(time_now+1) + (*time_now + total_time) / 60 ; //Get the current minutes, rounding to the nearest 5
  *time_now = (*time_now + total_time) % 60; //Get the seconds
}

boolean nightTime(int hours) { //Boolean test to determine if it's night time
  return hours >= NIGHT_START || hours < NIGHT_STOP;
}
