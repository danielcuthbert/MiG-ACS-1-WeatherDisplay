/***************************************************
 Weather station code for a Russian ACS-1 Mig-25
 ****************************************************/

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_EPD.h"    // Core Adafruit Display library
#include <WiFi.h>            // The Wifi stuffs
#include <HTTPClient.h>     // Needed for OpenWeather API
#include <ArduinoJson.h>   // Needed to format
#include "credentials.h"   // where we store secrets
#include <Fonts/FreeSans9pt7b.h>


/*
 * Setting up the WiFi & OpenWeatherMap API
 *
 */

bool id = false;
WiFiClient client;
char servername[] = "api.openweathermap.org";
unsigned long lastTime = 0;
unsigned long timerDelay = 1.8e+6; // Let's check every 30 minutes
String result;


/*
 * Setting up the 2.9 E-ink Display. This is with a ESP32 Dev Board with the following pinout
 * VIN --> 3V3
 * GND --> GND
 * SCK --> 18
 * MISO --> 19
 * MOSI --> 23
 * ECS --> 33
 * D/C --> 12
 * SRCS --> 27
 * RST --> 32
 * BUSY --> 14
 *
 */


#define EPD_CS     33
#define EPD_DC     12
#define SRAM_CS    27
#define EPD_RESET  32 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY   14 // can set to -1 to not use a pin (will wait a fixed delay)

Adafruit_IL0373 display(296, 128, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);


void setup() {


  Serial.begin(115200);
  Serial.print("Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  display.begin(); // Initialised the display
  delay(200);
  //display.clearDisplay(); //Clears the buffer
  //display.setTextSize(1);
  //display.setFont(&FreeSans9pt7b);
  //display.setTextColor(EPD_RED);
  //display.setCursor(0, 0);
  //display.print("Welcome Comrade");
  //display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Your IP address is: ");
  Serial.println(WiFi.localIP());
  Serial.println("Connected ");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  delay(1000);

}



void loop()
{
  if (client.connect(servername, 80))
  { //starts client connection, checks for connection
    client.println("GET /data/2.5/weather?id=" + cityID + "&units=metric&APPID=" + apikey);


    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
  }
  else {
    Serial.println("connection failed");
    Serial.println();
  }

  while (client.connected() && !client.available())
    delay(1);                                          //patience young skywalker
  while (client.connected() || client.available())
  { //connected or data available
    char c = client.read();                     //grabs the next byte from the connected stream
    result = result + c;
  }

  client.stop();
  result.replace('[', ' ');
  result.replace(']', ' ');
  //Serial.println(result);
  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  StaticJsonDocument<1024> doc;
  DeserializationError  error = deserializeJson(doc, jsonArray);


  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }

/*
 * Getting ready to work with the API results
 *
 */

  String location = doc["name"];
  String country = doc["sys"]["country"];
  int temperature = doc["main"]["temp"];
  int humidity = doc["main"]["humidity"];
  String weather = doc["main"]["description"];
  int id = doc["id"];
  float Speed = doc["wind"]["speed"];
  float longitude = doc["coord"]["lon"];
  float latitude = doc["coord"]["lat"];
  String forecast = doc["weather"]["description"];

  Serial.println();
  Serial.print("Country: ");
  Serial.println(country);
  Serial.print("Location: ");
  Serial.println(location);
  Serial.print("Location ID: ");
  Serial.println(id);
  Serial.printf("Temperature: %d°C\r\n", temperature);
  Serial.printf("Humidity: %d %%\r\n", humidity);
  Serial.printf("Wind speed: %.1f m/s\r\n", Speed);
  Serial.printf("Longitude: %.2f\r\n", longitude);
  Serial.printf("Latitude: %.2f\r\n", latitude);
  Serial.printf("Forecast is: ");
  Serial.println(forecast);

 
  display.clearDisplay();
  display.setCursor(0, 12);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(EPD_RED);
  display.setTextSize(1);
  display.print("Todays weather for: ");
  display.println(location);
  void drawLine(uint16_t x13, uint16_t y0, uint16_t x30, uint16_t y1, uint16_t black); //draw a black border around the screen
  display.print("Temp: ");
  display.print(temperature);
  display.print((char)247);
  display.print("C & ");
  display.print("Humidity: ");
  display.print(humidity);
  display.println("% ");
  display.print("Wind Speed: ");
  display.print(Speed);
  display.print("m/s  ");
  display.println((char)247);
  display.print("Lat: ");
  display.print(latitude);
  display.print(" & ");
  display.print("Long: ");
  display.println(longitude);
  display.print("Forecast looks like: ");
  display.setTextColor(EPD_BLACK);
  display.println(forecast);
  
  display.display();
  delay(1.8e+6);
}