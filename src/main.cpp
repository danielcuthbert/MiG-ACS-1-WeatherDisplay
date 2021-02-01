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
#include <Fonts/FreeMonoBold12pt7b.h>
#include "epaper_fonts.h"


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
#define SCREEN_WIDTH  296
#define SCREEN_HEIGHT 
enum alignment {LEFT, RIGHT, CENTER};

/*
* Here we set up stuff to better display the results from the API
*/
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  display.setCursor(x, y + h);
  display.print(text);
}

/*
* Sorting out alignment issues - a work in progress
*/
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  if (text.length() > text_width * 2) text = text.substring(0, text_width * 2); // Truncate if too long for 2 rows of text
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  display.setCursor(x, y);
  display.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    display.setCursor(x, y + h);
    display.println(text.substring(text_width));
  }
}

/*
 * Here we store the converted images 
 */
const unsigned char HighTemperature [] PROGMEM = {
// 'high-temperature', 16x16px
0xff, 0xff, 0xf5, 0xbf, 0xfb, 0xf7, 0xfb, 0x5b, 0xfb, 0x77, 0xfb, 0x77, 0xfb, 0xed, 0xfb, 0xc7,
0xfb, 0xff, 0xfb, 0xff, 0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 
};

const unsigned char HumiditySymbol [] PROGMEM = {
// 'humidity', 16x16px
0xf9, 0xff, 0xfa, 0xff, 0xf6, 0x7f, 0xef, 0x63, 0xef, 0xab, 0xdf, 0x9d, 0xdf, 0xdd, 0xbf, 0xdd,
0xbe, 0xed, 0xbd, 0xed, 0xfd, 0xe3, 0xba, 0xef, 0xbf, 0xdf, 0xdf, 0xdf, 0xcf, 0x3f, 0xf0, 0xff, 
};

const unsigned char Cloudy [] PROGMEM = {
// 'cloudy', 24x24px
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0x7c, 0x1f, 0x7f, 0x18, 0xc6, 0x7f, 0xf3, 0xf7, 0xff, 0xe7, 0xf1, 0xff, 0xe7, 0xc0,
0x7f, 0xef, 0xdf, 0x3f, 0xe7, 0x9f, 0x9f, 0xe7, 0xbf, 0xc3, 0xf3, 0x3f, 0xf9, 0x18, 0x3f, 0xfc,
0x79, 0xff, 0xfc, 0xf9, 0xff, 0xfe, 0xfb, 0xff, 0xfc, 0xf9, 0xff, 0xfc, 0xfc, 0xff, 0xf9, 0xfe,
0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
};

const unsigned char Hot [] PROGMEM = {
// 'hot', 24x24px
0xfc, 0x3b, 0xff, 0xf8, 0x13, 0xff, 0xfb, 0x9f, 0xdf, 0xfa, 0x5f, 0x9f, 0xfa, 0x58, 0xff, 0xfa,
0x5c, 0x7f, 0xfa, 0x5e, 0x7f, 0xfa, 0x5f, 0x67, 0xfa, 0x5f, 0x67, 0xfa, 0x5f, 0x7f, 0xfa, 0x5e,
0x7f, 0xfa, 0x58, 0xff, 0xfa, 0x5f, 0x9f, 0xfa, 0x5f, 0xdf, 0xfa, 0x53, 0xff, 0xf2, 0x43, 0xff,
0xf6, 0x6f, 0xff, 0xe4, 0x67, 0xff, 0xec, 0x37, 0xff, 0xec, 0x37, 0xff, 0xe6, 0x67, 0xff, 0xf7,
0xcf, 0xff, 0xf1, 0x9f, 0xff, 0xfc, 0x3f, 0xff, 
};

const unsigned char Rain [] PROGMEM = {
// 'rain', 24x24px
0xff, 0xc3, 0xff, 0xff, 0x99, 0xff, 0xf0, 0x3c, 0x0f, 0xe7, 0x7e, 0xe7, 0xc7, 0xff, 0xe3, 0x9f,
0xff, 0xf9, 0x3f, 0xff, 0xfc, 0x7f, 0xff, 0xfe, 0x3f, 0xff, 0xfc, 0x8f, 0xff, 0xf1, 0xc7, 0xff,
0xe3, 0xe7, 0x7e, 0xe7, 0xf0, 0x3c, 0x0f, 0xff, 0x81, 0xff, 0xf7, 0xc3, 0xef, 0xe3, 0xff, 0xc7,
0xe1, 0xff, 0x87, 0xe9, 0xff, 0x97, 0xe1, 0xff, 0x87, 0xf3, 0xe7, 0xcf, 0xff, 0xc3, 0xff, 0xff,
0xdb, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xe7, 0xff, 
};

const unsigned char Sun [] PROGMEM = {
// 'sun', 24x24px
0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xe7, 0xff, 0xf3, 0xff, 0xcf, 0xf3,
0xff, 0xcf, 0xff, 0xc3, 0xff, 0xff, 0x00, 0xff, 0xfe, 0x7e, 0x7f, 0xfe, 0xff, 0x7f, 0xfc, 0xff,
0x3f, 0x8c, 0xff, 0x31, 0x0c, 0xff, 0x31, 0xfc, 0xff, 0x3f, 0xfe, 0xff, 0x7f, 0xfe, 0x7e, 0x7f,
0xff, 0x00, 0xff, 0xff, 0xc3, 0xff, 0xf3, 0xff, 0xcf, 0xf3, 0xff, 0xcf, 0xff, 0xe7, 0xff, 0xff,
0xe7, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 
};

const unsigned char Wind [] PROGMEM = {
// 'wind', 24x24px
0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xff, 0xfe, 0x03, 0xff, 0xfc, 0xf9, 0xff, 0xf9, 0xfc, 0xff,
0xf9, 0xfc, 0xff, 0xfb, 0xfc, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xf9, 0xff, 0xff,
0xe3, 0x80, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0x80, 0x00, 0x7f,
0xff, 0xff, 0x3f, 0xff, 0xff, 0x3f, 0xff, 0x9f, 0x3f, 0xff, 0x9f, 0x3f, 0xff, 0xcf, 0x3f, 0xff,
0xc0, 0x7f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff,
};

/*
 * Setting up the WiFi & OpenWeatherMap API and time
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

 void drawRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);
 void fillRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);
 //display.print("Todays weather for: ");
 //display.println(location);
 drawString(SCREEN_WIDTH / 2, 2, location, CENTER);
 
 display.drawLine(0, 22, 296, 22, EPD_BLACK);
 display.setCursor(0, 45);
 display.setTextColor(EPD_RED);
 display.setTextSize(1);
 
 display.print("Temp: ");
 display.print(temperature, 1);
 display.print((char)248);
 display.print("C & ");
 //display.drawBitmap(60, 30, HumiditySymbol, 16, 16, EPD_BLACK);
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
 //display.drawBitmap(250, 110, PartiallyCloudy, 24, 24, EPD_BLACK);
 display.display();
 delay(3.6e+6);
}