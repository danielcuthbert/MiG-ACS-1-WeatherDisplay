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
#include "time.h"


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


/*
 * Here we store the converted images 
 */

const unsigned char Overcast [] PROGMEM = {
  // overcast, 32x32px
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 
  0x04, 0x0c, 0x08, 0x00, 0x06, 0x0c, 0x18, 0x00, 0x03, 0x00, 0x30, 0x00, 0x01, 0x00, 0x60, 0x00, 
  0x00, 0x3e, 0x00, 0x00, 0x00, 0x79, 0xf0, 0x00, 0x00, 0x77, 0xfc, 0x00, 0x18, 0xee, 0x0e, 0x00, 
  0x3c, 0xd8, 0x07, 0xe0, 0x00, 0xd8, 0x03, 0xf0, 0x00, 0x30, 0x00, 0x38, 0x00, 0xf0, 0x00, 0x18, 
  0x01, 0xc0, 0x00, 0x0c, 0x01, 0x80, 0x00, 0x0c, 0x01, 0x80, 0x00, 0x0c, 0x05, 0x80, 0x00, 0x08, 
  0x05, 0x80, 0x40, 0x18, 0x01, 0xe1, 0xc0, 0x70, 0x00, 0xf3, 0xdf, 0xe0, 0x00, 0x07, 0x80, 0x00, 
  0x00, 0x4f, 0x82, 0x00, 0x00, 0xc7, 0x86, 0x00, 0x01, 0xc7, 0x0c, 0x00, 0x01, 0x80, 0x0c, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

const unsigned char Sun [] PROGMEM = {
 // 'sun', 24x24px
0xff, 0xe7, 0xff, 0xff, 0xe7, 0xff, 0xdf, 0x66, 0xfb, 0xe7, 0x66, 0xe7, 0xf3, 0x7e, 0xe7, 0xf3, 
0xff, 0xcf, 0xff, 0x81, 0xff, 0xff, 0x3c, 0xff, 0xc6, 0x7e, 0x63, 0xfc, 0xff, 0x3f, 0xfd, 0xff, 
0xbf, 0x0d, 0xff, 0xb0, 0x0d, 0xff, 0xb0, 0xfd, 0xff, 0xbf, 0xfc, 0xff, 0x3f, 0xc6, 0x7e, 0x63, 
0xff, 0x3c, 0xff, 0xff, 0x81, 0xff, 0xf3, 0xff, 0xcf, 0xf3, 0x7e, 0xe7, 0xe7, 0x66, 0xe7, 0xdf, 
0x66, 0xfb, 0xff, 0xe7, 0xff, 0xff, 0xe7, 0xff
};

/*
 * Setting up the WiFi & OpenWeatherMap API and time
 *
 */

bool id = false;
WiFiClient client;
char servername[] = "api.openweathermap.org";
unsigned long lastTime = 0;
unsigned long timerDelay = 1.8e+6; // Let's check every 30 minutes
String result;
// updating every hour rather than hammering the API
const unsigned long SECOND = 1000; 
const unsigned long HOUR = 3600*SECOND;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

void printLocalTime()
  {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    
  }


void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  display.begin(); // Start the display
  delay(200);
  
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

    //Grab the time from the NTP server and print it 
    //TODO: work out how to make this display via display.print 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
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
 */

 String location = doc["name"];
 String country = doc["sys"]["country"];
 int temperature = doc["main"]["temp"];
 int humidity = doc["main"]["humidity"];
 String weather = doc["main"]["description"];
 int id = doc["id"];
 float Speed = doc["wind"]["speed"];
 int Feelslike = doc["main"]["feels_like"];
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
 Serial.printf("Forecast is: ");
 Serial.println(forecast);

 display.clearDisplay();
 void drawLine(uint16_t x13, uint16_t y0, uint16_t x30, uint16_t y1, uint16_t black); //draw a black border around the screen
 display.setCursor(0, 12);
 display.setFont(&FreeSans9pt7b);
 display.setTextColor(EPD_BLACK);
 display.setTextSize(1);
 display.print("Todays weather for: ");
 display.println(location);
 display.drawLine(0, 22, 296, 22, EPD_BLACK);
 display.setCursor(0, 45);
 display.setTextColor(EPD_RED);
 display.setTextSize(1);
 display.print("Temp: ");
 display.print(temperature, 1);
 display.print((char)248);
 display.print("C & ");
 display.print("Humidity: ");
 display.print(humidity);
 display.println("% ");
 display.print("It actually feels like: ");
 display.print(round(Feelslike));
 display.print((char)248);
 display.println("C");
 display.print("The wind is blowing at: ");
 display.print(Speed);
 display.print("m/s  ");
 display.println((char)247);
 display.setCursor(0, 110);
 display.print("Forecast looks like: ");
 display.setTextColor(EPD_BLACK);
 display.println(forecast);
 display.drawBitmap(250, 90, Overcast, 24, 24, EPD_BLACK);
 display.display();
 delay(3.6e+6);
}