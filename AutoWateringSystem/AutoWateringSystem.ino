/*
 * Copyright (C) 2013 John Sofikitis.
 * Automated Watering system
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Headers */
#include "Wire.h"
#include <DHT.h>

/* #Define */
// Relay Header
#define RELAY_I2C_ADDR  0x20
// RTC Address
#define RTC_I2C_ADDR 0x68
// Temp headers
#define DHTPIN 2
// Type of sensor
#define DHTTYPE DHT22

/////// LEDs ////////////
#define MASTERPIN 3
#define RAINPIN 4
#define RELAY1LED 5
#define RELAY2LED 6
#define RELAY3LED 7
#define RELAY4LED 8
#define RELAY5LED 9
/////// LEDs ////////////

// How many relays are connected
#define ACTIVERELAYS 2

//store temp & humidity
typedef struct
{
  float t;
  float h;
}TH_t;

//store the date
typedef struct {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t dayOfWeek;
  uint8_t dayOfMonth;
  uint8_t month;
  uint8_t year;
} Date_t;

//store the master switch
//and rainsensor
typedef struct{
  uint8_t MD;
  uint8_t RD;
} Switch_t;

/* Begin Variables */

DHT dht(DHTPIN, DHTTYPE);
TH_t THData;
Date_t pD;

Switch_t SwitchOn = {0,0};

/*
Relay	Decimal	Binary
1	1	  00000001
2	2	  00000010
3	4	  00000100
4	8	  00001000
5	16	00010000
6	32	00100000
7	64	01000000
8	128	10000000
*/
uint8_t relay_arr [] = {1,2,4,8,16,32,64,128};

uint16_t lightLevel = 0;

// All constants
const uint8_t  Zero = 0;
const uint8_t  OneHundred = 100;
const uint16_t OneThousand = 1000;
const uint32_t OneDay = 86400;
const uint16_t MaxAnalog = 1023;
const uint16_t PrintTime = 5000;

// Water delay in seconds
//const uint8_t  WATER_DELAY = 3;
//const uint8_t  WATER_DELAY = 60;
const uint16_t  WATER_DELAY = 900;

//Watering period in days
const uint8_t WATERING_PERIOD = 3;

// Temp Threshold at 25 degrees
const uint8_t TEMP_THRESHOLD = 25;

// Light threshold at 10%
const uint8_t LIGHT_THRESHOLD = 10;
/// End constants

// Print timers
unsigned long lastsensorprint = 0;
unsigned long lastmessageprint = 0;

/// Messages
char msg1 [] = "WARNING: Temperature below threshold";
char msg2 [] = "WARNING: Light level above threshold";
char msg3 [] = "WARNING: Aborting watering due to rain";
char msg4 [] = "Master switch: OFF";
char msg5 [] = "WARNING: Temperature below threshold";
char *msg;
/* End Variables */


void setup()
{
  // set up the input pin
  pinMode(RAINPIN, INPUT);
  digitalWrite(RAINPIN, HIGH);
  
  // set up the output pin for the master led
  pinMode(MASTERPIN, INPUT);

  for (uint8_t i = 0; i < ACTIVERELAYS; i++)
  {
    Serial.print("Setting LED for relay ");
    Serial.println((i+1), DEC);
    // set up the output pins for the active relay leds 
    pinMode((RELAY1LED + i), OUTPUT);
    // Initialise the LEDs
    digitalWrite((RELAY1LED + i), HIGH);
    delay(500);
    digitalWrite((RELAY1LED + i), LOW);
  }
  
  // Set up the serial so messages can be shown
  Serial.begin(38400);
  
  // Start the temperature reading
  dht.begin();
  
  // Wake up I2C bus
  Wire.begin();  
  
  // Set I/O bank A to outputs
  Wire.beginTransmission(RELAY_I2C_ADDR);
  Wire.write(0x00); // IODIRA register
  Wire.write(0x00); // Set all of bank A to outputs 
  Wire.endTransmission();
}

void loop()
{

  // Read all the attached sensors
  readsensors();

  // Master switch is on
  if (SwitchOn.MD == HIGH)
  { // No rain
    if (SwitchOn.RD == HIGH)
    {
      if ((THData.t >= TEMP_THRESHOLD) && (lightLevel <= LIGHT_THRESHOLD))
      {
        for (int i=0; i < ACTIVERELAYS; i++ )
        {
          delayFor(WATER_DELAY, relay_arr[i], i);
        }
        Serial.println("Resetting all relays");
        writeToRelay(0);
      }
      else if (THData.t < TEMP_THRESHOLD)
      {
        printMessage(msg1);
      }
      else if (lightLevel > LIGHT_THRESHOLD)
      {
        printMessage(msg2);
      }
    }
    else if (SwitchOn.RD == LOW)
    {
      printMessage(msg3);
    }
  }
  else
  {
    printMessage(msg4);
  }
 gardendelayFor(5);

//gardendelayfor(WATERING_PERIOD);
}

