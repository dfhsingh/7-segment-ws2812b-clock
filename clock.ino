/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Author: Ashok Singh

This code refers and inherit code from following

Reference:
#1. Instructibles Article and reference code
https://www.instructables.com/id/RGB-7-segment-Display-Clock-With-WS2812B/

#2.
Alexander Wilhelmer code for integrating MQTT for use with FastLED
https://github.com/awilhelmer/esp8266-fastled-mqtt

*/

#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "PubSubClient.h"

// Definitions for the WS2812B 
#define DATA_PIN D5
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 114
int r = 138;
int g = 1;
int b = 40;
int brightness = 2;
CRGB leds[NUM_LEDS];
const char *ssid     = "JAMwifi";
const char *password = "9811175599";
const long utcOffsetInSeconds = 16200;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", utcOffsetInSeconds, 3600000);
const int TOTAL_SEGMENTS = 4; // Total amount of segments
const int LEDS_PER_SEGMENT = 28; // Amount of LEDs per segment
const int DISPLAY_SEGMENT[] = {0, 28, 28 * 2 + 2, 28 * 3 + 2}; // LED starting position of each segment
const int DISPLAY_NUMBER[][LEDS_PER_SEGMENT] = { // True: Lit, False:  Not lit
{true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true}, // digit 0
{false, false, false, false, false, false, false, false, true, true, true, true, false, false, false, false, false, false, false, false, false, false, false, false, true, true, true, true}, // digit 1
{true, true, true, true, true, true, true, true, false, false, false, false, true, true, true, true, false, false, false, false, true, true, true, true, true, true, true, true}, // digit 2
{false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, true, true, true, true, true, true, true}, // digit 3
{false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, true, true, true}, // digit 4
{false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false}, // digit 5
{true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false}, // digit 6
{false, false, false, false, false, false, false, false, true, true, true, true, false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true}, // digit 7
{true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true}, // digit 8
{false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true}, // digit 9
{false, false, false, false, false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true}, // digit °
{true, true, true, true,true, true, true, true,false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, false, false, false, false}, // digit C
};

// Definitions for BME280
Adafruit_BME280 bme;


// Definitions for MQTT client
#define MQTT_SERVER      "192.168.0.100"
#define MQTT_SERVERPORT  1883
#define mqttOutTopic "dfh/sensor/BME280"
#define mqttInTopic "dfh/sensor/clock"

// Create an ESP8266 MQTT Client
WiFiClient espClient;
PubSubClient mqttclient(espClient);
int oldtime = 0;
char payloadMsg[80];

// SETUP Starts
void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip); // initializes LED strip
  FastLED.setBrightness(brightness);// global brightness

  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();

  Serial.println("BME280 test");
  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }

  mqttclient.setServer(MQTT_SERVER, MQTT_SERVERPORT);
  mqttclient.setCallback(callback);
  mqttclient.setKeepAlive(90);
}

void loop() {
  if (!mqttclient.connected()) {
    reconnectMqtt();
  }
  mqttclient.loop();
  
  timeClient.update();
  int hour = timeClient.getHours() + 1; // Get the hour
  int minute = timeClient.getMinutes(); // Get the minute
  int second = timeClient.getSeconds(); // Get the second
  
  int hourFirstDigit = hour / 10; // Take the first digit
  int hourSecondDigit = hour % 10; // Take the second digit
  int minuteFirstDigit = minute / 10; // Take the first digit
  int minuteSecondDigit = minute % 10; // Take the second digit
  int secondFirstDigit = second / 10; // Take the first digit
  int secondSecondDigit = second % 10; // Take the second digit

  if ( secondSecondDigit == 5) { // this code runs at every 05,15,25,35,45 and 55 second
    int temperature = bme.readTemperature();
    int tempFirstDigit = temperature / 10; // Take the first digit
    int tempSecondDigit = temperature % 10; // Take the second digit
    FastLED.clear(); // Clear the LEDs
    displayNumber(0, tempFirstDigit, CRGB(r,g,b));
    displayNumber(1, tempSecondDigit, CRGB(r,g,b));
    displayNumber(2, 10, CRGB(r,g,b)); // Display degree symbol
    displayNumber(3, 11, CRGB(r,g,b)); // Display 'C' symbol
    FastLED.show(); // Show the current LEDs
    oldtime = 10;
    Serial.println("Temperature Show");
    delay(2000); // delay 1 second
  }

  if (oldtime != minuteSecondDigit){
    FastLED.clear(); // Clear the LEDs
    displayNumber(0, hourFirstDigit, CRGB(r,g,b));
    displayNumber(1, hourSecondDigit, CRGB(r,g,b));
    displayNumber(2, minuteFirstDigit, CRGB(r,g,b));
    displayNumber(3, minuteSecondDigit, CRGB(r,g,b));
    // Light the dots
    leds[28 * 2] = CRGB(r,g,b); 
    leds[28 * 2 + 1] = CRGB(r,g,b);
    FastLED.show(); // Show the current LEDs
    oldtime = minuteSecondDigit;
    Serial.println("FastLED.show");
    sendBME280Values(); 
  }
  delay(1000); // delay 1 second
}

void displayNumber(int segment, int number, CRGB color) {
  for (int j = 0; j < LEDS_PER_SEGMENT; j++) { // Loop over each LED of said segment
    if (DISPLAY_NUMBER[number][j]) { // If this LED should be on
      leds[DISPLAY_SEGMENT[segment] + j] =  color; // Turn it on
    }
  }
}

void sendBME280Values() {
      String payload = "{\"Temperature\":"; 
      payload += bme.readTemperature(); // Default unit is Centigrade
      payload += ",\"Pressure\":";
      payload += bme.readPressure() / 100 ; // Convert Pascal to Hecto-Pascal (hPa)
      payload += ",\"Humidity\":";
      payload += bme.readHumidity();
      payload += "}";
      Serial.print(payload);
      payload.toCharArray(payloadMsg, (payload.length() + 1));
      mqttclient.publish(mqttOutTopic,  payloadMsg);   
}

// Format is: command:value
// value has to be a number, except rgb commands
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  char tmp[length + 1];
  strncpy(tmp, (char*)payload, length);
  tmp[length] = '\0';
  String data(tmp);
  Serial.printf("Received Data from Topic: %s", data.c_str());
  Serial.println();
  if ( data.length() > 0) {
    // Message is 'rgb:255,255,255'
    if (data.startsWith("rgb:")) {
      data.replace("rgb:","");
      String red =  getValue(data, ',', 0);
      String green =  getValue(data, ',', 1);
      String blue =  getValue(data, ',', 2);
//      b.replace("hallo","");
      Serial.printf("Received R: %s G: %s B: %s", red.c_str(), green.c_str(), blue.c_str());
      Serial.println();
      
      if (red.length() > 0 && green.length() > 0 && blue.length() > 0) {
        r = red.toInt();
        g = green.toInt();
        b = blue.toInt();
      }
    }else {
      String command =  getValue(data, ':', 0);
      String value = getValue(data, ':', 1);
  
      if (command.length() > 0) {
        if (command.equals("brightness")) {
          if (value.toInt()>0 && value.toInt() < 256) {
            FastLED.setBrightness(value.toInt());
          }
        }
      }
    }   
  }
  Serial.println("Finished Topic Data ...");
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void reconnectMqtt() {
  while (!mqttclient.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (mqttclient.connect("ESP8266_Clock")) {
      Serial.println("connected");
      mqttclient.subscribe(mqttInTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
