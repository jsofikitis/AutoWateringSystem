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

#define MASTERLED 13

#define ACTIVERELAYS 5
#define RAINPIN 4
#define RELAY1LED 5
#define RELAY2LED 6
#define RELAY3LED 7
#define RELAY4LED 8
#define RELAY5LED 9


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
const uint16_t MaxAnalog = 1023;
// Water delay in seconds
const uint16_t  WATER_DELAY = 3;
// Temp Threshold at 25 degrees
const uint8_t TEMP_THRESHOLD = 20;
// Light threshold at 10%
const uint8_t LIGHT_THRESHOLD = 10;
/* End Variables */

/*    End Setter/Getter Functions */
void setup()
{
  // set up the input pin
  pinMode(RAINPIN, INPUT);
  digitalWrite(RAINPIN, HIGH);
  
  // set up the output pin for the master led
  pinMode(MASTERLED, INPUT);

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
  // are we supposed to start
  readSwitch();
  readlight();
  //grab time and date
  getTemp();
  printTemp();
  getRTC();
  printTime();

  // Master switch is on
  if (SwitchOn.MD == HIGH)
  { // No rain
    if (SwitchOn.RD == HIGH)
    {
      if ((THData.t >= TEMP_THRESHOLD) && (lightLevel <= LIGHT_THRESHOLD))
      {
        for (int i=0; i < ACTIVERELAYS; i++ )
        {
          digitalWrite((RELAY1LED + i), HIGH);
          writeToRelay(relay_arr[i]);
          delay(WATER_DELAY * OneThousand);
          digitalWrite((RELAY1LED + i), LOW);
        }
       writeToRelay(0);
      }
      else if (THData.t < TEMP_THRESHOLD)
      {
        Serial.println("WARNING: Temperature below threshold");
        //TODO Print time as well
      }
      else if (lightLevel > LIGHT_THRESHOLD)
      {
        Serial.println("WARNING: Light level above threshold");
        //TODO Print time as well
      }
    }
    else if (SwitchOn.RD == LOW)
    {
      Serial.println("It's raining");
      //TODO Print time as well

    }
  }
  else
  {
    Serial.println("Master switch: OFF");
    //TODO Print time as well
  }
  delay(5000);
}

/*    Begin Setter/Getter Functions */
void readSwitch()
{
  SwitchOn.MD = digitalRead(MASTERLED);
  SwitchOn.RD = digitalRead(RAINPIN);
}

void readlight()
{
  lightLevel = analogRead(A0);
  // Map the light level to 0 to 100
  lightLevel = map(lightLevel,Zero,MaxAnalog,Zero,OneHundred);
  Serial.print("[LightLevel: ");
  Serial.print(lightLevel, DEC);
  Serial.print(" %] ");
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

void getTemp()
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

// Convert binary coded decimal to normal decimal numbers
uint8_t bcdToDec(uint8_t val)
{
  return ( (val/16*10) + (val%16) );
}

void getRTC()
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
  if (latchValue == 0)
  {
    Serial.println("Resetting all relays");
  }
  else
  {
    Serial.print("Activating Relay: ");
    Serial.println(latchValue,DEC);
  }
  Wire.beginTransmission(RELAY_I2C_ADDR);
  Wire.write(0x12);        // Select GPIOA
  Wire.write(latchValue);  // Send value to bank A
  Wire.endTransmission();
}