void delayFor (uint16_t seconds, uint8_t relaynum, uint8_t index)
{
  unsigned long start = millis();
  unsigned long lastprint = millis();
  unsigned long duration = seconds * OneThousand;

  while (millis() - start <= duration)
  {
    // Read the sensors just in case something changed
    readsensors();
    if (SwitchOn.MD == LOW || SwitchOn.RD == LOW )
    {
      Serial.println("WARNING: Aborting watering sequence");
      digitalWrite((RELAY1LED + index), LOW);
      break;
    }

    if (millis() > lastprint)
    {
      Serial.print("Activating Relay: ");
      Serial.println(index + 1, DEC);
      lastprint = millis() + PrintTime;
    }

    digitalWrite((RELAY1LED + index), HIGH);
    writeToRelay(relaynum);
  }

  digitalWrite((RELAY1LED + index), LOW);
}

void gardendelayFor (uint8_t period)
{
  unsigned long start = millis();
  //unsigned long duration = period * OneDay * OneThousand;
  unsigned long duration = period * OneThousand;

  while (millis() - start <= duration)
  {
    // Read the sensors just in case something changed
    readsensors();
    if (SwitchOn.MD == LOW || SwitchOn.RD == LOW )
    {
      Serial.println("WARNING: Aborting watering sequence due to sensor readings");
      break;
    }
  }
}

/*    Begin Setter/Getter Functions */
void readsensors()
{
  // are we supposed to start
  readSwitch();
  readlight();
  
  //grab time and date
  readTemp();
  
  readRTC();

  if (millis() > lastsensorprint)
  {
    printSensors();
  }
}

void readSwitch()
{
  SwitchOn.MD = digitalRead(MASTERPIN);
  SwitchOn.RD = digitalRead(RAINPIN);
}

void readlight()
{ 
  lightLevel = analogRead(A0);
  // Map the light level to 0 to 100
  lightLevel = map(lightLevel,Zero,MaxAnalog,Zero,OneHundred);
}

void readTemp()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  THData.h = dht.readHumidity();
  THData.t = dht.readTemperature();

  // check if returns are valid, if they are NaN (
  // not a number) then something went wrong!
  if (isnan(THData.t) || isnan(THData.h)) {
    Serial.println("WARNING: Failed to read from DHT");
    THData.h = 0;
    THData.t = 0;
  }
}

void readRTC()
{
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0); // set DS3232 register pointer to 00h
  Wire.endTransmission();
  // request 7 bytes of data from DS3232 starting from register 00h
  Wire.requestFrom(RTC_I2C_ADDR, 7); 

  // A few of these need masks because certain bits are control bits
  pD.second     = bcdToDec(Wire.read() & 0x7f);
  pD.minute     = bcdToDec(Wire.read());
  pD.hour       = bcdToDec(Wire.read() & 0x3f);  // Need to change this if 12 hour am/pm
  pD.dayOfWeek  = bcdToDec(Wire.read());
  pD.dayOfMonth = bcdToDec(Wire.read());
  pD.month      = bcdToDec(Wire.read());
  pD.year       = bcdToDec(Wire.read());
}

void writeToRelay(uint8_t latchValue)
{
  Wire.beginTransmission(RELAY_I2C_ADDR);
  Wire.write(0x12);        // Select GPIOA
  Wire.write(latchValue);  // Send value to bank A
  Wire.endTransmission();
}

void printMessage(char * message)
{
  if (millis() > lastmessageprint)
  {
    msg = message;
    Serial.print("[");
    Serial.print(message);
    Serial.print("]");
    Serial.println();
    printTime();

    lastmessageprint = millis() + PrintTime;
  }
}

void printlight()
{
  Serial.print("[LightLevel: ");
  Serial.print(lightLevel, DEC);
  Serial.print(" %] ");
  Serial.println();
}

void printSensors()
{
  printlight();
  printTemp();
  lastsensorprint = millis() + PrintTime;
}

void printTemp()
{ 
  Serial.print("[");
  Serial.print("Humidity: ");
  Serial.print(THData.h);
  Serial.print(" % ");
  Serial.print("Temperature: ");
  Serial.print(THData.t);
  Serial.print(" *C");
  Serial.print("]");
  Serial.println();
}

void printTime()
{ 
  Serial.print("[");
  // send it to the serial monitor
  Serial.print(pD.hour, DEC);
  Serial.print(":");
  if (pD.minute<10)
  {
      Serial.print("0");
  }
  Serial.print(pD.minute, DEC);
  Serial.print(":");
  if (pD.second<10)
  {
      Serial.print("0");
  }
  Serial.print(pD.second, DEC);
  Serial.print("  ");
  Serial.print(pD.dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(pD.month, DEC);
  Serial.print("/");
  Serial.print(pD.year, DEC);
  Serial.print("  Day of week: ");
  switch(pD.dayOfWeek){
  case 1:
    Serial.print("Sunday");
    break;
  case 2:
    Serial.print("Monday");
    break;
  case 3:
    Serial.print("Tuesday");
    break;
  case 4:
    Serial.print("Wednesday");
    break;
  case 5:
    Serial.print("Thursday");
    break;
  case 6:
    Serial.print("Friday");
    break;
  case 7:
    Serial.print("Saturday");
    break;
  default:
    Serial.print("Error! ");
    Serial.print("Num ");
    Serial.print(pD.dayOfWeek,DEC);
  }
  Serial.println("]");
}

// Convert binary coded decimal to normal decimal numbers
uint8_t bcdToDec(uint8_t val)
{
  return ( (val/16*10) + (val%16) );
}
/*    End Setter/Getter Functions */
/* END */