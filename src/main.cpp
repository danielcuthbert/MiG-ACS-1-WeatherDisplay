/***************************************************
 Weather station code for a Russian ACS-1 Mig-25
 Version 0.4
 ****************************************************/

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_EPD.h"    // Core Adafruit Display library
#include <WiFi.h>            // The Wifi stuffs
#include <HTTPClient.h>     // Needed for OpenWeather API
#include <ArduinoJson.h>   // Needed to format
#include "credentials.h"   // where we store secrets
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "epaper_fonts.h"
#include <ezTime.h> //brilliant time library



/*
 Setting up the 2.9 E-ink Display. This is with a ESP32 Dev Board with the following pinout
 VIN --> 3V3
 GND --> GND
 SCK --> 18
 MISO --> 19
 MOSI --> 23
 ECS --> 33
 D/C --> 12
 SRCS --> 27
 RST --> 32
 BUSY --> 14
 */


#define EPD_CS     33
#define EPD_DC     12
#define SRAM_CS    27
#define EPD_RESET  32 
#define EPD_BUSY   14 
#define SCREEN_WIDTH  296
#define SCREEN_HEIGHT 128
#define UK_POSIX	"BST0GMT,M3.2.0/2:00:00,M11.1.0/2:00:00"
enum alignment {LEFT, RIGHT, CENTER};
Adafruit_IL0373 display(296, 128, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
Timezone local;
Timezone UK;



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
void updateNTP();


 /*
The main setup() function is called once at startup. 
 */

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

    //Grab the time from the NTP server and print it 
  UK.setLocation("Europe/London");
}


 /*
The main loop() function is called repeatedly.
 */


void loop()
{
  Serial.println("Connecting to the weather API()");
  if (client.connect(servername, 80))
  { //starts client connection, checks for connection
    client.println("GET /data/2.5/weather?id=" + cityID + "&units=metric&APPID=" + apikey);
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
    events();
  }
  else {
    Serial.println("connection failed");
    Serial.println();
  }

  while (client.connected() && !client.available())
    delay(1);                                          //patience young skywalker
  while (client.connected() || client.available())

 /*
Once we have the data from the Weather API, we store it in a String called result.
This grabs the next byte from the connected stream. 
There was a bug where I didn't clear the results string, which resulted in it getting filled and not updating
 */

  { 
    char c = client.read();                     
    result = result + c;
  }

  client.stop();
  result.replace('[', ' ');
  result.replace(']', ' ');
  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  StaticJsonDocument<1024> doc;
  DeserializationError  error = deserializeJson(doc, jsonArray);
  result.clear(); 
  Serial.println("Clearing the result buffer");

  if (error) 
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }


 /*
Getting ready to work with the API results
 */

 const char *location = doc["name"];
 const char *country = doc["sys"]["country"];
 int temperature = doc["main"]["temp"];
 int humidity = doc["main"]["humidity"];
 int id = doc["id"];
 float Speed = doc["wind"]["speed"];
 int Feelslike = doc["main"]["feels_like"];
 const char *forecast = doc["weather"]["description"];

 /*
 Here we are just printing to serial to make sure all is good
 */
 
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

 /*
Start preparing the display by clearing it and setting the font/size/colour etc. 
 */

 display.clearDisplay();
 void drawLine(uint16_t x13, uint16_t y0, uint16_t x30, uint16_t y1, uint16_t black); //draw a black border around the screen
 display.setCursor(2, 12);
 display.setFont(&FreeSans9pt7b);
 display.setTextColor(EPD_BLACK);
 display.setTextSize(1);

 void drawRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);
 void fillRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);
 drawString(SCREEN_WIDTH / 4, 2, UK.dateTime(), CENTER);
 
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
 display.display();
 delay(10*60*1000UL);

}